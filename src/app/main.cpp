#include "app/CommandLine.h"
#include "app/Instance.h"
#include "app/Logging.h"
#include "app/SingleInstance.h"
#include "core/models/ModelInstaller.h"
#include "core/text/CleanupPipeline.h"
#include "core/text/SpeechRules.h"
#include "core/tts/VoiceRegistry.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QTextStream>

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

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
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
    out() << "Installed " << result.modelId << " in " << directory << '\n';
    const VoiceRegistry registry(directory);
    for (const Voice &voice : registry.voices()) {
        if (voice.modelId == result.modelId) {
            out() << "  " << voice.id << "  " << voice.displayName << '\n';
        }
    }
    return 0;
}

// Lists the installed voices, one per line: the ID that --voice and the
// voice/default setting take, then the name.
int listVoices()
{
    const VoiceRegistry registry(VoiceRegistry::defaultDirectory());
    if (registry.voices().isEmpty()) {
        err() << "legilo: no voices are installed; install one with --install-voice" << Qt::endl;
        return ExitError;
    }
    for (const Voice &voice : registry.voices()) {
        out() << voice.id << "  " << voice.displayName << '\n';
    }
    return 0;
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
    out() << (explanation ? explain(document.detection, settings.detectThreshold)
                          : spokenText(document))
          << '\n';
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

QStringList argumentsOf(int argc, char **argv)
{
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }
    return arguments;
}

} // namespace

int main(int argc, char *argv[])
{
    // The arguments are parsed before the application object exists, so
    // only a launch that may become the instance connects to the display.
    const CommandLineAction action = parseCommandLine(argumentsOf(argc, argv), QDir::currentPath());
    const bool display = action.kind == CommandLineAction::Kind::Instance && action.mayBecome;

    std::unique_ptr<QCoreApplication> app;
    if (display) {
        app = std::make_unique<QApplication>(argc, argv);
        QGuiApplication::setDesktopFileName(QString::fromLatin1(DesktopFileName));
        QGuiApplication::setApplicationDisplayName(QStringLiteral("Legilo"));
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setWindowIcon(QIcon(QStringLiteral(":/legilo/icons/sh.arek.legilo.svg")));
    } else {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    applyLoggingDefaults();
    QCoreApplication::setApplicationName(QStringLiteral("legilo"));
    QCoreApplication::setApplicationVersion(QStringLiteral(LEGILO_VERSION));

    switch (action.kind) {
    case CommandLineAction::Kind::Help:
        out() << commandLineHelp();
        return 0;
    case CommandLineAction::Kind::Version:
        out() << "legilo " << LEGILO_VERSION << '\n';
        return 0;
    case CommandLineAction::Kind::Usage:
        err() << "legilo: " << action.error << '\n'
              << "Run legilo --help for the commands." << Qt::endl;
        return ExitUsage;
    case CommandLineAction::Kind::CleanOnly:
        return cleanOnly(action.files, false);
    case CommandLineAction::Kind::Explain:
        return cleanOnly(action.files, true);
    case CommandLineAction::Kind::ListVoices:
        return listVoices();
    case CommandLineAction::Kind::InstallVoice:
        return installVoice(action.archive);
    case CommandLineAction::Kind::Instance:
        break;
    }
    return runInstanceCommand(action.request, action.mayBecome);
}
