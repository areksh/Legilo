#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace legilo {

// What is known about a capture besides its text.
struct DetectionHints
{
    bool hadLigatures = false; // from normalisation (7.2.2)
    QStringList mimeTypes;     // offered by the selection or clipboard
    QString ownerHint;         // the owning process, on Windows
};

// Feature weights and the decision threshold (design 7.2.3), fitted by
// logistic regression on the regression corpus's tuning set (9.2) and
// scaled so the threshold is 1. Signals against PDF text carry negative
// weights. The design counted list markers against it too, but numbered
// clauses and recipe steps wrap in PDF text as prose does, and reflow
// keeps the break before each list item anyway, so the fit gives list a
// small positive weight. No corpus sample has the
// owner or html hint, so those keep the design's weights, scaled alike.
struct DetectorWeights
{
    double wrap = 2.58;
    double midline = 0.94;
    double hyphen = 0.95;
    double ligature = 0.0;
    double pagenum = 0.41;
    double owner = 0.22;
    double list = 0.26;
    double symbols = -1.26;
    double html = -0.11;
    double threshold = 1.0;
};

struct DetectionResult
{
    double score = 0.0;
    bool isPdf = false;
    QMap<QString, double> features; // by name, for the preview's explanation
};

// Pipeline step 2 (design 7.2.3): how much normalised text looks like it
// was copied from a PDF, which decides whether reflow runs in Auto mode.
//
// Features are computed over the non-empty lines; blank lines are ignored,
// because no PDF reader's copy marks paragraphs with them reliably. A
// line's successor is the next non-empty line, and every line but the last
// is a body line.
//
// - wrap: the longest run of body lines that end mid-sentence and run on
//   into a line starting with a lowercase letter, as a fraction of eight
//   lines. A run, not a share of all lines, so that an address block or a
//   table above wrapped prose does not hide it.
// - midline: lines in which a sentence ends and another begins, as they
//   do in wrapped prose and rarely in lists, chat or poems. The word
//   before the stop must be lower case, so "Dr. Smith" does not count.
// - hyphen: 1 if a word breaks with a hyphen across lines, or inside one
//   as "word- word", as Chromium copies it.
// - ligature, owner, html: 1 if the hint holds.
// - pagenum: 1 if a line holds only a page number.
// - list: lines starting with a list marker.
// - symbols: lines in which at least 5% of the characters are symbols
//   such as brackets, "=", "$" or "/", which marks code, shell, markup
//   and tables rather than prose.
//
// Text of fewer than three non-empty lines is never PDF text: there is
// nothing to reflow.
DetectionResult detectPdf(QStringView text, const DetectionHints &hints = {},
                          const DetectorWeights &weights = {});

} // namespace legilo
