#include "app/Instance.h"

#include "app/Logging.h"
#include "core/tts/SherpaTtsEngine.h"
#include "core/tts/VoiceRegistry.h"
#include "platform/GlobalShortcuts.h"
#include "platform/Platform.h"
#include "platform/TextSource.h"

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
}

Instance::~Instance()
{
    m_speaker.stop();
    // A voice still loading at quit finishes first: the thread cannot be
    // stopped part-way through sherpa-onnx's set-up.
    if (!m_loader.isNull()) {
        m_loader->wait();
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
    } else if (command == "pause"_L1) {
        m_speaker.togglePause();
    } else if (command == "stop"_L1) {
        m_speaker.stop();
    } else if (command == "show"_L1) {
        // The reader arrives in stage 6; until then there is nothing to show.
    } else {
        return {false, tr("The command %1 is not available yet.").arg(command)};
    }
    return {};
}

// The actions of design 7.6.2 that this stage carries out, with their
// Linux defaults. The others are registered as they are built.
void Instance::registerShortcuts()
{
    m_shortcuts = createGlobalShortcuts();
    connect(m_shortcuts.get(), &GlobalShortcuts::activated, this, &Instance::onShortcut);
    m_shortcuts->registerAction(u"speak-selection"_s, tr("Speak selection"),
                                QKeySequence(u"Meta+Alt+S"_s));
    m_shortcuts->registerAction(u"speak-clipboard"_s, tr("Speak clipboard"),
                                QKeySequence(u"Meta+Alt+C"_s));
    m_shortcuts->registerAction(u"pause-resume"_s, tr("Pause or resume"),
                                QKeySequence(u"Meta+Alt+P"_s));
    m_shortcuts->registerAction(u"stop"_s, tr("Stop"), QKeySequence(u"Meta+Alt+X"_s));
}

void Instance::onShortcut(const QString &id)
{
    qCDebug(lcApp) << "Shortcut" << id;
    if (id == "speak-selection"_L1) {
        m_speaker.speakSelection();
    } else if (id == "speak-clipboard"_L1) {
        m_speaker.speakClipboard();
    } else if (id == "pause-resume"_L1) {
        m_speaker.togglePause();
    } else if (id == "stop"_L1) {
        m_speaker.stop();
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
    connect(loader, &QThread::finished, this, [this, loaded, id = voice->id] {
        if (*loaded) {
            qCDebug(lcApp) << "Loaded the voice" << id;
            m_speaker.setEngine(*loaded);
        } else {
            m_tray.showNotice(Speaker::Notice::Problem, tr("Legilo could not load its voice"),
                              tr("The voice %1 could not be loaded.").arg(id));
        }
    });
    connect(loader, &QThread::finished, loader, &QObject::deleteLater);
    m_loader = loader;
    loader->start();
}

} // namespace legilo
