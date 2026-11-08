#include "app/Settings.h"

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

template <typename T> struct Key
{
    QLatin1StringView name;
    T defaultValue;
};

template <typename E> struct EnumName
{
    E value;
    QLatin1StringView name;
};

using SpeakWhileSpeaking = Settings::SpeakWhileSpeaking;
using ShowCaptureInReader = Settings::ShowCaptureInReader;

constexpr Key<bool> autostartKey{"general/autostart"_L1, false};
constexpr Key<bool> notifyStartKey{"general/notifyStart"_L1, true};
constexpr Key<bool> notifyPauseKey{"general/notifyPause"_L1, true};
constexpr Key<bool> notifySourceKey{"general/notifySource"_L1, true};
constexpr Key<SpeakWhileSpeaking> speakWhileSpeakingKey{"behaviour/speakWhileSpeaking"_L1,
                                                        SpeakWhileSpeaking::Stop};
constexpr Key<bool> selectionFallbackToClipboardKey{"behaviour/selectionFallbackToClipboard"_L1,
                                                    false};
constexpr Key<ShowCaptureInReader> showCaptureInReaderKey{"behaviour/showCaptureInReader"_L1,
                                                          ShowCaptureInReader::IfVisible};
constexpr Key<QLatin1StringView> defaultVoiceKey{"voice/default"_L1, {}};
constexpr Key<double> speedKey{"voice/speed"_L1, 1.0};

constexpr std::array speakWhileSpeakingNames{
    EnumName<SpeakWhileSpeaking>{SpeakWhileSpeaking::Stop, "stop"_L1},
    EnumName<SpeakWhileSpeaking>{SpeakWhileSpeaking::Restart, "restart"_L1},
};

constexpr std::array showCaptureInReaderNames{
    EnumName<ShowCaptureInReader>{ShowCaptureInReader::Never, "never"_L1},
    EnumName<ShowCaptureInReader>{ShowCaptureInReader::IfVisible, "ifVisible"_L1},
    EnumName<ShowCaptureInReader>{ShowCaptureInReader::Always, "always"_L1},
    EnumName<ShowCaptureInReader>{ShowCaptureInReader::LongText, "longText"_L1},
};

template <typename E, std::size_t N>
QLatin1StringView nameOf(const std::array<EnumName<E>, N> &names, E value)
{
    for (const auto &entry : names) {
        if (entry.value == value) {
            return entry.name;
        }
    }
    Q_UNREACHABLE_RETURN(QLatin1StringView());
}

template <typename E, std::size_t N>
E readEnum(const QSettings &store, const Key<E> &key, const std::array<EnumName<E>, N> &names)
{
    const QString stored = store.value(key.name).toString();
    for (const auto &entry : names) {
        if (entry.name == stored) {
            return entry.value;
        }
    }
    return key.defaultValue;
}

bool readBool(const QSettings &store, const Key<bool> &key)
{
    return store.value(key.name, key.defaultValue).toBool();
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_store(QSettings::IniFormat, QSettings::UserScope, u"legilo"_s, u"legilo"_s)
{}

Settings::Settings(const QString &fileName, QObject *parent)
    : QObject(parent)
    , m_store(fileName, QSettings::IniFormat)
{}

void Settings::store(QLatin1StringView key, const QVariant &current, const QVariant &value)
{
    if (current == value) {
        return;
    }
    m_store.setValue(key, value);
    emit changed(key);
}

bool Settings::autostart() const
{
    return readBool(m_store, autostartKey);
}

void Settings::setAutostart(bool value)
{
    store(autostartKey.name, autostart(), value);
}

bool Settings::notifyStart() const
{
    return readBool(m_store, notifyStartKey);
}

void Settings::setNotifyStart(bool value)
{
    store(notifyStartKey.name, notifyStart(), value);
}

bool Settings::notifyPause() const
{
    return readBool(m_store, notifyPauseKey);
}

void Settings::setNotifyPause(bool value)
{
    store(notifyPauseKey.name, notifyPause(), value);
}

bool Settings::notifySource() const
{
    return readBool(m_store, notifySourceKey);
}

void Settings::setNotifySource(bool value)
{
    store(notifySourceKey.name, notifySource(), value);
}

Settings::SpeakWhileSpeaking Settings::speakWhileSpeaking() const
{
    return readEnum(m_store, speakWhileSpeakingKey, speakWhileSpeakingNames);
}

void Settings::setSpeakWhileSpeaking(SpeakWhileSpeaking value)
{
    store(speakWhileSpeakingKey.name, nameOf(speakWhileSpeakingNames, speakWhileSpeaking()),
          nameOf(speakWhileSpeakingNames, value));
}

bool Settings::selectionFallbackToClipboard() const
{
    return readBool(m_store, selectionFallbackToClipboardKey);
}

void Settings::setSelectionFallbackToClipboard(bool value)
{
    store(selectionFallbackToClipboardKey.name, selectionFallbackToClipboard(), value);
}

Settings::ShowCaptureInReader Settings::showCaptureInReader() const
{
    return readEnum(m_store, showCaptureInReaderKey, showCaptureInReaderNames);
}

void Settings::setShowCaptureInReader(ShowCaptureInReader value)
{
    store(showCaptureInReaderKey.name, nameOf(showCaptureInReaderNames, showCaptureInReader()),
          nameOf(showCaptureInReaderNames, value));
}

QString Settings::defaultVoice() const
{
    return m_store.value(defaultVoiceKey.name, defaultVoiceKey.defaultValue).toString();
}

void Settings::setDefaultVoice(const QString &value)
{
    store(defaultVoiceKey.name, defaultVoice(), value);
}

double Settings::speed() const
{
    bool valid = false;
    const double value = m_store.value(speedKey.name).toDouble(&valid);
    return valid ? std::clamp(value, MinSpeed, MaxSpeed) : speedKey.defaultValue;
}

void Settings::setSpeed(double value)
{
    store(speedKey.name, speed(), std::clamp(value, MinSpeed, MaxSpeed));
}

} // namespace legilo
