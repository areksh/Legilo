#pragma once

#include "app/IpcProtocol.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace legilo {

// What a launch of legilo is asked to do (design Appendix B), decided from
// its arguments before any application object exists, so that only a
// launch that may become the instance connects to the display (7.8).
struct CommandLineAction
{
    enum class Kind
    {
        Help,
        Version,
        Usage, // invalid arguments: error says why; exit code 2
        CleanOnly,
        Explain,
        ListVoices,
        InstallVoice,
        Instance // hand request to the running instance, or become it
    };

    Kind kind = Kind::Usage;
    QString error;                     // Usage
    QStringList files;                 // CleanOnly and Explain: the input, or none for stdin
    QString archive;                   // InstallVoice
    std::optional<IpcRequest> request; // Instance: none only checks it runs
    bool mayBecome = true;             // Instance: may this launch become it
};

// Parses the arguments, the program name first, as main() receives them.
// Relative paths are made absolute against workingDirectory, because the
// instance runs elsewhere.
CommandLineAction parseCommandLine(const QStringList &arguments, const QString &workingDirectory);

// The --help text.
QString commandLineHelp();

} // namespace legilo
