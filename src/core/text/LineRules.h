#pragma once

#include <QStringView>

namespace legilo {

// Line-level rules shared by reflow (design 7.2.4) and PDF detection
// (7.2.3).

// Whether line ends a sentence: with ".!?:;", allowing for closing quotes
// and brackets after it.
bool endsSentence(QStringView line);

// Whether line holds only a page number: an Arabic number of up to four
// digits or a valid lowercase Roman numeral, or an uppercase one after
// "Page", with an optional "Page" or "p." before it, " of N" after it,
// and dashes around it. Words that only look Roman, such as "civil",
// do not count.
bool isPageNumberLine(QStringView line);

// Whether line starts with a list marker: "-", "*", a bullet, "1." or
// "a)", followed by whitespace.
bool isListItemLine(QStringView line);

} // namespace legilo
