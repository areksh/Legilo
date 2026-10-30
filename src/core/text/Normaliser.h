#pragma once

#include <QString>
#include <QStringView>

namespace legilo {

// The result of normalisation: the text, and what was found on the way,
// which PDF detection uses (design 7.2.3).
struct NormalisedText
{
    QString text;
    bool hadLigatures = false;
};

// Pipeline step 1 (design 7.2.2): character fixes that are safe for any
// text. Expands the ligatures U+FB00 to U+FB06, maps full-width ASCII
// forms to ASCII, removes soft hyphens and zero-width characters, turns
// non-breaking and other Unicode spaces into plain spaces, and turns CRLF
// and CR line endings into LF.
//
// It deliberately does not apply NFKC, which would turn "x" with a
// superscript two into "x2" and a vulgar half into "1/2" with a fraction
// slash; a speech rule says those instead (7.2.5).
NormalisedText normalise(QStringView raw);

} // namespace legilo
