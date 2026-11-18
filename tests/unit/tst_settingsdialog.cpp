#include "app/Settings.h"
#include "app/SettingsDialog.h"
#include "app/ShortcutActions.h"
#include "fakes/FakeAutostart.h"
#include "fakes/FakeGlobalShortcuts.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using legilo::CleanupSettings;
using legilo::Settings;
using legilo::SettingsDialog;
using legilo::Voice;
using legilo::testing::FakeAutostart;
using legilo::testing::FakeGlobalShortcuts;
using namespace Qt::StringLiterals;

namespace {

Voice voice(const QString &id, const QString &name)
{
    Voice result;
    result.id = id;
    result.displayName = name;
    return result;
}

} // namespace

class TestSettingsDialog : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void generalSwitchesWriteTheSettings_data();
    void generalSwitchesWriteTheSettings();
    void speakWhileSpeakingChoiceIsSaved();
    void autostartSwitchTurnsAutostartOnAndOff();
    void autostartSwitchShowsTheSystemState();
    void failedAutostartChangeIsUndone();
    void voiceListShowsTheInstalledVoicesAndSavesTheChoice();
    void speedIsSaved();
    void shortcutIsAppliedThroughTheSystem();
    void shortcutWithoutAModifierIsRefused();
    void shortcutTakenElsewhereIsRefused();
    void shortcutChangedInSystemSettingsIsShown();
    void shortcutCanBeResetToItsDefault();
    void cleanupChoicesWriteTheSettings();

private:
    template <typename T> T *find(const QString &name)
    {
        auto *widget = m_dialog->findChild<T *>(name);
        if (widget == nullptr) {
            qWarning("No widget called %s", qPrintable(name));
        }
        return widget;
    }

    void makeDialog();

    QTemporaryDir m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<FakeGlobalShortcuts> m_shortcuts;
    std::unique_ptr<FakeAutostart> m_autostart;
    std::unique_ptr<SettingsDialog> m_dialog;
};

void TestSettingsDialog::init()
{
    QVERIFY(m_dir.isValid());
    QFile::remove(m_dir.filePath(u"legilo.conf"_s));
    m_settings = std::make_unique<Settings>(m_dir.filePath(u"legilo.conf"_s));
    m_shortcuts = std::make_unique<FakeGlobalShortcuts>();
    for (const legilo::ShortcutAction &action : legilo::shortcutActions()) {
        m_shortcuts->registerAction(action.id, action.label, action.defaultSequence);
    }
    m_autostart = std::make_unique<FakeAutostart>();
    makeDialog();
}

void TestSettingsDialog::cleanup()
{
    m_dialog.reset();
}

void TestSettingsDialog::makeDialog()
{
    m_dialog = std::make_unique<SettingsDialog>(
        m_settings.get(), m_shortcuts.get(), m_autostart.get(),
        QList<Voice>{voice(u"piper-a"_s, u"Alpha"_s), voice(u"piper-b"_s, u"Beta"_s)});
}

void TestSettingsDialog::generalSwitchesWriteTheSettings_data()
{
    using Getter = bool (Settings::*)() const;
    QTest::addColumn<QString>("name");
    QTest::addColumn<Getter>("getter");

    QTest::newRow("notifyStart") << u"notifyStart"_s << Getter(&Settings::notifyStart);
    QTest::newRow("notifySource") << u"notifySource"_s << Getter(&Settings::notifySource);
    QTest::newRow("notifyPause") << u"notifyPause"_s << Getter(&Settings::notifyPause);
    QTest::newRow("fallback") << u"selectionFallbackToClipboard"_s
                              << Getter(&Settings::selectionFallbackToClipboard);
}

void TestSettingsDialog::generalSwitchesWriteTheSettings()
{
    using Getter = bool (Settings::*)() const;
    QFETCH(QString, name);
    QFETCH(Getter, getter);
    auto *box = find<QCheckBox>(name);
    QVERIFY(box);
    QCOMPARE(box->isChecked(), ((*m_settings).*getter)());

    box->click();

    QCOMPARE(((*m_settings).*getter)(), box->isChecked());
}

void TestSettingsDialog::speakWhileSpeakingChoiceIsSaved()
{
    auto *choice = find<QComboBox>(u"speakWhileSpeaking"_s);
    QVERIFY(choice);

    choice->setCurrentIndex(choice->findData(u"restart"_s));

    QCOMPARE(m_settings->speakWhileSpeaking(), Settings::SpeakWhileSpeaking::Restart);
}

void TestSettingsDialog::autostartSwitchTurnsAutostartOnAndOff()
{
    auto *box = find<QCheckBox>(u"autostart"_s);
    QVERIFY(box);
    QVERIFY(!box->isChecked());

    box->click();
    QVERIFY(m_autostart->enabled);
    QVERIFY(m_settings->autostart());

    box->click();
    QVERIFY(!m_autostart->enabled);
    QVERIFY(!m_settings->autostart());
}

void TestSettingsDialog::autostartSwitchShowsTheSystemState()
{
    m_autostart->enabled = true;
    makeDialog();

    QVERIFY(find<QCheckBox>(u"autostart"_s)->isChecked());
}

void TestSettingsDialog::failedAutostartChangeIsUndone()
{
    m_autostart->fails = true;
    auto *box = find<QCheckBox>(u"autostart"_s);
    QVERIFY(box);

    box->click();

    QVERIFY(!box->isChecked());
    QVERIFY(!m_settings->autostart());
}

void TestSettingsDialog::voiceListShowsTheInstalledVoicesAndSavesTheChoice()
{
    auto *list = find<QComboBox>(u"voice"_s);
    QVERIFY(list);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->itemText(0), u"Alpha"_s);

    list->setCurrentIndex(1);

    QCOMPARE(m_settings->defaultVoice(), u"piper-b"_s);
    makeDialog();
    QCOMPARE(find<QComboBox>(u"voice"_s)->currentIndex(), 1);
}

void TestSettingsDialog::speedIsSaved()
{
    auto *speed = find<QDoubleSpinBox>(u"speed"_s);
    QVERIFY(speed);
    QCOMPARE(speed->value(), 1.0);

    speed->setValue(1.4);

    QCOMPARE(m_settings->speed(), 1.4);
}

void TestSettingsDialog::shortcutIsAppliedThroughTheSystem()
{
    auto *edit = find<QKeySequenceEdit>(u"shortcut-stop"_s);
    QVERIFY(edit);
    QCOMPARE(edit->keySequence(), QKeySequence(u"Meta+Alt+X"_s));

    edit->setKeySequence(QKeySequence(u"Ctrl+Alt+F9"_s));
    emit edit->editingFinished();

    QCOMPARE(m_shortcuts->shortcut(u"stop"_s), QKeySequence(u"Ctrl+Alt+F9"_s));
}

void TestSettingsDialog::shortcutWithoutAModifierIsRefused()
{
    auto *edit = find<QKeySequenceEdit>(u"shortcut-stop"_s);
    QVERIFY(edit);

    edit->setKeySequence(QKeySequence(u"Shift+X"_s));
    emit edit->editingFinished();

    QCOMPARE(m_shortcuts->shortcut(u"stop"_s), QKeySequence(u"Meta+Alt+X"_s));
    QCOMPARE(edit->keySequence(), QKeySequence(u"Meta+Alt+X"_s));
    QVERIFY(!find<QLabel>(u"shortcutProblem"_s)->text().isEmpty());
}

void TestSettingsDialog::shortcutTakenElsewhereIsRefused()
{
    m_shortcuts->taken.insert(QKeySequence(u"Ctrl+Alt+T"_s));
    auto *edit = find<QKeySequenceEdit>(u"shortcut-stop"_s);
    QVERIFY(edit);

    edit->setKeySequence(QKeySequence(u"Ctrl+Alt+T"_s));
    emit edit->editingFinished();

    QCOMPARE(m_shortcuts->shortcut(u"stop"_s), QKeySequence(u"Meta+Alt+X"_s));
    QCOMPARE(edit->keySequence(), QKeySequence(u"Meta+Alt+X"_s));
    QVERIFY(find<QLabel>(u"shortcutProblem"_s)->text().contains(u"in use"_s));
}

void TestSettingsDialog::shortcutChangedInSystemSettingsIsShown()
{
    auto *edit = find<QKeySequenceEdit>(u"shortcut-faster"_s);
    QVERIFY(edit);

    m_shortcuts->changeElsewhere(u"faster"_s, QKeySequence(u"Meta+Alt+Up"_s));

    QCOMPARE(edit->keySequence(), QKeySequence(u"Meta+Alt+Up"_s));
}

void TestSettingsDialog::shortcutCanBeResetToItsDefault()
{
    QVERIFY(m_shortcuts->setShortcut(u"stop"_s, QKeySequence(u"Ctrl+Alt+F9"_s)));
    makeDialog();
    auto *reset = find<QAbstractButton>(u"reset-stop"_s);
    QVERIFY(reset);

    reset->click();

    QCOMPARE(m_shortcuts->shortcut(u"stop"_s), QKeySequence(u"Meta+Alt+X"_s));
    QCOMPARE(find<QKeySequenceEdit>(u"shortcut-stop"_s)->keySequence(),
             QKeySequence(u"Meta+Alt+X"_s));
}

void TestSettingsDialog::cleanupChoicesWriteTheSettings()
{
    auto *mode = find<QComboBox>(u"reflowMode"_s);
    auto *pageNumbers = find<QCheckBox>(u"pageNumbers"_s);
    auto *repeatedLines = find<QCheckBox>(u"repeatedLines"_s);
    auto *dehyphenate = find<QCheckBox>(u"dehyphenate"_s);
    auto *urls = find<QComboBox>(u"urls"_s);
    auto *emails = find<QComboBox>(u"emails"_s);
    auto *citations = find<QComboBox>(u"citations"_s);
    auto *authorYear = find<QCheckBox>(u"authorYearCitations"_s);
    QVERIFY(mode && pageNumbers && repeatedLines && dehyphenate && urls && emails && citations &&
            authorYear);

    mode->setCurrentIndex(mode->findData(u"always"_s));
    pageNumbers->click();
    repeatedLines->click();
    dehyphenate->click();
    urls->setCurrentIndex(urls->findData(u"remove"_s));
    emails->setCurrentIndex(emails->findData(u"full"_s));
    citations->setCurrentIndex(citations->findData(u"never"_s));
    authorYear->click();

    CleanupSettings expected;
    expected.reflowMode = legilo::ReflowMode::Always;
    expected.pageNumbers = false;
    expected.repeatedLines = false;
    expected.dehyphenate = false;
    expected.urls = legilo::UrlRule::Remove;
    expected.emails = legilo::EmailRule::Full;
    expected.citations = legilo::CitationRule::Never;
    expected.authorYearCitations = true;
    QCOMPARE(m_settings->cleanup(), expected);
}

QTEST_MAIN(TestSettingsDialog)
#include "tst_settingsdialog.moc"
