#include "core/text/PdfDetector.h"

#include <QTest>

using legilo::DetectionHints;
using legilo::DetectorWeights;
using legilo::detectPdf;
using namespace Qt::StringLiterals;

namespace {

// Four wrapped lines of prose: each runs on into the next.
const QString WrappedProse = u"The quick brown fox jumps over the lazy dog and\n"
                             u"then runs into the forest where it hides from the\n"
                             u"hunters who are looking for it all through the\n"
                             u"long winter night until the morning comes."_s;

// Nine lines that run on, eight times in a row.
const QString LongProse = u"The survey covered every school in the county and\n"
                          u"asked the teachers how often they read aloud to\n"
                          u"their classes, how long each reading lasted, and\n"
                          u"which books they chose, both for the youngest\n"
                          u"children and for those about to leave, so that\n"
                          u"the results could be compared with the figures\n"
                          u"from the year before, when the same questions\n"
                          u"had been put to a smaller group of schools in\n"
                          u"the neighbouring county."_s;

double feature(const QString &text, const QString &name, const DetectionHints &hints = {})
{
    return detectPdf(text, hints).features.value(name, -1.0);
}

} // namespace

class TestPdfDetector : public QObject
{
    Q_OBJECT

private slots:
    void fewerThanThreeLinesIsNeverPdf();
    void reportsEveryFeature();
    void wrappedProseIsPdf();
    void shortRunOfWrappedLinesScoresLess();
    void wrapIsTheLongestRun();
    void poemIsNotPdf();
    void linesStartingWithDigitsOrBracketsDoNotWrap();
    void blankLinesAreIgnored();
    void fragmentsDoNotWrap();
    void shortLinesOfProseStillWrap();
    void hyphenatedBreakAcrossLines();
    void hyphenatedBreakInsideLine();
    void compoundWordIsNoBreak();
    void hintsSetTheirFeatures();
    void pageNumberLine();
    void listMarkers();
    void symbolDenseLines();
    void sentencesEndingInsideLines();
    void scoreIsWeightedSumAgainstThreshold();
};

void TestPdfDetector::fewerThanThreeLinesIsNeverPdf()
{
    const auto result = detectPdf(u"One line that runs on\nand on into the next one."_s);

    QVERIFY(!result.isPdf);
    QCOMPARE(result.score, 0.0);
}

void TestPdfDetector::reportsEveryFeature()
{
    const auto result = detectPdf(WrappedProse);

    QCOMPARE(result.features.keys(),
             (QStringList{u"html"_s, u"hyphen"_s, u"ligature"_s, u"list"_s, u"midline"_s,
                          u"owner"_s, u"pagenum"_s, u"symbols"_s, u"wrap"_s}));
}

void TestPdfDetector::wrappedProseIsPdf()
{
    const auto result = detectPdf(LongProse);

    QCOMPARE(result.features.value(u"wrap"_s), 1.0);
    QCOMPARE(result.score, DetectorWeights{}.wrap);
    QVERIFY(result.isPdf);
}

// Eight wrapped lines in a row are full marks; three are three eighths.
void TestPdfDetector::shortRunOfWrappedLinesScoresLess()
{
    QCOMPARE(feature(WrappedProse, u"wrap"_s), 3.0 / 8.0);
}

// A letter's address block does not dilute the wrapped body after it.
void TestPdfDetector::wrapIsTheLongestRun()
{
    const QString letter = u"14 Orchard Lane\nLittle Hampden\nOxfordshire\nOX7 4QT\n"_s + LongProse;

    QCOMPARE(feature(letter, u"wrap"_s), 1.0);
}

// Each line starts with a capital, so none runs on.
void TestPdfDetector::poemIsNotPdf()
{
    const QString poem = u"Hope is the thing with feathers\nThat perches in the soul,\n"
                         u"And sings the tune without the words,\nAnd never stops at all,"_s;

    const auto result = detectPdf(poem);

    QCOMPARE(result.features.value(u"wrap"_s), 0.0);
    QVERIFY(!result.isPdf);
}

// Logs, tables, chat and checklists start line after line with a digit or
// a bracket; in the corpus that marked structure far more often than prose
// running on.
void TestPdfDetector::linesStartingWithDigitsOrBracketsDoNotWrap()
{
    const QString log = u"2026-10-30 09:12:01 INFO Starting service on port\n"
                        u"2026-10-30 09:12:02 INFO Connected to the database\n"
                        u"[x] renew the passport before the trip abroad\n"
                        u"(and remember to book the dentist for Friday)"_s;

    QCOMPARE(feature(log, u"wrap"_s), 0.0);
}

// Firefox puts blank lines between the lines of a paragraph.
void TestPdfDetector::blankLinesAreIgnored()
{
    QString spaced = WrappedProse;
    spaced.replace(u'\n', u"\n\n"_s);

    QCOMPARE(feature(spaced, u"wrap"_s), feature(WrappedProse, u"wrap"_s));
}

// Equations and table cells, copied a symbol or a cell to a line, can
// start line after line with a lowercase letter without being prose.
void TestPdfDetector::fragmentsDoNotWrap()
{
    const QString equation = u"sum over\ni and j\nwith k\nfixed by\nr = 0.10\nthe rule"_s;

    QCOMPARE(feature(equation, u"wrap"_s), 0.0);
}

// Three words are enough for a line to run on.
void TestPdfDetector::shortLinesOfProseStillWrap()
{
    const QString prose = u"It was a long\nway home for\nthe tired dog\nand its owner."_s;

    QCOMPARE(feature(prose, u"wrap"_s), 3.0 / 8.0);
}

void TestPdfDetector::hyphenatedBreakAcrossLines()
{
    const QString text = u"The measurements came from the inter-\nferometric array, which ran\n"
                         u"for three nights in a row."_s;

    QCOMPARE(feature(text, u"hyphen"_s), 1.0);
    QCOMPARE(feature(WrappedProse, u"hyphen"_s), 0.0);
}

// Chromium copies a hyphenated break as one line, with a space after the
// hyphen.
void TestPdfDetector::hyphenatedBreakInsideLine()
{
    const QString text =
        u"The measurements came from the inter- ferometric array\nwhich ran for three\n"
        u"nights in a row."_s;

    QCOMPARE(feature(text, u"hyphen"_s), 1.0);
}

void TestPdfDetector::compoundWordIsNoBreak()
{
    const QString text =
        u"It is a well-known fact\nthat foxes are quick-witted\nand rarely caught."_s;

    QCOMPARE(feature(text, u"hyphen"_s), 0.0);
}

void TestPdfDetector::hintsSetTheirFeatures()
{
    DetectionHints hints;
    hints.hadLigatures = true;
    hints.mimeTypes = {u"text/plain"_s, u"text/html"_s};
    hints.ownerHint = u"SumatraPDF.exe"_s;

    QCOMPARE(feature(WrappedProse, u"ligature"_s, hints), 1.0);
    QCOMPARE(feature(WrappedProse, u"html"_s, hints), 1.0);
    QCOMPARE(feature(WrappedProse, u"owner"_s, hints), 1.0);
    QCOMPARE(feature(WrappedProse, u"ligature"_s), 0.0);
    QCOMPARE(feature(WrappedProse, u"html"_s), 0.0);
    DetectionHints other;
    other.ownerHint = u"notepad.exe"_s;
    QCOMPARE(feature(WrappedProse, u"owner"_s, other), 0.0);
}

void TestPdfDetector::pageNumberLine()
{
    QCOMPARE(feature(WrappedProse + u"\n12"_s, u"pagenum"_s), 1.0);
    QCOMPARE(feature(WrappedProse, u"pagenum"_s), 0.0);
}

void TestPdfDetector::listMarkers()
{
    QCOMPARE(feature(u"Shopping\n- eggs\n- milk\n- bread"_s, u"list"_s), 0.75);
}

// Code, shell sessions and markup are dense in symbols that prose rarely
// uses; a line counts when at least 5% of its characters are among them.
void TestPdfDetector::symbolDenseLines()
{
    QCOMPARE(feature(u"for f in *.jpg; do\n    convert \"$f\" small/$f\ndone"_s, u"symbols"_s),
             2.0 / 3.0);
    QCOMPARE(feature(WrappedProse, u"symbols"_s), 0.0);
}

// Wrapped prose ends sentences in the middle of lines; lists, chat and
// poems end them, if at all, at the end.
void TestPdfDetector::sentencesEndingInsideLines()
{
    const QString prose = u"The first sentence ends here. The second one begins\n"
                          u"and runs on into a third line, which ends. Then\n"
                          u"another sentence starts and comes to an end."_s;

    QCOMPARE(feature(prose, u"midline"_s), 2.0 / 3.0);
    QCOMPARE(feature(u"Buy eggs.\nBuy milk.\nBuy bread."_s, u"midline"_s), 0.0);
    QCOMPARE(feature(u"See e.g. this.\nWe met Dr. Smith.\nAt 3.5 metres."_s, u"midline"_s), 0.0);
}

void TestPdfDetector::scoreIsWeightedSumAgainstThreshold()
{
    DetectorWeights weights{.wrap = 1.0,
                            .midline = 0,
                            .hyphen = 0,
                            .ligature = 0,
                            .pagenum = 0,
                            .owner = 0,
                            .list = 0,
                            .symbols = 0,
                            .html = 0,
                            .threshold = 0.9};

    const auto above = detectPdf(LongProse, {}, weights);
    weights.threshold = 1.1;
    const auto below = detectPdf(LongProse, {}, weights);

    QCOMPARE(above.score, 1.0);
    QVERIFY(above.isPdf);
    QVERIFY(!below.isPdf);
}

QTEST_GUILESS_MAIN(TestPdfDetector)
#include "tst_pdfdetector.moc"
