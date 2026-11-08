#pragma once

#include "core/document/TextCapture.h"

#include <QFuture>
#include <QObject>

namespace legilo {

// Reads the text the user wants spoken: the current selection or the
// clipboard (design 7.1.1). Each platform implements it in its own way,
// so the app depends only on this interface and tests substitute a fake.
//
// Both reads are asynchronous, because the Windows fallback has to wait
// for the target application to answer a simulated copy (7.1.3). A read
// that finds no text completes with an empty capture rather than failing.
class TextSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual QFuture<TextCapture> readSelection() = 0;
    virtual QFuture<TextCapture> readClipboard() = 0;
};

} // namespace legilo
