#include "app/Speaker.h"

#include "app/Settings.h"
#include "core/playback/PlaybackController.h"
#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "platform/TextSource.h"

#include <QRegularExpression>

#include <utility>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

// How many words of the text a started notification shows.
constexpr qsizetype NoticeWords = 8;

QString firstWords(const QString &text)
{
    static const QRegularExpression space(u"\\s+"_s,
                                          QRegularExpression::UseUnicodePropertiesOption);
    const QStringList words = text.split(space, Qt::SkipEmptyParts);
    QString shown = words.mid(0, NoticeWords).join(u' ');
    if (words.size() > NoticeWords) {
        shown += u"..."_s;
    }
    return shown;
}

} // namespace

Speaker::Speaker(TextSource *source, PlaybackController *controller, const Settings *settings,
                 QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_controller(controller)
    , m_settings(settings)
{
    connect(m_controller, &PlaybackController::error, this, [this](const QString &message) {
        emit notify(Notice::Problem, tr("Legilo could not speak"), message);
    });
}

void Speaker::setEngine(std::shared_ptr<TtsEngine> engine)
{
    m_engine = std::move(engine);
    m_controller->setEngine(m_engine);
    playWhenReady();
}

void Speaker::speakSelection()
{
    request(CaptureSource::Selection);
}

void Speaker::speakClipboard()
{
    request(CaptureSource::Clipboard);
}

void Speaker::togglePause()
{
    switch (m_controller->state()) {
    case PlaybackController::State::Starting:
    case PlaybackController::State::Playing:
        m_controller->pause();
        if (m_settings->notifyPause()) {
            emit notify(Notice::Paused, tr("Paused"), tr("Pause again to resume."));
        }
        break;
    case PlaybackController::State::Paused:
        m_controller->resume();
        break;
    case PlaybackController::State::Idle:
        break;
    }
}

void Speaker::stop()
{
    m_waiting.reset();
    m_controller->stop();
}

bool Speaker::isSpeaking() const
{
    return m_waiting.has_value() || m_controller->state() != PlaybackController::State::Idle;
}

void Speaker::request(CaptureSource source)
{
    if (isSpeaking() && m_settings->speakWhileSpeaking() == Settings::SpeakWhileSpeaking::Stop) {
        stop();
        return;
    }
    if (source == CaptureSource::Clipboard) {
        m_source->readClipboard().then(
            this, [this](const TextCapture &capture) { speak(capture, false); });
        return;
    }
    m_source->readSelection().then(this, [this](const TextCapture &capture) {
        if (capture.text.trimmed().isEmpty() && m_settings->selectionFallbackToClipboard()) {
            m_source->readClipboard().then(
                this, [this](const TextCapture &clipboard) { speak(clipboard, true); });
            return;
        }
        speak(capture, false);
    });
}

void Speaker::speak(const TextCapture &capture, bool fellBack)
{
    // The clean-up settings come from the settings with the Clean-up page.
    const Document document = CleanupPipeline().run(capture, CleanupSettings{});
    QStringList segments = spokenSegments(document);
    if (segments.isEmpty()) {
        QString message;
        if (fellBack) {
            message = tr("Nothing is selected, and the clipboard is empty.");
        } else if (capture.source == CaptureSource::Clipboard) {
            message = tr("The clipboard holds no text.");
        } else {
            message = tr("Nothing is selected.");
        }
        emit notify(Notice::Problem, tr("Nothing to read"), message);
        return;
    }
    notifyStarted(capture, segments.join(u' '), fellBack);
    m_waiting = std::move(segments);
    playWhenReady();
}

void Speaker::notifyStarted(const TextCapture &capture, const QString &spoken, bool fellBack)
{
    // FR-03: falling back to the clipboard is announced even when started
    // notifications are off, so the user knows what is being read.
    const bool source = m_settings->notifySource();
    if (!m_settings->notifyStart() && !(fellBack && source)) {
        return;
    }
    QString title = tr("Reading");
    if (fellBack && source) {
        title = tr("Nothing selected: reading the clipboard");
    } else if (source) {
        title = capture.source == CaptureSource::Clipboard ? tr("Reading the clipboard")
                                                           : tr("Reading the selection");
    }
    emit notify(Notice::Started, title, firstWords(spoken));
}

void Speaker::playWhenReady()
{
    if (!m_engine) {
        return;
    }
    if (m_waiting.has_value()) {
        const QStringList segments = *m_waiting;
        m_waiting.reset();
        m_controller->play(segments);
    }
}

} // namespace legilo
