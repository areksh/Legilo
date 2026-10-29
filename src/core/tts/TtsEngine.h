#pragma once

#include <QString>

#include <optional>
#include <vector>

namespace legilo {

// Speech synthesis with one loaded voice. The playback controller and the
// exporter talk to this interface rather than to sherpa-onnx, so their
// tests can use a fake that returns tone bursts of known length.
class TtsEngine
{
public:
    TtsEngine() = default;
    TtsEngine(const TtsEngine &) = delete;
    TtsEngine &operator=(const TtsEngine &) = delete;
    virtual ~TtsEngine() = default;

    // The rate of the samples synthesise() returns, in hertz.
    virtual int sampleRate() const = 0;

    // Synthesises text, normally one segment or one chunk of it, at speed
    // (1.0 is normal, larger is faster). How much faster depends on the
    // model: a Piper voice at 2.0 takes about two thirds as long, not half,
    // because each sound keeps at least one frame. Returns mono samples in the
    // range -1 to 1, or no samples for text with nothing to say, such as
    // whitespace. Returns nothing if synthesis fails. Blocks until done; a
    // call cannot be interrupted part-way.
    virtual std::optional<std::vector<float>> synthesise(const QString &text, double speed) = 0;
};

} // namespace legilo
