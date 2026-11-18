#pragma once

#include "platform/Autostart.h"

#include <QString>

namespace legilo {

// Autostart through an XDG autostart entry, legilo.desktop in the user's
// autostart directory (design Appendix C), which starts the given program
// with --tray. The entry names the program by its full path, so a build
// run from anywhere starts the same build at login.
class XdgAutostart : public Autostart
{
public:
    // directory is normally ~/.config/autostart; program is the absolute
    // path of the legilo executable.
    XdgAutostart(QString directory, QString program);

    // An entry that System Settings has hidden (Hidden=true) is off.
    bool isEnabled() const override;
    bool setEnabled(bool enabled) override;

    // The entry's contents.
    QString entry() const;

private:
    QString filePath() const;

    QString m_directory;
    QString m_program;
};

// Quotes one argument for a desktop entry's Exec key, as the Desktop Entry
// Specification requires: in double quotes, with the characters that are
// special there escaped, if it holds any reserved character; then with
// the escapes of a string value applied, and % doubled.
QString quoteExecArgument(const QString &argument);

} // namespace legilo
