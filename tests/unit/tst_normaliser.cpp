#include "core/text/Normaliser.h"

#include <QTest>

using legilo::normalise;
using namespace Qt::StringLiterals;

// Characters are written as escapes, so none of them is invisible here.
class TestNormaliser : public QObject
{
    Q_OBJECT

private slots:
    void normalisesText_data();
    void normalisesText();
    void recordsLigatures_data();
    void recordsLigatures();
};

void TestNormaliser::normalisesText_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain text unchanged")
        << u"Plain text, as typed."_s << u"Plain text, as typed."_s;
    QTest::newRow("ff ligature") << u"e\uFB00ect"_s << u"effect"_s;
    QTest::newRow("fi ligature") << u"\uFB01nd"_s << u"find"_s;
    QTest::newRow("fl ligature") << u"\uFB02ow"_s << u"flow"_s;
    QTest::newRow("ffi ligature") << u"o\uFB03ce"_s << u"office"_s;
    QTest::newRow("ffl ligature") << u"ba\uFB04e"_s << u"baffle"_s;
    QTest::newRow("long s t ligature") << u"\uFB05op"_s << u"stop"_s;
    QTest::newRow("s t ligature") << u"\uFB06ar"_s << u"star"_s;
    QTest::newRow("full-width letters and digits")
        << u"\uFF21\uFF42\uFF43 \uFF11\uFF12"_s << u"Abc 12"_s;
    QTest::newRow("full-width punctuation") << u"\uFF01 \uFF5E"_s << u"! ~"_s;
    QTest::newRow("soft hyphen removed") << u"hyphen\u00ADation"_s << u"hyphenation"_s;
    QTest::newRow("zero-width space removed") << u"zero\u200Bwidth"_s << u"zerowidth"_s;
    QTest::newRow("zero-width non-joiner removed") << u"a\u200Cb"_s << u"ab"_s;
    QTest::newRow("zero-width joiner removed") << u"a\u200Db"_s << u"ab"_s;
    QTest::newRow("byte order mark removed") << u"\uFEFFStart"_s << u"Start"_s;
    QTest::newRow("non-breaking space") << u"10\u00A0km"_s << u"10 km"_s;
    QTest::newRow("narrow non-breaking space") << u"10\u202Fkm"_s << u"10 km"_s;
    QTest::newRow("em and thin spaces") << u"a\u2003b\u2009c"_s << u"a b c"_s;
    QTest::newRow("ideographic space") << u"a\u3000b"_s << u"a b"_s;
    // Fix 5: NFKC turned these into "2" and "1/2", which espeak-ng then
    // read as "x two" and "one slash two".
    QTest::newRow("superscripts and fractions kept (fix 5)")
        << u"x\u00B2 y\u00B3 \u00BD \u00BC \u00BE"_s << u"x\u00B2 y\u00B3 \u00BD \u00BC \u00BE"_s;
    QTest::newRow("CRLF line endings") << u"one\r\ntwo\r\n"_s << u"one\ntwo\n"_s;
    QTest::newRow("CR line endings") << u"one\rtwo"_s << u"one\ntwo"_s;
    QTest::newRow("superscript left alone") << u"x\u00B2 + y\u00B3"_s << u"x\u00B2 + y\u00B3"_s;
    QTest::newRow("vulgar fraction left alone") << u"\u00BD cup"_s << u"\u00BD cup"_s;
    QTest::newRow("accented letters left alone")
        << u"caf\u00E9 na\u00EFve"_s << u"caf\u00E9 na\u00EFve"_s;
    QTest::newRow("dashes left alone") << u"a \u2013 b \u2014 c"_s << u"a \u2013 b \u2014 c"_s;
}

void TestNormaliser::normalisesText()
{
    QFETCH(QString, raw);
    QFETCH(QString, expected);

    QCOMPARE(normalise(raw).text, expected);
}

void TestNormaliser::recordsLigatures_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<bool>("hadLigatures");

    QTest::newRow("none") << u"effect"_s << false;
    QTest::newRow("fi") << u"\uFB01nd"_s << true;
    QTest::newRow("st") << u"\uFB06ar"_s << true;
}

void TestNormaliser::recordsLigatures()
{
    QFETCH(QString, raw);
    QFETCH(bool, hadLigatures);

    QCOMPARE(normalise(raw).hadLigatures, hadLigatures);
}

QTEST_GUILESS_MAIN(TestNormaliser)
#include "tst_normaliser.moc"
