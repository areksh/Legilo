#include "core/tts/SherpaTtsEngine.h"

#include <QFileInfo>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <thread>

using legilo::SherpaTtsEngine;
using legilo::Voice;
using namespace Qt::StringLiterals;

// Synthesis through sherpa-onnx with real voices, fetched by
// scripts/fetch-deps --test-voices. A test whose voice is absent is
// skipped.
namespace {

const QString Sentence = u"The quick brown fox jumps over the lazy dog."_s;

Voice cori()
{
    const QString directory =
        QStringLiteral(LEGILO_TEST_VOICES_DIR "/vits-piper-en_GB-cori-medium");
    Voice voice;
    voice.id = u"piper-en_GB-cori-medium"_s;
    voice.engine = u"piper"_s;
    voice.directory = directory;
    voice.modelPath = directory + u"/en_GB-cori-medium.onnx"_s;
    voice.tokensPath = directory + u"/tokens.txt"_s;
    return voice;
}

Voice kokoro(int speakerId, const QString &espeakVoice = {})
{
    const QString directory = QStringLiteral(LEGILO_TEST_VOICES_DIR "/kokoro-en-v0_19");
    Voice voice;
    voice.id = u"kokoro-en-v0_19"_s;
    voice.engine = u"kokoro"_s;
    voice.speakerId = speakerId;
    voice.espeakVoice = espeakVoice;
    voice.directory = directory;
    voice.modelPath = directory + u"/model.onnx"_s;
    voice.tokensPath = directory + u"/tokens.txt"_s;
    voice.voicesPath = directory + u"/voices.bin"_s;
    return voice;
}

double seconds(const std::vector<float> &samples, int sampleRate)
{
    return static_cast<double>(samples.size()) / sampleRate;
}

// Speech for Sentence: about 2.5 s at normal speed, audible, in range.
bool plausibleSpeech(const std::optional<std::vector<float>> &samples, int sampleRate)
{
    if (!samples || samples->empty()) {
        return false;
    }
    const double duration = seconds(*samples, sampleRate);
    const auto [low, high] = std::ranges::minmax(*samples);
    const float peak = std::max(-low, high);
    return duration > 1.0 && duration < 6.0 && peak > 0.05F && peak <= 1.0F;
}

} // namespace

class TestSynthesis : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void piperSynthesisesSentence();
    void piperSpeedShortensSpeech();
    void piperDoesNotBabbleOnShortWord();
    void textWithNothingToSayGivesNoAudio_data();
    void textWithNothingToSayGivesNoAudio();
    void concurrentCallsOnOneEngineSucceed();
    void concurrentCallsOnTwoEnginesSucceed();
    void kokoroSpeaksFromPiperPhonemes();
    void espeakVoiceComesFromModelMetadata();
    void speakerEspeakVoiceOverridesModel();

private:
    std::unique_ptr<SherpaTtsEngine> m_cori;
    std::unique_ptr<SherpaTtsEngine> m_kokoro;
};

void TestSynthesis::initTestCase()
{
    if (QFileInfo::exists(cori().modelPath)) {
        m_cori = SherpaTtsEngine::load(cori(), 2);
        QVERIFY(m_cori);
    }
    if (QFileInfo::exists(kokoro(0).modelPath)) {
        m_kokoro = SherpaTtsEngine::load(kokoro(0), 2);
        QVERIFY(m_kokoro);
    }
}

#define REQUIRE(engine)                                                                            \
    if (!(engine)) {                                                                               \
        QSKIP("Test voice not installed; run scripts/fetch-deps --test-voices");                   \
    }

void TestSynthesis::piperSynthesisesSentence()
{
    REQUIRE(m_cori);

    const auto samples = m_cori->synthesise(Sentence, 1.0);

    QCOMPARE(m_cori->sampleRate(), 22050);
    QVERIFY(plausibleSpeech(samples, m_cori->sampleRate()));
}

// Piper draws fresh noise for every call, durations included, so one
// sentence varies by about 7% from call to call; the test averages three.
// Speed does not scale duration in proportion: at 2.0 cori takes about 0.66
// of the time (measured over 20 calls each), because each sound keeps at
// least one frame.
void TestSynthesis::piperSpeedShortensSpeech()
{
    REQUIRE(m_cori);
    const auto meanSeconds = [&](double speed) {
        double total = 0;
        for (int i = 0; i < 3; ++i) {
            const auto samples = m_cori->synthesise(Sentence, speed).value_or(std::vector<float>{});
            total += seconds(samples, m_cori->sampleRate());
        }
        return total / 3;
    };

    const double normal = meanSeconds(1.0);
    const double fast = meanSeconds(2.0);

    QVERIFY(normal > 0);
    const double ratio = fast / normal;
    QVERIFY2(ratio > 0.45 && ratio < 0.8, qPrintable(u"ratio %1"_s.arg(ratio)));
}

// sherpa-onnx 2.0.1 puts a pad token after the start marker, which Piper's
// own runtime does not. With it, cori babbles on "Open." in nearly every
// call (20 of 20 when measured, against none without it), running to two
// seconds or more; Legilo patches it out (cmake/SherpaOnnx.cmake). "Open."
// is a short word that takes about 0.6 s.
void TestSynthesis::piperDoesNotBabbleOnShortWord()
{
    REQUIRE(m_cori);
    int babbled = 0;

    for (int i = 0; i < 10; ++i) {
        const auto samples = m_cori->synthesise(u"Open."_s, 1.0).value_or(std::vector<float>{});
        if (seconds(samples, m_cori->sampleRate()) > 1.0) {
            ++babbled;
        }
    }

    QVERIFY2(babbled <= 3, qPrintable(u"%1 of 10 calls ran over 1 s"_s.arg(babbled)));
}

void TestSynthesis::textWithNothingToSayGivesNoAudio_data()
{
    QTest::addColumn<QString>("text");

    QTest::newRow("empty") << QString();
    QTest::newRow("whitespace") << u"  \n\t "_s;
}

void TestSynthesis::textWithNothingToSayGivesNoAudio()
{
    QFETCH(QString, text);
    REQUIRE(m_cori);

    const auto samples = m_cori->synthesise(text, 1.0);

    QVERIFY(samples.has_value());
    QVERIFY(samples.value_or(std::vector<float>{1.0F}).empty());
}

// The design leaves open whether sherpa-onnx's Generate is safe to call
// from several threads at once (7.3.1). These run it so, and check that
// every result is still sound speech.
void TestSynthesis::concurrentCallsOnOneEngineSucceed()
{
    REQUIRE(m_cori);
    constexpr int threadCount = 3;
    std::atomic<int> failures = 0;

    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 2; ++i) {
                if (!plausibleSpeech(m_cori->synthesise(Sentence, 1.0), m_cori->sampleRate())) {
                    ++failures;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    QCOMPARE(failures.load(), 0);
}

void TestSynthesis::concurrentCallsOnTwoEnginesSucceed()
{
    REQUIRE(m_cori);
    const auto second = SherpaTtsEngine::load(cori(), 2);
    QVERIFY(second);
    std::atomic<int> failures = 0;

    const auto speak = [&](SherpaTtsEngine &engine) {
        for (int i = 0; i < 3; ++i) {
            if (!plausibleSpeech(engine.synthesise(Sentence, 1.0), engine.sampleRate())) {
                ++failures;
            }
        }
    };
    std::thread first([&] { speak(*m_cori); });
    speak(*second);
    first.join();

    QCOMPARE(failures.load(), 0);
}

void TestSynthesis::kokoroSpeaksFromPiperPhonemes()
{
    REQUIRE(m_kokoro);

    const auto samples = m_kokoro->synthesise(Sentence, 1.0);

    QCOMPARE(m_kokoro->sampleRate(), 24000);
    QVERIFY(plausibleSpeech(samples, m_kokoro->sampleRate()));
}

void TestSynthesis::espeakVoiceComesFromModelMetadata()
{
    REQUIRE(m_cori);
    REQUIRE(m_kokoro);

    QCOMPARE(m_cori->espeakVoice(), u"en"_s);
    QCOMPARE(m_kokoro->espeakVoice(), u"en-us"_s);
}

// Kokoro's metadata says en-us for every speaker, so a British speaker
// needs its own espeakVoice. Synthesis is not deterministic (each call
// draws fresh noise), so this checks the voice the engine phonemises with
// rather than comparing audio.
void TestSynthesis::speakerEspeakVoiceOverridesModel()
{
    REQUIRE(m_kokoro);

    const auto british = SherpaTtsEngine::load(kokoro(7, u"en"_s), 1);

    QVERIFY(british);
    QCOMPARE(british->espeakVoice(), u"en"_s);
}

QTEST_GUILESS_MAIN(TestSynthesis)
#include "tst_synthesis.moc"
