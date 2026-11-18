#include "app/ShortcutActions.h"

#include <QSet>
#include <QTest>

using legilo::ShortcutAction;
using namespace Qt::StringLiterals;

class TestShortcutActions : public QObject
{
    Q_OBJECT

private slots:
    void actionsHaveTheDesignDefaults();
    void actionIdsAreUniqueAndLabelled();
    void sequenceProblems_data();
    void sequenceProblems();
};

void TestShortcutActions::actionsHaveTheDesignDefaults()
{
    QHash<QString, QKeySequence> defaults;
    for (const ShortcutAction &action : legilo::shortcutActions()) {
        defaults.insert(action.id, action.defaultSequence);
    }

    QCOMPARE(defaults.size(), 7);
    QCOMPARE(defaults.value(u"speak-selection"_s), QKeySequence(u"Meta+Alt+S"_s));
    QCOMPARE(defaults.value(u"speak-clipboard"_s), QKeySequence(u"Meta+Alt+C"_s));
    QCOMPARE(defaults.value(u"pause-resume"_s), QKeySequence(u"Meta+Alt+P"_s));
    QCOMPARE(defaults.value(u"stop"_s), QKeySequence(u"Meta+Alt+X"_s));
    QVERIFY(defaults.contains(u"speak-pdf-text"_s));
    QVERIFY(defaults.value(u"speak-pdf-text"_s).isEmpty());
    QVERIFY(defaults.contains(u"faster"_s));
    QVERIFY(defaults.value(u"faster"_s).isEmpty());
    QVERIFY(defaults.contains(u"slower"_s));
    QVERIFY(defaults.value(u"slower"_s).isEmpty());
}

void TestShortcutActions::actionIdsAreUniqueAndLabelled()
{
    QSet<QString> ids;
    for (const ShortcutAction &action : legilo::shortcutActions()) {
        QVERIFY2(!ids.contains(action.id), qPrintable(action.id));
        ids.insert(action.id);
        QVERIFY(!action.label.isEmpty());
    }
}

void TestShortcutActions::sequenceProblems_data()
{
    QTest::addColumn<QKeySequence>("sequence");
    QTest::addColumn<bool>("allowed");

    QTest::newRow("empty clears") << QKeySequence() << true;
    QTest::newRow("Meta+Alt") << QKeySequence(u"Meta+Alt+S"_s) << true;
    QTest::newRow("Ctrl") << QKeySequence(u"Ctrl+F12"_s) << true;
    QTest::newRow("Alt+Shift") << QKeySequence(u"Alt+Shift+R"_s) << true;
    QTest::newRow("no modifier") << QKeySequence(u"S"_s) << false;
    QTest::newRow("Shift alone") << QKeySequence(u"Shift+S"_s) << false;
    QTest::newRow("two chords") << QKeySequence(u"Ctrl+K, Ctrl+L"_s) << false;
}

void TestShortcutActions::sequenceProblems()
{
    QFETCH(QKeySequence, sequence);
    QFETCH(bool, allowed);

    const QString problem = legilo::shortcutProblem(sequence);

    QCOMPARE(problem.isEmpty(), allowed);
}

QTEST_GUILESS_MAIN(TestShortcutActions)
#include "tst_shortcutactions.moc"
