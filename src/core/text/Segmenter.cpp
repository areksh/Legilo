#include "core/text/Segmenter.h"

#include <QSet>
#include <QString>

#include <algorithm>
#include <optional>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

struct Range
{
    qsizetype start = 0;
    qsizetype end = 0;

    qsizetype size() const { return end - start; }
};

constexpr char16_t Ellipsis = 0x2026;
constexpr char16_t LeftSingleQuote = 0x2018;
constexpr char16_t RightSingleQuote = 0x2019;
constexpr char16_t LeftDoubleQuote = 0x201C;
constexpr char16_t RightDoubleQuote = 0x201D;
constexpr char16_t EnDash = 0x2013;
constexpr char16_t EmDash = 0x2014;

// Abbreviations that never end a sentence (design 7.2.7, rule 3).
const QSet<QString> &neverFinal()
{
    static const QSet<QString> words{
        u"mr"_s,  u"mrs"_s, u"ms"_s,  u"dr"_s,     u"prof"_s, u"st"_s,  u"vs"_s,  u"e.g"_s,
        u"i.e"_s, u"cf"_s,  u"al"_s,  u"approx"_s, u"jan"_s,  u"feb"_s, u"mar"_s, u"apr"_s,
        u"jun"_s, u"jul"_s, u"aug"_s, u"sep"_s,    u"sept"_s, u"oct"_s, u"nov"_s, u"dec"_s};
    return words;
}

// Abbreviations that end no sentence when a number follows ("No. 5").
const QSet<QString> &beforeNumbers()
{
    static const QSet<QString> words{u"no"_s,  u"nos"_s, u"fig"_s, u"figs"_s, u"eq"_s,
                                     u"eqs"_s, u"vol"_s, u"p"_s,   u"pp"_s};
    return words;
}

bool isTerminal(QChar c)
{
    return c == u'.' || c == u'!' || c == u'?' || c == Ellipsis;
}

bool isCloser(QChar c)
{
    return c == u'"' || c == u'\'' || c == u')' || c == u']' || c == RightSingleQuote ||
           c == RightDoubleQuote;
}

bool isOpener(QChar c)
{
    return c == u'"' || c == u'\'' || c == u'(' || c == u'[' || c == LeftSingleQuote ||
           c == LeftDoubleQuote;
}

bool isDash(QChar c)
{
    return c == u'-' || c == EnDash || c == EmDash;
}

Range trimmed(QStringView text, Range range)
{
    while (range.start < range.end && text[range.start].isSpace()) {
        ++range.start;
    }
    while (range.end > range.start && text[range.end - 1].isSpace()) {
        --range.end;
    }
    return range;
}

// Paragraphs are separated by lines holding nothing but whitespace.
QList<Range> paragraphsOf(QStringView text)
{
    QList<Range> paragraphs;
    std::optional<Range> current;
    qsizetype lineStart = 0;
    while (lineStart <= text.size()) {
        qsizetype lineEnd = text.indexOf(u'\n', lineStart);
        if (lineEnd < 0) {
            lineEnd = text.size();
        }
        if (text.sliced(lineStart, lineEnd - lineStart).trimmed().isEmpty()) {
            if (current) {
                paragraphs.append(*current);
                current.reset();
            }
        } else if (current) {
            current->end = lineEnd;
        } else {
            current = Range{lineStart, lineEnd};
        }
        lineStart = lineEnd + 1;
    }
    if (current) {
        paragraphs.append(*current);
    }
    return paragraphs;
}

// The word before a full stop at i, with any dots inside it ("e.g").
QString tokenBefore(QStringView text, qsizetype i, qsizetype from)
{
    qsizetype start = i;
    while (start > from && (text[start - 1].isLetter() || text[start - 1] == u'.')) {
        --start;
    }
    return text.sliced(start, i - start).toString();
}

bool fullStopEndsNothing(const QString &token, QChar next, const SegmenterOptions &options)
{
    const QString lower = token.toLower();
    if (neverFinal().contains(lower) ||
        options.abbreviations.contains(lower, Qt::CaseInsensitive)) {
        return true;
    }
    if (beforeNumbers().contains(lower) && next.isDigit()) {
        return true;
    }
    return token.size() == 1 && token[0].isUpper(); // an initial
}

QList<Range> sentencesIn(QStringView text, Range paragraph, const SegmenterOptions &options)
{
    QList<Range> sentences;
    qsizetype start = paragraph.start;
    for (qsizetype i = paragraph.start; i < paragraph.end; ++i) {
        if (!isTerminal(text[i])) {
            continue;
        }
        qsizetype end = i + 1;
        while (end < paragraph.end && isCloser(text[end])) {
            ++end;
        }
        if (end >= paragraph.end || !text[end].isSpace()) {
            continue;
        }
        qsizetype next = end;
        while (next < paragraph.end && text[next].isSpace()) {
            ++next;
        }
        if (next >= paragraph.end) {
            break;
        }
        const QChar c = text[next];
        if (!c.isUpper() && !c.isDigit() && !isOpener(c)) {
            continue;
        }
        if (text[i] == u'.' &&
            fullStopEndsNothing(tokenBefore(text, i, paragraph.start), c, options)) {
            continue;
        }
        sentences.append(trimmed(text, {start, end}));
        start = next;
        i = next - 1;
    }
    if (const Range last = trimmed(text, {start, paragraph.end}); last.size() > 0) {
        sentences.append(last);
    }
    return sentences;
}

// Where a sentence may be split, by preference (rule 6). Returns the
// offset to cut at, or nothing.
std::optional<qsizetype> cutAt(QStringView text, qsizetype i, int kind)
{
    const QChar c = text[i];
    switch (kind) {
    case 0:
        return c == u';' ? std::optional(i + 1) : std::nullopt;
    case 1:
        return c == u':' ? std::optional(i + 1) : std::nullopt;
    case 2:
        return isDash(c) && text[i - 1].isSpace() && i + 1 < text.size() && text[i + 1].isSpace()
                   ? std::optional(i + 1)
                   : std::nullopt;
    case 3:
        return c == u',' ? std::optional(i + 1) : std::nullopt;
    default:
        return c.isSpace() ? std::optional(i) : std::nullopt;
    }
}

void splitLong(QStringView text, Range range, qsizetype maxLength, QList<Range> &out)
{
    if (range.size() <= maxLength) {
        out.append(range);
        return;
    }
    const qsizetype middle = range.start + (range.size() / 2);
    constexpr int Kinds = 5;
    for (int kind = 0; kind < Kinds; ++kind) {
        std::optional<qsizetype> best;
        for (qsizetype i = range.start + 1; i < range.end - 1; ++i) {
            const auto cut = cutAt(text, i, kind);
            if (!cut) {
                continue;
            }
            const Range left = trimmed(text, {range.start, *cut});
            const Range right = trimmed(text, {*cut, range.end});
            if (left.size() > 0 && right.size() > 0 &&
                (!best || qAbs(*cut - middle) < qAbs(*best - middle))) {
                best = cut;
            }
        }
        if (best) {
            splitLong(text, trimmed(text, {range.start, *best}), maxLength, out);
            splitLong(text, trimmed(text, {*best, range.end}), maxLength, out);
            return;
        }
    }
    out.append(range); // one long word: nothing to split at
}

bool isLoneWord(QStringView text, Range range)
{
    constexpr qsizetype MinimumLength = 3;
    if (range.size() < MinimumLength) {
        return true;
    }
    const QStringView part = text.sliced(range.start, range.size());
    return std::ranges::none_of(part, [](QChar c) { return c.isSpace(); });
}

// A lone word joins the segment before it, or the next if it comes first
// (rule 7). Ranges are within one paragraph, so merging never crosses one.
QList<Range> withoutLoneWords(QStringView text, const QList<Range> &ranges)
{
    QList<Range> merged;
    std::optional<Range> carried;
    for (Range range : ranges) {
        const bool lone = isLoneWord(text, range);
        if (lone && !merged.isEmpty()) {
            merged.last().end = range.end;
        } else if (lone && carried) {
            carried->end = range.end;
        } else if (lone) {
            carried = range;
        } else {
            if (carried) {
                range.start = carried->start;
                carried.reset();
            }
            merged.append(range);
        }
    }
    if (carried) {
        merged.append(*carried);
    }
    return merged;
}

} // namespace

QList<Segment> segment(QStringView text, const SegmenterOptions &options)
{
    QList<Segment> segments;
    int paragraph = 0;
    for (const Range &range : paragraphsOf(text)) {
        QList<Range> parts;
        for (const Range &sentence : sentencesIn(text, range, options)) {
            splitLong(text, sentence, options.maxLength, parts);
        }
        for (const Range &part : withoutLoneWords(text, parts)) {
            segments.append(Segment{part.start, part.end, paragraph});
        }
        ++paragraph;
    }
    return segments;
}

} // namespace legilo
