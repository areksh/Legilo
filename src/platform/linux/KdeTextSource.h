#pragma once

#include "platform/TextSource.h"

namespace legilo {

// Reads the primary selection and the clipboard through KDE's
// KSystemClipboard (design 7.1.2). On Wayland it uses the data-control
// protocol, so it reads without the window focus that a global shortcut
// never gives Legilo; on X11 it falls back to QClipboard. Reads complete
// at once.
class KdeTextSource : public TextSource
{
    Q_OBJECT

public:
    using TextSource::TextSource;

    QFuture<TextCapture> readSelection() override;
    QFuture<TextCapture> readClipboard() override;
};

} // namespace legilo
