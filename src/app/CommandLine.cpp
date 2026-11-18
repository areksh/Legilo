#include "app/CommandLine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>

#include <array>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("CommandLine", text);
}

// How a command is carried out. Each command is an option, and a launch
// gives at most one.
enum class Handling
{
    Instance,      // a request the instance may start for
    InstanceIfAny, // a request that needs no instance when none runs
    Local          // runs in this process
};

struct Command
{
    const char *name;
    const char *description;
    const char *valueName; // nullptr for a flag
    Handling handling;
};

// The commands of design Appendix B, in the order --help lists them.
// --voice and --speed for a single command come with the voice cache, in
// stage 4.
constexpr std::array Commands{
    Command{"tray", "Start in the tray (used by autostart).", nullptr, Handling::Instance},
    Command{"speak-selection", "Read the selection.", nullptr, Handling::Instance},
    Command{"speak-clipboard", "Read the clipboard.", nullptr, Handling::Instance},
    Command{"speak-pdf-text", "Read the selection with PDF reflow forced on.", nullptr,
            Handling::Instance},
    Command{"speak-text", "Read the given text (needs ipc/allowSpeakText in the settings).", "text",
            Handling::Instance},
    Command{"pause", "Pause or resume.", nullptr, Handling::InstanceIfAny},
    Command{"stop", "Stop.", nullptr, Handling::InstanceIfAny},
    Command{"faster", "Speak faster, now and from now on.", nullptr, Handling::InstanceIfAny},
    Command{"slower", "Speak slower, now and from now on.", nullptr, Handling::InstanceIfAny},
    Command{"read", "Read FILE aloud; with no file, show the reader.", nullptr, Handling::Instance},
    Command{"show", "Show the reader.", nullptr, Handling::Instance},
    Command{"clean-only", "Print FILE, or standard input, as it would be spoken.", nullptr,
            Handling::Local},
    Command{"explain", "Print PDF detection's features and score for FILE or standard input.",
            nullptr, Handling::Local},
    Command{"list-voices", "List the installed voices.", nullptr, Handling::Local},
    Command{"install-voice", "Install a voice from a downloaded sherpa-onnx model archive.",
            "archive", Handling::Local},
};

// The commands whose FILE argument is allowed.
bool takesFile(QLatin1StringView name)
{
    return name == "read"_L1 || name == "clean-only"_L1 || name == "explain"_L1;
}

void setUp(QCommandLineParser &parser)
{
    parser.setApplicationDescription(tr("Legilo reads text aloud with voices that run offline."));
    parser.addHelpOption();
    parser.addVersionOption();
    for (const Command &command : Commands) {
        QCommandLineOption option(QString::fromLatin1(command.name), tr(command.description));
        if (command.valueName != nullptr) {
            option.setValueName(QString::fromLatin1(command.valueName));
        }
        parser.addOption(option);
    }
    parser.addPositionalArgument(u"file"_s, tr("The file for --read, --clean-only or --explain."),
                                 u"[FILE]"_s);
}

CommandLineAction usage(const QString &error)
{
    CommandLineAction action;
    action.kind = CommandLineAction::Kind::Usage;
    action.error = error;
    return action;
}

CommandLineAction instance(std::optional<IpcRequest> request, bool mayBecome)
{
    CommandLineAction action;
    action.kind = CommandLineAction::Kind::Instance;
    action.request = std::move(request);
    action.mayBecome = mayBecome;
    return action;
}

CommandLineAction local(QLatin1StringView name, const QCommandLineParser &parser)
{
    CommandLineAction action;
    if (name == "clean-only"_L1) {
        action.kind = CommandLineAction::Kind::CleanOnly;
    } else if (name == "explain"_L1) {
        action.kind = CommandLineAction::Kind::Explain;
    } else if (name == "list-voices"_L1) {
        action.kind = CommandLineAction::Kind::ListVoices;
    } else {
        action.kind = CommandLineAction::Kind::InstallVoice;
        action.archive = parser.value(name);
    }
    action.files = parser.positionalArguments();
    return action;
}

} // namespace

CommandLineAction parseCommandLine(const QStringList &arguments, const QString &workingDirectory)
{
    QCommandLineParser parser;
    setUp(parser);
    if (!parser.parse(arguments)) {
        return usage(parser.errorText());
    }
    if (parser.isSet(u"help"_s)) {
        CommandLineAction action;
        action.kind = CommandLineAction::Kind::Help;
        return action;
    }
    if (parser.isSet(u"version"_s)) {
        CommandLineAction action;
        action.kind = CommandLineAction::Kind::Version;
        return action;
    }

    const Command *given = nullptr;
    for (const Command &command : Commands) {
        if (parser.isSet(QString::fromLatin1(command.name))) {
            if (given != nullptr) {
                return usage(tr("Give one command at a time."));
            }
            given = &command;
        }
    }
    const QStringList files = parser.positionalArguments();
    if (given == nullptr) {
        if (!files.isEmpty()) {
            return usage(tr("To read a file, use --read FILE."));
        }
        // A bare start shows the reader of a running instance (Appendix B).
        return instance(IpcRequest{u"show"_s, {}, {}}, true);
    }

    const QLatin1StringView name(given->name);
    if (files.size() > (takesFile(name) ? 1 : 0)) {
        return usage(takesFile(name) ? tr("Give at most one file.")
                                     : tr("--%1 takes no file.").arg(name));
    }
    if (given->handling == Handling::Local) {
        return local(name, parser);
    }
    if (name == "tray"_L1) {
        return instance(std::nullopt, true);
    }
    if (name == "read"_L1) {
        if (files.isEmpty()) {
            return instance(IpcRequest{u"show"_s, {}, {}}, true);
        }
        const QString path =
            QDir::cleanPath(QDir(workingDirectory).absoluteFilePath(files.first()));
        return instance(IpcRequest{u"read-file"_s, path, {}}, true);
    }
    if (name == "speak-text"_L1) {
        const QString text = parser.value(u"speak-text"_s);
        if (text.isEmpty()) {
            return usage(tr("--speak-text needs the text to speak."));
        }
        return instance(IpcRequest{u"speak-text"_s, {}, text}, true);
    }
    return instance(IpcRequest{QString(name), {}, {}}, given->handling == Handling::Instance);
}

QString commandLineHelp()
{
    QCommandLineParser parser;
    setUp(parser);
    return parser.helpText();
}

} // namespace legilo
