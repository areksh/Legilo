#include "core/text/Reflow.h"

#include "core/text/LineRules.h"
#include "core/text/WordList.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr auto Unicode = QRegularExpression::UseUnicodePropertiesOption;

bool isBlank(QStringView line)
{
    return line.trimmed().isEmpty();
}

QString removePageNumbers(const QString &text)
{
    const QStringList lines = text.split(u'\n');
    QList<bool> numbers;
    numbers.reserve(lines.size());
    for (const QString &line : lines) {
        numbers.append(isPageNumberLine(line));
    }
    QStringList kept;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        // Two or more number lines in a row are a table column.
        const bool column = (i > 0 && numbers[i - 1]) || (i + 1 < lines.size() && numbers[i + 1]);
        if (!numbers[i] || column) {
            kept.append(lines[i]);
        }
    }
    return kept.join(u'\n');
}

// Rule 2: short lines that repeat, digits aside, and are not sentences.
QString removeRunningHeaders(const QString &text)
{
    constexpr qsizetype MaxLength = 60;
    constexpr int MinLetters = 3;
    static const QRegularExpression digit(u"\\d"_s, Unicode);
    const auto keyOf = [](const QString &line) {
        return QString(line).replace(digit, u"#"_s).trimmed();
    };

    const QStringList lines = text.split(u'\n');
    QHash<QString, int> counts;
    for (const QString &line : lines) {
        if (!isBlank(line)) {
            ++counts[keyOf(line)];
        }
    }
    QStringList kept;
    for (const QString &line : lines) {
        const QString key = keyOf(line);
        const auto letters = std::ranges::count_if(key, [](QChar c) { return c.isLetter(); });
        const bool header = !isBlank(line) && key.size() < MaxLength && letters >= MinLetters &&
                            counts.value(key) >= 2 && !endsSentence(key);
        if (!header) {
            kept.append(line);
        }
    }
    return kept.join(u'\n');
}

// Firefox puts a blank line between many lines of one paragraph, so in
// PDF text a blank line is a paragraph break only where a sentence could
// end: after terminal punctuation, or before a line that does not run on
// with a lowercase letter.
QString joinBlankLinesInSentences(const QString &text)
{
    const QStringList lines = text.split(u'\n');
    QStringList kept;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (!isBlank(lines[i]) || kept.isEmpty() || isBlank(kept.last())) {
            kept.append(lines[i]);
            continue;
        }
        qsizetype next = i;
        while (next < lines.size() && isBlank(lines[next])) {
            ++next;
        }
        const bool insideSentence = next < lines.size() && !endsSentence(kept.last().trimmed()) &&
                                    lines[next].trimmed().front().isLower();
        if (insideSentence) {
            i = next - 1; // drop the blank run
        } else {
            kept.append(lines[i]);
        }
    }
    return kept.join(u'\n');
}

// What the word list says about a word broken by a hyphen.
enum class Break : std::uint8_t
{
    Unknown,    // no word list, or nothing it confirms ("foo-" + "barz")
    Join,       // "re-" + "sults": only the joined word is a word
    KeepHyphen, // "well-" + "known": both halves are words, the joined one not
    Ambiguous,  // "re-" + "cover": all three are words
};

Break judge(const QString &first, const QString &second, const WordList *words)
{
    if (words == nullptr) {
        return Break::Unknown;
    }
    const bool firstIsWord = words->contains(first);
    const bool secondIsWord = words->contains(second);
    const bool joinedIsWord = words->contains(first + second);
    if (firstIsWord && secondIsWord) {
        return joinedIsWord ? Break::Ambiguous : Break::KeepHyphen;
    }
    return joinedIsWord ? Break::Join : Break::Unknown;
}

// Rule 3: a word hyphenated across a line break. In PDF text the joined
// word ends the first line and the rest of the second line stays on its
// own: merging the two lines would make one twice the usual width, and
// rule 5 judges paragraph ends by that width.
QString dehyphenate(const QString &text, bool pdf, const WordList *words)
{
    static const QRegularExpression broken(u"(\\w+)-[ \\t]*\\n[ \\t]*(\\w+)(\\S*)"_s, Unicode);
    QString result;
    qsizetype last = 0;
    for (const auto &match : broken.globalMatch(text)) {
        const QString first = match.captured(1);
        const QString second = match.captured(2);
        const QString rest = match.captured(3);  // punctuation after the word
        QString replacement = match.captured(0); // left as it is
        switch (judge(first, second, words)) {
        case Break::Join:
            replacement = first + second + rest; // confirmed: in every mode
            break;
        case Break::KeepHyphen:
            if (pdf) {
                replacement = first + u'-' + second + rest;
            }
            break;
        case Break::Unknown:
        case Break::Ambiguous:
            if (pdf) {
                replacement = first + second + rest;
            }
            break;
        }
        result += QStringView(text).sliced(last, match.capturedStart() - last);
        last = match.capturedEnd();
        if (pdf && replacement != match.captured(0)) {
            while (last < text.size() && (text[last] == u' ' || text[last] == u'\t')) {
                ++last;
            }
            if (last < text.size() && text[last] != u'\n') {
                replacement += u'\n';
            }
        }
        result += replacement;
    }
    result += QStringView(text).sliced(last);
    return result;
}

// Chromium copies a hyphenated break as one line, "inter- ferometric".
// Inside a line the hyphen and space can also be meant ("pre- and
// post-tests"), so only what the word list confirms is joined.
QString dehyphenateInLines(const QString &text, const WordList *words)
{
    static const QRegularExpression broken(u"(\\p{L}+)- (\\p{Ll}+)"_s, Unicode);
    QString result;
    qsizetype last = 0;
    for (const auto &match : broken.globalMatch(text)) {
        const QString first = match.captured(1);
        const QString second = match.captured(2);
        QString replacement = match.captured(0);
        switch (judge(first, second, words)) {
        case Break::Join:
            replacement = first + second;
            break;
        case Break::KeepHyphen:
            replacement = first + u'-' + second;
            break;
        case Break::Unknown:
        case Break::Ambiguous:
            break;
        }
        result += QStringView(text).sliced(last, match.capturedStart() - last);
        result += replacement;
        last = match.capturedEnd();
    }
    result += QStringView(text).sliced(last);
    return result;
}

// Rule 4: a line ending in an en or em dash runs on without a space.
QString joinDashes(const QString &text)
{
    static const QRegularExpression dash(u"([\\x{2013}\\x{2014}])[ \\t]*\\n[ \\t]*(?=\\S)"_s,
                                         Unicode);
    return QString(text).replace(dash, u"\\1"_s);
}

// Rule 5: within a paragraph a line break becomes a space, except before
// a list item or after a line that ends a sentence well short of the
// usual width. That is a heading, or the last line of a paragraph whose
// blank line the copy lost: Okular's and Chromium's copies have none. A
// shorter line still that has no stop is a heading too when the next line
// starts with a capital ("Method", "1. Introduction").
QString joinLines(const QString &text)
{
    QList<QStringList> paragraphs{{}};
    QList<qsizetype> lengths;
    for (const QString &line : text.split(u'\n')) {
        if (isBlank(line)) {
            if (!paragraphs.last().isEmpty()) {
                paragraphs.append(QStringList{});
            }
            continue;
        }
        paragraphs.last().append(line.trimmed());
        lengths.append(line.trimmed().size());
    }
    if (lengths.isEmpty()) {
        return {};
    }
    std::ranges::sort(lengths);
    constexpr double Percentile = 0.9;
    const auto index =
        static_cast<qsizetype>(std::ceil(Percentile * static_cast<double>(lengths.size()))) - 1;
    // ParagraphEnd is tuned on the clean-up corpus (9.2), for the most
    // paragraphs recovered exactly. A heading without a stop must be
    // shorter still: under half, as the first draft had it.
    constexpr double ParagraphEnd = 0.8;
    constexpr double Heading = 0.5;
    const auto usual = static_cast<double>(lengths.at(std::max<qsizetype>(index, 0)));
    const auto isHeading = [&](const QString &line, const QString &next) {
        return static_cast<double>(line.size()) < Heading * usual && !endsSentence(line) &&
               std::ranges::any_of(
                   line, [](QChar c) { return c.isLetter(); }) &&
               next.front().isUpper();
    };

    QStringList joined;
    for (const QStringList &lines : paragraphs) {
        if (lines.isEmpty()) {
            continue;
        }
        QString paragraph = lines.first();
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QString &previous = lines.at(i - 1);
            const bool paragraphEnd =
                (static_cast<double>(previous.size()) < ParagraphEnd * usual &&
                 endsSentence(previous)) ||
                isHeading(previous, lines.at(i));
            paragraph += isListItemLine(lines.at(i)) || paragraphEnd ? u"\n\n"_s : u" "_s;
            paragraph += lines.at(i);
        }
        joined.append(paragraph);
    }
    return joined.join(u"\n\n"_s);
}

} // namespace

QString reflow(QStringView text, const ReflowOptions &options, const WordList *words)
{
    QString result = text.toString();
    if (options.pdf) {
        if (options.removePageNumbers) {
            result = removePageNumbers(result);
        }
        if (options.removeRunningHeaders) {
            result = removeRunningHeaders(result);
        }
        result = joinBlankLinesInSentences(result);
    }
    if (options.dehyphenate) {
        result = dehyphenate(result, options.pdf, words);
    }
    if (options.pdf) {
        if (options.dehyphenate) {
            result = dehyphenateInLines(result, words);
        }
        result = joinDashes(result);
        result = joinLines(result);
    }
    return result;
}

QString tidyWhitespace(QStringView text)
{
    static const QRegularExpression spaces(u"[ \\t]+"_s);
    QStringList lines;
    bool blankPending = false;
    for (QStringView line : text.split(u'\n')) {
        const QString tidy = line.toString().replace(spaces, u" "_s).trimmed();
        if (tidy.isEmpty()) {
            blankPending = !lines.isEmpty();
            continue;
        }
        if (blankPending) {
            lines.append(QString());
            blankPending = false;
        }
        lines.append(tidy);
    }
    return lines.join(u'\n');
}

} // namespace legilo
