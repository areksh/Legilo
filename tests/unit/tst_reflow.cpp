#include "core/text/Reflow.h"
#include "core/text/WordList.h"

#include <QTest>

using legilo::ReflowOptions;
using legilo::WordList;
using namespace Qt::StringLiterals;

namespace {

const WordList &words()
{
    static const WordList list({u"results"_s, u"the"_s, u"well"_s, u"known"_s, u"are"_s, u"in"_s,
                                u"fact"_s, u"a"_s, u"re"_s, u"cover"_s, u"recover"_s});
    return list;
}

QString pdf(const QString &text)
{
    return legilo::reflow(text, ReflowOptions{.pdf = true}, &words());
}

QString plain(const QString &text)
{
    return legilo::reflow(text, ReflowOptions{.pdf = false}, &words());
}

} // namespace

class TestReflow : public QObject
{
    Q_OBJECT

private slots:
    void pdfMode_data();
    void pdfMode();
    void plainText_data();
    void plainText();
    void runningHeadersCanBeKept();
    void withoutWordListPdfJoinsEveryBreak();
    void tidiesWhitespace_data();
    void tidiesWhitespace();
};

void TestReflow::pdfMode_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    const auto row = [](const char *name, const QString &text, const QString &expected) {
        QTest::newRow(name) << text << expected;
    };

    row("lines of a paragraph join", u"The quick brown\nfox jumps over\nthe lazy dog."_s,
        u"The quick brown fox jumps over the lazy dog."_s);
    row("blank line stays a paragraph break", u"First paragraph here.\n\nSecond one here."_s,
        u"First paragraph here.\n\nSecond one here."_s);
    row("Arabic page number", u"End of the page text\n12\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("page of", u"End of the page text\nPage 3 of 10\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("dashed page number", u"End of the page text\n- 7 -\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("en-dashed page number", u"End of the page text\n\u2013 7 \u2013\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("p. page number", u"End of the page text\np. 12\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("lowercase Roman page number", u"End of the page text\nxiv\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("uppercase Roman after Page", u"End of the page text\nPage IV\nand the next page."_s,
        u"End of the page text and the next page."_s);
    row("words that look Roman stay (fix 1)",
        u"The first\nI\nand\nill\nor\ncivil\nyet\nvivid\nwords."_s,
        u"The first I and ill or civil yet vivid words."_s);
    row("uppercase Roman without Page stays", u"Chapter\nIV\nbegins here."_s,
        u"Chapter IV begins here."_s);
    row("a column of numbers is a table", u"The values are\n12\n13\n14\nin all."_s,
        u"The values are 12 13 14 in all."_s);
    row("running header (fix 12)",
        u"Journal of Things, Vol. 12\nThe body of the first page\ngoes on here.\n\n"
        u"Journal of Things, Vol. 13\nThe body of the second page\ngoes on too."_s,
        u"The body of the first page goes on here.\n\nThe body of the second page goes on too."_s);
    row("a repeated sentence is no header",
        u"It is so.\nThe body of the text goes on.\n\nIt is so.\nAnd so does this part."_s,
        u"It is so.\n\nThe body of the text goes on.\n\nIt is so.\n\nAnd so does this part."_s);
    row("repeated numbers are no header",
        u"The first value\n12.5\nis low\n\nThe second value\n13.7\nis high"_s,
        u"The first value 12.5 is low\n\nThe second value 13.7 is high"_s);
    row("confirmed join", u"The re-\nsults are in."_s, u"The results are in."_s);
    row("capital kept in a join", u"Re-\nsults are in."_s, u"Results are in."_s);
    row("hyphen kept when both halves are words", u"It is a well-\nknown fact."_s,
        u"It is a well-known fact."_s);
    row("unconfirmed break joined in PDF mode", u"The foo-\nbarz thing."_s,
        u"The foobarz thing."_s);
    row("ambiguous break joined in PDF mode", u"They re-\ncover it."_s, u"They recover it."_s);
    row("dash at line end joins without a space (fix 13)",
        u"It was the end\u2014\nand then more."_s, u"It was the end\u2014and then more."_s);
    row("list items stay apart (fix 7)", u"Buy these things\n- eggs\n- milk\n- bread"_s,
        u"Buy these things\n\n- eggs\n\n- milk\n\n- bread"_s);
    row("numbered list items stay apart",
        u"The steps follow here\n1. Open the lid\n2) Pour it out"_s,
        u"The steps follow here\n\n1. Open the lid\n\n2) Pour it out"_s);
    row("short heading stays apart (fix 7)",
        u"Introduction.\nThis is a long line of body text that goes on.\n"
        u"And it continues on this line as well."_s,
        u"Introduction.\n\nThis is a long line of body text that goes on. "
        u"And it continues on this line as well."_s);
}

void TestReflow::pdfMode()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);

    QCOMPARE(pdf(text), expected);
}

// Ordinary text keeps its line breaks (fix 8); only confirmed joins apply.
void TestReflow::plainText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    QTest::newRow("line breaks kept")
        << u"Roses are red\nViolets are blue"_s << u"Roses are red\nViolets are blue"_s;
    QTest::newRow("page-number-like line kept") << u"Lines\n12\nhere"_s << u"Lines\n12\nhere"_s;
    QTest::newRow("confirmed join") << u"The re-\nsults are in."_s << u"The results are in."_s;
    QTest::newRow("hyphen and break kept when both halves are words")
        << u"a well-\nknown fact"_s << u"a well-\nknown fact"_s;
    QTest::newRow("unconfirmed break kept")
        << u"The foo-\nbarz thing."_s << u"The foo-\nbarz thing."_s;
    // "re-cover" or "recover": all three are words, so it is not confirmed.
    QTest::newRow("ambiguous break kept") << u"They re-\ncover it."_s << u"They re-\ncover it."_s;
}

void TestReflow::plainText()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);

    QCOMPARE(plain(text), expected);
}

void TestReflow::runningHeadersCanBeKept()
{
    const QString text = u"Journal of Things, Vol. 12\nBody one.\n\n"
                         u"Journal of Things, Vol. 13\nBody two."_s;

    const QString kept =
        legilo::reflow(text, ReflowOptions{.pdf = true, .removeRunningHeaders = false}, &words());

    QCOMPARE(kept,
             u"Journal of Things, Vol. 12 Body one.\n\nJournal of Things, Vol. 13 Body two."_s);
}

void TestReflow::withoutWordListPdfJoinsEveryBreak()
{
    QCOMPARE(legilo::reflow(u"a well-\nknown fact"_s, ReflowOptions{.pdf = true}, nullptr),
             u"a wellknown fact"_s);
    QCOMPARE(legilo::reflow(u"The re-\nsults"_s, ReflowOptions{.pdf = false}, nullptr),
             u"The re-\nsults"_s);
}

void TestReflow::tidiesWhitespace_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    QTest::newRow("runs of spaces and tabs") << u"a  b\t\tc"_s << u"a b c"_s;
    QTest::newRow("lines trimmed") << u"  one  \n  two  "_s << u"one\ntwo"_s;
    QTest::newRow("blank lines collapse") << u"one\n\n\n\n \ntwo"_s << u"one\n\ntwo"_s;
    QTest::newRow("leading and trailing blank lines") << u"\n\n one \n\n"_s << u"one"_s;
}

void TestReflow::tidiesWhitespace()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);

    QCOMPARE(legilo::tidyWhitespace(text), expected);
}

QTEST_GUILESS_MAIN(TestReflow)
#include "tst_reflow.moc"
