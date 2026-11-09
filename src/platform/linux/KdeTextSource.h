#pragma once

#include "platform/TextSource.h"

namespace legilo {

// Reads the primary selection and the clipboard through KDE's
// KSystemClipboard (design 7.1.2). On Wayland it uses the data-control
// protocol, so it reads without the window focus that a global shortcut
// never gives Legilo; on X11 it falls back to QClipboard. Reads complete
// at once.
//
// The system clipboard is set up when the source is created, at start-up:
// on Wayland it learns the current selection from events that arrive
// after it binds to data-control, and until then it reports none.
class KdeTextSource : public TextSource
{
    Q_OBJECT

public:
    explicit KdeTextSource(QObject *parent = nullptr);

    QFuture<TextCapture> readSelection() override;
    QFuture<TextCapture> readClipboard() override;
};

} // namespace legilo
