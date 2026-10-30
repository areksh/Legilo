#pragma once

#include <QStringView>

namespace legilo {

// Where to split the segment that playback starts with (design 7.5).
// Synthesis returns nothing until a whole call finishes, and a full
// segment takes seconds, so the first segment is synthesised in two
// chunks: a short first one that plays while the rest is synthesised.
//
// Returns the length of the first chunk: up to and including the first
// clause separator (";", ":", "," or a spaced dash) at or after
// targetChars, or else up to the first whitespace at or after it. A
// separator more than targetChars beyond the target is passed over for
// the whitespace, which is nearer. Returns the whole length, so no split,
// if the segment has no such point or either chunk would be a single
// word: Piper voices babble on one-word input.
qsizetype firstChunkLength(QStringView segment, qsizetype targetChars);

} // namespace legilo
