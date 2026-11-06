#pragma once

#include "core/text/PdfDetector.h"

#include <QStringList>

#include <cstdint>

namespace legilo {

// When reflow's PDF rules run (design 7.2.3): when detection says so, or
// always, or never.
enum class ReflowMode : std::uint8_t
{
    Auto,
    Always,
    Never
};

// How a URL is spoken (7.2.5): "a link to example.com", in full, or not.
enum class UrlRule : std::uint8_t
{
    Host,
    Full,
    Remove
};

// How an email address is spoken: "an email address at example.com", or
// in full.
enum class EmailRule : std::uint8_t
{
    Host,
    Full
};

// Where numeric citation markers ("[12]") are removed.
enum class CitationRule : std::uint8_t
{
    Never,
    PdfOnly,
    Always
};

// The clean-up settings (Appendix A, cleanup/...), as one value the
// pipeline takes and a document records, so a resume position can be
// mapped when they change (7.11.5).
struct CleanupSettings
{
    ReflowMode reflowMode = ReflowMode::Auto;
    double detectThreshold = DetectorWeights{}.threshold;
    bool pageNumbers = true;
    bool repeatedLines = true;
    bool dehyphenate = true;
    QStringList abbreviations; // extra abbreviations that never end a sentence
    int maxSegmentChars = 300;
    bool detectInFiles = false; // run detection on opened files in Auto mode
    UrlRule urls = UrlRule::Host;
    EmailRule emails = EmailRule::Host;
    CitationRule citations = CitationRule::PdfOnly;
    bool authorYearCitations = false; // remove "(Smith et al., 2020)"

    bool operator==(const CleanupSettings &) const = default;
};

} // namespace legilo
