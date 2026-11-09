#include "core/playback/SynthesisWorker.h"

#include "core/audio/AudioQueue.h"
#include "core/playback/FirstChunk.h"
#include "core/tts/TtsEngine.h"

namespace legilo {

SynthesisWorker::SynthesisWorker(AudioQueue *queue, Settings settings, QObject *parent)
    : QObject(parent)
    , m_queue(queue)
    , m_settings(settings)
    , m_timer(this)
{
    // While there is work, check for room every few milliseconds; each
    // step synthesises at most one chunk, so commands are never kept
    // waiting for more than one.
    m_timer.setInterval(5);
    connect(&m_timer, &QTimer::timeout, this, &SynthesisWorker::step);
}

void SynthesisWorker::start(const std::shared_ptr<TtsEngine> &engine, const QStringList &segments,
                            int startSegment, quint64 generation, double speed)
{
    m_engine = engine;
    m_segments = segments;
    begin(startSegment, generation, speed);

    // Split the starting segment, so its first audio is ready quickly.
    if (!m_units.empty()) {
        const Unit first = m_units.front();
        const qsizetype length = firstChunkLength(first.text, m_settings.firstChunkChars);
        if (length < first.text.size()) {
            m_units.front().text = first.text.first(length).trimmed();
            m_units.insert(m_units.begin() + 1,
                           Unit{first.segment, first.text.sliced(length).trimmed()});
        }
    }
    step();
}

void SynthesisWorker::restartFrom(int segment, quint64 generation, double speed)
{
    begin(segment, generation, speed);
    step();
}

void SynthesisWorker::stop()
{
    m_timer.stop();
    m_units.clear();
    m_pendingUnit.reset();
    m_pendingSamples.clear();
}

void SynthesisWorker::begin(int segment, quint64 generation, double speed)
{
    stop();
    m_generation = generation;
    m_speed = speed;
    m_firstSegment = segment;
    m_lastPushedSegment = segment - 1;
    m_announced = false;
    for (int i = std::max(segment, 0); i < m_segments.size(); ++i) {
        m_units.push_back(Unit{i, m_segments.at(i)});
    }
    m_timer.start();
}

// Whether enough audio is queued beyond the segment playing. Before any
// audio of this generation has played, the segment before the first one
// counts as playing. Only this generation's audio counts: after a stop the
// discarded audio stays in the queue until the device starts again, and
// the device starts only once this generation's first audio is queued.
bool SynthesisWorker::lookAheadFull() const
{
    const int playing = m_queue->playingGeneration() == m_generation ? m_queue->currentSegment()
                                                                     : m_firstSegment - 1;
    if (m_lastPushedSegment - playing >= m_settings.lookAheadSegments) {
        return true;
    }
    const double queuedSeconds =
        static_cast<double>(m_queue->queuedFramesOf(m_generation)) / m_engine->sampleRate();
    return queuedSeconds >= m_settings.lookAheadSeconds;
}

void SynthesisWorker::step()
{
    // A flush has started a newer generation: this work is stale.
    if (m_queue->generation() != m_generation) {
        stop();
        return;
    }

    if (!m_pendingUnit) {
        if (m_units.empty() || lookAheadFull()) {
            return;
        }
        const Unit unit = m_units.front();
        m_units.erase(m_units.begin());
        auto samples = m_engine->synthesise(unit.text, m_speed);
        if (m_queue->generation() != m_generation) {
            stop();
            return;
        }
        if (!samples) {
            emit failed(m_generation, unit.segment);
        } else {
            m_pendingUnit = unit;
            m_pendingSamples = std::move(*samples);
        }
    }

    if (m_pendingUnit) {
        if (!m_queue->push(m_generation, m_pendingUnit->segment, m_pendingSamples)) {
            return; // no room yet; try again on the next step
        }
        m_lastPushedSegment = m_pendingUnit->segment;
        m_pendingUnit.reset();
        m_pendingSamples.clear();
        if (!m_announced) {
            m_announced = true;
            emit firstAudioQueued(m_generation);
        }
    }

    if (m_units.empty() && !m_pendingUnit) {
        m_timer.stop();
        emit finished(m_generation);
    }
}

} // namespace legilo
