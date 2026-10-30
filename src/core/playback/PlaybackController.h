#pragma once

#include "core/audio/AudioQueue.h"
#include "core/playback/SynthesisWorker.h"

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <memory>

namespace legilo {

class AudioOutput;
class TtsEngine;

// Plays a document: drives the synthesis worker and the audio output, and
// keeps the playback state (design 7.5). All of it runs on the thread that
// owns the controller, normally the GUI thread.
//
// Stage 1 covers play, pause, resume, stop and speed. Navigation, voice
// changes and documents with paragraphs come in later stages.
class PlaybackController : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Idle,     // nothing to play
        Starting, // synthesising the first audio
        Playing,
        Paused
    };
    Q_ENUM(State)

    // Plays through output, which must outlive the controller.
    explicit PlaybackController(AudioOutput *output, SynthesisWorker::Settings settings = {},
                                QObject *parent = nullptr);
    ~PlaybackController() override;

    // The voice to synthesise with, from the next play() on.
    void setEngine(std::shared_ptr<TtsEngine> engine);

    // Stops anything playing, then plays segments from startSegment.
    void play(const QStringList &segments, int startSegment = 0);
    void pause();
    void resume();
    void stop();
    // Changes the speed (1.0 is normal). The segment playing finishes at
    // the old speed; the rest is synthesised again at the new one.
    void setSpeed(double speed);

    State state() const;
    double speed() const;
    // The segment being heard, or -1.
    int currentSegment() const;

signals:
    void stateChanged(legilo::PlaybackController::State state);
    void segmentChanged(int segment);
    void documentFinished();
    void error(const QString &message);

    // To the synthesis worker, on its thread.
    void startRequested(const std::shared_ptr<legilo::TtsEngine> &engine,
                        const QStringList &segments, int startSegment, quint64 generation,
                        double speed, QPrivateSignal);
    void restartRequested(int segment, quint64 generation, double speed, QPrivateSignal);
    void stopRequested(QPrivateSignal);

private:
    void setState(State state);
    void onFirstAudioQueued(quint64 generation);
    void onWorkerFinished(quint64 generation);
    void onWorkerFailed(quint64 generation, int segment);
    void poll();

    AudioOutput *m_output;
    AudioQueue m_queue;
    QThread m_thread;
    SynthesisWorker *m_worker;
    QTimer m_pollTimer;

    std::shared_ptr<TtsEngine> m_engine;
    int m_openSampleRate = 0;
    State m_state = State::Idle;
    double m_speed = 1.0;
    quint64 m_generation = 0;         // the latest flush's
    quint64 m_documentGeneration = 0; // the generation the document started with
    int m_startSegment = 0;
    int m_currentSegment = -1;
    bool m_synthesisDone = false;
};

} // namespace legilo
