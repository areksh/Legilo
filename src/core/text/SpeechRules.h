#pragma once

#include "core/text/CleanupSettings.h"

#include <QString>
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

} // namespace legilo
