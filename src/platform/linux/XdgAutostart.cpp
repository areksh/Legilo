#include "linux/XdgAutostart.h"

#include "platform/Logging.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

const auto EntryName = u"legilo.desktop"_s;

// Characters that make an Exec argument need quotes, and those that must
// be escaped inside the quotes (Desktop Entry Specification, "The Exec
// key").
const auto Reserved = u" \t\n\"'\\><~|&;$*?#()`"_s;
const auto EscapedInQuotes = u"\"`$\\"_s;

} // namespace

XdgAutostart::XdgAutostart(QString directory, QString program)
    : m_directory(std::move(directory))
    , m_program(std::move(program))
{}

bool XdgAutostart::isEnabled() const
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    while (!file.atEnd()) {
        if (file.readLine().trimmed() == "Hidden=true") {
            return false;
        }
    }
    return true;
}

bool XdgAutostart::setEnabled(bool enabled)
{
    if (!enabled) {
        return !QFile::exists(filePath()) || QFile::remove(filePath());
    }
    if (!QDir().mkpath(m_directory)) {
        qCWarning(lcPlatform) << "Cannot create" << m_directory;
        return false;
    }
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(lcPlatform) << "Cannot write" << file.fileName() << file.errorString();
        return false;
    }
    file.write(entry().toUtf8());
    return file.commit();
}

QString XdgAutostart::entry() const
{
    return u"[Desktop Entry]\n"
           "Type=Application\n"
           "Name=Legilo\n"
           "Exec=%1 --tray\n"
           "Icon=sh.arek.legilo\n"
           "X-KDE-autostart-phase=2\n"_s.arg(quoteExecArgument(m_program));
}

QString XdgAutostart::filePath() const
{
    return m_directory + u'/' + EntryName;
}

QString quoteExecArgument(const QString &argument)
{
    QString quoted = argument;
    const bool needsQuotes =
        std::ranges::any_of(argument, [](QChar character) { return Reserved.contains(character); });
    if (needsQuotes) {
        quoted.clear();
        for (const QChar character : argument) {
            if (EscapedInQuotes.contains(character)) {
                quoted += u'\\';
            }
            quoted += character;
        }
        quoted = u'"' + quoted + u'"';
    }
    // The escapes of a string value apply on top of the quoting, so each
    // backslash is written twice; % is the field code character.
    quoted.replace(u"\\"_s, u"\\\\"_s);
    quoted.replace(u"%"_s, u"%%"_s);
    return quoted;
}

} // namespace legilo
