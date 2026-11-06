#pragma once

#include "core/document/Segment.h"
#include "core/document/TextCapture.h"
#include "core/text/CleanupSettings.h"
#include "core/text/PdfDetector.h"

#include <QList>
#include <QString>

namespace legilo {

// A piece of text being read (design 7.2.8): the text as captured, the
// cleaned text the reader displays, and its segments. Speech rules are
// applied per segment when it is synthesised, so cleaned keeps URLs and
// citations as written.
struct Document
{
    QString id;    // SHA-256 of original, the key for resume positions (7.11.5)
    QString title; // the first words of the text
    CaptureSource source = CaptureSource::Selection;
    QString original; // after normalisation, before everything else
    QString cleaned;  // what is displayed
    QList<Segment> segments;
    DetectionResult detection; // empty unless the mode was Auto
    CleanupSettings settingsUsed;
};

} // namespace legilo
