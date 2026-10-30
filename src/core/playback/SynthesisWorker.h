#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <optional>
#include <vector>

namespace legilo {

class AudioQueue;
class TtsEngine;

// Synthesises a document's segments ahead of playback and pushes the audio
// into an AudioQueue (design 6.3, 7.4, 7.5). It lives on its own thread;
// the PlaybackController commands it through queued calls, each carrying
// the generation the work is for. Audio for a generation that a flush has
// since replaced is dropped, here or by the queue.
//
// It keeps a limited amount of audio ahead of the segment playing, and
// synthesises the segment playback starts with in two chunks, so the
// first audio is ready quickly. It never waits on a full queue, so new
// commands reach it between chunks.
class SynthesisWorker : public QObject
{
    Q_OBJECT

public:
    struct Settings
    {
        int lookAheadSegments = 3;      // segments queued beyond the one playing
        double lookAheadSeconds = 15.0; // audio queued, in seconds
        int firstChunkChars = 80;       // the voice/firstChunkChars setting
    };

    SynthesisWorker(AudioQueue *queue, Settings settings, QObject *parent = nullptr);

    // Starts synthesising segments from startSegment, for generation.
    void start(const std::shared_ptr<TtsEngine> &engine, const QStringList &segments,
               int startSegment, quint64 generation, double speed);
    // Synthesises again from segment onwards, for a speed change.
    void restartFrom(int segment, quint64 generation, double speed);
    // Stops all work.
    void stop();

signals:
    // The first audio for generation is in the queue.
    void firstAudioQueued(quint64 generation);
    // Every segment for generation is in the queue.
    void finished(quint64 generation);
    // A segment could not be synthesised and was skipped.
    void failed(quint64 generation, int segment);

private:
    struct Unit
    {
        int segment = 0;
        QString text;
    };

    void step();
    void begin(int segment, quint64 generation, double speed);
    bool lookAheadFull() const;

    AudioQueue *m_queue;
    Settings m_settings;
    QTimer m_timer;

    std::shared_ptr<TtsEngine> m_engine;
    QStringList m_segments;
    std::vector<Unit> m_units; // still to synthesise, in order
    std::optional<Unit> m_pendingUnit;
    std::vector<float> m_pendingSamples; // synthesised, waiting for room
    quint64 m_generation = 0;
    double m_speed = 1.0;
    int m_firstSegment = 0;
    int m_lastPushedSegment = -1;
    bool m_announced = false;
};

} // namespace legilo
