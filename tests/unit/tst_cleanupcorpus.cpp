#include "core/text/CleanupPipeline.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QTest>

#include <algorithm>

using legilo::CleanupPipeline;
using legilo::CleanupSettings;
using legilo::TextCapture;
using namespace Qt::StringLiterals;

// The clean-up corpus (design 9.2): documents typeset for the corpus,
// copied out of their PDFs by pdftotext, whose clean text is known. The
// pipeline must recover it exactly in Auto mode. Some of it cannot be
// recovered from text alone; those pairs are expected to differ, for the
// reason given, and must still recover at least as many paragraphs
// exactly as they do now, so a regression fails. Set
// LEGILO_CLEANUP_OUTPUT to a directory to write each output there.
namespace {

struct Known
{
    const char *gap; // why the output cannot match yet; null if it must
    int paragraphs;  // paragraphs recovered exactly, at least
};

// By pair. Qt fails a test with XPASS when an expected failure passes, so
// a gap closed by a change to the pipeline has to be taken out here.
const QHash<QString, Known> &known()
{
    static const QHash<QString, Known> pairs{
        {u"made-newsletter"_s, {"three columns, and a title wrapped in a large font", 3}},
        {u"made-novel"_s,
         {"a running header on one page, and a paragraph ending at full width", 15}},
        {u"made-paper"_s, {"a title wrapped in a large font", 11}},
        {u"made-recipe"_s, {nullptr, 17}},
        {u"made-report"_s, {"paragraphs whose last line runs to full width", 11}},
        {u"made-terms"_s, {nullptr, 10}},
    };
    return pairs;
}

QStringList paragraphsOf(const QString &text)
{
    QStringList paragraphs = text.trimmed().split(u"\n\n"_s);
    for (QString &paragraph : paragraphs) {
        paragraph = paragraph.trimmed();
    }
    return paragraphs;
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Cannot read %s", qPrintable(path));
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestCleanupCorpus : public QObject
{
    Q_OBJECT

private slots:
    void recoversTheCleanText_data();
    void recoversTheCleanText();
};

void TestCleanupCorpus::recoversTheCleanText_data()
{
    QTest::addColumn<QString>("folder");

    const QDir pairs(QStringLiteral(LEGILO_CORPUS_DIR) + u"/cleanup"_s);
    const QStringList names = pairs.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QVERIFY(!names.isEmpty());
    for (const QString &name : names) {
        QTest::newRow(qPrintable(name)) << pairs.filePath(name);
    }
}

void TestCleanupCorpus::recoversTheCleanText()
{
    QFETCH(QString, folder);
    const QString name = QDir(folder).dirName();
    QVERIFY2(known().contains(name), "a new pair needs an entry in known()");
    const Known pair = known().value(name);

    TextCapture capture;
    capture.text = readFile(folder + u"/input.txt"_s);
    const QString cleaned = CleanupPipeline().run(capture, CleanupSettings{}).cleaned;

    const QByteArray output = qgetenv("LEGILO_CLEANUP_OUTPUT");
    if (!output.isEmpty()) {
        QFile file(QDir(QString::fromLocal8Bit(output)).filePath(name + u".txt"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(cleaned.toUtf8());
    }
    const QString expected = readFile(folder + u"/expected.txt"_s).trimmed();

    const QStringList wanted = paragraphsOf(expected);
    const auto recovered = std::ranges::count_if(
        paragraphsOf(cleaned), [&](const QString &p) { return wanted.contains(p); });
    qInfo("%s: %lld of %lld paragraphs recovered exactly", qPrintable(name),
          static_cast<long long>(recovered), static_cast<long long>(wanted.size()));
    QVERIFY(recovered >= pair.paragraphs);

    if (pair.gap != nullptr) {
        QEXPECT_FAIL("", pair.gap, Continue);
    }
    QCOMPARE(cleaned, expected);
}

QTEST_GUILESS_MAIN(TestCleanupCorpus)
#include "tst_cleanupcorpus.moc"
