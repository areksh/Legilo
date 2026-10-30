#include "core/text/WordList.h"

#include <QTest>

using legilo::WordList;
using namespace Qt::StringLiterals;

class TestWordList : public QObject
{
    Q_OBJECT

private slots:
    void matchesIgnoringCase();
    void bundledListHasBothSpellings_data();
    void bundledListHasBothSpellings();
    void bundledListLeavesOutPossessivesAndFragments_data();
    void bundledListLeavesOutPossessivesAndFragments();
    void bundledListIsTheStandardSize();
};

void TestWordList::matchesIgnoringCase()
{
    const WordList list({u"Results"_s, u"well"_s});

    QVERIFY(list.contains(u"results"));
    QVERIFY(list.contains(u"RESULTS"));
    QVERIFY(list.contains(u"Well"));
    QVERIFY(!list.contains(u"sults"));
    QCOMPARE(list.size(), 2);
}

void TestWordList::bundledListHasBothSpellings_data()
{
    QTest::addColumn<QString>("word");

    QTest::newRow("common word") << u"results"_s;
    QTest::newRow("British spelling") << u"colour"_s;
    QTest::newRow("American spelling") << u"color"_s;
    QTest::newRow("British -ise") << u"realise"_s;
    QTest::newRow("hyphenation halves") << u"known"_s;
    QTest::newRow("capitalised entry") << u"london"_s;
}

void TestWordList::bundledListHasBothSpellings()
{
    QFETCH(QString, word);

    QVERIFY(WordList::bundled().contains(word));
}

void TestWordList::bundledListLeavesOutPossessivesAndFragments_data()
{
    QTest::addColumn<QString>("word");

    QTest::newRow("possessive") << u"colour's"_s;
    QTest::newRow("fragment") << u"sults"_s;
    QTest::newRow("joined compound") << u"wellknown"_s;
}

void TestWordList::bundledListLeavesOutPossessivesAndFragments()
{
    QFETCH(QString, word);

    QVERIFY(!WordList::bundled().contains(word));
}

// Both lists hold about 100,000 entries each, mostly shared.
void TestWordList::bundledListIsTheStandardSize()
{
    const qsizetype size = WordList::bundled().size();

    QVERIFY2(size > 60000 && size < 150000, qPrintable(QString::number(size)));
}

QTEST_GUILESS_MAIN(TestWordList)
#include "tst_wordlist.moc"
