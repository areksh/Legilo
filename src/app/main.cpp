#include "core/models/ModelInstaller.h"
#include "core/tts/VoiceRegistry.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

using namespace legilo;

namespace {

// Installs a voice from a downloaded archive and lists the voices it adds.
int installVoice(const QString &archive)
{
    const QString directory = VoiceRegistry::defaultDirectory();
    const auto result = ModelInstaller(directory).install(archive);
    if (!result.ok()) {
        QTextStream(stderr) << "legilo: " << result.error << '\n';
        return 1;
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
    parser.addOption(installVoiceOption);
    parser.process(app);

    if (parser.isSet(installVoiceOption)) {
        return installVoice(parser.value(installVoiceOption));
    }

    // Nothing else is implemented yet, so a bare start shows the help.
    parser.showHelp(0);
}
