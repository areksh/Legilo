#include "core/audio/AudioQueue.h"

#include <algorithm>

namespace legilo {

AudioQueue::AudioQueue(qsizetype capacityFrames, qsizetype maxChunks, qsizetype maxPendingFlushes)
    : m_samples(capacityFrames)
    , m_chunks(maxChunks)
    , m_flushes(maxPendingFlushes)
{}

bool AudioQueue::push(quint64 generation, int segment, std::span<const float> samples)
{
    const auto frames = static_cast<qint64>(samples.size());
    if (frames == 0) {
        return true;
    }
    const auto capacity = static_cast<qint64>(m_samples.size());
    const qint64 sampleWrite = m_sampleWrite.load(std::memory_order_relaxed);
    const qint64 chunkWrite = m_chunkWrite.load(std::memory_order_relaxed);
    if (sampleWrite + frames - m_sampleRead.load(std::memory_order_acquire) > capacity ||
        chunkWrite - m_chunkRead.load(std::memory_order_acquire) >=
            static_cast<qint64>(m_chunks.size())) {
        return false;
    }

    // Copy in up to two parts, around the end of the ring.
    const qint64 start = sampleWrite % capacity;
    const qint64 firstPart = std::min(frames, capacity - start);
    std::copy_n(samples.begin(), firstPart, m_samples.begin() + start);
    std::copy_n(samples.begin() + firstPart, frames - firstPart, m_samples.begin());

    m_chunks[chunkWrite % static_cast<qint64>(m_chunks.size())] =
        Chunk{generation, segment, frames, 0, true, generation};
    m_sampleWrite.store(sampleWrite + frames, std::memory_order_release);
    m_chunkWrite.store(chunkWrite + 1, std::memory_order_release);
    return true;
}

std::optional<quint64> AudioQueue::requestFlush(int keepThroughSegment)
{
    const qint64 flushWrite = m_flushWrite.load(std::memory_order_relaxed);
    if (flushWrite - m_flushRead.load(std::memory_order_acquire) >=
        static_cast<qint64>(m_flushes.size())) {
        return std::nullopt;
    }
    const quint64 generation = m_generation.load(std::memory_order_relaxed) + 1;
    m_flushes[flushWrite % static_cast<qint64>(m_flushes.size())] =
        Flush{generation, keepThroughSegment};
    m_flushWrite.store(flushWrite + 1, std::memory_order_release);
    m_generation.store(generation, std::memory_order_release);
    return generation;
}

quint64 AudioQueue::generation() const
{
    return m_generation.load(std::memory_order_acquire);
}

// A chunk lives if no flush has discarded it, and it either belongs to the
// latest generation or survived the latest flush. A chunk pushed for an
// old generation after that flush was applied was never checked, so dies.
bool AudioQueue::isAlive(const Chunk &chunk) const
{
    return chunk.alive && (chunk.generation >= m_appliedGeneration ||
                           chunk.checkedGeneration >= m_appliedGeneration);
}

// Applies waiting flushes in order to every chunk not yet played out.
void AudioQueue::applyFlushes()
{
    const qint64 flushWrite = m_flushWrite.load(std::memory_order_acquire);
    qint64 flushRead = m_flushRead.load(std::memory_order_relaxed);
    if (flushRead == flushWrite) {
        return;
    }
    const qint64 chunkWrite = m_chunkWrite.load(std::memory_order_acquire);
    const qint64 chunkRead = m_chunkRead.load(std::memory_order_relaxed);
    const auto chunkSlots = static_cast<qint64>(m_chunks.size());
    for (; flushRead < flushWrite; ++flushRead) {
        const Flush flush = m_flushes[flushRead % static_cast<qint64>(m_flushes.size())];
        for (qint64 i = chunkRead; i < chunkWrite; ++i) {
            Chunk &chunk = m_chunks[i % chunkSlots];
            if (!isAlive(chunk) ||
                (chunk.generation < flush.generation && chunk.segment > flush.keepThroughSegment)) {
                chunk.alive = false;
            } else {
                chunk.checkedGeneration = flush.generation;
            }
        }
        m_appliedGeneration = flush.generation;
    }
    m_flushRead.store(flushRead, std::memory_order_release);
}

qsizetype AudioQueue::pull(std::span<float> out)
{
    applyFlushes();

    const auto capacity = static_cast<qint64>(m_samples.size());
    const auto chunkSlots = static_cast<qint64>(m_chunks.size());
    const qint64 chunkWrite = m_chunkWrite.load(std::memory_order_acquire);
    qint64 chunkRead = m_chunkRead.load(std::memory_order_relaxed);
    qint64 sampleRead = m_sampleRead.load(std::memory_order_relaxed);
    const auto wanted = static_cast<qint64>(out.size());
    qint64 written = 0;

    while (written < wanted && chunkRead < chunkWrite) {
        Chunk &chunk = m_chunks[chunkRead % chunkSlots];
        const qint64 remaining = chunk.frames - chunk.consumed;
        if (!isAlive(chunk)) {
            // Skip the discarded chunk's samples in the ring.
            sampleRead += remaining;
            ++chunkRead;
            continue;
        }
        const qint64 count = std::min(remaining, wanted - written);
        const qint64 start = sampleRead % capacity;
        const qint64 firstPart = std::min(count, capacity - start);
        std::copy_n(m_samples.begin() + start, firstPart, out.begin() + written);
        std::copy_n(m_samples.begin(), count - firstPart, out.begin() + written + firstPart);
        written += count;
        sampleRead += count;
        chunk.consumed += count;
        m_currentSegment.store(chunk.segment, std::memory_order_relaxed);
        if (chunk.consumed == chunk.frames) {
            ++chunkRead;
        }
    }

    std::fill(out.begin() + written, out.end(), 0.0F);
    m_framesPlayed.fetch_add(written, std::memory_order_relaxed);
    m_sampleRead.store(sampleRead, std::memory_order_release);
    m_chunkRead.store(chunkRead, std::memory_order_release);
    return written;
}

int AudioQueue::currentSegment() const
{
    return m_currentSegment.load(std::memory_order_relaxed);
}

qint64 AudioQueue::framesPlayed() const
{
    return m_framesPlayed.load(std::memory_order_relaxed);
}

qint64 AudioQueue::queuedFrames() const
{
    return m_sampleWrite.load(std::memory_order_acquire) -
           m_sampleRead.load(std::memory_order_acquire);
}

} // namespace legilo
