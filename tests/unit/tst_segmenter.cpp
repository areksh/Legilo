#include "core/text/Segmenter.h"

#include <QTest>

using legilo::Segment;
using legilo::SegmenterOptions;
using namespace Qt::StringLiterals;

namespace {

// The text of each segment, for readable comparisons.
QStringList textsOf(const QString &text, const SegmenterOptions &options = {})
{
    QStringList texts;
    for (const Segment &s : legilo::segment(text, options)) {
        texts.append(text.sliced(s.start, s.end - s.start));
    }
    return texts;
}

} // namespace

class TestSegmenter : public QObject
{
    Q_OBJECT

private slots:
    void splitsSentences_data();
    void splitsSentences();
    void numbersParagraphs();
    void excludesSurroundingWhitespace();
    void acceptsExtraAbbreviations();
    void splitsLongSentences_data();
    void splitsLongSentences();
};

void TestSegmenter::splitsSentences_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QStringList>("expected");

    const auto row = [](const char *name, const QString &text, const QStringList &expected) {
        QTest::newRow(name) << text << expected;
    };

    row("empty", QString(), {});
    row("whitespace only", u"  \n\n  "_s, {});
    row("one sentence", u"The quick brown fox."_s, {u"The quick brown fox."_s});
    row("no final stop", u"No full stop here"_s, {u"No full stop here"_s});
    row("full stop, question and exclamation",
        u"It rained. Did it stop? It did not! Then it snowed."_s,
        {u"It rained."_s, u"Did it stop?"_s, u"It did not!"_s, u"Then it snowed."_s});
    row("closing quote stays with its sentence", u"He said \"Stop.\" Then he left."_s,
        {u"He said \"Stop.\""_s, u"Then he left."_s});
    row("closing bracket stays with its sentence", u"It ended (at last.) Then quiet."_s,
        {u"It ended (at last.)"_s, u"Then quiet."_s});
    row("lowercase after a full stop is no boundary", u"I went home. then I slept."_s,
        {u"I went home. then I slept."_s});
    row("digit starts a sentence", u"The hall was big. 50 people came."_s,
        {u"The hall was big."_s, u"50 people came."_s});
    row("opening quote starts a sentence", u"He left the room. \"Why?\" she asked him."_s,
        {u"He left the room."_s, u"\"Why?\" she asked him."_s});
    row("titles do not end sentences", u"Mr. Smith met Dr. Jones and Prof. Brown."_s,
        {u"Mr. Smith met Dr. Jones and Prof. Brown."_s});
    row("St and vs", u"They played St. Paul vs. The Rest at home."_s,
        {u"They played St. Paul vs. The Rest at home."_s});
    row("e.g. and i.e.", u"Use a tool, e.g. A spanner, i.e. The big one."_s,
        {u"Use a tool, e.g. A spanner, i.e. The big one."_s});
    row("et al.", u"As Smith et al. Showed in their work."_s,
        {u"As Smith et al. Showed in their work."_s});
    row("months", u"It was due on Jan. 5 and paid on Feb. 2 this year."_s,
        {u"It was due on Jan. 5 and paid on Feb. 2 this year."_s});
    row("No. before a number", u"See item No. 5 for the details."_s,
        {u"See item No. 5 for the details."_s});
    row("No. before a word ends a sentence", u"The answer was No. Then he left."_s,
        {u"The answer was No."_s, u"Then he left."_s});
    row("Fig. and pp. before numbers", u"See Fig. 3 on pp. 12 to 14 for this."_s,
        {u"See Fig. 3 on pp. 12 to 14 for this."_s});
    row("initials", u"J. R. R. Tolkien wrote the book."_s, {u"J. R. R. Tolkien wrote the book."_s});
    row("etc. can end a sentence", u"Apples, pears etc. Then came more fruit."_s,
        {u"Apples, pears etc."_s, u"Then came more fruit."_s});
    row("Inc. can end a sentence", u"He works for Acme Inc. Was it founded long ago?"_s,
        {u"He works for Acme Inc."_s, u"Was it founded long ago?"_s});
    row("ellipsis before a capital", u"Wait for it... Then go on."_s,
        {u"Wait for it..."_s, u"Then go on."_s});
    row("ellipsis before lowercase", u"Wait for it... and then go on."_s,
        {u"Wait for it... and then go on."_s});
    row("ellipsis character", u"Wait for it\u2026 Then go on."_s,
        {u"Wait for it\u2026"_s, u"Then go on."_s});
    row("decimal number", u"It was 3.5 metres long. Then it grew."_s,
        {u"It was 3.5 metres long."_s, u"Then it grew."_s});
    row("URL kept whole", u"See https://example.com/A.Html for more. Next one here."_s,
        {u"See https://example.com/A.Html for more."_s, u"Next one here."_s});
    row("email kept whole", u"Write to Jo.Smith@Example.org with it. Then wait."_s,
        {u"Write to Jo.Smith@Example.org with it."_s, u"Then wait."_s});
    row("blank line ends a segment", u"A title line\n\nThe body text follows."_s,
        {u"A title line"_s, u"The body text follows."_s});
    row("single line break is not a boundary", u"The first line\nruns on here."_s,
        {u"The first line\nruns on here."_s});
    row("one word joins the sentence before", u"Count with me. Eight. Then nine and ten."_s,
        {u"Count with me. Eight."_s, u"Then nine and ten."_s});
    row("one word at the start joins the next", u"Hello. This is Legilo."_s,
        {u"Hello. This is Legilo."_s});
    row("very short joins the sentence before", u"It was over. Ok! We left."_s,
        {u"It was over. Ok!"_s, u"We left."_s});
    row("one word never crosses a paragraph", u"A first paragraph.\n\nYes.\n\nAnd a third one."_s,
        {u"A first paragraph."_s, u"Yes."_s, u"And a third one."_s});
}

void TestSegmenter::splitsSentences()
{
    QFETCH(QString, text);
    QFETCH(QStringList, expected);

    QCOMPARE(textsOf(text), expected);
}

void TestSegmenter::numbersParagraphs()
{
    const QString text = u"One. Two words.\n\n\n\nThree is here.\n \nFour is last."_s;

    const QList<Segment> segments = legilo::segment(text);

    QList<int> paragraphs;
    for (const Segment &s : segments) {
        paragraphs.append(s.paragraph);
    }
    QCOMPARE(paragraphs, (QList<int>{0, 1, 2}));
    QCOMPARE(textsOf(text),
             (QStringList{u"One. Two words."_s, u"Three is here."_s, u"Four is last."_s}));
}

void TestSegmenter::excludesSurroundingWhitespace()
{
    const QString text = u"  First one.   Second one.  "_s;

    const QList<Segment> segments = legilo::segment(text);

    QCOMPARE(segments, (QList<Segment>{{2, 12, 0}, {15, 26, 0}}));
}

void TestSegmenter::acceptsExtraAbbreviations()
{
    const QString text = u"See Sect. Four for the rest of it."_s;
    SegmenterOptions options;
    options.abbreviations = {u"sect"_s};

    QCOMPARE(textsOf(text), (QStringList{u"See Sect."_s, u"Four for the rest of it."_s}));
    QCOMPARE(textsOf(text, options), QStringList{text});
}

void TestSegmenter::splitsLongSentences_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("at a semicolon")
        << u"The first clause runs on here; the second clause runs on too."_s
        << QStringList{u"The first clause runs on here;"_s, u"the second clause runs on too."_s};
    QTest::newRow("semicolon before a nearer comma")
        << u"Alpha beta, gamma delta epsilon; zeta eta theta iota kappa."_s
        << QStringList{u"Alpha beta, gamma delta epsilon;"_s, u"zeta eta theta iota kappa."_s};
    QTest::newRow("at a spaced dash")
        << u"The first part goes on for a while - and the rest goes on too."_s
        << QStringList{u"The first part goes on for a while -"_s, u"and the rest goes on too."_s};
    QTest::newRow("at whitespace nearest the middle")
        << u"one two three four five six seven eight nine ten eleven"_s
        << QStringList{u"one two three four five six"_s, u"seven eight nine ten eleven"_s};
    QTest::newRow("repeatedly until every part fits")
        << u"one two three four five six seven eight nine ten eleven twelve thirteen "
           u"fourteen fifteen sixteen seventeen eighteen nineteen twenty"_s
        << QStringList{u"one two three four five six seven"_s, u"eight nine ten eleven twelve"_s,
                       u"thirteen fourteen fifteen sixteen"_s,
                       u"seventeen eighteen nineteen twenty"_s};
    QTest::newRow("one long word cannot be split")
        << u"Supercalifragilisticexpialidociousandthensomemoreletters."_s
        << QStringList{u"Supercalifragilisticexpialidociousandthensomemoreletters."_s};
}

void TestSegmenter::splitsLongSentences()
{
    QFETCH(QString, text);
    QFETCH(QStringList, expected);
    SegmenterOptions options;
    options.maxLength = 40;

    QCOMPARE(textsOf(text, options), expected);
}

QTEST_GUILESS_MAIN(TestSegmenter)
#include "tst_segmenter.moc"
