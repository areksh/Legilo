#include "core/models/ModelInstaller.h"
#include "core/tts/VoiceRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

using legilo::ModelInstaller;
using legilo::Voice;
using legilo::VoiceRegistry;
using namespace Qt::StringLiterals;

namespace {

struct Entry
{
    enum Kind
    {
        File,
        Directory,
        Symlink,
        Hardlink,
        Fifo
    };

    QString path;
    QByteArray data;
    Kind kind = File;
    QString target; // for links
};

enum class Compression
{
    Bzip2, // as sherpa-onnx publishes models
    Gzip   // decoded as a stream, so damage shows part-way through
};

// Writes entries to a compressed tar archive.
void writeArchive(const QString &fileName, const QList<Entry> &entries,
                  Compression compression = Compression::Bzip2)
{
    archive *a = archive_write_new();
    if (compression == Compression::Bzip2) {
        archive_write_add_filter_bzip2(a);
    } else {
        archive_write_add_filter_gzip(a);
    }
    archive_write_set_format_pax_restricted(a);
    QCOMPARE(archive_write_open_filename(a, QFile::encodeName(fileName).constData()), ARCHIVE_OK);
    for (const Entry &entry : entries) {
        archive_entry *e = archive_entry_new();
        archive_entry_set_pathname(e, entry.path.toUtf8().constData());
        switch (entry.kind) {
        case Entry::File:
            archive_entry_set_filetype(e, AE_IFREG);
            archive_entry_set_perm(e, 0644);
            archive_entry_set_size(e, entry.data.size());
            break;
        case Entry::Directory:
            archive_entry_set_filetype(e, AE_IFDIR);
            archive_entry_set_perm(e, 0755);
            break;
        case Entry::Symlink:
            archive_entry_set_filetype(e, AE_IFLNK);
            archive_entry_set_symlink(e, entry.target.toUtf8().constData());
            break;
        case Entry::Hardlink:
            archive_entry_set_filetype(e, AE_IFREG);
            archive_entry_set_hardlink(e, entry.target.toUtf8().constData());
            break;
        case Entry::Fifo:
            archive_entry_set_filetype(e, AE_IFIFO);
            archive_entry_set_perm(e, 0644);
            break;
        }
        QCOMPARE(archive_write_header(a, e), ARCHIVE_OK);
        if (entry.kind == Entry::File) {
            archive_write_data(a, entry.data.constData(), entry.data.size());
        }
        archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
}

QByteArray piperConfig(const QJsonObject &speakerIdMap = {})
{
    return QJsonDocument(QJsonObject{
                             {u"dataset"_s, u"cori"_s},
                             {u"language"_s, QJsonObject{{u"code"_s, u"en_GB"_s}}},
                             {u"audio"_s,
                              QJsonObject{{u"quality"_s, u"medium"_s}, {u"sample_rate"_s, 22050}}},
                             {u"espeak"_s, QJsonObject{{u"voice"_s, u"en"_s}}},
                             {u"num_speakers"_s, std::max<qsizetype>(speakerIdMap.size(), 1)},
                             {u"speaker_id_map"_s, speakerIdMap},
                         })
        .toJson();
}

// The layout of sherpa-onnx's Piper archives, under prefix.
QList<Entry> piperEntries(const QString &prefix = u"vits-piper-en_GB-cori-medium/"_s,
                          const QByteArray &config = piperConfig())
{
    QList<Entry> entries;
    if (!prefix.isEmpty()) {
        entries.append({prefix, {}, Entry::Directory, {}});
    }
    entries.append({prefix + u"en_GB-cori-medium.onnx"_s, "model", Entry::File, {}});
    entries.append({prefix + u"en_GB-cori-medium.onnx.json"_s, config, Entry::File, {}});
    entries.append({prefix + u"tokens.txt"_s, "_ 0\n", Entry::File, {}});
    entries.append({prefix + u"MODEL_CARD"_s, "card", Entry::File, {}});
    entries.append({prefix + u"espeak-ng-data/"_s, {}, Entry::Directory, {}});
    entries.append({prefix + u"espeak-ng-data/phontab"_s, "data", Entry::File, {}});
    return entries;
}

QList<Entry> kokoroEntries()
{
    return {
        {u"kokoro-en-v0_19/"_s, {}, Entry::Directory, {}},
        {u"kokoro-en-v0_19/model.onnx"_s, "model", Entry::File, {}},
        {u"kokoro-en-v0_19/voices.bin"_s, "voices", Entry::File, {}},
        {u"kokoro-en-v0_19/tokens.txt"_s, "_ 0\n", Entry::File, {}},
        {u"kokoro-en-v0_19/espeak-ng-data/phontab"_s, "data", Entry::File, {}},
    };
}

QStringList idsOf(const QList<Voice> &voices)
{
    QStringList ids;
    for (const Voice &voice : voices) {
        ids.append(voice.id);
    }
    return ids;
}

} // namespace

Q_DECLARE_METATYPE(Entry)

class TestModelInstaller : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void installsPiperModel();
    void installsFileContents();
    void installsEachSpeakerOfPiperModel();
    void installsKokoroModel();
    void skipsEspeakData();
    void namesModelAfterArchiveWithoutTopDirectory();
    void createsVoicesDirectory();
    void refusesUnsafeEntry_data();
    void refusesUnsafeEntry();
    void refusesMissingArchive();
    void refusesFileThatIsNotArchive();
    void refusesTruncatedArchive();
    void refusesArchiveOverSizeLimit();
    void refusesArchiveWithoutModel();
    void refusesModelIdUnfitForDirectoryName();
    void refusesModelAlreadyInstalled();

private:
    QString archivePath(const QString &name = u"vits-piper-en_GB-cori-medium.tar.bz2"_s) const;
    QString voicesPath() const;
    QStringList voicesDirectoryEntries() const;

    std::unique_ptr<QTemporaryDir> m_root;
};

void TestModelInstaller::init()
{
    m_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_root->isValid());
    QVERIFY(QDir(m_root->path()).mkdir(u"voices"_s));
}

QString TestModelInstaller::archivePath(const QString &name) const
{
    return m_root->filePath(name);
}

QString TestModelInstaller::voicesPath() const
{
    return m_root->filePath(u"voices"_s);
}

QStringList TestModelInstaller::voicesDirectoryEntries() const
{
    return QDir(voicesPath()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
}

void TestModelInstaller::installsPiperModel()
{
    writeArchive(archivePath(), piperEntries());

    const auto result = ModelInstaller(voicesPath()).install(archivePath());

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.modelId, u"piper-en_GB-cori-medium"_s);
    QCOMPARE(voicesDirectoryEntries(), QStringList{u"piper-en_GB-cori-medium"_s});
    const QString directory = voicesPath() + u"/piper-en_GB-cori-medium"_s;
    Voice expected;
    expected.id = u"piper-en_GB-cori-medium"_s;
    expected.modelId = expected.id;
    expected.engine = u"piper"_s;
    expected.displayName = u"Cori (en_GB, medium)"_s;
    expected.language = u"en-GB"_s;
    expected.directory = directory;
    expected.modelPath = directory + u"/en_GB-cori-medium.onnx"_s;
    expected.tokensPath = directory + u"/tokens.txt"_s;
    expected.licence = u"unknown"_s;
    QCOMPARE(VoiceRegistry(voicesPath()).voices(), QList<Voice>{expected});
}

void TestModelInstaller::installsFileContents()
{
    writeArchive(archivePath(), piperEntries());

    QVERIFY(ModelInstaller(voicesPath()).install(archivePath()).ok());

    QFile model(voicesPath() + u"/piper-en_GB-cori-medium/en_GB-cori-medium.onnx"_s);
    QVERIFY(model.open(QIODevice::ReadOnly));
    QCOMPARE(model.readAll(), QByteArray("model"));
}

void TestModelInstaller::installsEachSpeakerOfPiperModel()
{
    const QJsonObject speakers{{u"p226"_s, 1}, {u"p225"_s, 0}};
    writeArchive(archivePath(),
                 piperEntries(u"vits-piper-en_GB-vctk-medium/"_s, piperConfig(speakers)));

    const auto result = ModelInstaller(voicesPath()).install(archivePath());

    QVERIFY2(result.ok(), qPrintable(result.error));
    const VoiceRegistry registry(voicesPath());
    QCOMPARE(idsOf(registry.voices()),
             (QStringList{u"piper-en_GB-vctk-medium:p225"_s, u"piper-en_GB-vctk-medium:p226"_s}));
    QCOMPARE(registry.voices().at(1).speakerId, 1);
}

void TestModelInstaller::installsKokoroModel()
{
    writeArchive(archivePath(u"kokoro-en-v0_19.tar.bz2"_s), kokoroEntries());

    const auto result =
        ModelInstaller(voicesPath()).install(archivePath(u"kokoro-en-v0_19.tar.bz2"_s));

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.modelId, u"kokoro-en-v0_19"_s);
    const VoiceRegistry registry(voicesPath());
    QCOMPARE(registry.voices().size(), 1);
    const Voice &voice = registry.voices().first();
    QCOMPARE(voice.engine, u"kokoro"_s);
    QCOMPARE(voice.voicesPath, voicesPath() + u"/kokoro-en-v0_19/voices.bin"_s);
}

void TestModelInstaller::skipsEspeakData()
{
    writeArchive(archivePath(), piperEntries());

    QVERIFY(ModelInstaller(voicesPath()).install(archivePath()).ok());

    QVERIFY(!QFileInfo::exists(voicesPath() + u"/piper-en_GB-cori-medium/espeak-ng-data"_s));
    QVERIFY(QFileInfo::exists(voicesPath() + u"/piper-en_GB-cori-medium/MODEL_CARD"_s));
}

void TestModelInstaller::namesModelAfterArchiveWithoutTopDirectory()
{
    const QString archive = archivePath(u"vits-piper-en_US-amy-low.tar.bz2"_s);
    writeArchive(archive, piperEntries(QString()));

    const auto result = ModelInstaller(voicesPath()).install(archive);

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.modelId, u"piper-en_US-amy-low"_s);
}

void TestModelInstaller::createsVoicesDirectory()
{
    writeArchive(archivePath(), piperEntries());
    const QString voices = m_root->filePath(u"new/voices"_s);

    const auto result = ModelInstaller(voices).install(archivePath());

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(VoiceRegistry(voices).voices().size(), 1);
}

void TestModelInstaller::refusesUnsafeEntry_data()
{
    QTest::addColumn<Entry>("entry");
    QTest::addColumn<QString>("escapedTo"); // a path that must not appear

    const QString prefix = u"vits-piper-en_GB-cori-medium/"_s;
    QTest::newRow("parent directory")
        << Entry{u"../evil.txt"_s, "evil", Entry::File, {}} << u"evil.txt"_s;
    QTest::newRow("climbs out of model directory")
        << Entry{prefix + u"../../evil.txt"_s, "evil", Entry::File, {}} << u"evil.txt"_s;
    QTest::newRow("absolute path")
        << Entry{u"/tmp/legilo-evil.txt"_s, "evil", Entry::File, {}} << QString();
    QTest::newRow("symbolic link")
        << Entry{prefix + u"link"_s, {}, Entry::Symlink, u"/etc/passwd"_s} << QString();
    QTest::newRow("hard link")
        << Entry{prefix + u"link"_s, {}, Entry::Hardlink, prefix + u"tokens.txt"_s} << QString();
    QTest::newRow("device") << Entry{prefix + u"fifo"_s, {}, Entry::Fifo, {}} << QString();
}

// The bad entry comes after the model's files, so the install has already
// written some of them when it fails, and must clean them up.
void TestModelInstaller::refusesUnsafeEntry()
{
    QFETCH(Entry, entry);
    QFETCH(QString, escapedTo);
    writeArchive(archivePath(), piperEntries() << entry);

    const auto result = ModelInstaller(voicesPath()).install(archivePath());

    QVERIFY(!result.ok());
    QVERIFY(result.modelId.isEmpty());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
    if (!escapedTo.isEmpty()) {
        QVERIFY(!QFileInfo::exists(m_root->filePath(escapedTo)));
    }
    QVERIFY(!QFileInfo::exists(u"/tmp/legilo-evil.txt"_s));
}

void TestModelInstaller::refusesMissingArchive()
{
    const auto result = ModelInstaller(voicesPath()).install(archivePath(u"absent.tar.bz2"_s));

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
}

void TestModelInstaller::refusesFileThatIsNotArchive()
{
    QFile file(archivePath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(4096, 'x'));
    file.close();

    const auto result = ModelInstaller(voicesPath()).install(archivePath());

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
}

void TestModelInstaller::refusesTruncatedArchive()
{
    QList<Entry> entries = piperEntries();
    // A large entry puts the cut well past libarchive's 64 KiB read-ahead,
    // so the damage shows part-way through unpacking, after files have
    // been written, rather than when the archive is opened.
    QByteArray noise(qsizetype{1024} * 1024, Qt::Uninitialized);
    for (qsizetype i = 0; i < noise.size(); ++i) {
        noise[i] = static_cast<char>((i * 7919 + i / 13) % 251);
    }
    entries.insert(2, Entry{u"vits-piper-en_GB-cori-medium/big"_s, noise, Entry::File, {}});
    const QString archive = archivePath(u"vits-piper-en_GB-cori-medium.tar.gz"_s);
    writeArchive(archive, entries, Compression::Gzip);
    QFile file(archive);
    QVERIFY(file.resize(file.size() / 2));

    const auto result = ModelInstaller(voicesPath()).install(archive);

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
}

void TestModelInstaller::refusesArchiveOverSizeLimit()
{
    QList<Entry> entries = piperEntries();
    entries.append(
        Entry{u"vits-piper-en_GB-cori-medium/big"_s, QByteArray(2000, 'a'), Entry::File, {}});
    writeArchive(archivePath(), entries);

    const auto result = ModelInstaller(voicesPath(), 1000).install(archivePath());

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
}

void TestModelInstaller::refusesArchiveWithoutModel()
{
    writeArchive(archivePath(), {{u"notes/"_s, {}, Entry::Directory, {}},
                                 {u"notes/readme.txt"_s, "hello", Entry::File, {}}});

    const auto result = ModelInstaller(voicesPath()).install(archivePath());

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
}

void TestModelInstaller::refusesModelIdUnfitForDirectoryName()
{
    writeArchive(archivePath(), piperEntries(u"vits-piper cori/"_s));

    const auto result = ModelInstaller(voicesPath()).install(archivePath());

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList());
}

void TestModelInstaller::refusesModelAlreadyInstalled()
{
    writeArchive(archivePath(), piperEntries());
    const ModelInstaller installer(voicesPath());
    QVERIFY(installer.install(archivePath()).ok());
    const QString model = voicesPath() + u"/piper-en_GB-cori-medium/en_GB-cori-medium.onnx"_s;
    const QDateTime installed = QFileInfo(model).lastModified();

    const auto result = installer.install(archivePath());

    QVERIFY(!result.ok());
    QCOMPARE(voicesDirectoryEntries(), QStringList{u"piper-en_GB-cori-medium"_s});
    QCOMPARE(QFileInfo(model).lastModified(), installed);
}

QTEST_GUILESS_MAIN(TestModelInstaller)
#include "tst_modelinstaller.moc"
