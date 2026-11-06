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

    bool operator==(const CleanupSettings &) const = default;
};

} // namespace legilo
