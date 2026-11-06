#include "core/text/CleanupPipeline.h"

#include "core/text/Normaliser.h"
#include "core/text/PdfDetector.h"
#include "core/text/Reflow.h"
#include "core/text/Segmenter.h"
#include "core/text/WordList.h"

#include <QCryptographicHash>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr qsizetype TitleWords = 6;

QString titleOf(const QString &text)
{
    static const QRegularExpression space(u"\\s+"_s);
    return text.trimmed().split(space, Qt::SkipEmptyParts).mid(0, TitleWords).join(u' ');
}

} // namespace

CleanupPipeline::CleanupPipeline(const WordList &words)
    : m_words(&words)
{}

CleanupPipeline::CleanupPipeline()
    : CleanupPipeline(WordList::bundled())
{}

Document CleanupPipeline::run(const TextCapture &capture, const CleanupSettings &settings) const
{
    const NormalisedText normalised = normalise(capture.text);

    Document document;
    document.source = capture.source;
    document.original = normalised.text;
    document.id = QString::fromLatin1(
        QCryptographicHash::hash(normalised.text.toUtf8(), QCryptographicHash::Sha256).toHex());
    document.title = titleOf(normalised.text);
    document.settingsUsed = settings;

    bool pdf = settings.reflowMode == ReflowMode::Always;
    const bool detect = settings.reflowMode == ReflowMode::Auto &&
                        (capture.source != CaptureSource::File || settings.detectInFiles);
    if (detect) {
        DetectionHints hints;
        hints.hadLigatures = normalised.hadLigatures;
        hints.mimeTypes = capture.mimeTypes;
        hints.ownerHint = capture.ownerHint;
        DetectorWeights weights;
        weights.threshold = settings.detectThreshold;
        document.detection = detectPdf(normalised.text, hints, weights);
        pdf = document.detection.isPdf;
    }

    document.pdf = pdf;
    ReflowOptions reflowOptions;
    reflowOptions.pdf = pdf;
    reflowOptions.removePageNumbers = settings.pageNumbers;
    reflowOptions.removeRunningHeaders = settings.repeatedLines;
    reflowOptions.dehyphenate = settings.dehyphenate;
    document.cleaned = tidyWhitespace(reflow(normalised.text, reflowOptions, m_words));

    SegmenterOptions segmenterOptions;
    segmenterOptions.maxLength = settings.maxSegmentChars;
    segmenterOptions.abbreviations = settings.abbreviations;
    document.segments = segment(document.cleaned, segmenterOptions);
    return document;
}

} // namespace legilo
