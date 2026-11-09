#include "linux/KdeTextSource.h"

#include "platform/Logging.h"

#include <KSystemClipboard>
#include <QMimeData>

namespace legilo {

namespace {

TextCapture read(QClipboard::Mode mode, CaptureSource source)
{
    TextCapture capture;
    capture.source = source;
    KSystemClipboard *clipboard = KSystemClipboard::instance();
    if (clipboard == nullptr) {
        qCWarning(lcPlatform) << "No system clipboard is available";
        return capture;
    }
    if (const QMimeData *data = clipboard->mimeData(mode)) {
        capture.mimeTypes = data->formats();
        capture.text = data->text();
    }
    // The length only: captured text is never logged (7.16).
    qCDebug(lcPlatform) << "Read" << capture.text.size() << "characters from the"
                        << (mode == QClipboard::Selection ? "selection" : "clipboard");
    return capture;
}

} // namespace

KdeTextSource::KdeTextSource(QObject *parent)
    : TextSource(parent)
{
    // Created on the first read instead, it found no selection there: the
    // first shortcut press after start-up said nothing was selected.
    if (KSystemClipboard::instance() == nullptr) {
        qCWarning(lcPlatform) << "No system clipboard is available";
    }
}

QFuture<TextCapture> KdeTextSource::readSelection()
{
    return QtFuture::makeReadyValueFuture(read(QClipboard::Selection, CaptureSource::Selection));
}

QFuture<TextCapture> KdeTextSource::readClipboard()
{
    return QtFuture::makeReadyValueFuture(read(QClipboard::Clipboard, CaptureSource::Clipboard));
}

} // namespace legilo
