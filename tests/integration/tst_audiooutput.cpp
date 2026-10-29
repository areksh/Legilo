#include "core/audio/AudioQueue.h"
#include "core/audio/MiniaudioOutput.h"

#include <QTest>

using legilo::AudioQueue;
using legilo::MiniaudioOutput;

// MiniaudioOutput on miniaudio's null backend, which consumes audio in
// real time without hardware, so this runs anywhere, the build container
// included. Device loss cannot be simulated this way; it is in the manual
// checklist (design 9.4).
class TestAudioOutput : public QObject
{
    Q_OBJECT

private slots:
    void playsQueuedAudio();
    void stopHaltsAndStartResumes();
    void reopensAtAnotherSampleRate();
    void startFailsWithoutOpenDevice();

private:
    static constexpr int SampleRate = 22050;
};

void TestAudioOutput::playsQueuedAudio()
{
    AudioQueue queue(SampleRate);
    MiniaudioOutput output(MiniaudioOutput::Backend::Null);
    QVERIFY(queue.push(0, 3, std::vector<float>(SampleRate / 4, 0.25F)));

    QVERIFY(output.open(SampleRate, &queue));
    QVERIFY(output.start());

    QVERIFY(output.isStarted());
    QTRY_COMPARE_WITH_TIMEOUT(queue.framesPlayed(), SampleRate / 4, 5000);
    QCOMPARE(queue.currentSegment(), 3);
    QCOMPARE(queue.queuedFrames(), 0);
}

void TestAudioOutput::stopHaltsAndStartResumes()
{
    AudioQueue queue(qsizetype{SampleRate} * 4);
    MiniaudioOutput output(MiniaudioOutput::Backend::Null);
    QVERIFY(queue.push(0, 0, std::vector<float>(size_t{SampleRate} * 3, 0.25F)));
    QVERIFY(output.open(SampleRate, &queue));
    QVERIFY(output.start());
    QTRY_VERIFY_WITH_TIMEOUT(queue.framesPlayed() > 0, 5000);

    output.stop();

    QVERIFY(!output.isStarted());
    const qint64 played = queue.framesPlayed();
    QTest::qWait(200);
    QCOMPARE(queue.framesPlayed(), played);
    QVERIFY(output.start());
    QTRY_VERIFY_WITH_TIMEOUT(queue.framesPlayed() > played, 5000);
}

void TestAudioOutput::reopensAtAnotherSampleRate()
{
    AudioQueue queue(SampleRate);
    MiniaudioOutput output(MiniaudioOutput::Backend::Null);
    QVERIFY(output.open(SampleRate, &queue));
    QVERIFY(output.start());

    QVERIFY(output.open(24000, &queue));

    QVERIFY(!output.isStarted());
    QVERIFY(queue.push(0, 1, std::vector<float>(2400, 0.25F)));
    QVERIFY(output.start());
    QTRY_COMPARE_WITH_TIMEOUT(queue.framesPlayed(), 2400, 5000);
}

void TestAudioOutput::startFailsWithoutOpenDevice()
{
    AudioQueue queue(SampleRate);
    MiniaudioOutput output(MiniaudioOutput::Backend::Null);

    QVERIFY(!output.start());

    QVERIFY(output.open(SampleRate, &queue));
    output.close();
    QVERIFY(!output.start());
}

QTEST_GUILESS_MAIN(TestAudioOutput)
#include "tst_audiooutput.moc"
