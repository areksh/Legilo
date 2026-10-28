#include "core/tts/Phonemiser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <array>
#include <atomic>
#include <thread>

using legilo::Phonemiser;

namespace {

// Phonemises text and returns the sentences as strings, or a marker that
// no expected value contains if the Phonemiser fails.
QStringList phonemise(const QString &text, const QString &voice)
{
    const auto sentences = Phonemiser::instance().phonemise(text, voice);
    if (!sentences) {
        return {QStringLiteral("<phonemise failed>")};
    }
    QStringList strings;
    for (const auto &sentence : *sentences) {
        strings.append(QString::fromStdU32String(sentence));
    }
    return strings;
}

QJsonArray loadCases(const QString &voice)
{
    QFile file(QStringLiteral(LEGILO_FIXTURES_DIR "/phonemes/%1.json").arg(voice));
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Cannot open %s", qPrintable(file.fileName()));
    }
    return QJsonDocument::fromJson(file.readAll()).object().value(u"cases").toArray();
}

// The fixtures come from piper-phonemize, which ships its own fork of
// espeak-ng's data; the Phonemiser uses the system's. They differ in one
// place these cases reach: espeak-ng 1.52 reads the American "or" as "ɔːɹ"
// where the fork has "oːɹ". Given the fork's data, the Phonemiser matches
// every case unchanged.
QString withSystemDataDifferences(QString sentence, const QString &voice)
{
    if (voice == u"en-us") {
        sentence.replace(QStringLiteral("oːɹ"), QStringLiteral("ɔːɹ"));
    }
    return sentence;
}

QStringList expectedSentences(const QJsonObject &testCase, const QString &voice)
{
    QStringList sentences;
    for (const auto &sentence : testCase.value(u"sentences").toArray()) {
        // piper-phonemize returns an empty sentence for empty input;
        // sherpa-onnx rejects those, so the Phonemiser leaves them out.
        if (!sentence.toString().isEmpty()) {
            sentences.append(withSystemDataDifferences(sentence.toString(), voice));
        }
    }
    return sentences;
}

// Phonemises text count times and returns how many results were wrong.
int countMismatches(const QString &text, const QString &voice, const QStringList &expected,
                    int count)
{
    int mismatches = 0;
    for (int i = 0; i < count; ++i) {
        if (phonemise(text, voice) != expected) {
            ++mismatches;
        }
    }
    return mismatches;
}

} // namespace

class TestPhonemiser : public QObject
{
    Q_OBJECT

private slots:
    void matchesPiperPhonemize_data();
    void matchesPiperPhonemize();
    void unknownVoiceFails();
    void voiceCanChangeBetweenCalls();
    void concurrentCallsGiveCorrectResults();
};

void TestPhonemiser::matchesPiperPhonemize_data()
{
    QTest::addColumn<QString>("voice");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QStringList>("expected");

    for (const QString &voice : {QStringLiteral("en"), QStringLiteral("en-us")}) {
        const QJsonArray cases = loadCases(voice);
        for (qsizetype i = 0; i < cases.size(); ++i) {
            const QJsonObject testCase = cases.at(i).toObject();
            QTest::addRow("%s-%02lld", qPrintable(voice), static_cast<long long>(i))
                << voice << testCase.value(u"text").toString()
                << expectedSentences(testCase, voice);
        }
    }
}

void TestPhonemiser::matchesPiperPhonemize()
{
    QFETCH(QString, voice);
    QFETCH(QString, text);
    QFETCH(QStringList, expected);

    QCOMPARE(phonemise(text, voice), expected);
}

void TestPhonemiser::unknownVoiceFails()
{
    const auto sentences = Phonemiser::instance().phonemise(QStringLiteral("Hello."),
                                                            QStringLiteral("xx-no-such-voice"));

    QVERIFY(!sentences.has_value());
}

void TestPhonemiser::voiceCanChangeBetweenCalls()
{
    const QString text = QStringLiteral("The lazy dog.");

    const QStringList british = phonemise(text, QStringLiteral("en"));
    const QStringList american = phonemise(text, QStringLiteral("en-us"));
    const QStringList britishAgain = phonemise(text, QStringLiteral("en"));

    QCOMPARE(british, QStringList{QStringLiteral("ðə lˈeɪzi dˈɒɡ.")});
    QCOMPARE(american, QStringList{QStringLiteral("ðə lˈeɪzi dˈɑːɡ.")});
    QCOMPARE(britishAgain, british);
}

void TestPhonemiser::concurrentCallsGiveCorrectResults()
{
    // espeak-ng keeps global state: unserialised calls from several threads
    // mix up voices and clauses, or deadlock (the test then times out).
    const QString text = QStringLiteral("Read the record, then record the read.");
    const std::array voices{QStringLiteral("en"), QStringLiteral("en-us")};
    const std::array expected{phonemise(text, voices[0]), phonemise(text, voices[1])};
    QVERIFY(expected[0] != expected[1]);
    constexpr int threadCount = 4;
    std::atomic<int> mismatches = 0;

    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back(
            [&, v = t % 2] { mismatches += countMismatches(text, voices[v], expected[v], 25); });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    QCOMPARE(mismatches.load(), 0);
}

QTEST_GUILESS_MAIN(TestPhonemiser)
#include "tst_phonemiser.moc"
