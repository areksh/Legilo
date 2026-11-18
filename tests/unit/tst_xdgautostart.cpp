#include "linux/XdgAutostart.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using legilo::XdgAutostart;
using namespace Qt::StringLiterals;

class TestXdgAutostart : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void isOffWithoutAnEntry();
    void enablingWritesTheEntryAndItsDirectory();
    void entryStartsTheProgramInTheTray();
    void disablingRemovesTheEntry();
    void disablingWithoutAnEntrySucceeds();
    void entryHiddenBySystemSettingsIsOff();
    void execArgumentIsQuotedWhenItMustBe_data();
    void execArgumentIsQuotedWhenItMustBe();

private:
    QString directory() const { return m_dir->filePath(u"config/autostart"_s); }
    QString entryPath() const { return directory() + u"/legilo.desktop"_s; }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestXdgAutostart::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void TestXdgAutostart::isOffWithoutAnEntry()
{
    const XdgAutostart autostart(directory(), u"/usr/bin/legilo"_s);

    QVERIFY(!autostart.isEnabled());
}

void TestXdgAutostart::enablingWritesTheEntryAndItsDirectory()
{
    XdgAutostart autostart(directory(), u"/usr/bin/legilo"_s);

    QVERIFY(autostart.setEnabled(true));

    QVERIFY(autostart.isEnabled());
    QFile file(entryPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readAll()), autostart.entry());
}

void TestXdgAutostart::entryStartsTheProgramInTheTray()
{
    const XdgAutostart autostart(directory(), u"/usr/bin/legilo"_s);

    const QStringList lines = autostart.entry().split(u'\n', Qt::SkipEmptyParts);

    QCOMPARE(lines, QStringList({u"[Desktop Entry]"_s, u"Type=Application"_s, u"Name=Legilo"_s,
                                 u"Exec=/usr/bin/legilo --tray"_s, u"Icon=sh.arek.legilo"_s,
                                 u"X-KDE-autostart-phase=2"_s}));
}

void TestXdgAutostart::disablingRemovesTheEntry()
{
    XdgAutostart autostart(directory(), u"/usr/bin/legilo"_s);
    QVERIFY(autostart.setEnabled(true));

    QVERIFY(autostart.setEnabled(false));

    QVERIFY(!autostart.isEnabled());
    QVERIFY(!QFile::exists(entryPath()));
}

void TestXdgAutostart::disablingWithoutAnEntrySucceeds()
{
    XdgAutostart autostart(directory(), u"/usr/bin/legilo"_s);

    QVERIFY(autostart.setEnabled(false));
}

void TestXdgAutostart::entryHiddenBySystemSettingsIsOff()
{
    XdgAutostart autostart(directory(), u"/usr/bin/legilo"_s);
    QVERIFY(autostart.setEnabled(true));
    QFile file(entryPath());
    QVERIFY(file.open(QIODevice::Append));
    file.write("Hidden=true\n");
    file.close();

    QVERIFY(!autostart.isEnabled());
    // Turning it on again writes a fresh entry.
    QVERIFY(autostart.setEnabled(true));
    QVERIFY(autostart.isEnabled());
}

void TestXdgAutostart::execArgumentIsQuotedWhenItMustBe_data()
{
    QTest::addColumn<QString>("argument");
    QTest::addColumn<QString>("quoted");

    QTest::newRow("plain") << u"/usr/bin/legilo"_s << u"/usr/bin/legilo"_s;
    QTest::newRow("space") << u"/home/a b/legilo"_s << u"\"/home/a b/legilo\""_s;
    QTest::newRow("dollar") << u"/opt/$x/legilo"_s << u"\"/opt/\\\\$x/legilo\""_s;
    QTest::newRow("quote") << u"/opt/a\"b/legilo"_s << u"\"/opt/a\\\\\"b/legilo\""_s;
    QTest::newRow("backslash") << u"/opt/a\\b/legilo"_s << u"\"/opt/a\\\\\\\\b/legilo\""_s;
    QTest::newRow("percent") << u"/opt/100%/legilo"_s << u"/opt/100%%/legilo"_s;
}

void TestXdgAutostart::execArgumentIsQuotedWhenItMustBe()
{
    QFETCH(QString, argument);
    QFETCH(QString, quoted);

    QCOMPARE(legilo::quoteExecArgument(argument), quoted);
}

QTEST_GUILESS_MAIN(TestXdgAutostart)
#include "tst_xdgautostart.moc"
