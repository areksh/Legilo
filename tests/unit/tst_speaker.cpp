#include "app/Settings.h"
#include "app/Speaker.h"
#include "core/playback/PlaybackController.h"
#include "fakes/FakeAudioOutput.h"
#include "fakes/FakeTextSource.h"
#include "fakes/FakeTtsEngine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <memory>

using legilo::PlaybackController;
using legilo::Settings;
using legilo::Speaker;
using legilo::testing::FakeAudioOutput;
using legilo::testing::FakeTextSource;
using legilo::testing::FakeTtsEngine;
using namespace Qt::StringLiterals;
using State = PlaybackController::State;

namespace {

bool waitUntil(const std::function<bool()> &done, int timeoutMs = 5000)
{
    return QTest::qWaitFor(done, timeoutMs);
}

} // namespace

class TestSpeaker : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void speakSelectionSpeaksTheCleanedSelection();
    void speakClipboardSpeaksTheClipboard();
    void aRequestBeforeTheVoiceIsReadyIsSpokenOnceItIs();
    void speakingWhileSpeakingStopsByDefault_data();
    void speakingWhileSpeakingStopsByDefault();
    void speakingWhileSpeakingRestartsWithTheNewTextWhenSet();
    void emptySelectionFallsBackToTheClipboardWhenSet();
    void emptySelectionIsReportedWithoutTheFallback();
    void emptyClipboardIsReported();
    void togglePausePausesAndResumes();
    void stopStopsSpeech();

    void startedNoticeNamesTheSourceAndTheFirstWords();
    void startedNoticeLeavesOutTheSourceWhenTurnedOff();
    void startedNoticeIsNotShownWhenTurnedOff();
    void fallbackIsAnnouncedEvenWithStartedNoticesOff();
    void pausedNoticeFollowsItsSetting();
    void playbackErrorsAreAlwaysReported();

private:
    // Speaks the selection and waits until speech has started.
    bool startSpeaking(const QString &text = u"Some words to speak."_s);

    QTemporaryDir m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<FakeTextSource> m_source;
    std::unique_ptr<FakeAudioOutput> m_output;
    std::unique_ptr<PlaybackController> m_controller;
    std::shared_ptr<FakeTtsEngine> m_engine;
    std::unique_ptr<Speaker> m_speaker;
};

void TestSpeaker::init()
{
    QVERIFY(m_dir.isValid());
    QFile::remove(m_dir.filePath(u"legilo.conf"_s));
    m_settings = std::make_unique<Settings>(m_dir.filePath(u"legilo.conf"_s));
    m_source = std::make_unique<FakeTextSource>();
    m_output = std::make_unique<FakeAudioOutput>();
    m_controller = std::make_unique<PlaybackController>(m_output.get());
    m_engine = std::make_shared<FakeTtsEngine>();
    m_speaker = std::make_unique<Speaker>(m_source.get(), m_controller.get(), m_settings.get());
}

void TestSpeaker::cleanup()
{
    m_speaker.reset();
    m_controller.reset();
    m_engine.reset();
    m_output.reset();
    m_source.reset();
    m_settings.reset();
}

void TestSpeaker::speakSelectionSpeaksTheCleanedSelection()
{
    m_speaker->setEngine(m_engine);
    m_source->selection = u"  The first   sentence.  The second one. "_s;

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return m_engine->calls().size() >= 2; }));
    QCOMPARE(m_engine->texts().first(2),
             QStringList({u"The first sentence."_s, u"The second one."_s}));
    QCOMPARE(m_source->selectionReads, 1);
}

bool TestSpeaker::startSpeaking(const QString &text)
{
    m_speaker->setEngine(m_engine);
    m_source->selection = text;
    m_speaker->speakSelection();
    return waitUntil([&] { return m_controller->state() == State::Playing; });
}

void TestSpeaker::speakClipboardSpeaksTheClipboard()
{
    m_speaker->setEngine(m_engine);
    m_source->selection = u"Not this."_s;
    m_source->clipboard = u"The clipboard."_s;

    m_speaker->speakClipboard();

    QVERIFY(waitUntil([&] { return !m_engine->calls().isEmpty(); }));
    QCOMPARE(m_engine->texts().first(), u"The clipboard."_s);
    QCOMPARE(m_source->selectionReads, 0);
}

void TestSpeaker::aRequestBeforeTheVoiceIsReadyIsSpokenOnceItIs()
{
    m_source->selection = u"Early words."_s;
    m_speaker->speakSelection();
    QTest::qWait(50);
    QCOMPARE(m_controller->state(), State::Idle);

    m_speaker->setEngine(m_engine);

    QVERIFY(waitUntil([&] { return !m_engine->calls().isEmpty(); }));
    QCOMPARE(m_engine->texts().first(), u"Early words."_s);
}

void TestSpeaker::speakingWhileSpeakingStopsByDefault_data()
{
    QTest::addColumn<bool>("paused");
    QTest::newRow("playing") << false;
    QTest::newRow("paused") << true;
}

void TestSpeaker::speakingWhileSpeakingStopsByDefault()
{
    QFETCH(bool, paused);
    QVERIFY(startSpeaking(u"The first text."_s));
    if (paused) {
        m_speaker->togglePause();
        QCOMPARE(m_controller->state(), State::Paused);
    }
    m_source->selection = u"The second text."_s;

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return m_controller->state() == State::Idle; }));
    QTest::qWait(50);
    QVERIFY(!m_engine->texts().contains(u"The second text."_s));
    QCOMPARE(m_controller->state(), State::Idle);
    // Stopping needs no capture: the selection is not read again.
    QCOMPARE(m_source->selectionReads, 1);
}

void TestSpeaker::speakingWhileSpeakingRestartsWithTheNewTextWhenSet()
{
    m_settings->setSpeakWhileSpeaking(Settings::SpeakWhileSpeaking::Restart);
    QVERIFY(startSpeaking(u"The first text."_s));
    m_source->clipboard = u"The second text."_s;

    m_speaker->speakClipboard();

    QVERIFY(waitUntil([&] { return m_engine->texts().contains(u"The second text."_s); }));
}

void TestSpeaker::emptySelectionFallsBackToTheClipboardWhenSet()
{
    m_settings->setSelectionFallbackToClipboard(true);
    m_speaker->setEngine(m_engine);
    m_source->selection = u"  \n "_s;
    m_source->clipboard = u"From the clipboard."_s;

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return !m_engine->calls().isEmpty(); }));
    QCOMPARE(m_engine->texts().first(), u"From the clipboard."_s);
}

void TestSpeaker::emptySelectionIsReportedWithoutTheFallback()
{
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);
    m_speaker->setEngine(m_engine);
    m_source->clipboard = u"From the clipboard."_s;

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return !notices.isEmpty(); }));
    QCOMPARE(notices.first().at(0).value<Speaker::Notice>(), Speaker::Notice::Problem);
    QCOMPARE(notices.first().at(2).toString(), u"Nothing is selected."_s);
    QCOMPARE(m_source->clipboardReads, 0);
    QVERIFY(m_engine->calls().isEmpty());
}

void TestSpeaker::emptyClipboardIsReported()
{
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);
    m_settings->setSelectionFallbackToClipboard(true);
    m_speaker->setEngine(m_engine);

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return !notices.isEmpty(); }));
    QCOMPARE(notices.first().at(0).value<Speaker::Notice>(), Speaker::Notice::Problem);
    QCOMPARE(notices.first().at(2).toString(),
             u"Nothing is selected, and the clipboard is empty."_s);
    QVERIFY(m_engine->calls().isEmpty());
}

void TestSpeaker::togglePausePausesAndResumes()
{
    QVERIFY(startSpeaking());

    m_speaker->togglePause();
    QCOMPARE(m_controller->state(), State::Paused);

    m_speaker->togglePause();
    QCOMPARE(m_controller->state(), State::Playing);
}

void TestSpeaker::stopStopsSpeech()
{
    QVERIFY(startSpeaking());

    m_speaker->stop();

    QCOMPARE(m_controller->state(), State::Idle);
}

void TestSpeaker::startedNoticeNamesTheSourceAndTheFirstWords()
{
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);

    QVERIFY(startSpeaking(u"One two three four five six seven eight nine ten."_s));

    QCOMPARE(notices.size(), 1);
    QCOMPARE(notices.first().at(0).value<Speaker::Notice>(), Speaker::Notice::Started);
    QCOMPARE(notices.first().at(1).toString(), u"Reading the selection"_s);
    QCOMPARE(notices.first().at(2).toString(), u"One two three four five six seven eight..."_s);
}

void TestSpeaker::startedNoticeLeavesOutTheSourceWhenTurnedOff()
{
    m_settings->setNotifySource(false);
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);

    QVERIFY(startSpeaking(u"Short."_s));

    QCOMPARE(notices.size(), 1);
    QCOMPARE(notices.first().at(1).toString(), u"Reading"_s);
    QCOMPARE(notices.first().at(2).toString(), u"Short."_s);
}

void TestSpeaker::startedNoticeIsNotShownWhenTurnedOff()
{
    m_settings->setNotifyStart(false);
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);

    QVERIFY(startSpeaking());

    QVERIFY(notices.isEmpty());
}

void TestSpeaker::fallbackIsAnnouncedEvenWithStartedNoticesOff()
{
    m_settings->setNotifyStart(false);
    m_settings->setSelectionFallbackToClipboard(true);
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);
    m_speaker->setEngine(m_engine);
    m_source->clipboard = u"From the clipboard."_s;

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return !notices.isEmpty(); }));
    QCOMPARE(notices.first().at(0).value<Speaker::Notice>(), Speaker::Notice::Started);
    QCOMPARE(notices.first().at(1).toString(), u"Nothing selected: reading the clipboard"_s);
}

void TestSpeaker::pausedNoticeFollowsItsSetting()
{
    m_settings->setNotifyStart(false);
    QVERIFY(startSpeaking());
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);

    m_speaker->togglePause();
    QCOMPARE(notices.size(), 1);
    QCOMPARE(notices.first().at(0).value<Speaker::Notice>(), Speaker::Notice::Paused);
    m_speaker->togglePause();

    m_settings->setNotifyPause(false);
    m_speaker->togglePause();
    QCOMPARE(notices.size(), 1);
}

void TestSpeaker::playbackErrorsAreAlwaysReported()
{
    QSignalSpy notices(m_speaker.get(), &Speaker::notify);
    m_settings->setNotifyStart(false);
    m_speaker->setEngine(m_engine);
    m_source->selection = u"FAIL"_s;

    m_speaker->speakSelection();

    QVERIFY(waitUntil([&] { return !notices.isEmpty(); }));
    QCOMPARE(notices.first().at(0).value<Speaker::Notice>(), Speaker::Notice::Problem);
}

QTEST_GUILESS_MAIN(TestSpeaker)
#include "tst_speaker.moc"
