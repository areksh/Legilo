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
// It covers the commands of 7.5 except togglePlay, which needs the last
// document and so lives with whoever keeps it (the app's Speaker).
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
    void play(const QStringList &segments, int startSegment = 0, const QList<int> &paragraphs = {});
    // Plays on from segment: at once if playing, on resume if paused.
    void seekToSegment(int segment);
    void nextSegment();
    void previousSegment();
    void nextParagraph();
    void previousParagraph();
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
    // How much of the segment being heard has been played.
    double secondsIntoSegment() const;

signals:
    void stateChanged(legilo::PlaybackController::State state);
    void segmentChanged(int segment);
    void documentFinished();
    void error(const QString &message);

    // To the synthesis worker, on its thread.
    void startRequested(const std::shared_ptr<legilo::TtsEngine> &engine,
                        const QStringList &segments, int startSegment, quint64 generation,
                        double speed, QPrivateSignal);
    void restartRequested(const std::shared_ptr<legilo::TtsEngine> &engine, int segment,
                          quint64 generation, double speed, QPrivateSignal);
    void stopRequested(QPrivateSignal);

private:
    void setState(State state);
    // Synthesises afresh from segment, discarding everything queued.
    bool restartAt(int segment);
    // Keeps the segment being heard and synthesises the rest again.
    void resynthesiseAfterHeard(const QString &failure);
    // The segment being heard, or else where playback is about to start.
    int position() const;
    int paragraphOf(int segment) const;
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
    quint64 m_generation = 0; // the latest flush's
    // The generation that last set the position: play or a seek. Audio of
    // older generations says nothing about where playback is.
    quint64 m_positionGeneration = 0;
    int m_startSegment = 0; // where play or the last seek started
    QStringList m_segments;
    QList<int> m_paragraphs; // each segment's paragraph; empty for one paragraph
    int m_currentSegment = -1;
    bool m_synthesisDone = false;
    bool m_ranDry = false;
};

} // namespace legilo
