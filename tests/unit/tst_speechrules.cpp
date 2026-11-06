#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "core/text/WordList.h"

#include <QTest>

using legilo::CitationRule;
using legilo::CleanupSettings;
using legilo::EmailRule;
using legilo::speakable;
using legilo::spokenSegments;
using legilo::spokenText;
using legilo::UrlRule;
using namespace Qt::StringLiterals;

namespace {

QString pdf(const QString &text, const CleanupSettings &settings = {})
{
    return speakable(text, settings, true);
}

QString plain(const QString &text, const CleanupSettings &settings = {})
{
    return speakable(text, settings, false);
}

CleanupSettings withUrls(UrlRule rule)
{
    CleanupSettings settings;
    settings.urls = rule;
    return settings;
}

CleanupSettings withCitations(CitationRule rule)
{
    CleanupSettings settings;
    settings.citations = rule;
    return settings;
}

} // namespace

class TestSpeechRules : public QObject
{
    Q_OBJECT

private slots:
    void urlsAreSpokenAsTheirHost_data();
    void urlsAreSpokenAsTheirHost();
    void urlsCanBeSpokenInFullOrRemoved();
    void closingBracketBelongsToTheUrlOnlyWhenOpenedInIt();
    void emailAddressesAreSpokenAsTheirDomain();
    void emailAddressesCanBeSpokenInFull();
    void numericCitationsGoInPdfText();
    void numericCitationsStayInOrdinaryText();
    void citationRuleCanBeAlwaysOrNever();
    void authorYearCitationsGoWhenAsked_data();
    void authorYearCitationsGoWhenAsked();
    void authorYearCitationsStayByDefault();
    void superscriptsAreSpoken();
    void fractionsAfterADigitTakeAnd();
    void documentIsSpokenSegmentBySegment();
};

void TestSpeechRules::urlsAreSpokenAsTheirHost_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("spoken");

    QTest::newRow("https and path") << u"See https://www.example.com/path?q=1 for more."_s
                                    << u"See a link to example.com for more."_s;
    QTest::newRow("full stop after")
        << u"Go to https://example.com/a."_s << u"Go to a link to example.com."_s;
    QTest::newRow("www") << u"Visit www.example.org today."_s
                         << u"Visit a link to example.org today."_s;
    QTest::newRow("port") << u"Open http://localhost:8080/x now."_s
                          << u"Open a link to localhost now."_s;
}

void TestSpeechRules::urlsAreSpokenAsTheirHost()
{
    QFETCH(QString, text);
    QFETCH(QString, spoken);

    QCOMPARE(plain(text), spoken);
}

void TestSpeechRules::urlsCanBeSpokenInFullOrRemoved()
{
    const QString text = u"See https://example.com/a for more."_s;

    QCOMPARE(plain(text, withUrls(UrlRule::Full)), text);
    QCOMPARE(plain(text, withUrls(UrlRule::Remove)), u"See for more."_s);
    QCOMPARE(plain(u"Read it at https://example.com."_s, withUrls(UrlRule::Remove)),
             u"Read it at."_s);
}

// Fix 6: the script stripped a closing bracket from every URL's end.
void TestSpeechRules::closingBracketBelongsToTheUrlOnlyWhenOpenedInIt()
{
    const CleanupSettings remove = withUrls(UrlRule::Remove);

    QCOMPARE(plain(u"(see https://en.wikipedia.org/wiki/Foo_(bar))"_s, remove), u"(see)"_s);
    QCOMPARE(plain(u"(see https://example.com/x)"_s, remove), u"(see)"_s);
    QCOMPARE(plain(u"(see https://example.com/x)"_s), u"(see a link to example.com)"_s);
}

void TestSpeechRules::emailAddressesAreSpokenAsTheirDomain()
{
    QCOMPARE(plain(u"Write to jo.smith@mail.example.com today."_s),
             u"Write to an email address at mail.example.com today."_s);
    QCOMPARE(plain(u"Ask info@www.example.com."_s), u"Ask an email address at www.example.com."_s);
}

void TestSpeechRules::emailAddressesCanBeSpokenInFull()
{
    CleanupSettings settings;
    settings.emails = EmailRule::Full;
    const QString text = u"Write to jo.smith@example.com today."_s;

    QCOMPARE(plain(text, settings), text);
}

void TestSpeechRules::numericCitationsGoInPdfText()
{
    QCOMPARE(pdf(u"Results vary [12]. Others agree [3, 4], and some [5-7] or [5\u20137] do."_s),
             u"Results vary. Others agree, and some or do."_s);
    QCOMPARE(pdf(u"It holds[2] here."_s), u"It holds here."_s);
}

// Square-bracketed numbers in ordinary text are often meant (fix 9).
void TestSpeechRules::numericCitationsStayInOrdinaryText()
{
    const QString text = u"Results vary [12] here."_s;

    QCOMPARE(plain(text), text);
    QCOMPARE(pdf(u"Note [a] and [1a] stay."_s), u"Note [a] and [1a] stay."_s);
}

void TestSpeechRules::citationRuleCanBeAlwaysOrNever()
{
    QCOMPARE(plain(u"Results vary [12] here."_s, withCitations(CitationRule::Always)),
             u"Results vary here."_s);
    QCOMPARE(pdf(u"Results vary [12] here."_s, withCitations(CitationRule::Never)),
             u"Results vary [12] here."_s);
}

void TestSpeechRules::authorYearCitationsGoWhenAsked_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("spoken");

    QTest::newRow("et al.") << u"This holds (Smith et al., 2020) widely."_s
                            << u"This holds widely."_s;
    QTest::newRow("two entries") << u"As shown (Smith and Jones 2019; Lee 2021a)."_s
                                 << u"As shown."_s;
    QTest::newRow("three authors")
        << u"Known (Smith, Jones & Lee, 2018) well."_s << u"Known well."_s;
    QTest::newRow("not a citation")
        << u"See it (see Smith 2020) there."_s << u"See it (see Smith 2020) there."_s;
    QTest::newRow("a year alone") << u"In that year (2020) it rained."_s
                                  << u"In that year (2020) it rained."_s;
}

void TestSpeechRules::authorYearCitationsGoWhenAsked()
{
    QFETCH(QString, text);
    QFETCH(QString, spoken);
    CleanupSettings settings;
    settings.authorYearCitations = true;

    QCOMPARE(plain(text, settings), spoken);
}

void TestSpeechRules::authorYearCitationsStayByDefault()
{
    const QString text = u"This holds (Smith et al., 2020) widely."_s;

    QCOMPARE(pdf(text), text);
}

// espeak-ng reads U+00B2 and U+00B3 as "two" and "three", but the
// fractions on their own correctly (design 7.2.5).
void TestSpeechRules::superscriptsAreSpoken()
{
    QCOMPARE(plain(u"x\u00B2 + y\u00B3 = z"_s), u"x squared + y cubed = z"_s);
    QCOMPARE(plain(u"Add \u00BD cup."_s), u"Add \u00BD cup."_s);
}

// After a digit espeak-ng reads U+00BD as "a half" without the "and":
// "two a half hours".
void TestSpeechRules::fractionsAfterADigitTakeAnd()
{
    QCOMPARE(plain(u"It took 2\u00BD hours and 3\u00BE days."_s),
             u"It took 2 and \u00BD hours and 3 and \u00BE days."_s);
}

// The rules apply per segment with the settings the document was cleaned
// with; the spoken text keeps the document's paragraphs.
void TestSpeechRules::documentIsSpokenSegmentBySegment()
{
    const legilo::WordList words({u"the"_s});
    legilo::TextCapture capture;
    capture.text = u"See https://example.com/a now. It works.\n\nNext [3] part."_s;
    CleanupSettings settings = withCitations(CitationRule::Always);
    const legilo::Document document = legilo::CleanupPipeline(words).run(capture, settings);

    QCOMPARE(spokenSegments(document),
             (QStringList{u"See a link to example.com now."_s, u"It works."_s, u"Next part."_s}));
    QCOMPARE(spokenText(document), u"See a link to example.com now. It works.\n\nNext part."_s);
}

QTEST_GUILESS_MAIN(TestSpeechRules)
#include "tst_speechrules.moc"
