#pragma once

#include <QtGlobal>

#include <atomic>
#include <optional>
#include <span>
#include <vector>

namespace legilo {

// The audio waiting to be played, between the synthesis worker and the
// audio device's real-time callback (design 7.4). Three threads use it,
// each through its own part of the interface:
//
// - The synthesis worker (the producer) pushes chunks of samples, each
//   tagged with its segment and the generation it was synthesised for.
// - The controller requests flushes, for stop, seek, speed and voice
//   changes. Each flush starts a new generation.
// - The audio callback (the consumer) pulls samples. It alone discards
//   audio, so neither of the others touches samples it may be reading.
//
// Nothing here locks or allocates after construction, so pull() is safe
// on a real-time thread. Flush requests reach the callback in order
// through a queue of their own: applying only the latest would let a seek
// followed quickly by a speed change keep audio from before the seek.
class AudioQueue
{
public:
    // Holds up to capacityFrames samples in up to maxChunks chunks, with
    // up to maxPendingFlushes flushes waiting for the callback.
    explicit AudioQueue(qsizetype capacityFrames, qsizetype maxChunks = 256,
                        qsizetype maxPendingFlushes = 64);

    AudioQueue(const AudioQueue &) = delete;
    AudioQueue &operator=(const AudioQueue &) = delete;

    // Producer. Appends samples for segment, synthesised for generation.
    // Returns false, appending nothing, if they do not fit; the producer
    // waits for the callback to make room. Empty samples append nothing.
    bool push(quint64 generation, int segment, std::span<const float> samples);

    // Controller. Discards all queued audio except the chunks of segments
    // up to keepThroughSegment (-1 keeps none) that are still alive, and
    // starts a new generation, which it returns. Audio pushed later for an
    // older generation is discarded too. Returns nothing if the callback
    // has fallen maxPendingFlushes flushes behind, which happens only if
    // it has stopped running.
    std::optional<quint64> requestFlush(int keepThroughSegment);

    // The generation the producer should synthesise for: the latest flush's.
    quint64 generation() const;

    // Consumer. Fills out with queued audio, then silence once the queue
    // runs dry. Returns the number of frames of audio, not silence.
    qsizetype pull(std::span<float> out);

    // The segment of the audio most recently pulled, or -1 if none yet.
    int currentSegment() const;
    // Frames of audio pulled so far, not counting silence.
    qint64 framesPlayed() const;
    // Frames pushed but not yet pulled or discarded. Approximate while
    // other threads are at work.
    qint64 queuedFrames() const;

private:
    struct Chunk
    {
        quint64 generation = 0;
        int segment = 0;
        qint64 frames = 0;
        // Owned by the consumer once the chunk is published.
        qint64 consumed = 0;
        bool alive = true;
        quint64 checkedGeneration = 0; // the latest flush it survived
    };
    struct Flush
    {
        quint64 generation = 0;
        int keepThroughSegment = -1;
    };

    void applyFlushes();
    bool isAlive(const Chunk &chunk) const;

    std::vector<float> m_samples;
    std::vector<Chunk> m_chunks;
    std::vector<Flush> m_flushes;

    // Positions only ever grow; the index into each ring is the position
    // modulo its size.
    std::atomic<qint64> m_sampleWrite = 0; // producer
    std::atomic<qint64> m_sampleRead = 0;  // consumer
    std::atomic<qint64> m_chunkWrite = 0;  // producer
    std::atomic<qint64> m_chunkRead = 0;   // consumer
    std::atomic<qint64> m_flushWrite = 0;  // controller
    std::atomic<qint64> m_flushRead = 0;   // consumer

    std::atomic<quint64> m_generation = 0; // controller
    quint64 m_appliedGeneration = 0;       // consumer

    std::atomic<int> m_currentSegment = -1; // consumer
    std::atomic<qint64> m_framesPlayed = 0; // consumer
};

} // namespace legilo
