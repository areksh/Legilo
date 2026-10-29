#pragma once

#include "core/audio/AudioOutput.h"

#include <memory>

namespace legilo {

// An AudioOutput on miniaudio: PipeWire, PulseAudio or ALSA on Linux,
// WASAPI on Windows. miniaudio converts to the device's own rate.
class MiniaudioOutput final : public AudioOutput
{
    Q_OBJECT

public:
    enum class Backend
    {
        System, // the platform's audio system
        Null    // a simulated device that consumes audio in real time, for tests
    };

    explicit MiniaudioOutput(Backend backend = Backend::System, QObject *parent = nullptr);
    ~MiniaudioOutput() override;

    bool open(int sampleRate, AudioQueue *queue) override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private Q_SLOTS:
    void stopAfterDeviceLoss();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace legilo
