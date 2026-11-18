#include "platform/Platform.h"

#include "linux/KGlobalAccelShortcuts.h"
#include "linux/KdeTextSource.h"
#include "linux/XdgAutostart.h"

#include <QCoreApplication>
#include <QStandardPaths>

namespace legilo {

// The wl-paste contingency of design 7.1.2 was not needed: the stage 0
// spike found KSystemClipboard reading the selection on Wayland.
std::unique_ptr<TextSource> createTextSource()
{
    return std::make_unique<KdeTextSource>();
}

std::unique_ptr<GlobalShortcuts> createGlobalShortcuts()
{
    return std::make_unique<KGlobalAccelShortcuts>();
}

std::unique_ptr<Autostart> createAutostart()
{
    return std::make_unique<XdgAutostart>(
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
            QStringLiteral("/autostart"),
        QCoreApplication::applicationFilePath());
}

} // namespace legilo
