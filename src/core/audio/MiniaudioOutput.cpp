#include "core/audio/MiniaudioOutput.h"

#include "core/Logging.h"
#include "core/audio/AudioQueue.h"

#include <atomic>
#include <miniaudio.h>
#include <span>

namespace legilo {

struct MiniaudioOutput::Private
{
    MiniaudioOutput *q = nullptr;
    Backend backend = Backend::System;
    ma_context context{};
    bool contextReady = false;
    ma_device device{};
    bool deviceOpen = false;
    AudioQueue *queue = nullptr;
    // Set while Legilo itself stops the device, so the notification that
    // follows is not taken for a lost device.
    std::atomic<bool> stopping = false;

    bool ensureContext();
    void lost() const;

    static void pullAudio(ma_device *device, void *output, const void *input, ma_uint32 frames);
    static void notify(const ma_device_notification *notification);
};

// Runs on miniaudio's real-time thread: no locks, no allocation, no Qt.
// The queue is set before the device starts and stays until it closes.
void MiniaudioOutput::Private::pullAudio(ma_device *device, void *output, const void * /*input*/,
                                         ma_uint32 frames)
{
    auto *d = static_cast<Private *>(device->pUserData);
    d->queue->pull(std::span<float>(static_cast<float *>(output), frames));
}

// Runs on a miniaudio thread.
void MiniaudioOutput::Private::notify(const ma_device_notification *notification)
{
    auto *d = static_cast<Private *>(notification->pDevice->pUserData);
    const bool unexpected =
        notification->type == ma_device_notification_type_rerouted ||
        (notification->type == ma_device_notification_type_stopped && !d->stopping.load());
    if (unexpected) {
        d->lost();
    }
}

bool MiniaudioOutput::Private::ensureContext()
{
    if (contextReady) {
        return true;
    }
    const ma_backend null = ma_backend_null;
    const ma_result result = backend == Backend::Null
                                 ? ma_context_init(&null, 1, nullptr, &context)
                                 : ma_context_init(nullptr, 0, nullptr, &context);
    if (result != MA_SUCCESS) {
        qCWarning(lcAudio) << "Cannot initialise audio:" << ma_result_description(result);
        return false;
    }
    contextReady = true;
    return true;
}

// The device stopped or moved without Legilo asking: stop on the object's
// own thread and say so. miniaudio must not be told to stop from its own
// notification. Qt drops the queued call if the object is gone by then.
void MiniaudioOutput::Private::lost() const
{
    QMetaObject::invokeMethod(q, "stopAfterDeviceLoss", Qt::QueuedConnection);
}

void MiniaudioOutput::stopAfterDeviceLoss()
{
    stop();
    emit deviceLost();
}

MiniaudioOutput::MiniaudioOutput(Backend backend, QObject *parent)
    : AudioOutput(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->backend = backend;
}

MiniaudioOutput::~MiniaudioOutput()
{
    close();
    if (d->contextReady) {
        ma_context_uninit(&d->context);
    }
}

bool MiniaudioOutput::open(int sampleRate, AudioQueue *queue)
{
    close();
    if (!d->ensureContext()) {
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = static_cast<ma_uint32>(sampleRate);
    config.dataCallback = Private::pullAudio;
    config.notificationCallback = Private::notify;
    config.pUserData = d.get();
    d->queue = queue;

    const ma_result result = ma_device_init(&d->context, &config, &d->device);
    if (result != MA_SUCCESS) {
        qCWarning(lcAudio) << "Cannot open the audio device:" << ma_result_description(result);
        d->queue = nullptr;
        return false;
    }
    d->deviceOpen = true;
    return true;
}

void MiniaudioOutput::close()
{
    if (!d->deviceOpen) {
        return;
    }
    d->stopping = true;
    ma_device_uninit(&d->device);
    d->stopping = false;
    d->deviceOpen = false;
    d->queue = nullptr;
}

bool MiniaudioOutput::start()
{
    if (!d->deviceOpen) {
        return false;
    }
    const ma_result result = ma_device_start(&d->device);
    if (result != MA_SUCCESS) {
        qCWarning(lcAudio) << "Cannot start the audio device:" << ma_result_description(result);
        return false;
    }
    return true;
}

void MiniaudioOutput::stop()
{
    if (!d->deviceOpen || !isStarted()) {
        return;
    }
    d->stopping = true;
    ma_device_stop(&d->device);
    d->stopping = false;
}

bool MiniaudioOutput::isStarted() const
{
    return d->deviceOpen && ma_device_is_started(&d->device) == MA_TRUE;
}

} // namespace legilo
