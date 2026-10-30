#include "core/text/Normaliser.h"

#include <array>

namespace legilo {

namespace {

// Named, never as literals: editors can silently drop invisible characters.
constexpr char16_t SoftHyphen = 0x00AD;
constexpr char16_t ZeroWidthSpace = 0x200B;
constexpr char16_t ZeroWidthNonJoiner = 0x200C;
constexpr char16_t ZeroWidthJoiner = 0x200D;
constexpr char16_t ByteOrderMark = 0xFEFF;

constexpr char16_t FirstLigature = 0xFB00;
constexpr char16_t LastLigature = 0xFB06;
// U+FB00 ff, fi, fl, ffi, ffl, long s t, s t.
constexpr std::array<const char *, 7> Ligatures = {"ff", "fi", "fl", "ffi", "ffl", "st", "st"};

constexpr char16_t FirstFullWidth = 0xFF01;
constexpr char16_t LastFullWidth = 0xFF5E;
constexpr char16_t FullWidthOffset = 0xFEE0; // U+FF01 is "!" (U+0021)

bool isRemoved(char16_t c)
{
    return c == SoftHyphen || c == ZeroWidthSpace || c == ZeroWidthNonJoiner ||
           c == ZeroWidthJoiner || c == ByteOrderMark;
}

} // namespace

NormalisedText normalise(QStringView raw)
{
    NormalisedText result;
    result.text.reserve(raw.size());
    for (qsizetype i = 0; i < raw.size(); ++i) {
        const QChar c = raw[i];
        const char16_t u = c.unicode();
        if (isRemoved(u)) {
            continue;
        }
        if (u >= FirstLigature && u <= LastLigature) {
            result.text += QLatin1StringView(Ligatures[u - FirstLigature]);
            result.hadLigatures = true;
        } else if (u >= FirstFullWidth && u <= LastFullWidth) {
            result.text += QChar(u - FullWidthOffset);
        } else if (c.category() == QChar::Separator_Space) {
            result.text += u' ';
        } else if (u == u'\r') {
            result.text += u'\n';
            if (i + 1 < raw.size() && raw[i + 1] == u'\n') {
                ++i;
            }
        } else {
            result.text += c;
        }
    }
    return result;
}

} // namespace legilo
