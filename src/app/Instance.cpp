#include "app/Instance.h"

#include "app/Logging.h"
#include "app/SettingsDialog.h"
#include "app/ShortcutActions.h"
#include "core/tts/SherpaTtsEngine.h"
#include "core/tts/VoiceRegistry.h"
#include "platform/Autostart.h"
#include "platform/GlobalShortcuts.h"
#include "platform/Platform.h"
#include "platform/TextSource.h"

#include <QFile>
#include <QFileInfo>
#include <QKeySequence>

using namespace Qt::StringLiterals;

namespace legilo {

Instance::Instance(QObject *parent)
    : QObject(parent)
    , m_source(createTextSource())
    , m_controller(&m_output)
    , m_speaker(m_source.get(), &m_controller, &m_settings)
    , m_tray(&m_speaker, &m_controller)
{
    m_controller.setSpeed(m_settings.speed());
    connect(&m_tray, &TrayController::quitRequested, this, &Instance::quitRequested);
    connect(&m_tray, &TrayController::settingsRequested, this, &Instance::showSettings);
    connect(&m_settings, &Settings::changed, this, &Instance::onSettingChanged);
}

Instance::~Instance()
{
    m_speaker.stop();
    delete m_settingsDialog;
    // A voice still loading at quit finishes first: the thread cannot be
    // stopped part-way through sherpa-onnx's set-up.
    for (const QPointer<QThread> &loader : std::as_const(m_loaders)) {
        if (!loader.isNull()) {
            loader->wait();
        }
    }
}

void Instance::start()
{
    m_tray.show();
    registerShortcuts();
    loadVoice();
}

IpcReply Instance::handle(const IpcRequest &request)
{
    const QString &command = request.command;
    if (command == "speak-selection"_L1) {
        m_speaker.speakSelection();
    } else if (command == "speak-clipboard"_L1) {
        m_speaker.speakClipboard();
    } else if (command == "speak-pdf-text"_L1) {
        m_speaker.speakSelectionAsPdfText();
    } else if (command == "pause"_L1) {
        m_speaker.togglePause();
    } else if (command == "stop"_L1) {
        m_speaker.stop();
    } else if (command == "faster"_L1) {
        m_speaker.faster();
    } else if (command == "slower"_L1) {
        m_speaker.slower();
    } else if (command == "speak-text"_L1) {
        // Section 8: other programs may not inject text unless allowed.
        if (!m_settings.allowSpeakText()) {
            return {false, tr("Speaking text sent by other programs is turned off. Set "
                              "ipc/allowSpeakText in the settings to allow it.")};
        }
        TextCapture capture;
        capture.text = request.text;
        capture.source = CaptureSource::Pasted;
        m_speaker.speakText(capture);
    } else if (command == "read-file"_L1) {
        return readFile(request.path);
    } else if (command == "show"_L1) {
        // The reader arrives in stage 6; until then there is nothing to show.
    } else {
        return {false, tr("The command %1 is not available yet.").arg(command)};
    }
    return {};
}

// Speaks a plain-text file, read as UTF-8. Only existing regular files are
// accepted (design 8). The reader shows it from stage 6; until then it is
// spoken like any other text.
IpcReply Instance::readFile(const QString &path)
{
    if (!QFileInfo(path).isFile()) {
        return {false, tr("%1 is not a file.").arg(path)};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {false, tr("Cannot read %1: %2").arg(path, file.errorString())};
    }
    TextCapture capture;
    capture.text = QString::fromUtf8(file.readAll());
    capture.source = CaptureSource::File;
    m_speaker.speakText(capture);
    return {};
}

void Instance::registerShortcuts()
{
    m_shortcuts = createGlobalShortcuts();
    connect(m_shortcuts.get(), &GlobalShortcuts::activated, this, &Instance::onShortcut);
    for (const ShortcutAction &action : shortcutActions()) {
        m_shortcuts->registerAction(action.id, action.label, action.defaultSequence);
    }
}

// The shortcut IDs are the IPC commands, except pause-resume.
void Instance::onShortcut(const QString &id)
{
    qCDebug(lcApp) << "Shortcut" << id;
    handle({id == "pause-resume"_L1 ? u"pause"_s : id, {}, {}});
}

// Opens the settings dialog, or raises it if it is open already.
void Instance::showSettings()
{
    if (m_settingsDialog.isNull()) {
        if (!m_autostart) {
            m_autostart = createAutostart();
        }
        const VoiceRegistry registry(VoiceRegistry::defaultDirectory());
        m_settingsDialog = new SettingsDialog(&m_settings, m_shortcuts.get(), m_autostart.get(),
                                              registry.voices());
        m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

// Settings changed in the dialog take effect at once: a new voice loads in
// the background, and a new speed applies from the next sentence (7.5).
void Instance::onSettingChanged(const QString &key)
{
    if (key == "voice/default"_L1) {
        loadVoice();
    } else if (key == "voice/speed"_L1) {
        m_controller.setSpeed(m_settings.speed());
    }
}

// Loads the chosen voice, or the first installed one, on a thread of its
// own: it takes one to three seconds (stage 0 results, 12).
void Instance::loadVoice()
{
    const VoiceRegistry registry(VoiceRegistry::defaultDirectory());
    if (registry.voices().isEmpty()) {
        m_tray.showNotice(Speaker::Notice::Problem, tr("No voices are installed"),
                          tr("Install one with legilo --install-voice."));
        return;
    }
    auto voice = registry.find(m_settings.defaultVoice());
    if (!voice) {
        if (!m_settings.defaultVoice().isEmpty()) {
            qCWarning(lcApp) << "The voice" << m_settings.defaultVoice()
                             << "is not installed; using the first installed voice";
        }
        voice = registry.voices().first();
    }
    const int threads = SherpaTtsEngine::threadsFor(0, QThread::idealThreadCount());
    auto loaded = std::make_shared<std::shared_ptr<TtsEngine>>();
    QThread *loader = QThread::create(
        [loaded, voice = *voice, threads] { *loaded = SherpaTtsEngine::load(voice, threads); });
    const quint64 load = ++m_voiceLoads;
    connect(loader, &QThread::finished, this, [this, loaded, load, id = voice->id] {
        // A voice chosen since has its own load under way; this one is stale.
        if (load != m_voiceLoads) {
            return;
        }
        if (*loaded) {
            qCDebug(lcApp) << "Loaded the voice" << id;
            m_speaker.setEngine(*loaded);
        } else {
            m_tray.showNotice(Speaker::Notice::Problem, tr("Legilo could not load its voice"),
                              tr("The voice %1 could not be loaded.").arg(id));
        }
    });
    connect(loader, &QThread::finished, loader, &QObject::deleteLater);
    m_loaders.removeAll(nullptr);
    m_loaders.append(loader);
    loader->start();
}

} // namespace legilo
