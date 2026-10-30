#pragma once

#include "core/audio/AudioOutput.h"
#include "core/audio/AudioQueue.h"

#include <vector>

namespace legilo::testing {

// An AudioOutput with no device: the test pulls audio when it chooses,
// as a device's callback would while started.
class FakeAudioOutput : public AudioOutput
{
public:
    bool open(int sampleRate, AudioQueue *queue) override
    {
        m_sampleRate = sampleRate;
        m_queue = queue;
        m_started = false;
        ++m_opens;
        return true;
    }

    void close() override
    {
        m_queue = nullptr;
        m_started = false;
    }

    bool start() override
    {
        m_started = m_queue != nullptr;
        return m_started;
    }

    void stop() override { m_started = false; }

    bool isStarted() const override { return m_started; }

    // Pulls up to frames frames, if started. Returns the frames of audio.
    qsizetype pull(qsizetype frames)
    {
        if (!m_started) {
            return 0;
        }
        std::vector<float> buffer(frames);
        const qsizetype pulled = m_queue->pull(buffer);
        m_framesPulled += pulled;
        return pulled;
    }

    // The device goes away, as when headphones are unplugged.
    void loseDevice()
    {
        m_started = false;
        emit deviceLost();
    }

    int sampleRate() const { return m_sampleRate; }
    int opens() const { return m_opens; }
    qint64 framesPulled() const { return m_framesPulled; }

private:
    AudioQueue *m_queue = nullptr;
    bool m_started = false;
    int m_sampleRate = 0;
    int m_opens = 0;
    qint64 m_framesPulled = 0;
};

} // namespace legilo::testing
