#include "core/tts/SherpaTtsEngine.h"

#include "core/Logging.h"
#include "core/tts/Phonemiser.h"

#include <algorithm>
#include <sherpa-onnx/c-api/cxx-api.h>

using namespace Qt::StringLiterals;

namespace legilo {

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

    auto tts = sherpa_onnx::cxx::OfflineTts::Create(config);
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
