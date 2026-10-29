#include "core/tts/VoiceRegistry.h"

#include "core/Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr int ManifestFormat = 1;
const QString ManifestName = u"legilo-voice.json"_s;
const QString InstallerPrefix = u".install-"_s;

struct Speaker
{
    int id = 0;
    QString name;
    QString espeakVoice;
};

// Each helper below reads part of a manifest into the voice that every
// speaker of the model starts from. On failure it returns false or nothing
// and says why in reason.

bool readModel(const QJsonObject &manifest, Voice &model, QString &reason)
{
    if (manifest.value(u"format").toInt(-1) != ManifestFormat) {
        reason = u"has an unsupported format"_s;
        return false;
    }
    model.modelId = manifest.value(u"id").toString();
    model.engine = manifest.value(u"engine").toString();
    model.displayName = manifest.value(u"name").toString();
    // The colon separates a model's ID from a speaker's in voice IDs.
    if (model.modelId.isEmpty() || model.modelId.contains(u':')) {
        reason = u"has no valid id"_s;
        return false;
    }
    if (model.engine != u"piper" && model.engine != u"kokoro") {
        reason = u"has an unknown engine"_s;
        return false;
    }
    if (model.displayName.isEmpty()) {
        reason = u"has no name"_s;
        return false;
    }
    model.language = manifest.value(u"language").toString();
    model.licence = manifest.value(u"licence").toString(u"unknown"_s);
    model.licenceUrl = QUrl(manifest.value(u"licenceUrl").toString());
    return true;
}

// Returns the absolute path of an existing file inside directory, or an
// empty string if relative is empty, absolute, escapes it or is missing.
QString fileInside(const QString &directory, const QString &relative)
{
    if (relative.isEmpty() || QDir::isAbsolutePath(relative)) {
        return {};
    }
    QString absolute = QDir::cleanPath(directory + u'/' + relative);
    if (!absolute.startsWith(directory + u'/') || !QFileInfo(absolute).isFile()) {
        return {};
    }
    return absolute;
}

bool readFiles(const QJsonObject &manifest, Voice &model, QString &reason)
{
    const QJsonObject files = manifest.value(u"files").toObject();
    model.modelPath = fileInside(model.directory, files.value(u"model").toString());
    model.tokensPath = fileInside(model.directory, files.value(u"tokens").toString());
    const bool kokoro = model.engine == u"kokoro";
    if (kokoro) {
        model.voicesPath = fileInside(model.directory, files.value(u"voices").toString());
    }
    if (model.modelPath.isEmpty() || model.tokensPath.isEmpty() ||
        (kokoro && model.voicesPath.isEmpty())) {
        reason = u"names a missing or invalid file"_s;
        return false;
    }
    return true;
}

std::optional<QList<Speaker>> readSpeakers(const QJsonObject &manifest, const Voice &model,
                                           QString &reason)
{
    if (!manifest.contains(u"speakers")) {
        return QList<Speaker>{{0, model.displayName, {}}};
    }
    QList<Speaker> speakers;
    for (const auto &value : manifest.value(u"speakers").toArray()) {
        const QJsonObject speaker = value.toObject();
        const int id = speaker.value(u"id").toInt(-1);
        const QString name = speaker.value(u"name").toString();
        const QString espeakVoice = speaker.value(u"espeakVoice").toString();
        if (id < 0 || name.isEmpty() ||
            (speaker.contains(u"espeakVoice") && espeakVoice.isEmpty())) {
            reason = u"has an invalid speaker"_s;
            return std::nullopt;
        }
        speakers.append({id, name, espeakVoice});
    }
    if (speakers.isEmpty()) {
        reason = u"lists no speakers"_s;
        return std::nullopt;
    }
    return speakers;
}

// Reads the manifest in directory into one Voice per speaker.
std::optional<QList<Voice>> readManifest(const QString &directory, QString &reason)
{
    QFile file(directory + u'/' + ManifestName);
    if (!file.open(QIODevice::ReadOnly)) {
        reason = u"cannot be read"_s;
        return std::nullopt;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        reason = u"is not a JSON object"_s;
        return std::nullopt;
    }
    const QJsonObject manifest = document.object();

    Voice model;
    model.directory = directory;
    if (!readModel(manifest, model, reason) || !readFiles(manifest, model, reason)) {
        return std::nullopt;
    }
    const auto speakers = readSpeakers(manifest, model, reason);
    if (!speakers) {
        return std::nullopt;
    }

    QList<Voice> voices;
    const bool several = speakers->size() > 1;
    for (const Speaker &speaker : *speakers) {
        Voice voice = model;
        voice.id = several ? model.modelId + u':' + speaker.name : model.modelId;
        if (several) {
            voice.displayName += u": "_s + speaker.name;
        }
        voice.speakerId = speaker.id;
        voice.espeakVoice = speaker.espeakVoice;
        voices.append(voice);
    }
    return voices;
}

} // namespace

QString VoiceRegistry::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + u"/voices"_s;
}

VoiceRegistry::VoiceRegistry(QString directory)
    : m_directory(std::move(directory))
{
    refresh();
}

void VoiceRegistry::refresh()
{
    m_voices.clear();
    QSet<QString> ids;

    // Name order makes the choice between duplicate IDs repeatable.
    const QDir root(m_directory);
    const QStringList entries =
        root.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const QString &entry : entries) {
        const QString directory = QDir::cleanPath(root.absoluteFilePath(entry));
        if (entry.startsWith(InstallerPrefix) ||
            !QFileInfo::exists(directory + u'/' + ManifestName)) {
            continue;
        }
        QString reason;
        const auto voices = readManifest(directory, reason);
        if (!voices) {
            qCWarning(lcTts) << "Skipping voice in" << directory << "- its manifest" << reason;
            continue;
        }
        for (const Voice &voice : *voices) {
            if (ids.contains(voice.id)) {
                qCWarning(lcTts) << "Skipping duplicate voice" << voice.id << "in" << directory;
                continue;
            }
            ids.insert(voice.id);
            m_voices.append(voice);
        }
    }

    std::ranges::stable_sort(m_voices, [](const Voice &a, const Voice &b) {
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });
}

const QList<Voice> &VoiceRegistry::voices() const
{
    return m_voices;
}

std::optional<Voice> VoiceRegistry::find(const QString &id) const
{
    const auto it = std::ranges::find(m_voices, id, &Voice::id);
    if (it == m_voices.cend()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace legilo
