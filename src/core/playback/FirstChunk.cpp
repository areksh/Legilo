#include "core/playback/FirstChunk.h"

#include <QString>

#include <algorithm>

namespace legilo {

namespace {

bool isDash(QChar c)
{
    return c == u'-' || c == QChar(0x2013) || c == QChar(0x2014); // hyphen-minus, en dash, em dash
}

// A clause separator: ";", ":", ",", or a dash with whitespace either side.
bool isSeparatorAt(QStringView text, qsizetype i)
{
    const QChar c = text[i];
    if (c == u';' || c == u':' || c == u',') {
        return true;
    }
    return isDash(c) && i > 0 && i + 1 < text.size() && text[i - 1].isSpace() &&
           text[i + 1].isSpace();
}

bool hasWhitespace(QStringView text)
{
    return std::ranges::any_of(text, [](QChar c) { return c.isSpace(); });
}

} // namespace

qsizetype firstChunkLength(QStringView segment, qsizetype targetChars)
{
    const qsizetype size = segment.size();
    const qsizetype from = std::max<qsizetype>(targetChars, 0);

    qsizetype split = -1;
    for (qsizetype i = from; i < size && i - from <= from; ++i) {
        if (isSeparatorAt(segment, i)) {
            split = i + 1; // the separator stays with the first chunk
            break;
        }
    }
    if (split < 0) {
        for (qsizetype i = from; i < size; ++i) {
            if (segment[i].isSpace()) {
                split = i;
                break;
            }
        }
    }
    if (split < 0) {
        return size;
    }

    const QStringView first = segment.first(split).trimmed();
    const QStringView rest = segment.sliced(split).trimmed();
    if (!hasWhitespace(first) || !hasWhitespace(rest)) {
        return size;
    }
    return split;
}

} // namespace legilo
