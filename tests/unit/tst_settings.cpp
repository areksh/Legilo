#include "app/Settings.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using legilo::Settings;

using BoolGetter = bool (Settings::*)() const;
using BoolSetter = void (Settings::*)(bool);
Q_DECLARE_METATYPE(BoolGetter)
Q_DECLARE_METATYPE(BoolSetter)

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void boolKeyHasDefaultWhenUnset_data() { boolKeys(); }
    void boolKeyHasDefaultWhenUnset();
    void boolKeyPersistsUnderItsName_data() { boolKeys(); }
    void boolKeyPersistsUnderItsName();

    void speakWhileSpeakingDefaultsToStop();
    void speakWhileSpeakingPersistsAsName();
    void showCaptureInReaderDefaultsToIfVisible();
    void showCaptureInReaderPersistsAsName_data();
    void showCaptureInReaderPersistsAsName();
    void unknownEnumValueFallsBackToDefault();

    void defaultVoiceIsUnsetUntilChosen();
    void defaultVoicePersistsUnderItsName();
    void speedDefaultsToNormal();
    void speedPersistsUnderItsName();
    void speedIsKeptWithinItsRange_data();
    void speedIsKeptWithinItsRange();

    void changedIsEmittedWithKeyWhenValueChanges();
    void changedIsNotEmittedWhenValueIsUnchanged();

private:
    static void boolKeys();
    QString fileName() const { return m_dir.filePath(QStringLiteral("legilo.conf")); }

    QTemporaryDir m_dir;
};

void TestSettings::init()
{
    QVERIFY(m_dir.isValid());
    QFile::remove(fileName());
}

void TestSettings::boolKeys()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<BoolGetter>("getter");
    QTest::addColumn<BoolSetter>("setter");
    QTest::addColumn<bool>("defaultValue");

    QTest::newRow("autostart") << QStringLiteral("general/autostart") << &Settings::autostart
                               << &Settings::setAutostart << false;
    QTest::newRow("notifyStart") << QStringLiteral("general/notifyStart") << &Settings::notifyStart
                                 << &Settings::setNotifyStart << true;
    QTest::newRow("notifyPause") << QStringLiteral("general/notifyPause") << &Settings::notifyPause
                                 << &Settings::setNotifyPause << true;
    QTest::newRow("notifySource") << QStringLiteral("general/notifySource")
                                  << &Settings::notifySource << &Settings::setNotifySource << true;
    QTest::newRow("selectionFallbackToClipboard")
        << QStringLiteral("behaviour/selectionFallbackToClipboard")
        << &Settings::selectionFallbackToClipboard << &Settings::setSelectionFallbackToClipboard
        << false;
}

void TestSettings::boolKeyHasDefaultWhenUnset()
{
    QFETCH(BoolGetter, getter);
    QFETCH(bool, defaultValue);

    const Settings settings(fileName());

    QCOMPARE((settings.*getter)(), defaultValue);
}

void TestSettings::boolKeyPersistsUnderItsName()
{
    QFETCH(QString, key);
    QFETCH(BoolGetter, getter);
    QFETCH(BoolSetter, setter);
    QFETCH(bool, defaultValue);

    {
        Settings settings(fileName());
        (settings.*setter)(!defaultValue);
    }

    const Settings reopened(fileName());
    QCOMPARE((reopened.*getter)(), !defaultValue);
    const QSettings raw(fileName(), QSettings::IniFormat);
    QCOMPARE(raw.value(key).toBool(), !defaultValue);
}

void TestSettings::speakWhileSpeakingDefaultsToStop()
{
    const Settings settings(fileName());

    QCOMPARE(settings.speakWhileSpeaking(), Settings::SpeakWhileSpeaking::Stop);
}

void TestSettings::speakWhileSpeakingPersistsAsName()
{
    {
        Settings settings(fileName());
        settings.setSpeakWhileSpeaking(Settings::SpeakWhileSpeaking::Restart);
    }

    const Settings reopened(fileName());
    QCOMPARE(reopened.speakWhileSpeaking(), Settings::SpeakWhileSpeaking::Restart);
    const QSettings raw(fileName(), QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("behaviour/speakWhileSpeaking")).toString(),
             QStringLiteral("restart"));
}

void TestSettings::showCaptureInReaderDefaultsToIfVisible()
{
    const Settings settings(fileName());

    QCOMPARE(settings.showCaptureInReader(), Settings::ShowCaptureInReader::IfVisible);
}

void TestSettings::showCaptureInReaderPersistsAsName_data()
{
    QTest::addColumn<Settings::ShowCaptureInReader>("value");
    QTest::addColumn<QString>("stored");

    QTest::newRow("never") << Settings::ShowCaptureInReader::Never << QStringLiteral("never");
    QTest::newRow("ifVisible") << Settings::ShowCaptureInReader::IfVisible
                               << QStringLiteral("ifVisible");
    QTest::newRow("always") << Settings::ShowCaptureInReader::Always << QStringLiteral("always");
    QTest::newRow("longText") << Settings::ShowCaptureInReader::LongText
                              << QStringLiteral("longText");
}

void TestSettings::showCaptureInReaderPersistsAsName()
{
    QFETCH(Settings::ShowCaptureInReader, value);
    QFETCH(QString, stored);

    {
        // Start from a different value so the setter has something to change.
        QSettings raw(fileName(), QSettings::IniFormat);
        raw.setValue(QStringLiteral("behaviour/showCaptureInReader"),
                     value == Settings::ShowCaptureInReader::Never ? QStringLiteral("always")
                                                                   : QStringLiteral("never"));
    }
    {
        Settings settings(fileName());
        settings.setShowCaptureInReader(value);
    }

    const Settings reopened(fileName());
    QCOMPARE(reopened.showCaptureInReader(), value);
    const QSettings raw(fileName(), QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("behaviour/showCaptureInReader")).toString(), stored);
}

void TestSettings::unknownEnumValueFallsBackToDefault()
{
    {
        QSettings raw(fileName(), QSettings::IniFormat);
        raw.setValue(QStringLiteral("behaviour/speakWhileSpeaking"), QStringLiteral("shout"));
        raw.setValue(QStringLiteral("behaviour/showCaptureInReader"), QStringLiteral("Always"));
    }

    const Settings settings(fileName());

    QCOMPARE(settings.speakWhileSpeaking(), Settings::SpeakWhileSpeaking::Stop);
    QCOMPARE(settings.showCaptureInReader(), Settings::ShowCaptureInReader::IfVisible);
}

void TestSettings::defaultVoiceIsUnsetUntilChosen()
{
    const Settings settings(fileName());

    QVERIFY(settings.defaultVoice().isEmpty());
}

void TestSettings::defaultVoicePersistsUnderItsName()
{
    {
        Settings settings(fileName());
        settings.setDefaultVoice(QStringLiteral("piper-en_GB-cori-medium"));
    }

    const Settings reopened(fileName());
    QCOMPARE(reopened.defaultVoice(), QStringLiteral("piper-en_GB-cori-medium"));
    const QSettings raw(fileName(), QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("voice/default")).toString(),
             QStringLiteral("piper-en_GB-cori-medium"));
}

void TestSettings::speedDefaultsToNormal()
{
    const Settings settings(fileName());

    QCOMPARE(settings.speed(), 1.0);
}

void TestSettings::speedPersistsUnderItsName()
{
    {
        Settings settings(fileName());
        settings.setSpeed(1.3);
    }

    const Settings reopened(fileName());
    QCOMPARE(reopened.speed(), 1.3);
    const QSettings raw(fileName(), QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("voice/speed")).toDouble(), 1.3);
}

void TestSettings::speedIsKeptWithinItsRange_data()
{
    QTest::addColumn<QString>("stored");
    QTest::addColumn<double>("expected");

    QTest::newRow("too slow") << QStringLiteral("0.2") << 0.5;
    QTest::newRow("too fast") << QStringLiteral("9") << 2.5;
    QTest::newRow("not a number") << QStringLiteral("quick") << 1.0;
}

void TestSettings::speedIsKeptWithinItsRange()
{
    QFETCH(QString, stored);
    QFETCH(double, expected);
    {
        QSettings raw(fileName(), QSettings::IniFormat);
        raw.setValue(QStringLiteral("voice/speed"), stored);
    }

    const Settings settings(fileName());

    QCOMPARE(settings.speed(), expected);
}

void TestSettings::changedIsEmittedWithKeyWhenValueChanges()
{
    Settings settings(fileName());
    QSignalSpy spy(&settings, &Settings::changed);

    settings.setAutostart(true);
    settings.setSpeakWhileSpeaking(Settings::SpeakWhileSpeaking::Restart);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("general/autostart"));
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("behaviour/speakWhileSpeaking"));
}

void TestSettings::changedIsNotEmittedWhenValueIsUnchanged()
{
    Settings settings(fileName());
    QSignalSpy spy(&settings, &Settings::changed);

    settings.setNotifyStart(true);
    settings.setShowCaptureInReader(Settings::ShowCaptureInReader::IfVisible);

    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
