#pragma once

#include "app/IpcProtocol.h"

#include <QLockFile>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class QLocalServer;
class QLocalSocket;

namespace legilo {

// Makes Legilo one process per user session (design 7.8, FR-13). A later
// launch hands its command to the running instance over a local socket
// and exits with the reply; if none is running, it may become the
// instance itself.
//
// A lock file next to the socket closes the race between two launches
// that both find no instance, and a socket left behind by a crash is
// removed before listening. Only the current user can connect.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // Carries out a command for the instance and gives the reply.
    using Handler = std::function<IpcReply(const IpcRequest &)>;

    enum class Outcome
    {
        Forwarded,  // a running instance took the request and replied
        Became,     // this process is now the instance
        NotRunning, // no instance, and this process was not to become one
        Failed      // no instance answered, and this process could not become one
    };

    struct Result
    {
        Outcome outcome = Outcome::Failed;
        IpcReply reply; // the running instance's, when Forwarded
    };

    // The socket is at socketPath; the lock file has the same name with
    // ".lock" in place of its extension.
    explicit SingleInstance(QString socketPath, QObject *parent = nullptr);
    ~SingleInstance() override;

    // legilo.sock in the user's runtime directory ($XDG_RUNTIME_DIR).
    static QString defaultPath();

    // Hands request to a running instance, or, with no request, only
    // checks that one is running. Without one, becomes the instance if
    // mayBecome holds. Blocks for at most a few seconds.
    Result start(const std::optional<IpcRequest> &request, bool mayBecome);

    // How the instance carries out requests from later launches. Until it
    // is set, they are refused.
    void setHandler(Handler handler);

private:
    std::optional<IpcReply> forward(const std::optional<IpcRequest> &request) const;
    bool listen();
    void serve(QLocalSocket *socket);

    QString m_path;
    QLockFile m_lock;
    QLocalServer *m_server = nullptr;
    Handler m_handler;
};

} // namespace legilo
