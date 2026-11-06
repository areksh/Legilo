#include "core/audio/MiniaudioOutput.h"
#include "core/models/ModelInstaller.h"
#include "core/playback/PlaybackController.h"
#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "core/tts/SherpaTtsEngine.h"
#include "core/tts/VoiceRegistry.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QThread>

#include <optional>

using namespace legilo;

namespace {

// Exit codes from design Appendix B.
constexpr int ExitError = 1;
constexpr int ExitUsage = 2;

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

// Installs a voice from a downloaded archive and lists the voices it adds.
int installVoice(const QString &archive)
{
    const QString directory = VoiceRegistry::defaultDirectory();
    const auto result = ModelInstaller(directory).install(archive);
    if (!result.ok()) {
        err() << "legilo: " << result.error << Qt::endl;
        return ExitError;
    }
    QTextStream out(stdout);
    out << "Installed " << result.modelId << " in " << directory << '\n';
    const VoiceRegistry registry(directory);
    for (const Voice &voice : registry.voices()) {
        if (voice.modelId == result.modelId) {
            out << "  " << voice.id << "  " << voice.displayName << '\n';
        }
    }
    return 0;
}

// Speaks text aloud and returns once it has been spoken. A stage 1 stand-in
// for --speak-text and --read, which stage 3 brings with a running instance.
int speak(QCoreApplication &app, const TextCapture &capture, const QString &voiceId, double speed)
{
    const VoiceRegistry registry(VoiceRegistry::defaultDirectory());
    if (registry.voices().isEmpty()) {
        err() << "legilo: no voices are installed; install one with --install-voice" << Qt::endl;
        return ExitError;
    }
    const auto voice =
        voiceId.isEmpty() ? std::optional(registry.voices().first()) : registry.find(voiceId);
    if (!voice) {
        err() << "legilo: no installed voice is called " << voiceId << Qt::endl;
        return ExitError;
    }
    const int threads = SherpaTtsEngine::threadsFor(0, QThread::idealThreadCount());
    std::shared_ptr<TtsEngine> engine = SherpaTtsEngine::load(*voice, threads);
    if (!engine) {
        err() << "legilo: cannot load the voice " << voice->id << Qt::endl;
        return ExitError;
    }

    // Stage 3 reads the clean-up settings; until then the defaults apply.
    const QStringList segments = spokenSegments(CleanupPipeline().run(capture, CleanupSettings{}));
    if (segments.isEmpty()) {
        return 0;
    }
    MiniaudioOutput output;
    PlaybackController controller(&output);
    controller.setEngine(engine);
    controller.setSpeed(speed);
    QObject::connect(&controller, &PlaybackController::error, &app,
                     [](const QString &message) { err() << "legilo: " << message << Qt::endl; });
    QObject::connect(&controller, &PlaybackController::documentFinished, &app,
                     &QCoreApplication::quit);
    controller.play(segments);
    if (controller.state() == PlaybackController::State::Idle) {
        return ExitError;
    }
    return QCoreApplication::exec();
}

// The text --clean-only and --explain work on: a file, or standard input.
std::optional<QString> readInput(const QStringList &files)
{
    QFile file;
    if (files.isEmpty()) {
        if (!file.open(stdin, QIODevice::ReadOnly)) {
            err() << "legilo: cannot read standard input" << Qt::endl;
            return std::nullopt;
        }
    } else {
        file.setFileName(files.first());
        if (!file.open(QIODevice::ReadOnly)) {
            err() << "legilo: cannot read " << file.fileName() << ": " << file.errorString()
                  << Qt::endl;
            return std::nullopt;
        }
    }
    return QString::fromUtf8(file.readAll());
}

// Runs the clean-up pipeline on text as pasted, so Auto mode detects PDF
// text in it, and prints the spoken text or the detection's explanation.
int cleanOnly(const QStringList &files, bool explanation)
{
    const auto text = readInput(files);
    if (!text) {
        return ExitError;
    }
    TextCapture capture;
    capture.text = *text;
    capture.source = CaptureSource::Pasted;
    const CleanupSettings settings;
    const Document document = CleanupPipeline().run(capture, settings);
    QTextStream out(stdout);
    out << (explanation ? explain(document.detection, settings.detectThreshold)
                        : spokenText(document))
        << '\n';
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("legilo"));
    QCoreApplication::setApplicationVersion(QStringLiteral(LEGILO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline text-to-speech reader"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption installVoiceOption(
        QStringLiteral("install-voice"),
        QStringLiteral("Install a voice from a downloaded sherpa-onnx model archive."),
        QStringLiteral("archive"));
    const QCommandLineOption speakOption(
        QStringLiteral("speak"), QStringLiteral("Speak the text aloud."), QStringLiteral("text"));
    const QCommandLineOption speakFileOption(QStringLiteral("speak-file"),
                                             QStringLiteral("Speak a UTF-8 text file aloud."),
                                             QStringLiteral("file"));
    const QCommandLineOption voiceOption(
        QStringLiteral("voice"),
        QStringLiteral("The voice to speak with, by ID; the first installed voice by default."),
        QStringLiteral("id"));
    const QCommandLineOption speedOption(QStringLiteral("speed"),
                                         QStringLiteral("The speed to speak at, from 0.5 to 2.5."),
                                         QStringLiteral("speed"), QStringLiteral("1.0"));
    const QCommandLineOption cleanOnlyOption(
        QStringLiteral("clean-only"),
        QStringLiteral("Print the text as it would be spoken, from FILE or standard input."));
    const QCommandLineOption explainOption(
        QStringLiteral("explain"),
        QStringLiteral("Print PDF detection's features and score, from FILE or standard input."));
    parser.addOptions({installVoiceOption, speakOption, speakFileOption, voiceOption, speedOption,
                       cleanOnlyOption, explainOption});
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("The text for --clean-only or --explain."),
                                 QStringLiteral("[FILE]"));
    parser.process(app);

    if (parser.isSet(cleanOnlyOption) || parser.isSet(explainOption)) {
        if (parser.positionalArguments().size() > 1) {
            err() << "legilo: give at most one file" << Qt::endl;
            return ExitUsage;
        }
        return cleanOnly(parser.positionalArguments(), parser.isSet(explainOption));
    }

    if (parser.isSet(installVoiceOption)) {
        return installVoice(parser.value(installVoiceOption));
    }

    if (parser.isSet(speakOption) || parser.isSet(speakFileOption)) {
        bool validSpeed = false;
        const double speed = parser.value(speedOption).toDouble(&validSpeed);
        if (!validSpeed || speed < 0.5 || speed > 2.5) {
            err() << "legilo: the speed must be a number from 0.5 to 2.5" << Qt::endl;
            return ExitUsage;
        }
        TextCapture capture;
        capture.text = parser.value(speakOption);
        capture.source = CaptureSource::Pasted;
        if (parser.isSet(speakFileOption)) {
            QFile file(parser.value(speakFileOption));
            if (!file.open(QIODevice::ReadOnly)) {
                err() << "legilo: cannot read " << file.fileName() << ": " << file.errorString()
                      << Qt::endl;
                return ExitError;
            }
            capture.text = QString::fromUtf8(file.readAll());
            capture.source = CaptureSource::File;
        }
        return speak(app, capture, parser.value(voiceOption), speed);
    }

    // Nothing else is implemented yet, so a bare start shows the help.
    parser.showHelp(0);
}
