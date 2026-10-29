#pragma once

#include <QObject>

namespace legilo {

class AudioQueue;

// The audio device: plays mono float samples pulled from an AudioQueue.
// The playback controller drives this interface, so its tests can use a
// fake that pulls on demand instead of in real time.
class AudioOutput : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Opens the default playback device at sampleRate, pulling from queue,
    // which must outlive the device. Closes any device already open, so
    // this also switches to a voice with another sample rate. The device
    // starts stopped. Returns false if no device could be opened.
    virtual bool open(int sampleRate, AudioQueue *queue) = 0;
    virtual void close() = 0;

    // Starts or resumes pulling audio. Returns false if no device is open
    // or it would not start.
    virtual bool start() = 0;
    // Stops pulling and leaves the queue as it is, so a later start()
    // resumes without a gap. Once it returns, the device's callback is no
    // longer running.
    virtual void stop() = 0;
    virtual bool isStarted() const = 0;

signals:
    // The device that was playing went away or changed, for example when
    // headphones are unplugged. The output has stopped, rather than carry
    // on through the speakers (design 7.4).
    void deviceLost();
};

} // namespace legilo
