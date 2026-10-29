#include "core/models/ModelInstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

#include <archive.h>
#include <archive_entry.h>
#include <map>
#include <memory>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

const QString EspeakData = u"espeak-ng-data"_s;
constexpr size_t ReadBlockSize = size_t{64} * 1024;
const QString ManifestName = u"legilo-voice.json"_s;

struct ArchiveDeleter
{
    void operator()(archive *a) const { archive_read_free(a); }
};
using ArchivePtr = std::unique_ptr<archive, ArchiveDeleter>;

// Splits an archive path into its components, or returns nothing if it is
// unsafe: absolute, climbing out with "..", or using a backslash or colon,
// which Windows would read as a separator or a drive. "." components are
// dropped, so "./" yields no components at all.
std::optional<QStringList> safeComponents(const QString &path)
{
    if (path.startsWith(u'/') || path.contains(u'\\') || path.contains(u':')) {
        return std::nullopt;
    }
    QStringList components;
    for (const QString &component : path.split(u'/', Qt::SkipEmptyParts)) {
        if (component == u"..") {
            return std::nullopt;
        }
        if (component != u".") {
            components.append(component);
        }
    }
    return components;
}

// Writes the entry's data to path, adding its size to written. Returns an
// error, or an empty string on success.
QString writeFile(archive *a, const QString &path, qint64 &written, qint64 maxBytes)
{
    if (!QDir().mkpath(QFileInfo(path).path())) {
        return u"cannot create a directory for %1"_s.arg(path);
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return u"cannot write %1: %2"_s.arg(path, file.errorString());
    }
    const void *block = nullptr;
    size_t size = 0;
    la_int64_t offset = 0;
    int status = ARCHIVE_OK;
    while ((status = archive_read_data_block(a, &block, &size, &offset)) == ARCHIVE_OK) {
        written += static_cast<qint64>(size);
        if (written > maxBytes) {
            return u"unpacks to more than %1 bytes"_s.arg(maxBytes);
        }
        if (file.write(static_cast<const char *>(block), static_cast<qint64>(size)) !=
            static_cast<qint64>(size)) {
            return u"cannot write %1: %2"_s.arg(path, file.errorString());
        }
    }
    if (status != ARCHIVE_EOF) {
        return u"is damaged: %1"_s.arg(QString::fromLocal8Bit(archive_error_string(a)));
    }
    return {};
}

// Unpacks the regular files and directories of the tar archive at
// archivePath into destination, skipping espeak-ng's data. Returns an
// error, or an empty string on success.
QString unpack(const QString &archivePath, const QString &destination, qint64 maxBytes)
{
    const ArchivePtr a(archive_read_new());
    archive_read_support_filter_all(a.get());
    archive_read_support_format_tar(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(archivePath).constData(),
                                   ReadBlockSize) != ARCHIVE_OK) {
        return u"cannot be read: %1"_s.arg(QString::fromLocal8Bit(archive_error_string(a.get())));
    }

    qint64 written = 0;
    archive_entry *entry = nullptr;
    int status = ARCHIVE_OK;
    while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK) {
        const char *utf8 = archive_entry_pathname_utf8(entry);
        const QString path = utf8 != nullptr
                                 ? QString::fromUtf8(utf8)
                                 : QString::fromLocal8Bit(archive_entry_pathname(entry));
        const auto components = safeComponents(path);
        if (!components) {
            return u"contains an unsafe path: %1"_s.arg(path);
        }
        const auto type = archive_entry_filetype(entry);
        if (archive_entry_hardlink(entry) != nullptr || (type != AE_IFREG && type != AE_IFDIR)) {
            return u"contains an entry that is not a file or directory: %1"_s.arg(path);
        }
        if (components->isEmpty() || components->contains(EspeakData)) {
            continue;
        }
        const QString target = destination + u'/' + components->join(u'/');
        if (type == AE_IFDIR) {
            if (!QDir().mkpath(target)) {
                return u"cannot create %1"_s.arg(target);
            }
            continue;
        }
        if (QString error = writeFile(a.get(), target, written, maxBytes); !error.isEmpty()) {
            return error;
        }
    }
    if (status != ARCHIVE_EOF) {
        return u"is damaged: %1"_s.arg(QString::fromLocal8Bit(archive_error_string(a.get())));
    }
    return {};
}

// The model ID for a directory or archive name: without the archive
// extension, and without sherpa-onnx's "vits-" prefix on Piper models.
QString modelIdFor(QString name)
{
    static const QRegularExpression extension(u"\\.(tar\\.(bz2|gz|xz)|tbz2|tgz|tar)$"_s);
    name.remove(extension);
    if (name.startsWith(u"vits-piper-")) {
        name.remove(0, 5);
    }
    return name;
}

// IDs become directory names and appear in voice IDs before a colon.
bool fitsDirectoryName(const QString &id)
{
    static const QRegularExpression pattern(u"^[A-Za-z0-9][A-Za-z0-9._-]*$"_s);
    return pattern.match(id).hasMatch();
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

// Builds the manifest for the model in directory, or returns an empty
// object if it holds no Piper or Kokoro model.
QJsonObject manifestFor(const QString &directory, const QString &id)
{
    const QDir dir(directory);
    QJsonObject manifest{{u"format"_s, 1}, {u"id"_s, id}, {u"licence"_s, u"unknown"_s}};
    if (!dir.exists(u"tokens.txt"_s)) {
        return {};
    }

    if (dir.exists(u"model.onnx"_s) && dir.exists(u"voices.bin"_s)) {
        manifest.insert(u"engine"_s, u"kokoro"_s);
        manifest.insert(u"name"_s, id);
        manifest.insert(u"files"_s, QJsonObject{{u"model"_s, u"model.onnx"_s},
                                                {u"tokens"_s, u"tokens.txt"_s},
                                                {u"voices"_s, u"voices.bin"_s}});
        return manifest;
    }

    // Piper: one model with its configuration beside it.
    const QStringList models = dir.entryList({u"*.onnx"_s}, QDir::Files);
    if (models.size() != 1 || !dir.exists(models.first() + u".json"_s)) {
        return {};
    }
    const QJsonObject config = readJson(dir.filePath(models.first() + u".json"_s));
    const QString dataset = config.value(u"dataset").toString();
    const QString code = config.value(u"language").toObject().value(u"code").toString();
    const QString quality = config.value(u"audio").toObject().value(u"quality").toString();
    QString name = id;
    if (!dataset.isEmpty()) {
        name = dataset.first(1).toUpper() + dataset.sliced(1);
        const QStringList details = QStringList{code, quality}.filter(QRegularExpression(u"."_s));
        if (!details.isEmpty()) {
            name += u" ("_s + details.join(u", "_s) + u')';
        }
    }
    manifest.insert(u"engine"_s, u"piper"_s);
    manifest.insert(u"name"_s, name);
    if (!code.isEmpty()) {
        manifest.insert(u"language"_s, QString(code).replace(u'_', u'-'));
    }
    manifest.insert(u"files"_s,
                    QJsonObject{{u"model"_s, models.first()}, {u"tokens"_s, u"tokens.txt"_s}});

    // Multi-speaker models map speaker names to IDs; list them by ID.
    const QJsonObject speakerIds = config.value(u"speaker_id_map").toObject();
    if (speakerIds.size() > 1) {
        std::map<int, QString> byId;
        for (auto it = speakerIds.begin(); it != speakerIds.end(); ++it) {
            byId.emplace(it.value().toInt(), it.key());
        }
        QJsonArray speakers;
        for (const auto &[speakerId, speakerName] : byId) {
            speakers.append(QJsonObject{{u"id"_s, speakerId}, {u"name"_s, speakerName}});
        }
        manifest.insert(u"speakers"_s, speakers);
    }
    return manifest;
}

bool writeManifest(const QString &directory, const QJsonObject &manifest)
{
    QSaveFile file(directory + u'/' + ManifestName);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(manifest).toJson()) > 0 &&
           file.commit();
}

} // namespace

ModelInstaller::ModelInstaller(QString voicesDirectory, qint64 maxBytes)
    : m_voicesDirectory(std::move(voicesDirectory))
    , m_maxBytes(maxBytes)
{}

ModelInstaller::Result ModelInstaller::install(const QString &archivePath) const
{
    // The caller reports the error: the command line prints it, the model
    // manager shows it.
    const auto fail = [&](const QString &reason) {
        return Result{{}, u"%1 %2"_s.arg(QFileInfo(archivePath).fileName(), reason)};
    };
    if (!QFileInfo(archivePath).isFile()) {
        return fail(u"does not exist"_s);
    }
    if (!QDir().mkpath(m_voicesDirectory)) {
        return fail(u"cannot be installed: cannot create %1"_s.arg(m_voicesDirectory));
    }

    // Unpack beside the final location, so the rename into place stays on
    // one file system. The registry ignores ".install-*" directories.
    const QTemporaryDir staging(m_voicesDirectory + u"/.install-XXXXXX"_s);
    if (!staging.isValid()) {
        return fail(u"cannot be installed: %1"_s.arg(staging.errorString()));
    }
    const QString unpacked = staging.filePath(u"unpacked"_s);
    if (const QString error = unpack(archivePath, unpacked, m_maxBytes); !error.isEmpty()) {
        return fail(error);
    }

    // sherpa-onnx's archives hold one directory named after the model.
    QString modelDirectory = unpacked;
    QString id = modelIdFor(QFileInfo(archivePath).fileName());
    const QFileInfoList top =
        QDir(unpacked).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    if (top.size() == 1 && top.first().isDir()) {
        modelDirectory = top.first().filePath();
        id = modelIdFor(top.first().fileName());
    }
    if (!fitsDirectoryName(id)) {
        return fail(u"gives the model an unusable ID: %1"_s.arg(id));
    }

    const QJsonObject manifest = manifestFor(modelDirectory, id);
    if (manifest.isEmpty()) {
        return fail(u"does not contain a Piper or Kokoro model"_s);
    }
    const QString target = m_voicesDirectory + u'/' + id;
    if (QFileInfo::exists(target)) {
        return fail(u"is already installed as %1"_s.arg(id));
    }
    if (!writeManifest(modelDirectory, manifest) || !QDir().rename(modelDirectory, target)) {
        return fail(u"cannot be installed into %1"_s.arg(target));
    }
    return Result{id, {}};
}

} // namespace legilo
