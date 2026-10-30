#include "core/text/Reflow.h"

#include "core/text/WordList.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr auto Unicode = QRegularExpression::UseUnicodePropertiesOption;

constexpr char16_t RightSingleQuote = 0x2019;
constexpr char16_t RightDoubleQuote = 0x201D;

bool isBlank(QStringView line)
{
    return line.trimmed().isEmpty();
}

// Ends with ".!?:;", allowing for closing quotes and brackets after it.
bool endsWithTerminalPunctuation(QStringView line)
{
    line = line.trimmed();
    while (!line.isEmpty() && (line.back() == u'"' || line.back() == u'\'' || line.back() == u')' ||
                               line.back() == u']' || line.back() == RightSingleQuote ||
                               line.back() == RightDoubleQuote)) {
        line.chop(1);
    }
    if (line.isEmpty()) {
        return false;
    }
    const QChar c = line.back();
    return c == u'.' || c == u'!' || c == u'?' || c == u':' || c == u';';
}

bool isValidRoman(const QString &numeral)
{
    static const QRegularExpression roman(
        u"^(?=[mdclxvi])m{0,3}(cm|cd|d?c{0,3})(xc|xl|l?x{0,3})(ix|iv|v?i{0,3})$"_s);
    return roman.match(numeral).hasMatch();
}

// Rule 1: a line holding only a page number, with an optional "Page" or
// "p." before it, " of N" after it, and dashes around it.
bool isPageNumber(QStringView line)
{
    static const QRegularExpression pattern(
        u"^[-\\x{2013}\\x{2014}]?\\s*((?i:page)\\s+|(?i:p)\\.\\s*)?([0-9]{1,4}|[A-Za-z]+)"
        u"(\\s+(?i:of)\\s+[0-9]+)?\\s*[-\\x{2013}\\x{2014}]?$"_s,
        Unicode);
    const auto match = pattern.matchView(line.trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    const QString number = match.captured(2);
    if (number.front().isDigit()) {
        return true;
    }
    if (number == number.toLower()) {
        return isValidRoman(number);
    }
    // Uppercase Roman numerals only after "Page": alone, "I" and "IV" are
    // as likely to be words or headings.
    const bool afterPage = match.captured(1).trimmed().compare(u"page", Qt::CaseInsensitive) == 0;
    return afterPage && number == number.toUpper() && isValidRoman(number.toLower());
}

QString removePageNumbers(const QString &text)
{
    const QStringList lines = text.split(u'\n');
    QList<bool> numbers;
    numbers.reserve(lines.size());
    for (const QString &line : lines) {
        numbers.append(isPageNumber(line));
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
                            counts.value(key) >= 2 && !endsWithTerminalPunctuation(key);
        if (!header) {
            kept.append(line);
        }
    }
    return kept.join(u'\n');
}

// Rule 3: a word hyphenated across a line break.
QString dehyphenate(const QString &text, bool pdf, const WordList *words)
{
    static const QRegularExpression broken(u"(\\w+)-[ \\t]*\\n[ \\t]*(\\w+)"_s, Unicode);
    QString result;
    qsizetype last = 0;
    for (const auto &match : broken.globalMatch(text)) {
        const QString first = match.captured(1);
        const QString second = match.captured(2);
        QString replacement = match.captured(0); // left as it is
        if (words != nullptr) {
            const bool firstIsWord = words->contains(first);
            const bool secondIsWord = words->contains(second);
            const bool joinedIsWord = words->contains(first + second);
            const bool keepHyphen = firstIsWord && secondIsWord && !joinedIsWord;   // "well-known"
            const bool confirmed = joinedIsWord && (!firstIsWord || !secondIsWord); // "results"
            if (keepHyphen) {
                if (pdf) {
                    replacement = first + u'-' + second;
                }
            } else if (confirmed || pdf) {
                // A confirmed join applies in every mode. Any other, such
                // as "re-" + "cover", where all three are words, only to
                // PDF text.
                replacement = first + second;
            }
        } else if (pdf) {
            replacement = first + second;
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

bool isListItem(QStringView line)
{
    static const QRegularExpression marker(
        u"^\\s*([-*\\x{2022}\\x{2023}\\x{25E6}]|\\d{1,3}[.)]|[A-Za-z][.)])\\s"_s, Unicode);
    return marker.matchView(line).hasMatch();
}

// Rule 5: within a paragraph a line break becomes a space, except before
// a list item or after a short line that ends a sentence (a heading).
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
    const double shortLine = 0.5 * static_cast<double>(lengths.at(std::max<qsizetype>(index, 0)));

    QStringList joined;
    for (const QStringList &lines : paragraphs) {
        if (lines.isEmpty()) {
            continue;
        }
        QString paragraph = lines.first();
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QString &previous = lines.at(i - 1);
            const bool heading = static_cast<double>(previous.size()) < shortLine &&
                                 endsWithTerminalPunctuation(previous);
            paragraph += isListItem(lines.at(i)) || heading ? u"\n\n"_s : u" "_s;
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
        result = removePageNumbers(result);
        if (options.removeRunningHeaders) {
            result = removeRunningHeaders(result);
        }
    }
    result = dehyphenate(result, options.pdf, words);
    if (options.pdf) {
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
