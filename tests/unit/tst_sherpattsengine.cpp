#include "core/tts/SherpaTtsEngine.h"

#include <QTemporaryDir>
#include <QTest>

using legilo::SherpaTtsEngine;
using legilo::Voice;
using namespace Qt::StringLiterals;

// What SherpaTtsEngine does without a model. Synthesis itself is covered by
// the integration tests (tests/integration/tst_synthesis.cpp).
class TestSherpaTtsEngine : public QObject
{
    Q_OBJECT

private slots:
    void threadsFor_data();
    void threadsFor();
    void loadFailsWithoutModelFiles();
};

void TestSherpaTtsEngine::threadsFor_data()
{
    QTest::addColumn<int>("setting");
    QTest::addColumn<int>("logicalCores");
    QTest::addColumn<int>("expected");

    QTest::newRow("automatic, 8 cores") << 0 << 8 << 4;
    QTest::newRow("automatic, 6 cores") << 0 << 6 << 3;
    QTest::newRow("automatic, 16 cores") << 0 << 16 << 4;
    QTest::newRow("automatic, 1 core") << 0 << 1 << 1;
    QTest::newRow("automatic, cores unknown") << 0 << -1 << 1;
    QTest::newRow("negative setting is automatic") << -3 << 8 << 4;
    QTest::newRow("setting wins") << 2 << 16 << 2;
    QTest::newRow("setting above the automatic maximum") << 8 << 8 << 8;
}

void TestSherpaTtsEngine::threadsFor()
{
    QFETCH(int, setting);
    QFETCH(int, logicalCores);
    QFETCH(int, expected);

    QCOMPARE(SherpaTtsEngine::threadsFor(setting, logicalCores), expected);
}

void TestSherpaTtsEngine::loadFailsWithoutModelFiles()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Voice voice;
    voice.id = u"piper-missing"_s;
    voice.engine = u"piper"_s;
    voice.directory = directory.path();
    voice.modelPath = directory.filePath(u"missing.onnx"_s);
    voice.tokensPath = directory.filePath(u"tokens.txt"_s);

    QVERIFY(SherpaTtsEngine::load(voice, 1) == nullptr);
}

QTEST_GUILESS_MAIN(TestSherpaTtsEngine)
#include "tst_sherpattsengine.moc"
