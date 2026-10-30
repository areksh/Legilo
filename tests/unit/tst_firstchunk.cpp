#include "core/playback/FirstChunk.h"

#include <QTest>

using legilo::firstChunkLength;
using namespace Qt::StringLiterals;

class TestFirstChunk : public QObject
{
    Q_OBJECT

private slots:
    void splitsAtTheRightPlace_data();
    void splitsAtTheRightPlace();
};

void TestFirstChunk::splitsAtTheRightPlace_data()
{
    QTest::addColumn<QString>("segment");
    QTest::addColumn<int>("target");
    QTest::addColumn<QString>("firstChunk"); // the whole segment if no split

    const auto row = [](const char *name, const QString &segment, int target,
                        const QString &firstChunk) {
        QTest::newRow(name) << segment << target << firstChunk;
    };
    const QString unsplit;

    row("shorter than the target", u"Short text."_s, 20, unsplit);
    row("comma after the target", u"The quick brown fox jumps, over the lazy dog and away."_s, 20,
        u"The quick brown fox jumps,"_s);
    row("separator before the target is ignored",
        u"Well, the quick brown fox jumps over the lazy dog."_s, 20, u"Well, the quick brown"_s);
    row("semicolon", u"Semicolons and colons both count here; then more words follow on."_s, 20,
        u"Semicolons and colons both count here;"_s);
    row("colon", u"There is one thing left to say: it is all done now."_s, 20,
        u"There is one thing left to say:"_s);
    row("spaced dash beats earlier whitespace",
        u"The first part of this sentence - and then the second part goes on."_s, 20,
        u"The first part of this sentence -"_s);
    row("spaced en dash", u"The first part of this sentence – and then the second part goes on."_s,
        20, u"The first part of this sentence –"_s);
    row("hyphen inside a word is not a separator",
        u"A long and well-known example of this kind of text, with words."_s, 20,
        u"A long and well-known"_s);
    row("separator too far beyond the target",
        u"A long run of words without any punctuation for quite a while, then a comma."_s, 20,
        u"A long run of words without"_s);
    row("newline is whitespace", u"The quick brown fox\njumps over the lazy dog."_s, 18,
        u"The quick brown fox"_s);
    row("one-word remainder", u"The quick brown fox jumps everywhere."_s, 20, unsplit);
    row("one-word first chunk", u"Hello there, friend and others."_s, 1, unsplit);
    row("no whitespace after the target", u"Some words and a verylongwordwithoutanyspaces."_s, 20,
        unsplit);
}

void TestFirstChunk::splitsAtTheRightPlace()
{
    QFETCH(QString, segment);
    QFETCH(int, target);
    QFETCH(QString, firstChunk);

    const qsizetype length = firstChunkLength(segment, target);

    QCOMPARE(segment.left(length), firstChunk.isNull() ? segment : firstChunk);
}

QTEST_GUILESS_MAIN(TestFirstChunk)
#include "tst_firstchunk.moc"
