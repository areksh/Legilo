#include "core/text/CleanupPipeline.h"
#include "core/text/WordList.h"

#include <QCryptographicHash>
#include <QTest>

using legilo::CaptureSource;
using legilo::CleanupPipeline;
using legilo::CleanupSettings;
using legilo::Document;
using legilo::ReflowMode;
using legilo::TextCapture;
using legilo::WordList;
using namespace Qt::StringLiterals;

namespace {

// Nine lines that run on, eight times in a row: detected as PDF text.
const QString WrappedProse = u"The survey covered every school in the county and\n"
                             u"asked the teachers how often they read aloud to\n"
                             u"their classes, how long each reading lasted, and\n"
                             u"which books they chose, both for the youngest\n"
                             u"children and for the oldest, so that the re-\n"
                             u"sults could be compared with the figures from\n"
                             u"the year before, when the same questions had\n"
                             u"been put to a smaller group of schools in the\n"
                             u"neighbouring county."_s;

const QString Joined = u"The survey covered every school in the county and asked the teachers "
                       u"how often they read aloud to their classes, how long each reading "
                       u"lasted, and which books they chose, both for the youngest children and "
                       u"for the oldest, so that the results could be compared with the figures "
                       u"from the year before, when the same questions had been put to a smaller "
                       u"group of schools in the neighbouring county."_s;

const QString Poem = u"Hope is the thing with feathers\nThat perches in the soul,\n"
                     u"And sings the tune without the words,\nAnd never stops at all."_s;

const WordList &words()
{
    static const WordList list({u"results"_s, u"the"_s});
    return list;
}

TextCapture capture(const QString &text, CaptureSource source = CaptureSource::Selection)
{
    TextCapture capture;
    capture.text = text;
    capture.source = source;
    return capture;
}

Document clean(const QString &text, const CleanupSettings &settings = {},
               CaptureSource source = CaptureSource::Selection)
{
    return CleanupPipeline(words()).run(capture(text, source), settings);
}

CleanupSettings withMode(ReflowMode mode)
{
    CleanupSettings settings;
    settings.reflowMode = mode;
    return settings;
}

} // namespace

class TestCleanupPipeline : public QObject
{
    Q_OBJECT

private slots:
    void pdfTextIsReflowedInAutoMode();
    void ordinaryTextKeepsItsLinesInAutoMode();
    void alwaysReflowsWithoutDetecting();
    void neverReflowsWithoutDetecting();
    void originalIsTheNormalisedText();
    void hintsReachDetection();
    void thresholdComesFromSettings();
    void filesAreNotDetectedUnlessAsked();
    void rulesCanBeTurnedOff();
    void segmentsIndexIntoTheCleanedText();
    void segmenterSettingsApply();
    void idIsTheHashOfTheOriginal();
    void titleIsTheFirstWords();
    void recordsSourceAndSettings();
};

void TestCleanupPipeline::pdfTextIsReflowedInAutoMode()
{
    const Document document = clean(WrappedProse);

    QVERIFY(document.detection.isPdf);
    QCOMPARE(document.cleaned, Joined);
}

void TestCleanupPipeline::ordinaryTextKeepsItsLinesInAutoMode()
{
    const Document document = clean(Poem);

    QVERIFY(!document.detection.isPdf);
    QCOMPARE(document.cleaned, Poem);
}

// Detection only decides Auto mode (7.2.1), so the other modes skip it.
void TestCleanupPipeline::alwaysReflowsWithoutDetecting()
{
    const Document document = clean(Poem, withMode(ReflowMode::Always));

    QVERIFY(document.detection.features.isEmpty());
    QCOMPARE(document.cleaned, u"Hope is the thing with feathers That perches in the soul, "
                               u"And sings the tune without the words, And never stops at all."_s);
}

void TestCleanupPipeline::neverReflowsWithoutDetecting()
{
    const Document document = clean(WrappedProse, withMode(ReflowMode::Never));

    QVERIFY(document.detection.features.isEmpty());
    QVERIFY(document.cleaned.contains(u"county and\nasked"_s));
}

void TestCleanupPipeline::originalIsTheNormalisedText()
{
    const Document document = clean(u"The of\u00ADfice\nis \uFB01ne."_s);

    QCOMPARE(document.original, u"The office\nis fine."_s);
}

void TestCleanupPipeline::hintsReachDetection()
{
    const Document plain = clean(Poem);
    const Document ligatures = clean(u"\uFB01rst line\nsecond line\nthird line"_s);
    TextCapture rich = capture(Poem);
    rich.mimeTypes = {u"text/html"_s};
    const Document html = CleanupPipeline(words()).run(rich, CleanupSettings{});

    QCOMPARE(plain.detection.features.value(u"ligature"_s), 0.0);
    QCOMPARE(ligatures.detection.features.value(u"ligature"_s), 1.0);
    QCOMPARE(html.detection.features.value(u"html"_s), 1.0);
}

void TestCleanupPipeline::thresholdComesFromSettings()
{
    CleanupSettings strict;
    strict.detectThreshold = 100.0;

    QVERIFY(!clean(WrappedProse, strict).detection.isPdf);
}

// An opened file is reflowed in Auto mode only if detectInFiles is set.
void TestCleanupPipeline::filesAreNotDetectedUnlessAsked()
{
    CleanupSettings detecting;
    detecting.detectInFiles = true;

    const Document file = clean(WrappedProse, {}, CaptureSource::File);
    const Document detected = clean(WrappedProse, detecting, CaptureSource::File);

    QVERIFY(!file.detection.isPdf);
    QVERIFY(file.cleaned.contains(u"county and\nasked"_s));
    QVERIFY(detected.detection.isPdf);
}

void TestCleanupPipeline::rulesCanBeTurnedOff()
{
    CleanupSettings settings = withMode(ReflowMode::Always);
    settings.pageNumbers = false;
    settings.dehyphenate = false;

    const Document document = clean(u"so that the re-\nsults\n12\nwere kept."_s, settings);

    QCOMPARE(document.cleaned, u"so that the re- sults 12 were kept."_s);
}

void TestCleanupPipeline::segmentsIndexIntoTheCleanedText()
{
    const Document document = clean(u"One sentence here. Another one there.\n\nA new paragraph."_s);

    QCOMPARE(document.segments.size(), 3);
    const auto text = [&](int i) {
        const auto &s = document.segments.at(i);
        return document.cleaned.sliced(s.start, s.end - s.start);
    };
    QCOMPARE(text(0), u"One sentence here."_s);
    QCOMPARE(text(2), u"A new paragraph."_s);
    QCOMPARE(document.segments.at(2).paragraph, 1);
}

void TestCleanupPipeline::segmenterSettingsApply()
{
    CleanupSettings settings;
    settings.abbreviations = {u"approx"_s, u"Ltd"_s};

    const Document document = clean(u"We met at Acme Ltd. Then we left."_s, settings);

    QCOMPARE(document.segments.size(), 1);
}

// The id keys resume positions (7.11.5), so it must not change with the
// clean-up settings.
void TestCleanupPipeline::idIsTheHashOfTheOriginal()
{
    const Document automatic = clean(WrappedProse);
    const Document never = clean(WrappedProse, withMode(ReflowMode::Never));
    const QByteArray hash =
        QCryptographicHash::hash(automatic.original.toUtf8(), QCryptographicHash::Sha256).toHex();

    QCOMPARE(automatic.id, QString::fromLatin1(hash));
    QCOMPARE(never.id, automatic.id);
}

void TestCleanupPipeline::titleIsTheFirstWords()
{
    QCOMPARE(clean(WrappedProse).title, u"The survey covered every school in"_s);
    QCOMPARE(clean(u"  Short\n note "_s).title, u"Short note"_s);
}

void TestCleanupPipeline::recordsSourceAndSettings()
{
    const CleanupSettings settings = withMode(ReflowMode::Never);

    const Document document = clean(Poem, settings, CaptureSource::Clipboard);

    QCOMPARE(document.source, CaptureSource::Clipboard);
    QCOMPARE(document.settingsUsed, settings);
}

QTEST_GUILESS_MAIN(TestCleanupPipeline)
#include "tst_cleanuppipeline.moc"
