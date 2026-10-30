#include "core/audio/AudioQueue.h"

#include <QTest>

#include <thread>

using legilo::AudioQueue;

namespace {

// Samples counting up from first, so their order shows in the output.
std::vector<float> ramp(float first, int count)
{
    std::vector<float> samples(count);
    for (int i = 0; i < count; ++i) {
        samples[i] = first + static_cast<float>(i);
    }
    return samples;
}

std::vector<float> pull(AudioQueue &queue, int frames)
{
    std::vector<float> out(frames, -1.0F);
    queue.pull(out);
    return out;
}

std::vector<float> join(std::initializer_list<std::vector<float>> parts)
{
    std::vector<float> joined;
    for (const auto &part : parts) {
        joined.insert(joined.end(), part.begin(), part.end());
    }
    return joined;
}

std::vector<float> silence(int frames)
{
    return std::vector<float>(frames, 0.0F);
}

} // namespace

class TestAudioQueue : public QObject
{
    Q_OBJECT

private slots:
    void emptyQueuePullsSilence();
    void playsChunksInOrderAcrossPulls();
    void wrapsAroundTheRing();
    void pushFailsWhenSamplesDoNotFit();
    void pushFailsWhenChunksDoNotFit();
    void emptyPushAddsNothing();
    void reportsSegmentFramesAndQueue();
    void reportsGenerationOfAudioPlaying();
    void flushWithoutKeepDiscardsEverything();
    void flushKeepsSegmentsUpToKeepThrough();
    void flushStartsNewGeneration();
    void audioOfNewGenerationPlaysAfterFlush();
    void lateAudioOfOldGenerationIsDiscarded();
    void seekThenSpeedChangeDropsAudioFromBeforeSeek();
    void repeatedSpeedChangesKeepPlayingSegment();
    void flushFailsWhenCallbackFallsBehind();
    void concurrentProducerAndConsumerKeepOrder();
};

void TestAudioQueue::emptyQueuePullsSilence()
{
    AudioQueue queue(16);
    std::vector<float> out(4, -1.0F);

    const qsizetype frames = queue.pull(out);

    QCOMPARE(frames, 0);
    QCOMPARE(out, silence(4));
    QCOMPARE(queue.currentSegment(), -1);
}

void TestAudioQueue::playsChunksInOrderAcrossPulls()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 0, ramp(1, 3)));
    QVERIFY(queue.push(0, 1, ramp(10, 3)));

    const auto first = pull(queue, 4);
    const auto second = pull(queue, 4);

    QCOMPARE(first, (std::vector<float>{1, 2, 3, 10}));
    QCOMPARE(second, (std::vector<float>{11, 12, 0, 0}));
}

void TestAudioQueue::wrapsAroundTheRing()
{
    AudioQueue queue(8);
    QVERIFY(queue.push(0, 0, ramp(1, 6)));
    QCOMPARE(pull(queue, 6), ramp(1, 6));

    QVERIFY(queue.push(0, 1, ramp(20, 6)));

    QCOMPARE(pull(queue, 6), ramp(20, 6));
}

void TestAudioQueue::pushFailsWhenSamplesDoNotFit()
{
    AudioQueue queue(8);
    QVERIFY(queue.push(0, 0, ramp(1, 6)));

    QVERIFY(!queue.push(0, 1, ramp(10, 3)));
    QVERIFY(queue.push(0, 1, ramp(10, 2)));

    QCOMPARE(pull(queue, 8), join({ramp(1, 6), ramp(10, 2)}));
}

void TestAudioQueue::pushFailsWhenChunksDoNotFit()
{
    AudioQueue queue(64, 2);
    QVERIFY(queue.push(0, 0, ramp(1, 1)));
    QVERIFY(queue.push(0, 1, ramp(2, 1)));

    QVERIFY(!queue.push(0, 2, ramp(3, 1)));
    pull(queue, 1);
    QVERIFY(queue.push(0, 2, ramp(3, 1)));
}

void TestAudioQueue::emptyPushAddsNothing()
{
    AudioQueue queue(8, 1);

    QVERIFY(queue.push(0, 0, {}));

    QVERIFY(queue.push(0, 1, ramp(1, 2)));
    QCOMPARE(queue.queuedFrames(), 2);
}

void TestAudioQueue::reportsSegmentFramesAndQueue()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 4, ramp(1, 3)));
    QVERIFY(queue.push(0, 5, ramp(10, 3)));
    QCOMPARE(queue.queuedFrames(), 6);

    pull(queue, 4);

    QCOMPARE(queue.currentSegment(), 5);
    QCOMPARE(queue.framesPlayed(), 4);
    QCOMPARE(queue.queuedFrames(), 2);
    pull(queue, 4);
    QCOMPARE(queue.framesPlayed(), 6);
    QCOMPARE(queue.queuedFrames(), 0);
}

void TestAudioQueue::reportsGenerationOfAudioPlaying()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 2, ramp(1, 2)));
    pull(queue, 1);
    QCOMPARE(queue.playingGeneration(), 0U);
    const quint64 generation = queue.requestFlush(2).value_or(0);
    QVERIFY(queue.push(generation, 3, ramp(10, 2)));

    pull(queue, 2);

    QCOMPARE(queue.currentSegment(), 3);
    QCOMPARE(queue.playingGeneration(), generation);
}

void TestAudioQueue::flushWithoutKeepDiscardsEverything()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 0, ramp(1, 4)));
    QVERIFY(queue.push(0, 1, ramp(10, 4)));
    pull(queue, 2);

    QVERIFY(queue.requestFlush(-1).has_value());

    QCOMPARE(pull(queue, 6), silence(6));
    QCOMPARE(queue.queuedFrames(), 0);
}

// A speed change keeps the segment playing and drops the look-ahead.
void TestAudioQueue::flushKeepsSegmentsUpToKeepThrough()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 0, ramp(1, 3)));
    QVERIFY(queue.push(0, 1, ramp(10, 3)));
    QVERIFY(queue.push(0, 2, ramp(20, 3)));
    pull(queue, 2);

    QVERIFY(queue.requestFlush(1).has_value());

    QCOMPARE(pull(queue, 6), join({ramp(3, 1), ramp(10, 3), silence(2)}));
}

void TestAudioQueue::flushStartsNewGeneration()
{
    AudioQueue queue(16);
    QCOMPARE(queue.generation(), 0U);

    const auto first = queue.requestFlush(-1);
    const auto second = queue.requestFlush(-1);

    QCOMPARE(first, std::optional<quint64>(1));
    QCOMPARE(second, std::optional<quint64>(2));
    QCOMPARE(queue.generation(), 2U);
}

void TestAudioQueue::audioOfNewGenerationPlaysAfterFlush()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 0, ramp(1, 3)));
    const quint64 generation = queue.requestFlush(-1).value_or(0);

    QVERIFY(queue.push(generation, 7, ramp(50, 3)));

    QCOMPARE(pull(queue, 4), join({ramp(50, 3), silence(1)}));
    QCOMPARE(queue.currentSegment(), 7);
}

// The worker may finish a segment for the old generation just after the
// flush; it must not play.
void TestAudioQueue::lateAudioOfOldGenerationIsDiscarded()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 0, ramp(1, 3)));
    pull(queue, 1);
    QVERIFY(queue.requestFlush(0).has_value());
    pull(queue, 1);

    QVERIFY(queue.push(0, 1, ramp(10, 3)));

    QCOMPARE(pull(queue, 4), join({ramp(3, 1), silence(3)}));
}

// Both flushes reach the queue before the callback runs, as they can
// while playback is paused. Applying only the latest would keep segments
// 2 to 4, from before the seek, because they are "up to segment 8".
void TestAudioQueue::seekThenSpeedChangeDropsAudioFromBeforeSeek()
{
    AudioQueue queue(32);
    QVERIFY(queue.push(0, 2, ramp(1, 3)));
    QVERIFY(queue.push(0, 3, ramp(10, 3)));
    QVERIFY(queue.push(0, 4, ramp(20, 3)));
    pull(queue, 1);

    const quint64 seek = queue.requestFlush(-1).value_or(0);
    QVERIFY(queue.push(seek, 8, ramp(80, 3)));
    QVERIFY(queue.requestFlush(8).has_value());

    QCOMPARE(pull(queue, 4), join({ramp(80, 3), silence(1)}));
}

void TestAudioQueue::repeatedSpeedChangesKeepPlayingSegment()
{
    AudioQueue queue(16);
    QVERIFY(queue.push(0, 5, ramp(1, 4)));
    QVERIFY(queue.push(0, 6, ramp(10, 4)));
    pull(queue, 1);

    QVERIFY(queue.requestFlush(5).has_value());
    QVERIFY(queue.requestFlush(5).has_value());

    QCOMPARE(pull(queue, 4), join({ramp(2, 3), silence(1)}));
}

void TestAudioQueue::flushFailsWhenCallbackFallsBehind()
{
    AudioQueue queue(16, 4, 2);
    QVERIFY(queue.requestFlush(-1).has_value());
    QVERIFY(queue.requestFlush(-1).has_value());

    QVERIFY(!queue.requestFlush(-1).has_value());
    pull(queue, 1);
    QVERIFY(queue.requestFlush(-1).has_value());
}

// The producer and the consumer on their own threads, as in use: every
// sample arrives once, in order, whatever the interleaving.
void TestAudioQueue::concurrentProducerAndConsumerKeepOrder()
{
    constexpr int chunks = 2000;
    constexpr int chunkFrames = 37;
    constexpr int totalFrames = chunks * chunkFrames;
    AudioQueue queue(256, 8);

    std::thread producer([&] {
        for (int chunk = 0; chunk < chunks; ++chunk) {
            const auto samples = ramp(static_cast<float>((chunk * chunkFrames) + 1), chunkFrames);
            while (!queue.push(0, chunk, samples)) {
                std::this_thread::yield();
            }
        }
    });
    std::vector<float> received;
    received.reserve(totalFrames);
    std::vector<float> out(64);
    while (received.size() < size_t{totalFrames}) {
        const qsizetype frames = queue.pull(out);
        received.insert(received.end(), out.begin(), out.begin() + frames);
        if (frames == 0) {
            std::this_thread::yield();
        }
    }
    producer.join();

    QCOMPARE(received, ramp(1, totalFrames));
}

QTEST_GUILESS_MAIN(TestAudioQueue)
#include "tst_audioqueue.moc"
