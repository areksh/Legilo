#include "app/SimpleSegmenter.h"

#include <QTest>

using legilo::simpleSegments;
using namespace Qt::StringLiterals;

class TestSimpleSegmenter : public QObject
{
    Q_OBJECT

private slots:
    void segments_data();
    void segments();
};

void TestSimpleSegmenter::segments_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("empty") << QString() << QStringList();
    QTest::newRow("whitespace only") << u"  \n\n \t"_s << QStringList();
    QTest::newRow("one sentence") << u"The quick brown fox."_s
                                  << QStringList{u"The quick brown fox."_s};
    QTest::newRow("no final stop") << u"No full stop here"_s << QStringList{u"No full stop here"_s};
    QTest::newRow("sentence ends")
        << u"It rained. Did it stop? It did not! Then it snowed."_s
        << QStringList{u"It rained."_s, u"Did it stop?"_s, u"It did not!"_s, u"Then it snowed."_s};
    QTest::newRow("closing quote stays with its sentence")
        << u"She said \"Go home.\" He went."_s
        << QStringList{u"She said \"Go home.\""_s, u"He went."_s};
    QTest::newRow("full stop inside a word")
        << u"See example.com for more. Then stop."_s
        << QStringList{u"See example.com for more."_s, u"Then stop."_s};
    QTest::newRow("lines of a paragraph are joined")
        << u"The quick brown fox\njumps over\nthe lazy dog."_s
        << QStringList{u"The quick brown fox jumps over the lazy dog."_s};
    QTest::newRow("blank line ends a paragraph")
        << u"First paragraph here\n\n  \nSecond paragraph here."_s
        << QStringList{u"First paragraph here"_s, u"Second paragraph here."_s};
    QTest::newRow("one word joins the sentence before")
        << u"Count with me. Eight. Then nine and ten."_s
        << QStringList{u"Count with me. Eight."_s, u"Then nine and ten."_s};
    QTest::newRow("one word at the start joins the next")
        << u"Hello. This is Legilo."_s << QStringList{u"Hello. This is Legilo."_s};
    QTest::newRow("one word never crosses a paragraph")
        << u"A first paragraph.\n\nYes.\n\nAnd a third."_s
        << QStringList{u"A first paragraph."_s, u"Yes."_s, u"And a third."_s};
    QTest::newRow("runs of whitespace collapse")
        << u"Too   many\tspaces.  Here."_s << QStringList{u"Too many spaces. Here."_s};
}

void TestSimpleSegmenter::segments()
{
    QFETCH(QString, text);
    QFETCH(QStringList, expected);

    QCOMPARE(simpleSegments(text), expected);
}

QTEST_GUILESS_MAIN(TestSimpleSegmenter)
#include "tst_simplesegmenter.moc"
