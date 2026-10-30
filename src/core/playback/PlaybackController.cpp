#include "core/playback/PlaybackController.h"

#include "core/Logging.h"
#include "core/audio/AudioOutput.h"
#include "core/tts/TtsEngine.h"

#include <algorithm>

namespace legilo {

namespace {

// Room for a minute of audio at any rate a voice uses.
constexpr qsizetype QueueFrames = qsizetype{48000} * 60;
constexpr int PollIntervalMs = 30;

} // namespace

PlaybackController::PlaybackController(AudioOutput *output, SynthesisWorker::Settings settings,
                                       QObject *parent)
    : QObject(parent)
    , m_output(output)
    , m_queue(QueueFrames)
    , m_worker(new SynthesisWorker(&m_queue, settings))
{
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &PlaybackController::startRequested, m_worker, &SynthesisWorker::start);
    connect(this, &PlaybackController::restartRequested, m_worker, &SynthesisWorker::restartFrom);
    connect(this, &PlaybackController::stopRequested, m_worker, &SynthesisWorker::stop);
    connect(m_worker, &SynthesisWorker::firstAudioQueued, this,
            &PlaybackController::onFirstAudioQueued);
    connect(m_worker, &SynthesisWorker::finished, this, &PlaybackController::onWorkerFinished);
    connect(m_worker, &SynthesisWorker::failed, this, &PlaybackController::onWorkerFailed);
    connect(m_output, &AudioOutput::deviceLost, this, &PlaybackController::pause);
    m_pollTimer.setInterval(PollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &PlaybackController::poll);
    m_thread.start();
}

PlaybackController::~PlaybackController()
{
    // The device pulls from m_queue, so it must close first.
    m_output->close();
    m_thread.quit();
    m_thread.wait();
}

void PlaybackController::setEngine(std::shared_ptr<TtsEngine> engine)
{
    m_engine = std::move(engine);
}

void PlaybackController::play(const QStringList &segments, int startSegment)
{
    if (!m_engine) {
        emit error(tr("No voice is loaded."));
        return;
    }
    m_output->stop();
    const int sampleRate = m_engine->sampleRate();
    if (sampleRate != m_openSampleRate) {
        if (!m_output->open(sampleRate, &m_queue)) {
            setState(State::Idle);
            emit error(tr("The audio device could not be opened."));
            return;
        }
        m_openSampleRate = sampleRate;
    }
    const auto generation = m_queue.requestFlush(-1);
    if (!generation) {
        emit error(tr("Playback could not be restarted."));
        return;
    }
    m_generation = *generation;
    m_documentGeneration = *generation;
    m_startSegment = startSegment;
    m_currentSegment = -1;
    m_synthesisDone = false;
    emit startRequested(m_engine, segments, startSegment, m_generation, m_speed, QPrivateSignal());
    setState(State::Starting);
    m_pollTimer.start();
}

void PlaybackController::pause()
{
    if (m_state == State::Playing || m_state == State::Starting) {
        m_output->stop();
        setState(State::Paused);
    }
}

void PlaybackController::resume()
{
    if (m_state == State::Paused) {
        m_output->start();
        setState(State::Playing);
    }
}

void PlaybackController::stop()
{
    if (m_state == State::Idle) {
        return;
    }
    m_output->stop();
    // A new generation also makes the worker's late signals stale.
    if (const auto generation = m_queue.requestFlush(-1)) {
        m_generation = *generation;
    }
    emit stopRequested(QPrivateSignal());
    m_pollTimer.stop();
    setState(State::Idle);
}

void PlaybackController::setSpeed(double speed)
{
    m_speed = std::clamp(speed, 0.5, 2.5);
    if (m_state == State::Idle) {
        return;
    }
    // Keep the segment being heard; synthesise the rest again. Before any
    // of this document is heard, start again from its first segment.
    const int playing =
        m_queue.playingGeneration() >= m_documentGeneration ? m_queue.currentSegment() : -1;
    const int restartAt = playing >= 0 ? playing + 1 : m_startSegment;
    const auto generation = m_queue.requestFlush(playing);
    if (!generation) {
        emit error(tr("The speed could not be changed."));
        return;
    }
    m_generation = *generation;
    m_synthesisDone = false;
    emit restartRequested(restartAt, m_generation, m_speed, QPrivateSignal());
}

PlaybackController::State PlaybackController::state() const
{
    return m_state;
}

double PlaybackController::speed() const
{
    return m_speed;
}

int PlaybackController::currentSegment() const
{
    return m_currentSegment;
}

void PlaybackController::setState(State state)
{
    if (state != m_state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void PlaybackController::onFirstAudioQueued(quint64 generation)
{
    if (generation == m_generation && m_state == State::Starting) {
        m_output->start();
        setState(State::Playing);
    }
}

void PlaybackController::onWorkerFinished(quint64 generation)
{
    if (generation != m_generation) {
        return;
    }
    m_synthesisDone = true;
    // Nothing could be synthesised at all, so nothing will play.
    if (m_state == State::Starting) {
        m_pollTimer.stop();
        setState(State::Idle);
        emit documentFinished();
    }
}

void PlaybackController::onWorkerFailed(quint64 generation, int segment)
{
    if (generation == m_generation) {
        qCWarning(lcPlayback) << "Skipping segment" << segment << "- synthesis failed";
        emit error(tr("Sentence %1 could not be spoken and was skipped.").arg(segment + 1));
    }
}

// Follows the audio being heard, and finishes the document once the
// worker is done and the queue has played out.
void PlaybackController::poll()
{
    if (m_state != State::Playing) {
        return;
    }
    if (m_queue.playingGeneration() >= m_documentGeneration) {
        const int segment = m_queue.currentSegment();
        if (segment != m_currentSegment) {
            m_currentSegment = segment;
            qCDebug(lcPlayback) << "Playing segment" << segment;
            emit segmentChanged(segment);
        }
    }
    // Playback has caught up with synthesis: the listener hears a gap.
    const bool dry = !m_synthesisDone && m_queue.queuedFrames() == 0;
    if (dry && !m_ranDry) {
        qCDebug(lcPlayback) << "Queue ran dry in segment" << m_currentSegment;
    }
    m_ranDry = dry;
    if (m_synthesisDone && m_queue.queuedFrames() == 0) {
        m_output->stop();
        m_pollTimer.stop();
        setState(State::Idle);
        emit documentFinished();
    }
}

} // namespace legilo
