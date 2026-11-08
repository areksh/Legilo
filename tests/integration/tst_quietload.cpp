#include "core/tts/SherpaTtsEngine.h"
#include "core/tts/Voice.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>
#include <iostream>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

using legilo::SherpaTtsEngine;
using legilo::Voice;
using namespace Qt::StringLiterals;

// Loading a voice must not fill the terminal or the journal. Debian's
// ONNX Runtime registers ONNX's operator schemas again when its
// environment starts, and libonnx prints each one it already has, about
// 570 lines, to std::cerr. That happens once per process, so this test
// has a process of its own and loads a voice once, with standard error
// redirected to a file.
namespace {

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

} // namespace

class TestQuietLoad : public QObject
{
    Q_OBJECT

private slots:
    void loadingAVoicePrintsNoSchemaErrors();
};

void TestQuietLoad::loadingAVoicePrintsNoSchemaErrors()
{
#ifdef Q_OS_UNIX
    if (!QFileInfo::exists(cori().modelPath)) {
        QSKIP("The test voices are not installed (scripts/fetch-deps --test-voices)");
    }
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(u"stderr.txt"_s);

    std::cerr.flush();
    std::fflush(stderr);
    const int saved = ::dup(STDERR_FILENO);
    const int file =
        ::open(QFile::encodeName(path).constData(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (saved < 0 || file < 0) {
        QFAIL("Cannot redirect standard error");
        return;
    }
    ::dup2(file, STDERR_FILENO);
    ::close(file);

    const auto engine = SherpaTtsEngine::load(cori(), 1);

    std::cerr.flush();
    std::fflush(stderr);
    ::dup2(saved, STDERR_FILENO);
    ::close(saved);

    QVERIFY(engine);
    QFile printed(path);
    QVERIFY(printed.open(QIODevice::ReadOnly));
    const QByteArray text = printed.readAll();
    QVERIFY2(!text.contains("Schema error"),
             qPrintable(u"%1 schema errors printed"_s.arg(text.count("Schema error"))));
#else
    QSKIP("Redirecting standard error is done for POSIX only");
#endif
}

QTEST_GUILESS_MAIN(TestQuietLoad)
#include "tst_quietload.moc"
