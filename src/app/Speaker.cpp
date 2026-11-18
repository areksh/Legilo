#include "app/Speaker.h"

#include "app/Settings.h"
#include "core/playback/PlaybackController.h"
#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "platform/TextSource.h"

#include <QRegularExpression>

#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

// How many words of the text a started notification shows.
constexpr qsizetype NoticeWords = 8;
// How much the faster and slower actions change the speed (7.5).
constexpr double SpeedStep = 0.1;

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

Speaker::Speaker(TextSource *source, PlaybackController *controller, Settings *settings,
                 QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_controller(controller)
    , m_settings(settings)
{
    connect(m_controller, &PlaybackController::error, this, [this](const QString &message) {
        emit notify(Notice::Problem, tr("Legilo could not speak"), message);
    });
    connect(m_controller, &PlaybackController::segmentChanged, this,
            [this](int segment) { m_lastPosition = segment; });
    connect(m_controller, &PlaybackController::documentFinished, this,
            [this] { m_lastPosition = 0; });
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

void Speaker::speakSelectionAsPdfText()
{
    request(CaptureSource::Selection, true);
}

void Speaker::faster()
{
    changeSpeed(SpeedStep);
}

void Speaker::slower()
{
    changeSpeed(-SpeedStep);
}

void Speaker::changeSpeed(double step)
{
    // Rounded to tenths, so repeated steps do not drift (1.2000000000000002).
    const double speed = std::round((m_settings->speed() + step) * 10.0) / 10.0;
    m_settings->setSpeed(speed);
    m_controller->setSpeed(m_settings->speed());
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
        if (m_engine && !m_last.isEmpty() && !m_waiting) {
            m_controller->play(m_last, m_lastPosition);
        }
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

void Speaker::request(CaptureSource source, bool forcePdf)
{
    if (isSpeaking() && m_settings->speakWhileSpeaking() == Settings::SpeakWhileSpeaking::Stop) {
        stop();
        return;
    }
    if (source == CaptureSource::Clipboard) {
        m_source->readClipboard().then(
            this, [this](const TextCapture &capture) { speak(capture, false, false); });
        return;
    }
    m_source->readSelection().then(this, [this, forcePdf](const TextCapture &capture) {
        if (capture.text.trimmed().isEmpty() && m_settings->selectionFallbackToClipboard()) {
            m_source->readClipboard().then(this, [this, forcePdf](const TextCapture &clipboard) {
                speak(clipboard, true, forcePdf);
            });
            return;
        }
        speak(capture, false, forcePdf);
    });
}

void Speaker::speak(const TextCapture &capture, bool fellBack, bool forcePdf)
{
    CleanupSettings cleanup = m_settings->cleanup();
    if (forcePdf) {
        cleanup.reflowMode = ReflowMode::Always;
    }
    const Document document = CleanupPipeline().run(capture, cleanup);
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
        m_last = *m_waiting;
        m_lastPosition = 0;
        m_waiting.reset();
        m_controller->play(m_last);
    }
}

} // namespace legilo
