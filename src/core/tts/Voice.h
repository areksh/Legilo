#pragma once

#include <QString>
#include <QUrl>

namespace legilo {

// One selectable voice: an installed model plus, for a multi-speaker model
// such as Kokoro, one of its speakers. Every speaker of a model is its own
// Voice, sharing the model's files. Paths are absolute.
struct Voice
{
    QString id;          // "piper-en_GB-cori-medium", or "kokoro-en-v0_19:bf_emma"
    QString modelId;     // the installed model the voice belongs to
    QString engine;      // "piper" or "kokoro"
    QString displayName; // the model's name, followed by the speaker's if it has several
    QString language;    // BCP 47, such as "en-GB"; empty if unknown
    int speakerId = 0;
    QString directory; // the model's installed directory
    QString modelPath;
    QString tokensPath;
    QString voicesPath; // Kokoro's speaker embeddings; empty for Piper
    QString licence;    // SPDX identifier or short name; "unknown" if not recorded
    QUrl licenceUrl;

    bool operator==(const Voice &) const = default;
};

} // namespace legilo
