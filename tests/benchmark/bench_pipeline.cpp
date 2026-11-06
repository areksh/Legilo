#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "core/text/WordList.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <functional>

using namespace legilo;
using namespace Qt::StringLiterals;

// Times the text pipeline on 5 MB of text (design 9.6) and prints one
// line per measure, "name<TAB>milliseconds", for scripts/bench to record.
// The text is the regression corpus's samples repeated, so reflow and
// segmentation see real copies of PDFs and the other kinds of text.
// Each measure is the best of three runs.
namespace {

constexpr qsizetype Size = qsizetype{5} * 1024 * 1024;
constexpr int Runs = 3;

QString corpusText()
{
    QString samples;
    const QDir detection(QStringLiteral(LEGILO_CORPUS_DIR) + u"/detection"_s);
    for (const QString &label : {u"positive"_s, u"negative"_s}) {
        const QDir folder(detection.filePath(label));
        for (const QString &name : folder.entryList({u"*.txt"_s}, QDir::Files, QDir::Name)) {
            QFile file(folder.filePath(name));
            if (file.open(QIODevice::ReadOnly)) {
                samples += QString::fromUtf8(file.readAll()) + u"\n\n"_s;
            }
        }
    }
    QString text;
    while (!samples.isEmpty() && text.size() < Size) {
        text += samples;
    }
    return text.first(std::min(text.size(), Size));
}

qint64 bestOf(const std::function<void()> &work)
{
    qint64 best = -1;
    for (int i = 0; i < Runs; ++i) {
        QElapsedTimer timer;
        timer.start();
        work();
        const qint64 elapsed = timer.elapsed();
        best = best < 0 ? elapsed : std::min(best, elapsed);
    }
    return best;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    TextCapture capture;
    capture.text = corpusText();
    capture.source = CaptureSource::Pasted;
    const CleanupPipeline pipeline(WordList::bundled()); // loaded outside the timing

    CleanupSettings automatic;
    CleanupSettings always;
    always.reflowMode = ReflowMode::Always;
    Document document;
    out << "pipeline-auto-5mb\t" << bestOf([&] { document = pipeline.run(capture, automatic); })
        << '\n';
    out << "pipeline-always-5mb\t" << bestOf([&] { document = pipeline.run(capture, always); })
        << '\n';
    out << "speech-rules-5mb\t" << bestOf([&] { spokenSegments(document); }) << '\n';
    return 0;
}
