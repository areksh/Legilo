#pragma once

#include "core/tts/TtsEngine.h"
#include "core/tts/Voice.h"

#include <memory>

namespace legilo {

// A TtsEngine on sherpa-onnx, for Piper and Kokoro voices. Text is
// phonemised by the Phonemiser with the speaker's espeakVoice, or else the
// espeak-ng voice in the model's metadata, and sherpa-onnx turns the
// phonemes into audio. Each instance holds one loaded model.
class SherpaTtsEngine final : public TtsEngine
{
public:
    // Loads voice's model, running inference on threads threads. Returns
    // nothing if the model cannot be loaded. Loading takes a second or more.
    static std::unique_ptr<SherpaTtsEngine> load(const Voice &voice, int threads);

    // The number of threads to synthesise with: the voice/threads setting
    // if it is positive, otherwise half the logical cores, from 1 to 4.
    // More rarely helps (stage 0 results in the design document).
    static int threadsFor(int setting, int logicalCores);

    ~SherpaTtsEngine() override;

    // The espeak-ng voice the text is phonemised with, such as "en".
    QString espeakVoice() const;

    int sampleRate() const override;
    std::optional<std::vector<float>> synthesise(const QString &text, double speed) override;

private:
    struct Private;
    explicit SherpaTtsEngine(std::unique_ptr<Private> d);

    std::unique_ptr<Private> d;
};

} // namespace legilo
