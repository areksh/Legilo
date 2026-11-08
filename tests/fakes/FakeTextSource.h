#pragma once

#include "platform/TextSource.h"

namespace legilo::testing {

// A TextSource whose selection and clipboard the test sets. Reads complete
// at once, as they do on Linux, and are counted.
class FakeTextSource : public TextSource
{
public:
    QString selection;
    QString clipboard;
    QStringList mimeTypes{QStringLiteral("text/plain")};

    QFuture<TextCapture> readSelection() override
    {
        ++selectionReads;
        return QtFuture::makeReadyValueFuture(capture(selection, CaptureSource::Selection));
    }

    QFuture<TextCapture> readClipboard() override
    {
        ++clipboardReads;
        return QtFuture::makeReadyValueFuture(capture(clipboard, CaptureSource::Clipboard));
    }

    int selectionReads = 0;
    int clipboardReads = 0;

private:
    TextCapture capture(const QString &text, CaptureSource source) const
    {
        TextCapture result;
        result.text = text;
        result.source = source;
        result.mimeTypes = text.isEmpty() ? QStringList() : mimeTypes;
        return result;
    }
};

} // namespace legilo::testing
