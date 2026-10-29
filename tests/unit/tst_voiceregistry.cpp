#include "core/tts/VoiceRegistry.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using legilo::Voice;
using legilo::VoiceRegistry;
using namespace Qt::StringLiterals;

namespace {

QJsonObject piperManifest()
{
    return QJsonObject{
        {u"format"_s, 1},
        {u"id"_s, u"piper-en_GB-cori-medium"_s},
        {u"engine"_s, u"piper"_s},
        {u"name"_s, u"Cori (British English, medium)"_s},
        {u"language"_s, u"en-GB"_s},
        {u"files"_s,
         QJsonObject{{u"model"_s, u"en_GB-cori-medium.onnx"_s}, {u"tokens"_s, u"tokens.txt"_s}}},
        {u"licence"_s, u"LicenseRef-public-domain"_s},
        {u"licenceUrl"_s, u"https://example.org/cori"_s},
    };
}

QJsonObject kokoroManifest()
{
    return QJsonObject{
        {u"format"_s, 1},
        {u"id"_s, u"kokoro-en-v0_19"_s},
        {u"engine"_s, u"kokoro"_s},
        {u"name"_s, u"Kokoro (English)"_s},
        {u"files"_s, QJsonObject{{u"model"_s, u"model.onnx"_s},
                                 {u"tokens"_s, u"tokens.txt"_s},
                                 {u"voices"_s, u"voices.bin"_s}}},
        {u"speakers"_s,
         QJsonArray{
             QJsonObject{{u"id"_s, 9}, {u"name"_s, u"bm_george"_s}},
             QJsonObject{{u"id"_s, 7}, {u"name"_s, u"bf_emma"_s}, {u"espeakVoice"_s, u"en"_s}}}},
        {u"licence"_s, u"Apache-2.0"_s},
    };
}

QStringList filesOf(const QJsonObject &manifest)
{
    QStringList files;
    for (const auto &file : manifest.value(u"files").toObject()) {
        files.append(file.toString());
    }
    return files;
}

// Writes a model directory: the manifest text plus empty model files.
void install(const QString &root, const QString &name, const QByteArray &manifest,
             const QStringList &files)
{
    const QString directory = root + u'/' + name;
    QVERIFY(QDir().mkpath(directory));
    QFile file(directory + u"/legilo-voice.json"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(manifest);
    for (const QString &fileName : files) {
        QFile model(directory + u'/' + fileName);
        QVERIFY(model.open(QIODevice::WriteOnly));
    }
}

void install(const QString &root, const QString &name, const QJsonObject &manifest)
{
    install(root, name, QJsonDocument(manifest).toJson(), filesOf(manifest));
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

class TestVoiceRegistry : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void emptyDirectoryHasNoVoices();
    void missingDirectoryHasNoVoices();
    void readsSingleSpeakerModel();
    void listsEachSpeakerOfMultiSpeakerModel();
    void optionalFieldsHaveDefaults();
    void speakerCanOverrideEspeakVoice();
    void skipsDirectoryWithoutManifest();
    void skipsInstallerTemporaryDirectory();
    void skipsInvalidManifest_data();
    void skipsInvalidManifest();
    void skipsUnreadableManifest();
    void keepsFirstOfDuplicateIds();
    void sortsByDisplayName();
    void refreshPicksUpNewInstall();
    void findsVoiceById();
    void findReturnsNothingForUnknownId();

private:
    std::unique_ptr<QTemporaryDir> m_root;
};

void TestVoiceRegistry::init()
{
    m_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_root->isValid());
}

void TestVoiceRegistry::emptyDirectoryHasNoVoices()
{
    const VoiceRegistry registry(m_root->path());

    QVERIFY(registry.voices().isEmpty());
}

void TestVoiceRegistry::missingDirectoryHasNoVoices()
{
    const VoiceRegistry registry(m_root->filePath(u"no-such-directory"_s));

    QVERIFY(registry.voices().isEmpty());
}

void TestVoiceRegistry::readsSingleSpeakerModel()
{
    install(m_root->path(), u"cori"_s, piperManifest());

    const VoiceRegistry registry(m_root->path());

    const QString directory = m_root->filePath(u"cori"_s);
    Voice expected;
    expected.id = u"piper-en_GB-cori-medium"_s;
    expected.modelId = u"piper-en_GB-cori-medium"_s;
    expected.engine = u"piper"_s;
    expected.displayName = u"Cori (British English, medium)"_s;
    expected.language = u"en-GB"_s;
    expected.speakerId = 0;
    expected.directory = directory;
    expected.modelPath = directory + u"/en_GB-cori-medium.onnx"_s;
    expected.tokensPath = directory + u"/tokens.txt"_s;
    expected.licence = u"LicenseRef-public-domain"_s;
    expected.licenceUrl = QUrl(u"https://example.org/cori"_s);
    QCOMPARE(registry.voices().size(), 1);
    QCOMPARE(registry.voices().first(), expected);
}

void TestVoiceRegistry::listsEachSpeakerOfMultiSpeakerModel()
{
    install(m_root->path(), u"kokoro"_s, kokoroManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(idsOf(registry.voices()),
             (QStringList{u"kokoro-en-v0_19:bf_emma"_s, u"kokoro-en-v0_19:bm_george"_s}));
    const Voice &emma = registry.voices().first();
    QCOMPARE(emma.modelId, u"kokoro-en-v0_19"_s);
    QCOMPARE(emma.speakerId, 7);
    QCOMPARE(emma.displayName, u"Kokoro (English): bf_emma"_s);
    QCOMPARE(emma.voicesPath, m_root->filePath(u"kokoro/voices.bin"_s));
}

void TestVoiceRegistry::optionalFieldsHaveDefaults()
{
    QJsonObject manifest = piperManifest();
    manifest.remove(u"language");
    manifest.remove(u"licence");
    manifest.remove(u"licenceUrl");
    install(m_root->path(), u"cori"_s, manifest);

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(registry.voices().size(), 1);
    const Voice &voice = registry.voices().first();
    QCOMPARE(voice.speakerId, 0);
    QCOMPARE(voice.language, QString());
    QCOMPARE(voice.licence, u"unknown"_s);
    QCOMPARE(voice.licenceUrl, QUrl());
}

void TestVoiceRegistry::speakerCanOverrideEspeakVoice()
{
    install(m_root->path(), u"kokoro"_s, kokoroManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(registry.find(u"kokoro-en-v0_19:bf_emma"_s).value_or(Voice{}).espeakVoice, u"en"_s);
    QCOMPARE(registry.find(u"kokoro-en-v0_19:bm_george"_s).value_or(Voice{}).espeakVoice,
             QString());
}

void TestVoiceRegistry::skipsDirectoryWithoutManifest()
{
    QVERIFY(QDir(m_root->path()).mkdir(u"stray"_s));
    install(m_root->path(), u"cori"_s, piperManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(idsOf(registry.voices()), QStringList{u"piper-en_GB-cori-medium"_s});
}

void TestVoiceRegistry::skipsInstallerTemporaryDirectory()
{
    install(m_root->path(), u".install-a1b2"_s, kokoroManifest());
    install(m_root->path(), u"cori"_s, piperManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(idsOf(registry.voices()), QStringList{u"piper-en_GB-cori-medium"_s});
}

void TestVoiceRegistry::skipsInvalidManifest_data()
{
    QTest::addColumn<QByteArray>("manifest");
    QTest::addColumn<QStringList>("files");

    const auto row = [](const char *name, const QJsonObject &manifest) {
        QTest::newRow(name) << QJsonDocument(manifest).toJson() << filesOf(manifest);
    };
    const auto without = [](QJsonObject manifest, const QString &key) {
        manifest.remove(key);
        return manifest;
    };
    const auto with = [](QJsonObject manifest, const QString &key, const QJsonValue &value) {
        manifest.insert(key, value);
        return manifest;
    };
    const auto withFiles = [&](const QJsonObject &manifest, const QString &key) {
        QJsonObject files = manifest.value(u"files").toObject();
        files.remove(key);
        return with(manifest, u"files"_s, files);
    };

    QTest::newRow("not JSON") << QByteArray("{ \"format\": 1,") << QStringList{};
    QTest::newRow("not an object") << QByteArray("[1, 2]") << QStringList{};
    row("no format", without(piperManifest(), u"format"_s));
    row("newer format", with(piperManifest(), u"format"_s, 2));
    row("no id", without(piperManifest(), u"id"_s));
    row("empty id", with(piperManifest(), u"id"_s, u""_s));
    row("id with colon", with(piperManifest(), u"id"_s, u"piper:cori"_s));
    row("no engine", without(piperManifest(), u"engine"_s));
    row("unknown engine", with(piperManifest(), u"engine"_s, u"festival"_s));
    row("no name", without(piperManifest(), u"name"_s));
    row("no files", without(piperManifest(), u"files"_s));
    row("no model file", withFiles(piperManifest(), u"model"_s));
    row("no tokens file", withFiles(piperManifest(), u"tokens"_s));
    row("Kokoro without voices file", withFiles(kokoroManifest(), u"voices"_s));
    row("file outside the directory",
        with(piperManifest(), u"files"_s,
             QJsonObject{{u"model"_s, u"../cori.onnx"_s}, {u"tokens"_s, u"tokens.txt"_s}}));
    row("empty speakers", with(piperManifest(), u"speakers"_s, QJsonArray{}));
    row("speaker without name",
        with(piperManifest(), u"speakers"_s, QJsonArray{QJsonObject{{u"id"_s, 0}}}));
    row("empty espeak voice",
        with(piperManifest(), u"speakers"_s,
             QJsonArray{
                 QJsonObject{{u"id"_s, 0}, {u"name"_s, u"Cori"_s}, {u"espeakVoice"_s, u""_s}}}));
    row("negative speaker id",
        with(piperManifest(), u"speakers"_s,
             QJsonArray{QJsonObject{{u"id"_s, -1}, {u"name"_s, u"Cori"_s}}}));
    QTest::newRow("model file missing")
        << QJsonDocument(piperManifest()).toJson() << QStringList{u"tokens.txt"_s};
}

void TestVoiceRegistry::skipsInvalidManifest()
{
    QFETCH(QByteArray, manifest);
    QFETCH(QStringList, files);
    install(m_root->path(), u"broken"_s, manifest, files);
    install(m_root->path(), u"kokoro"_s, kokoroManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(idsOf(registry.voices()),
             (QStringList{u"kokoro-en-v0_19:bf_emma"_s, u"kokoro-en-v0_19:bm_george"_s}));
}

void TestVoiceRegistry::skipsUnreadableManifest()
{
    QVERIFY(QDir(m_root->path()).mkpath(u"broken/legilo-voice.json"_s));
    install(m_root->path(), u"cori"_s, piperManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(idsOf(registry.voices()), QStringList{u"piper-en_GB-cori-medium"_s});
}

void TestVoiceRegistry::keepsFirstOfDuplicateIds()
{
    QJsonObject second = piperManifest();
    second.insert(u"name"_s, u"Second copy"_s);
    install(m_root->path(), u"a-cori"_s, piperManifest());
    install(m_root->path(), u"b-cori"_s, second);

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(registry.voices().size(), 1);
    QCOMPARE(registry.voices().first().directory, m_root->filePath(u"a-cori"_s));
}

void TestVoiceRegistry::sortsByDisplayName()
{
    QJsonObject alba = piperManifest();
    alba.insert(u"id"_s, u"piper-en_GB-alba-medium"_s);
    alba.insert(u"name"_s, u"Alba (Scottish English, medium)"_s);
    install(m_root->path(), u"z-alba"_s, alba);
    install(m_root->path(), u"a-kokoro"_s, kokoroManifest());
    install(m_root->path(), u"m-cori"_s, piperManifest());

    const VoiceRegistry registry(m_root->path());

    QCOMPARE(idsOf(registry.voices()),
             (QStringList{u"piper-en_GB-alba-medium"_s, u"piper-en_GB-cori-medium"_s,
                          u"kokoro-en-v0_19:bf_emma"_s, u"kokoro-en-v0_19:bm_george"_s}));
}

void TestVoiceRegistry::refreshPicksUpNewInstall()
{
    VoiceRegistry registry(m_root->path());
    install(m_root->path(), u"cori"_s, piperManifest());

    registry.refresh();

    QCOMPARE(idsOf(registry.voices()), QStringList{u"piper-en_GB-cori-medium"_s});
}

void TestVoiceRegistry::findsVoiceById()
{
    install(m_root->path(), u"kokoro"_s, kokoroManifest());
    const VoiceRegistry registry(m_root->path());

    const auto voice = registry.find(u"kokoro-en-v0_19:bm_george"_s);

    QVERIFY(voice.has_value());
    QCOMPARE(voice.value_or(Voice{}).speakerId, 9);
}

void TestVoiceRegistry::findReturnsNothingForUnknownId()
{
    install(m_root->path(), u"kokoro"_s, kokoroManifest());
    const VoiceRegistry registry(m_root->path());

    QVERIFY(!registry.find(u"kokoro-en-v0_19:nobody"_s).has_value());
}

QTEST_GUILESS_MAIN(TestVoiceRegistry)
#include "tst_voiceregistry.moc"
