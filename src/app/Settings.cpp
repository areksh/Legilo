#include "app/Settings.h"

#include <QStandardPaths>

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

// Not constexpr: CleanupSettings holds a QStringList.
const CleanupSettings cleanupDefaults{};
const Key<ReflowMode> reflowModeKey{"cleanup/reflowMode"_L1, cleanupDefaults.reflowMode};
const Key<double> detectThresholdKey{"cleanup/detectThreshold"_L1, cleanupDefaults.detectThreshold};
const Key<bool> pageNumbersKey{"cleanup/pageNumbers"_L1, cleanupDefaults.pageNumbers};
const Key<bool> repeatedLinesKey{"cleanup/repeatedLines"_L1, cleanupDefaults.repeatedLines};
const Key<bool> dehyphenateKey{"cleanup/dehyphenate"_L1, cleanupDefaults.dehyphenate};
constexpr auto abbreviationsKey = "cleanup/abbreviations"_L1;
const Key<int> maxSegmentCharsKey{"cleanup/maxSegmentChars"_L1, cleanupDefaults.maxSegmentChars};
const Key<bool> detectInFilesKey{"cleanup/detectInFiles"_L1, cleanupDefaults.detectInFiles};
const Key<UrlRule> urlsKey{"cleanup/urls"_L1, cleanupDefaults.urls};
const Key<EmailRule> emailsKey{"cleanup/emails"_L1, cleanupDefaults.emails};
const Key<CitationRule> citationsKey{"cleanup/citations"_L1, cleanupDefaults.citations};
const Key<bool> authorYearCitationsKey{"cleanup/authorYearCitations"_L1,
                                       cleanupDefaults.authorYearCitations};

constexpr std::array reflowModeNames{
    EnumName<ReflowMode>{ReflowMode::Auto, "auto"_L1},
    EnumName<ReflowMode>{ReflowMode::Always, "always"_L1},
    EnumName<ReflowMode>{ReflowMode::Never, "never"_L1},
};
constexpr std::array urlRuleNames{
    EnumName<UrlRule>{UrlRule::Host, "host"_L1},
    EnumName<UrlRule>{UrlRule::Full, "full"_L1},
    EnumName<UrlRule>{UrlRule::Remove, "remove"_L1},
};
constexpr std::array emailRuleNames{
    EnumName<EmailRule>{EmailRule::Host, "host"_L1},
    EnumName<EmailRule>{EmailRule::Full, "full"_L1},
};
constexpr std::array citationRuleNames{
    EnumName<CitationRule>{CitationRule::Never, "never"_L1},
    EnumName<CitationRule>{CitationRule::PdfOnly, "pdfOnly"_L1},
    EnumName<CitationRule>{CitationRule::Always, "always"_L1},
};

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

double readDouble(const QSettings &store, const Key<double> &key)
{
    bool valid = false;
    const double value = store.value(key.name).toDouble(&valid);
    return valid ? value : key.defaultValue;
}

int readInt(const QSettings &store, const Key<int> &key)
{
    bool valid = false;
    const int value = store.value(key.name).toInt(&valid);
    return valid ? value : key.defaultValue;
}

} // namespace

Settings::Settings(QObject *parent)
    : Settings(defaultFileName(), parent)
{}

// QSettings names an INI file legilo.ini wherever it puts it, so the
// Linux name is given in full.
QString Settings::defaultFileName()
{
#ifdef Q_OS_WIN
    return QSettings(QSettings::IniFormat, QSettings::UserScope, u"legilo"_s, u"legilo"_s)
        .fileName();
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           u"/legilo/legilo.conf"_s;
#endif
}

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

CleanupSettings Settings::cleanup() const
{
    CleanupSettings value;
    value.reflowMode = readEnum(m_store, reflowModeKey, reflowModeNames);
    value.detectThreshold = readDouble(m_store, detectThresholdKey);
    value.pageNumbers = readBool(m_store, pageNumbersKey);
    value.repeatedLines = readBool(m_store, repeatedLinesKey);
    value.dehyphenate = readBool(m_store, dehyphenateKey);
    value.abbreviations = m_store.value(abbreviationsKey).toStringList();
    value.maxSegmentChars = readInt(m_store, maxSegmentCharsKey);
    value.detectInFiles = readBool(m_store, detectInFilesKey);
    value.urls = readEnum(m_store, urlsKey, urlRuleNames);
    value.emails = readEnum(m_store, emailsKey, emailRuleNames);
    value.citations = readEnum(m_store, citationsKey, citationRuleNames);
    value.authorYearCitations = readBool(m_store, authorYearCitationsKey);
    return value;
}

void Settings::setCleanup(const CleanupSettings &value)
{
    const CleanupSettings current = cleanup();
    store(reflowModeKey.name, nameOf(reflowModeNames, current.reflowMode),
          nameOf(reflowModeNames, value.reflowMode));
    store(detectThresholdKey.name, current.detectThreshold, value.detectThreshold);
    store(pageNumbersKey.name, current.pageNumbers, value.pageNumbers);
    store(repeatedLinesKey.name, current.repeatedLines, value.repeatedLines);
    store(dehyphenateKey.name, current.dehyphenate, value.dehyphenate);
    store(abbreviationsKey, current.abbreviations, value.abbreviations);
    store(maxSegmentCharsKey.name, current.maxSegmentChars, value.maxSegmentChars);
    store(detectInFilesKey.name, current.detectInFiles, value.detectInFiles);
    store(urlsKey.name, nameOf(urlRuleNames, current.urls), nameOf(urlRuleNames, value.urls));
    store(emailsKey.name, nameOf(emailRuleNames, current.emails),
          nameOf(emailRuleNames, value.emails));
    store(citationsKey.name, nameOf(citationRuleNames, current.citations),
          nameOf(citationRuleNames, value.citations));
    store(authorYearCitationsKey.name, current.authorYearCitations, value.authorYearCitations);
}

} // namespace legilo
