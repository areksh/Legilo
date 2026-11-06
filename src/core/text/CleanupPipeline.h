#pragma once

#include "core/document/Document.h"
#include "core/document/TextCapture.h"
#include "core/text/CleanupSettings.h"

namespace legilo {

class WordList;

// The text pipeline's steps 1 to 5 (design 7.2.1): normalises a capture,
// detects PDF text in Auto mode, reflows, tidies whitespace and segments,
// and returns the Document the reader displays and playback speaks. Each
// step is a pure function; this only runs them in order with the
// settings, so the preview can re-run it whenever a setting changes.
//
// Opened files are detected only when detectInFiles is set, because a
// plain-text file's line breaks are usually meant. The caller replaces a
// file's id and title (7.11.5).
class CleanupPipeline
{
public:
    // words confirms de-hyphenation; it must outlive the pipeline.
    explicit CleanupPipeline(const WordList &words);
    CleanupPipeline();

    Document run(const TextCapture &capture, const CleanupSettings &settings) const;

private:
    const WordList *m_words;
};

} // namespace legilo
