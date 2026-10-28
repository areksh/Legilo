#pragma once

#include <QObject>
#include <QSettings>

namespace legilo {

// Typed access to the user's settings. Every key has one getter, one
// setter and one default, defined together in Settings.cpp. Enumerations
// are stored by name, and an unrecognised stored value reads as the default.
class Settings : public QObject
{
    Q_OBJECT

public:
    enum class SpeakWhileSpeaking
    {
        Stop,
        Restart
    };
    Q_ENUM(SpeakWhileSpeaking)

    enum class ShowCaptureInReader
    {
        Never,
        IfVisible,
        Always,
        LongText
    };
    Q_ENUM(ShowCaptureInReader)

    // Opens the per-user file: legilo/legilo.conf under the XDG config
    // directory on Linux, %APPDATA%\legilo\legilo.ini on Windows.
    explicit Settings(QObject *parent = nullptr);
    // Opens the given INI file instead.
    explicit Settings(const QString &fileName, QObject *parent = nullptr);

    bool autostart() const;
    void setAutostart(bool value);
    bool notifyStart() const;
    void setNotifyStart(bool value);
    bool notifyPause() const;
    void setNotifyPause(bool value);
    bool notifySource() const;
    void setNotifySource(bool value);

    SpeakWhileSpeaking speakWhileSpeaking() const;
    void setSpeakWhileSpeaking(SpeakWhileSpeaking value);
    bool selectionFallbackToClipboard() const;
    void setSelectionFallbackToClipboard(bool value);
    ShowCaptureInReader showCaptureInReader() const;
    void setShowCaptureInReader(ShowCaptureInReader value);

signals:
    // Emitted with the key name (for example "general/autostart") when a
    // setter changes a value. Setting a key to its current value is silent.
    void changed(const QString &key);

private:
    void store(QLatin1StringView key, const QVariant &current, const QVariant &value);

    QSettings m_store;
};

} // namespace legilo
