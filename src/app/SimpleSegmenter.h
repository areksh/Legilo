#pragma once

#include <QStringList>
#include <QStringView>

namespace legilo {

// Splits text into segments for the stage 1 `--speak` commands. A
// stand-in for the Segmenter of design 7.2.7, which replaces it in stage 2
// together with those commands.
//
// Paragraphs are separated by blank lines, and the lines within one are
// joined. A segment ends after ".", "!" or "?" (and any closing quotes or
// brackets) followed by whitespace. A one-word segment joins the one
// before it in its paragraph, or the next if it comes first: Piper voices
// can babble on a lone short word. It knows nothing of abbreviations.
QStringList simpleSegments(QStringView text);

} // namespace legilo
