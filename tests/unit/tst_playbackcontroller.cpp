#include "core/playback/PlaybackController.h"
#include "fakes/FakeAudioOutput.h"
#include "fakes/FakeTtsEngine.h"

#include <QDeadlineTimer>
#include <QSignalSpy>
#include <QTest>

#include <functional>

using legilo::PlaybackController;
using legilo::SynthesisWorker;
using legilo::testing::FakeAudioOutput;
using legilo::testing::FakeTtsEngine;
using namespace Qt::StringLiterals;
using State = PlaybackController::State;

namespace {

// Pulls audio in steps, as a playing device would, with the event loop
// running, until done() holds. Returns false on timing out.
bool playUntil(FakeAudioOutput &output, const std::function<bool()> &done, qsizetype step = 200,
               int timeoutMs = 5000)
{
    const QDeadlineTimer deadline(timeoutMs);
    while (!done()) {
        if (deadline.hasExpired()) {
            return false;
        }
        output.pull(step);
        QTest::qWait(2);
    }
    return true;
}

// Waits, with the event loop running, until done() holds.
bool waitUntil(const std::function<bool()> &done, int timeoutMs = 5000)
{
    return QTest::qWaitFor(done, timeoutMs);
}

// Segments short enough never to be split into a first chunk.
QStringList segments(int count)
{
    QStringList list;
    for (int i = 0; i < count; ++i) {
        list.append(u"Segment number %1 of the document."_s.arg(i));
    }
    return list;
}

qsizetype framesOf(const QStringList &texts, double speed = 1.0)
{
    qsizetype frames = 0;
    for (const QString &text : texts) {
        frames += FakeTtsEngine::framesFor(text, speed);
    }
    return frames;
}

} // namespace

class TestPlaybackController : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void playStartsOutputOnceFirstAudioIsQueued();
    void playsEverySegmentInOrderThenFinishes();
    void reportsEachSegmentAsItIsHeard();
    void startsFromTheGivenSegment();
    void splitsTheStartingSegmentIntoShortFirstChunk();
    void synthesisesAheadNoMoreThanLookAheadSegments();
    void synthesisesAheadNoMoreThanLookAheadSeconds();
    void pauseStopsOutputAndResumeRestartsIt();
    void stopDiscardsQueuedAudioAndStopsSynthesis();
    void playAfterStoppingAFullLookAheadPlays();
    void playWhilePlayingReplacesTheDocument();
    void speedChangeKeepsSegmentPlayingAndResynthesisesTheRest();
    void deviceLossPauses();
    void failedSegmentIsReportedAndSkipped();
    void playWithoutEngineReportsError();
    void opensOutputAtTheVoiceSampleRate();

private:
    std::unique_ptr<FakeAudioOutput> m_output;
    std::shared_ptr<FakeTtsEngine> m_engine;
    std::unique_ptr<PlaybackController> m_controller;

    void makeController(SynthesisWorker::Settings settings = {});
};

void TestPlaybackController::init()
{
    m_output = std::make_unique<FakeAudioOutput>();
    m_engine = std::make_shared<FakeTtsEngine>();
    makeController();
}

void TestPlaybackController::cleanup()
{
    m_controller.reset();
    m_output.reset();
}

void TestPlaybackController::makeController(SynthesisWorker::Settings settings)
{
    m_controller.reset();
    m_controller = std::make_unique<PlaybackController>(m_output.get(), settings);
    m_controller->setEngine(m_engine);
}

void TestPlaybackController::playStartsOutputOnceFirstAudioIsQueued()
{
    QSignalSpy states(m_controller.get(), &PlaybackController::stateChanged);

    m_controller->play(segments(2));

    QCOMPARE(m_controller->state(), State::Starting);
    QVERIFY(waitUntil([&] { return m_controller->state() == State::Playing; }));
    QVERIFY(m_output->isStarted());
    QCOMPARE(states.at(0).at(0).value<State>(), State::Starting);
    QCOMPARE(states.at(1).at(0).value<State>(), State::Playing);
}

void TestPlaybackController::playsEverySegmentInOrderThenFinishes()
{
    QSignalSpy finished(m_controller.get(), &PlaybackController::documentFinished);
    const QStringList document = segments(5);

    m_controller->play(document);

    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    QCOMPARE(m_engine->texts(), document);
    QCOMPARE(m_output->framesPulled(), framesOf(document));
    QCOMPARE(finished.size(), 1);
    QVERIFY(!m_output->isStarted());
}

void TestPlaybackController::reportsEachSegmentAsItIsHeard()
{
    QSignalSpy heard(m_controller.get(), &PlaybackController::segmentChanged);

    m_controller->play(segments(3));

    // Small steps, so the controller's 30 ms poll sees every segment.
    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }, 10));
    QList<int> reported;
    for (const auto &arguments : heard) {
        reported.append(arguments.at(0).toInt());
    }
    QCOMPARE(reported, (QList<int>{0, 1, 2}));
}

void TestPlaybackController::startsFromTheGivenSegment()
{
    const QStringList document = segments(4);

    m_controller->play(document, 2);

    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    QCOMPARE(m_engine->texts(), document.mid(2));
}

void TestPlaybackController::splitsTheStartingSegmentIntoShortFirstChunk()
{
    makeController({.firstChunkChars = 20});
    const QString first = u"The quick brown fox jumps, over the lazy dog and away."_s;
    const QString second = u"Then it ran home, and slept for a long, long time."_s;

    m_controller->play({first, second});

    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    QCOMPARE(m_engine->texts(), (QStringList{u"The quick brown fox jumps,"_s,
                                             u"over the lazy dog and away."_s, second}));
}

void TestPlaybackController::synthesisesAheadNoMoreThanLookAheadSegments()
{
    makeController({.lookAheadSegments = 3, .lookAheadSeconds = 1000});

    m_controller->play(segments(10));

    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 3; }));
    QTest::qWait(200);
    QCOMPARE(m_engine->calls().size(), 3);
    // Playing the first segment through makes room for one more.
    m_output->pull(FakeTtsEngine::framesFor(segments(1).first()));
    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 4; }));
    QTest::qWait(200);
    QCOMPARE(m_engine->calls().size(), 4);
}

void TestPlaybackController::synthesisesAheadNoMoreThanLookAheadSeconds()
{
    // Two and a half segments' worth: the third one goes over.
    const double segmentSeconds =
        static_cast<double>(FakeTtsEngine::framesFor(segments(1).first())) / FakeTtsEngine::Rate;
    makeController({.lookAheadSegments = 100, .lookAheadSeconds = 2.5 * segmentSeconds});

    m_controller->play(segments(10));

    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 3; }));
    QTest::qWait(200);
    QCOMPARE(m_engine->calls().size(), 3);
}

void TestPlaybackController::pauseStopsOutputAndResumeRestartsIt()
{
    m_controller->play(segments(3));
    QVERIFY(waitUntil([&] { return m_controller->state() == State::Playing; }));

    m_controller->pause();

    QCOMPARE(m_controller->state(), State::Paused);
    QVERIFY(!m_output->isStarted());
    m_controller->resume();
    QCOMPARE(m_controller->state(), State::Playing);
    QVERIFY(m_output->isStarted());
}

void TestPlaybackController::stopDiscardsQueuedAudioAndStopsSynthesis()
{
    makeController({.lookAheadSegments = 2});
    m_controller->play(segments(10));
    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 2; }));

    m_controller->stop();

    QCOMPARE(m_controller->state(), State::Idle);
    QVERIFY(!m_output->isStarted());
    QTest::qWait(200);
    const auto calls = m_engine->calls().size();
    QVERIFY(calls <= 3);
    // A new document plays only its own audio.
    const QStringList next{u"Something else entirely."_s};
    m_controller->play(next);
    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    QCOMPARE(m_output->framesPulled(), framesOf(next));
}

// Regression: the stopped document's audio stays in the queue until the
// device applies the flush, and the device starts only once the new
// document's first audio is queued. Counting the stale audio as look-ahead
// left the new document waiting for room that never came.
void TestPlaybackController::playAfterStoppingAFullLookAheadPlays()
{
    makeController({.lookAheadSegments = 10, .lookAheadSeconds = 1.0});
    m_controller->play(segments(10));
    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 3; }));
    QTest::qWait(100); // the worker has filled the look-ahead

    m_controller->stop();
    m_controller->play({u"Something else entirely."_s});

    QVERIFY(waitUntil([&] { return m_controller->state() == State::Playing; }, 2000));
}

void TestPlaybackController::playWhilePlayingReplacesTheDocument()
{
    QSignalSpy finished(m_controller.get(), &PlaybackController::documentFinished);
    m_controller->play(segments(10));
    QVERIFY(waitUntil([&] { return m_controller->state() == State::Playing; }));
    const qsizetype before = m_output->pull(100);

    const QStringList next{u"A new text."_s, u"With two sentences."_s};
    m_controller->play(next);

    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    QCOMPARE(m_output->framesPulled(), before + framesOf(next));
    QCOMPARE(finished.size(), 1);
}

void TestPlaybackController::speedChangeKeepsSegmentPlayingAndResynthesisesTheRest()
{
    const QStringList document = segments(4);
    m_controller->play(document);
    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 3; }));
    m_output->pull(100); // part-way through segment 0

    m_controller->setSpeed(2.0);

    QCOMPARE(m_controller->speed(), 2.0);
    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    // Segment 0 played in full at the old speed, the rest at the new one.
    QCOMPARE(m_output->framesPulled(),
             framesOf(document.first(1)) + framesOf(document.mid(1), 2.0));
    const auto calls = m_engine->calls();
    QCOMPARE(calls.last().text, document.last());
    QCOMPARE(calls.last().speed, 2.0);
}

void TestPlaybackController::deviceLossPauses()
{
    m_controller->play(segments(3));
    QVERIFY(waitUntil([&] { return m_controller->state() == State::Playing; }));

    m_output->loseDevice();

    QCOMPARE(m_controller->state(), State::Paused);
}

void TestPlaybackController::failedSegmentIsReportedAndSkipped()
{
    QSignalSpy errors(m_controller.get(), &PlaybackController::error);
    const QStringList document{u"First segment."_s, u"This one will FAIL."_s, u"Third segment."_s};

    m_controller->play(document);

    QVERIFY(playUntil(*m_output, [&] { return m_controller->state() == State::Idle; }));
    QCOMPARE(errors.size(), 1);
    QCOMPARE(m_output->framesPulled(), framesOf({document.at(0), document.at(2)}));
}

void TestPlaybackController::playWithoutEngineReportsError()
{
    QSignalSpy errors(m_controller.get(), &PlaybackController::error);
    m_controller->setEngine(nullptr);

    m_controller->play(segments(2));

    QCOMPARE(errors.size(), 1);
    QCOMPARE(m_controller->state(), State::Idle);
}

void TestPlaybackController::opensOutputAtTheVoiceSampleRate()
{
    m_controller->play(segments(1));

    QCOMPARE(m_output->sampleRate(), FakeTtsEngine::Rate);
    m_controller->play(segments(1));
    QCOMPARE(m_output->opens(), 1);
}

QTEST_GUILESS_MAIN(TestPlaybackController)
#include "tst_playbackcontroller.moc"
