#pragma once

#include "core/document/Document.h"
#include "core/text/CleanupSettings.h"

#include <QString>
#include <QStringList>
#include <QStringView>

namespace legilo {

// Pipeline step 6 (design 7.2.5): the text a segment is spoken as. Speech
// rules change what is said, not what is shown, so they run on each
// segment as it is synthesised, never on the document's cleaned text.
// URLs and email addresses are said by their host or spoken in full,
// numeric citation markers are dropped in PDF text (pdf: whether the PDF
// rules ran) or as the settings say, author-year citations when asked,
// and superscripts that espeak-ng misreads are spelt out.
QString speakable(QStringView text, const CleanupSettings &settings, bool pdf);

// Each of the document's segments as it is spoken, in order, with the
// settings it was cleaned with.
QStringList spokenSegments(const Document &document);

// The whole document as spoken: a paragraph's segments joined by spaces,
// paragraphs by a blank line. What --clean-only prints.
QString spokenText(const Document &document);

} // namespace legilo
