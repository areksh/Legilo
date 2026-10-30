#pragma once

#include "core/document/Segment.h"

#include <QList>
#include <QStringList>
#include <QStringView>

namespace legilo {

struct SegmenterOptions
{
    // Longer sentences are split at the separator nearest the middle.
    qsizetype maxLength = 300;
    // Extra abbreviations that never end a sentence, without the full
    // stop and in any case (the cleanup/abbreviations setting).
    QStringList abbreviations;
};

// Pipeline step 5 (design 7.2.7): splits cleaned text into segments.
//
// A blank line always ends a segment. Otherwise a segment ends after ".",
// "!", "?" or an ellipsis, with any closing quotes or brackets, followed by
// whitespace and then an uppercase letter, a digit, or an opening quote or
// bracket. A full stop does not end one after an abbreviation that never
// ends a sentence (Mr, Dr, e.g ...), after one that precedes a number when
// a digit follows (No, Fig, pp ...), or after a single capital letter
// (initials). URLs and email addresses hold no whitespace, so no boundary
// falls inside them.
//
// A segment longer than maxLength is split at the separator nearest its
// middle, preferring ";" then ":" then a spaced dash then "," then any
// whitespace, until every part fits. A segment of one word, or shorter
// than three characters, joins the one before it in its paragraph, or the
// next if it is the first; Piper voices can babble on a lone short word.
// Segments never include surrounding whitespace.
QList<Segment> segment(QStringView text, const SegmenterOptions &options = {});

} // namespace legilo
