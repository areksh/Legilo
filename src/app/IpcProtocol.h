#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <optional>

namespace legilo {

// A command sent to the running instance (design 7.8): one JSON object
// per line, such as {"cmd": "speak-selection"} or
// {"cmd": "read-file", "path": "/abs/path.txt"}.
struct IpcRequest
{
    QString command;
    QString path; // read-file: an absolute path
    QString text; // speak-text

    bool operator==(const IpcRequest &) const = default;
};

// The instance's answer: {"ok": true} or {"ok": false, "error": "..."}.
struct IpcReply
{
    bool ok = true;
    QString error;

    bool operator==(const IpcReply &) const = default;
};

// The commands the protocol knows. Whether the instance carries one out is
// its own decision; an unknown command is refused here.
bool isIpcCommand(const QString &command);

QByteArray encodeRequest(const IpcRequest &request);
// Parses one line. Returns nothing, and says why in error, if the line is
// not a valid request: not a JSON object, an unknown command, or a missing
// or relative path, or missing text, for the commands that need one.
std::optional<IpcRequest> parseRequest(QByteArrayView line, QString *error = nullptr);

QByteArray encodeReply(const IpcReply &reply);
std::optional<IpcReply> parseReply(QByteArrayView line);

} // namespace legilo
