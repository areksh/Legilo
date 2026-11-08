#include "app/IpcProtocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr std::array Commands{
    "speak-selection"_L1,
    "speak-clipboard"_L1,
    "speak-pdf-text"_L1,
    "speak-text"_L1,
    "read-file"_L1,
    "pause"_L1,
    "stop"_L1,
    "faster"_L1,
    "slower"_L1,
    "show"_L1,
};

const auto CommandKey = u"cmd"_s;
const auto PathKey = u"path"_s;
const auto TextKey = u"text"_s;
const auto OkKey = u"ok"_s;
const auto ErrorKey = u"error"_s;

// Compact JSON escapes line breaks inside strings, so the object is one
// line and the newline after it ends the message.
QByteArray asLine(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<QJsonObject> objectIn(QByteArrayView text)
{
    const QJsonDocument document = QJsonDocument::fromJson(text.trimmed().toByteArray());
    if (!document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

QString tr(const char *text)
{
    return QCoreApplication::translate("IpcProtocol", text);
}

} // namespace

bool isIpcCommand(const QString &command)
{
    return std::ranges::find(Commands, command) != Commands.end();
}

QByteArray encodeRequest(const IpcRequest &request)
{
    QJsonObject object{{CommandKey, request.command}};
    if (!request.path.isEmpty()) {
        object.insert(PathKey, request.path);
    }
    if (!request.text.isEmpty()) {
        object.insert(TextKey, request.text);
    }
    return asLine(object);
}

std::optional<IpcRequest> parseRequest(QByteArrayView line, QString *error)
{
    const auto fail = [error](const QString &reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return std::nullopt;
    };
    const auto object = objectIn(line);
    if (!object) {
        return fail(tr("The request is not a JSON object."));
    }
    IpcRequest request;
    request.command = object->value(CommandKey).toString();
    request.path = object->value(PathKey).toString();
    request.text = object->value(TextKey).toString();
    if (!isIpcCommand(request.command)) {
        return fail(tr("Unknown command \"%1\".").arg(request.command));
    }
    if (request.command == "read-file"_L1 &&
        (request.path.isEmpty() || !QDir::isAbsolutePath(request.path))) {
        return fail(tr("read-file needs an absolute path."));
    }
    if (request.command == "speak-text"_L1 && request.text.isEmpty()) {
        return fail(tr("speak-text needs the text to speak."));
    }
    return request;
}

QByteArray encodeReply(const IpcReply &reply)
{
    QJsonObject object{{OkKey, reply.ok}};
    if (!reply.error.isEmpty()) {
        object.insert(ErrorKey, reply.error);
    }
    return asLine(object);
}

std::optional<IpcReply> parseReply(QByteArrayView line)
{
    const auto object = objectIn(line);
    if (!object || !object->value(OkKey).isBool()) {
        return std::nullopt;
    }
    return IpcReply{object->value(OkKey).toBool(), object->value(ErrorKey).toString()};
}

} // namespace legilo
