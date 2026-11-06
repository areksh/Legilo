#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>

namespace legilo {

// Where a piece of text came from (design 7.1.1).
enum class CaptureSource : std::uint8_t
{
    Selection,
    Clipboard,
    File,
    Pasted
};

// Text as a text source captured it, before any clean-up. The MIME types
// and owner hint are weak signals for PDF detection (7.2.3).
struct TextCapture
{
    QString text;
    CaptureSource source = CaptureSource::Selection;
    QStringList mimeTypes; // formats offered by the selection or clipboard
    QString ownerHint;     // Windows: the owning process, if known; empty on Linux
};

} // namespace legilo
