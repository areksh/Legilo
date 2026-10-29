#pragma once

#include "core/tts/Voice.h"

#include <QList>
#include <QString>

#include <optional>

namespace legilo {

// The voices installed in a voices directory. Each model lives in its own
// subdirectory with a legilo-voice.json manifest, written by the installer:
//
//   {
//     "format": 1,
//     "id": "kokoro-en-v0_19",
//     "engine": "kokoro",
//     "name": "Kokoro (English)",
//     "language": "en-US",
//     "files": { "model": "model.onnx", "tokens": "tokens.txt",
//                "voices": "voices.bin" },
//     "speakers": [ { "id": 7, "name": "bf_emma", "espeakVoice": "en" } ],
//     "licence": "Apache-2.0",
//     "licenceUrl": "https://..."
//   }
//
// "format", "id", "engine", "name" and "files" are required, and "voices"
// only for Kokoro. Without "speakers" the model has one speaker, 0. A
// speaker's "espeakVoice" overrides the phonemiser voice in the model's
// metadata: Kokoro's metadata says "en-us" for all its speakers, British
// ones included.
//
// A directory without a manifest, an installer's ".install-*" directory,
// or a manifest that is invalid or names a missing file is skipped, so one
// broken install never hides the others.
class VoiceRegistry
{
public:
    // The per-user voices directory: voices/ under the application's local
    // data location (not the roaming one on Windows; voices are large).
    static QString defaultDirectory();

    // Reads the voices in directory. Call refresh() to read them again.
    explicit VoiceRegistry(QString directory);

    // Rereads the directory, for example after an install.
    void refresh();

    // The installed voices, sorted by display name.
    const QList<Voice> &voices() const;

    std::optional<Voice> find(const QString &id) const;

private:
    QString m_directory;
    QList<Voice> m_voices;
};

} // namespace legilo
