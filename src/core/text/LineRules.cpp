#include "core/text/LineRules.h"

#include <QRegularExpression>
#include <QString>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr auto Unicode = QRegularExpression::UseUnicodePropertiesOption;

constexpr char16_t RightSingleQuote = 0x2019;
constexpr char16_t RightDoubleQuote = 0x201D;

bool isValidRoman(const QString &numeral)
{
    static const QRegularExpression roman(
        u"^(?=[mdclxvi])m{0,3}(cm|cd|d?c{0,3})(xc|xl|l?x{0,3})(ix|iv|v?i{0,3})$"_s, Unicode);
    return roman.match(numeral).hasMatch();
}

} // namespace

bool endsSentence(QStringView line)
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

bool isPageNumberLine(QStringView line)
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

bool isListItemLine(QStringView line)
{
    // A clause number ("2.1") counts only before a capital, so a decimal
    // running on from the line before ("3.5 metres") does not.
    static const QRegularExpression marker(
        u"^\\s*(([-*\\x{2022}\\x{2023}\\x{25E6}]|\\d{1,3}[.)]|[A-Za-z][.)])\\s|"
        u"\\d{1,3}(\\.\\d{1,3})+\\s+\\p{Lu})"_s,
        Unicode);
    return marker.matchView(line).hasMatch();
}

} // namespace legilo
