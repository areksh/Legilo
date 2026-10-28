#include "core/tts/Phonemiser.h"

#include "core/Logging.h"

#include <QMutexLocker>

#include <espeak-ng/speak_lib.h>

namespace legilo {

namespace {

// Clause terminators reported by espeak_TextToPhonemesWithTerminator. They
// live in espeak-ng's private translate.h, so they are repeated here (from
// espeak-ng 1.52). The low bits hold the pause length, then the intonation,
// then the clause type.
constexpr int ClauseIntonationFullStop = 0x00000000;
constexpr int ClauseIntonationComma = 0x00001000;
constexpr int ClauseIntonationQuestion = 0x00002000;
constexpr int ClauseIntonationExclamation = 0x00003000;
constexpr int ClauseTypeClause = 0x00040000;
constexpr int ClauseTypeSentence = 0x00080000;

constexpr int ClausePeriod = 40 | ClauseIntonationFullStop | ClauseTypeSentence;
constexpr int ClauseComma = 20 | ClauseIntonationComma | ClauseTypeClause;
constexpr int ClauseQuestion = 40 | ClauseIntonationQuestion | ClauseTypeSentence;
constexpr int ClauseExclamation = 45 | ClauseIntonationExclamation | ClauseTypeSentence;
constexpr int ClauseColon = 30 | ClauseIntonationFullStop | ClauseTypeClause;
constexpr int ClauseSemicolon = 30 | ClauseIntonationComma | ClauseTypeClause;

constexpr int ClausePunctuationMask = 0x000FFFFF;

// Appends a clause's phonemes, leaving out the "(fr)" style flags espeak-ng
// puts around words it reads with another language's rules.
void appendWithoutLanguageFlags(std::u32string &sentence, const std::u32string &clause)
{
    bool inLanguageFlag = false;
    for (const char32_t c : clause) {
        if (inLanguageFlag) {
            inLanguageFlag = c != U')';
        } else if (c == U'(') {
            inLanguageFlag = true;
        } else {
            sentence.push_back(c);
        }
    }
}

void appendPunctuation(std::u32string &sentence, int terminator)
{
    switch (terminator & ClausePunctuationMask) {
    case ClausePeriod:
        sentence += U'.';
        break;
    case ClauseQuestion:
        sentence += U'?';
        break;
    case ClauseExclamation:
        sentence += U'!';
        break;
    case ClauseComma:
        sentence += U", ";
        break;
    case ClauseColon:
        sentence += U": ";
        break;
    case ClauseSemicolon:
        sentence += U"; ";
        break;
    default:
        break;
    }
}

} // namespace

Phonemiser &Phonemiser::instance()
{
    static Phonemiser phonemiser;
    return phonemiser;
}

void Phonemiser::setDataPath(const QString &path)
{
    const QMutexLocker locker(&m_mutex);
    m_dataPath = path;
}

bool Phonemiser::ensureInitialised()
{
    if (m_initialised) {
        return true;
    }
    const QByteArray path = m_dataPath.toLocal8Bit();
    const int sampleRate =
        espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, path.isEmpty() ? nullptr : path.constData(),
                          espeakINITIALIZE_DONT_EXIT);
    if (sampleRate < 0) {
        qCWarning(lcTts) << "espeak-ng failed to start, data path:"
                         << (m_dataPath.isEmpty() ? QStringLiteral("(built-in)") : m_dataPath);
        return false;
    }
    m_initialised = true;
    return true;
}

// A port of piper-phonemize's phonemize_eSpeak, so the voices hear the same
// phonemes they were trained on. espeak-ng hands back one clause at a time
// with the punctuation that ended it; piper puts that punctuation back as a
// character and starts a new sentence after a sentence-ending clause.
std::optional<std::vector<std::u32string>> Phonemiser::phonemise(const QString &text,
                                                                 const QString &voice)
{
    const QMutexLocker locker(&m_mutex);
    if (!ensureInitialised()) {
        return std::nullopt;
    }
    if (espeak_SetVoiceByName(voice.toUtf8().constData()) != EE_OK) {
        qCWarning(lcTts) << "espeak-ng does not know the voice" << voice;
        return std::nullopt;
    }

    const QByteArray utf8 = text.toUtf8();
    const void *position = utf8.constData();
    std::vector<std::u32string> sentences;
    std::u32string sentence;

    while (position != nullptr) {
        int terminator = 0;
        const char *clause = espeak_TextToPhonemesWithTerminator(&position, espeakCHARS_AUTO,
                                                                 espeakPHONEMES_IPA, &terminator);
        // Decomposed, so that a letter with a diacritic arrives as the
        // separate codepoints the voices' tokens are made of.
        const QString phonemes = QString::fromUtf8(clause).normalized(QString::NormalizationForm_D);
        appendWithoutLanguageFlags(sentence, phonemes.toStdU32String());
        appendPunctuation(sentence, terminator);

        if ((terminator & ClauseTypeSentence) == ClauseTypeSentence) {
            sentences.push_back(std::move(sentence));
            sentence.clear();
        }
    }
    sentences.push_back(std::move(sentence));

    // Unlike piper, which returns an empty sentence for empty text: the
    // engine rejects sentences without phonemes.
    std::erase_if(sentences, [](const std::u32string &s) { return s.empty(); });
    return sentences;
}

} // namespace legilo
