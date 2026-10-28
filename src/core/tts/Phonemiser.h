#pragma once

#include <QMutex>
#include <QString>

#include <optional>
#include <string>
#include <vector>

namespace legilo {

// Turns text into IPA phonemes with espeak-ng, the way piper-phonemize
// does: the Piper voices were trained on its output, and sherpa-onnx 2.x
// takes phonemes rather than text. Each sentence becomes one string of
// codepoints, in the shape of sherpa-onnx's
// GenerationConfig::phoneme_codepoints.
//
// espeak-ng keeps global state, so there is one instance per process and
// its calls are serialised.
class Phonemiser
{
public:
    static Phonemiser &instance();

    Phonemiser(const Phonemiser &) = delete;
    Phonemiser &operator=(const Phonemiser &) = delete;

    // Sets where espeak-ng's data lives. An empty path, the default, uses
    // the location espeak-ng was built with. Has no effect once the first
    // text has been phonemised.
    void setDataPath(const QString &path);

    // Returns the phonemes of each sentence of text, as read by the
    // espeak-ng voice (for example "en" for British English or "en-us").
    // Sentences without phonemes are left out. Returns nothing if
    // espeak-ng cannot start or does not know the voice.
    std::optional<std::vector<std::u32string>> phonemise(const QString &text, const QString &voice);

private:
    Phonemiser() = default;

    bool ensureInitialised();

    QMutex m_mutex;
    QString m_dataPath;
    bool m_initialised = false;
};

} // namespace legilo
