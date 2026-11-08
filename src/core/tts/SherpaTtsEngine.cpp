#include "core/tts/SherpaTtsEngine.h"

#include "core/Logging.h"
#include "core/tts/Phonemiser.h"

#include <algorithm>
#include <iostream>
#include <sherpa-onnx/c-api/cxx-api.h>
#include <sstream>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

// Collects what is written to std::cerr until release(). Debian's ONNX
// Runtime registers ONNX's operator schemas again when its environment
// starts, and libonnx prints each one it already has, about 570 lines,
// to std::cerr. That happens once per process, inside the first
// OfflineTts::Create. Another thread writing to std::cerr meanwhile is
// caught too, and passed on by reportCaptured().
class CerrCapture
{
public:
    CerrCapture()
        : m_previous(std::cerr.rdbuf(m_buffer.rdbuf()))
    {}
    ~CerrCapture() { release(); }
    CerrCapture(const CerrCapture &) = delete;
    CerrCapture &operator=(const CerrCapture &) = delete;
    CerrCapture(CerrCapture &&) = delete;
    CerrCapture &operator=(CerrCapture &&) = delete;

    // Gives std::cerr back and returns what it collected.
    std::string release()
    {
        if (m_previous != nullptr) {
            std::cerr.rdbuf(m_previous);
            m_previous = nullptr;
        }
        return m_buffer.str();
    }

private:
    std::ostringstream m_buffer;
    std::streambuf *m_previous;
};

// Logs captured output: the duplicate schemas as one debug line, anything
// else as a warning, so a real error is not lost.
void reportCaptured(const std::string &captured)
{
    int duplicates = 0;
    std::istringstream lines(captured);
    for (std::string line; std::getline(lines, line);) {
        if (line.starts_with("Schema error: Trying to register schema")) {
            ++duplicates;
        } else if (!line.empty()) {
            qCWarning(lcTts).noquote() << QString::fromStdString(line);
        }
    }
    if (duplicates > 0) {
        qCDebug(lcTts) << "Ignored" << duplicates << "duplicate ONNX schema registrations";
    }
}

} // namespace

struct SherpaTtsEngine::Private
{
    sherpa_onnx::cxx::OfflineTts tts;
    QString espeakVoice;
    int speakerId = 0;
};

std::unique_ptr<SherpaTtsEngine> SherpaTtsEngine::load(const Voice &voice, int threads)
{
    sherpa_onnx::cxx::OfflineTtsConfig config;
    config.model.num_threads = std::max(threads, 1);
    // Legilo segments the text itself and passes one segment at a time.
    config.max_num_sentences = 1;
    if (voice.engine == u"piper") {
        config.model.vits.model = voice.modelPath.toStdString();
        config.model.vits.tokens = voice.tokensPath.toStdString();
    } else if (voice.engine == u"kokoro") {
        config.model.kokoro.model = voice.modelPath.toStdString();
        config.model.kokoro.voices = voice.voicesPath.toStdString();
        config.model.kokoro.tokens = voice.tokensPath.toStdString();
    } else {
        qCWarning(lcTts) << "Cannot load voice" << voice.id << "- unknown engine" << voice.engine;
        return nullptr;
    }

    std::string captured;
    auto tts = [&] {
        CerrCapture capture;
        auto created = sherpa_onnx::cxx::OfflineTts::Create(config);
        captured = capture.release();
        return created;
    }();
    reportCaptured(captured);
    if (tts.Get() == nullptr) {
        qCWarning(lcTts) << "Cannot load voice" << voice.id << "from" << voice.directory;
        return nullptr;
    }
    // The model's metadata names the espeak-ng voice it was trained with,
    // unless the speaker overrides it (Kokoro's British speakers).
    QString espeakVoice = voice.espeakVoice;
    if (espeakVoice.isEmpty()) {
        espeakVoice = QString::fromStdString(tts.Lang());
    }
    if (espeakVoice.isEmpty()) {
        qCWarning(lcTts) << "Cannot load voice" << voice.id
                         << "- its model names no espeak-ng voice";
        return nullptr;
    }

    auto d = std::make_unique<Private>(Private{std::move(tts), espeakVoice, voice.speakerId});
    return std::unique_ptr<SherpaTtsEngine>(new SherpaTtsEngine(std::move(d)));
}

int SherpaTtsEngine::threadsFor(int setting, int logicalCores)
{
    if (setting > 0) {
        return setting;
    }
    return std::clamp(logicalCores / 2, 1, 4);
}

SherpaTtsEngine::SherpaTtsEngine(std::unique_ptr<Private> d)
    : d(std::move(d))
{}

SherpaTtsEngine::~SherpaTtsEngine() = default;

QString SherpaTtsEngine::espeakVoice() const
{
    return d->espeakVoice;
}

int SherpaTtsEngine::sampleRate() const
{
    return d->tts.SampleRate();
}

std::optional<std::vector<float>> SherpaTtsEngine::synthesise(const QString &text, double speed)
{
    const auto sentences = Phonemiser::instance().phonemise(text, d->espeakVoice);
    if (!sentences) {
        return std::nullopt;
    }
    // sherpa-onnx falls back to reading text, which it can no longer do,
    // when given no phonemes.
    if (sentences->empty()) {
        return std::vector<float>{};
    }

    sherpa_onnx::cxx::GenerationConfig config;
    config.sid = d->speakerId;
    config.speed = static_cast<float>(speed);
    for (const std::u32string &sentence : *sentences) {
        config.phoneme_codepoints.emplace_back(sentence.begin(), sentence.end());
    }
    auto audio = d->tts.Generate(std::string(), config);
    if (audio.samples.empty()) {
        qCWarning(lcTts) << "Synthesis failed for a text of" << text.size() << "characters";
        return std::nullopt;
    }
    return std::move(audio.samples);
}

} // namespace legilo
