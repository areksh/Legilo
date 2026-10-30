#pragma once

#include <QtGlobal>

namespace legilo {

// One unit of synthesis, highlighting and navigation, normally a sentence
// (design 7.2.8): a range of a document's cleaned text.
struct Segment
{
    qsizetype start = 0; // offset into the cleaned text, inclusive
    qsizetype end = 0;   // exclusive
    int paragraph = 0;   // paragraph index, for paragraph navigation

    bool operator==(const Segment &) const = default;
};

} // namespace legilo
