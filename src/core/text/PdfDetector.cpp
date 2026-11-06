#include "core/text/PdfDetector.h"

#include "core/text/LineRules.h"

#include <QFileInfo>
#include <QList>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr auto Unicode = QRegularExpression::UseUnicodePropertiesOption;
constexpr qsizetype MinimumLines = 3;
// Wrapped lines in a row that make the wrap feature 1. Fewer score in
// proportion, so a few short lines that happen to run on, as chat and
// notes do, count for little.
constexpr qsizetype FullRun = 8;

struct Line
{
    QStringView text; // trimmed
    QStringView raw;  // as it came, for indentation
};

// A line that carries on the sentence of the one before. Design 7.2.3 also
// counted a leading digit or opening bracket, but in the corpus those
// marked logs, tables, chat and checklists far more often than prose.
bool continues(QStringView line)
{
    return line.front().isLower();
}

// Equations and table cells come a symbol or a cell to a line, and their
// lines can start lowercase too; a line of prose holds a few words.
bool holdsWords(QStringView line)
{
    constexpr int MinimumWords = 3;
    static const QRegularExpression word(u"\\p{L}{2,}"_s, Unicode);
    int words = 0;
    for (auto it = word.globalMatchView(line); it.hasNext() && words < MinimumWords; it.next()) {
        ++words;
    }
    return words >= MinimumWords;
}

// Code, shell, markup and table rows use these far more than prose does.
bool isSymbolDense(QStringView line)
{
    constexpr double MinimumShare = 0.05;
    static const QString symbols = u"{}[]()<>=;$#/\\|&*_`~^@%"_s;
    const auto count = std::ranges::count_if(line, [](QChar c) { return symbols.contains(c); });
    return static_cast<double>(count) >= MinimumShare * static_cast<double>(line.size());
}

// PDF readers whose windows own a capture on Windows (7.1.3).
bool isPdfReader(const QString &owner)
{
    static const QStringList readers{
        u"acrord32.exe"_s,    u"acrobat.exe"_s,  u"sumatrapdf.exe"_s, u"foxitpdfreader.exe"_s,
        u"foxitreader.exe"_s, u"pdfxedit.exe"_s, u"pdfxcview.exe"_s,  u"okular.exe"_s};
    return readers.contains(QFileInfo(owner).fileName(), Qt::CaseInsensitive);
}

double fraction(qsizetype count, qsizetype of)
{
    return of > 0 ? static_cast<double>(count) / static_cast<double>(of) : 0.0;
}

} // namespace

DetectionResult detectPdf(QStringView text, const DetectionHints &hints,
                          const DetectorWeights &weights)
{
    QList<Line> lines;
    for (QStringView raw : text.split(u'\n')) {
        if (!raw.trimmed().isEmpty()) {
            lines.append({raw.trimmed(), raw});
        }
    }
    if (lines.size() < MinimumLines) {
        return {};
    }

    static const QRegularExpression brokenAtEnd(u"\\p{L}-$"_s, Unicode);
    static const QRegularExpression brokenInside(u"\\p{L}{2,}- \\p{Ll}{2,}"_s, Unicode);
    static const QRegularExpression sentenceInside(
        u"(?:(?<!\\p{L})\\p{Ll}{2,}|\\))[.!?][\"')\\x{2019}\\x{201D}]?\\s+\\p{Lu}"_s, Unicode);

    // Every line but the last is a body line; its successor is the next.
    const qsizetype body = lines.size() - 1;
    qsizetype run = 0;
    qsizetype longestRun = 0;
    bool hyphen = false;
    for (qsizetype i = 0; i < body; ++i) {
        const QStringView line = lines.at(i).text;
        const QStringView next = lines.at(i + 1).text;
        run = !endsSentence(line) && holdsWords(line) && continues(next) ? run + 1 : 0;
        longestRun = std::max(longestRun, run);
        if (brokenAtEnd.matchView(line).hasMatch() && next.front().isLower()) {
            hyphen = true;
        }
    }
    qsizetype listed = 0;
    qsizetype midline = 0;
    qsizetype symbolic = 0;
    bool pageNumber = false;
    for (const Line &line : lines) {
        hyphen = hyphen || brokenInside.matchView(line.text).hasMatch();
        pageNumber = pageNumber || isPageNumberLine(line.text);
        listed += isListItemLine(line.raw) ? 1 : 0;
        midline += sentenceInside.matchView(line.text).hasMatch() ? 1 : 0;
        symbolic += isSymbolDense(line.text) ? 1 : 0;
    }
    const bool html = std::ranges::any_of(
        hints.mimeTypes, [](const QString &type) { return type.startsWith(u"text/html"); });

    DetectionResult result;
    result.features = {
        {u"wrap"_s, fraction(std::min(longestRun, FullRun), FullRun)},
        {u"midline"_s, fraction(midline, lines.size())},
        {u"hyphen"_s, hyphen ? 1.0 : 0.0},
        {u"ligature"_s, hints.hadLigatures ? 1.0 : 0.0},
        {u"pagenum"_s, pageNumber ? 1.0 : 0.0},
        {u"owner"_s, isPdfReader(hints.ownerHint) ? 1.0 : 0.0},
        {u"list"_s, fraction(listed, lines.size())},
        {u"symbols"_s, fraction(symbolic, lines.size())},
        {u"html"_s, html ? 1.0 : 0.0},
    };
    const auto &f = result.features;
    result.score = (weights.wrap * f[u"wrap"_s]) + (weights.midline * f[u"midline"_s]) +
                   (weights.hyphen * f[u"hyphen"_s]) + (weights.ligature * f[u"ligature"_s]) +
                   (weights.pagenum * f[u"pagenum"_s]) + (weights.owner * f[u"owner"_s]) +
                   (weights.list * f[u"list"_s]) + (weights.symbols * f[u"symbols"_s]) +
                   (weights.html * f[u"html"_s]);
    result.isPdf = result.score >= weights.threshold;
    return result;
}

} // namespace legilo
