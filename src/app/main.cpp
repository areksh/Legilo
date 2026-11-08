#include "app/Instance.h"
#include "app/Logging.h"
#include "app/SingleInstance.h"
#include "core/audio/MiniaudioOutput.h"
#include "core/models/ModelInstaller.h"
#include "core/playback/PlaybackController.h"
#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "core/tts/SherpaTtsEngine.h"
#include "core/tts/VoiceRegistry.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QThread>

#include <array>
#include <memory>
#include <optional>

using namespace legilo;

namespace {

// Exit codes from design Appendix B.
constexpr int ExitError = 1;
constexpr int ExitUsage = 2;
constexpr int ExitNoInstance = 3;

// The application ID (design 1.3): the desktop file's name, under which
// KDE groups Legilo's shortcuts and notifications.
constexpr auto DesktopFileName = "sh.arek.legilo";

// Options whose command may make this process the instance, which needs
// a display for the tray. A bare start is one too.
constexpr std::array InstanceOptions{"--tray", "--speak-selection", "--speak-clipboard"};

bool mayBecomeInstance(int argc, char **argv)
{
    if (argc <= 1) {
        return true;
    }
    for (int i = 1; i < argc; ++i) {
        for (const char *option : InstanceOptions) {
            if (qstrcmp(argv[i], option) == 0) {
                return true;
            }
        }
    }
    return false;
}

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

// Hands a command to the running instance, or becomes the instance and
// carries it out (design 7.8). With no request, a running instance is
// left as it is; mayBecome is false for commands that need no instance
// when none is running, such as stop.
int runInstanceCommand(const std::optional<IpcRequest> &request, bool mayBecome)
{
    SingleInstance single(SingleInstance::defaultPath());
    const auto result = single.start(request, mayBecome);
    switch (result.outcome) {
    case SingleInstance::Outcome::Forwarded:
        if (!result.reply.ok) {
            err() << "legilo: " << result.reply.error << Qt::endl;
            return ExitError;
        }
        return 0;
    case SingleInstance::Outcome::NotRunning:
        return 0;
    case SingleInstance::Outcome::Failed:
        err() << "legilo: cannot start Legilo or reach the running instance" << Qt::endl;
        return ExitNoInstance;
    case SingleInstance::Outcome::Became:
        break;
    }

    Instance instance;
    single.setHandler([&instance](const IpcRequest &later) { return instance.handle(later); });
    QObject::connect(&instance, &Instance::quitRequested, qApp, &QCoreApplication::quit);
    instance.start();
    if (request) {
        const IpcReply reply = instance.handle(*request);
        if (!reply.ok) {
            err() << "legilo: " << reply.error << Qt::endl;
        }
    }
    return QCoreApplication::exec();
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
    // Only a process that may become the instance connects to the display.
    std::unique_ptr<QCoreApplication> app;
    if (mayBecomeInstance(argc, argv)) {
        app = std::make_unique<QApplication>(argc, argv);
        QGuiApplication::setDesktopFileName(QString::fromLatin1(DesktopFileName));
        QGuiApplication::setApplicationDisplayName(QStringLiteral("Legilo"));
        QApplication::setQuitOnLastWindowClosed(false);
    } else {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    applyLoggingDefaults();
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
    const QCommandLineOption trayOption(QStringLiteral("tray"),
                                        QStringLiteral("Start in the tray (used by autostart)."));
    const QCommandLineOption speakSelectionOption(QStringLiteral("speak-selection"),
                                                  QStringLiteral("Read the selection."));
    const QCommandLineOption speakClipboardOption(QStringLiteral("speak-clipboard"),
                                                  QStringLiteral("Read the clipboard."));
    const QCommandLineOption pauseOption(QStringLiteral("pause"),
                                         QStringLiteral("Pause or resume."));
    const QCommandLineOption stopOption(QStringLiteral("stop"), QStringLiteral("Stop."));
    parser.addOptions({trayOption, speakSelectionOption, speakClipboardOption, pauseOption,
                       stopOption, installVoiceOption, speakOption, speakFileOption, voiceOption,
                       speedOption, cleanOnlyOption, explainOption});
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("The text for --clean-only or --explain."),
                                 QStringLiteral("[FILE]"));
    parser.process(*app);

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
        return speak(*app, capture, parser.value(voiceOption), speed);
    }

    const auto command = [](const char *name) {
        return IpcRequest{QString::fromLatin1(name), {}, {}};
    };
    if (parser.isSet(speakSelectionOption)) {
        return runInstanceCommand(command("speak-selection"), true);
    }
    if (parser.isSet(speakClipboardOption)) {
        return runInstanceCommand(command("speak-clipboard"), true);
    }
    if (parser.isSet(pauseOption)) {
        return runInstanceCommand(command("pause"), false);
    }
    if (parser.isSet(stopOption)) {
        return runInstanceCommand(command("stop"), false);
    }
    if (parser.isSet(trayOption)) {
        return runInstanceCommand(std::nullopt, true);
    }
    // A bare start shows the reader of a running instance (Appendix B).
    return runInstanceCommand(command("show"), true);
}
