#pragma once

#include <QString>
#include <QStringView>

namespace legilo {

class WordList;

struct ReflowOptions
{
    // Apply the PDF rules: detection said so, or the mode is Always.
    bool pdf = false;
    // Remove repeated running headers and footers (PDF mode only).
    bool removeRunningHeaders = true;
};

// Pipeline step 3 (design 7.2.4). In PDF mode, in this order: removes
// page-number lines and repeated running headers and footers, joins words
// hyphenated across line breaks, joins a line ending in a dash to the
// next without a space, and joins the lines of each paragraph, keeping
// list items and short headings apart. Outside PDF mode only the
// de-hyphenation that the word list confirms applies ("re-" + "sults"),
// so deliberate line breaks survive.
//
// words may be null, for no word list: PDF mode then joins every
// hyphenated break, and outside it nothing is joined.
QString reflow(QStringView text, const ReflowOptions &options, const WordList *words);

// Pipeline step 4: collapses runs of spaces and tabs, trims every line,
// and leaves at most one blank line between paragraphs.
QString tidyWhitespace(QStringView text);

} // namespace legilo
