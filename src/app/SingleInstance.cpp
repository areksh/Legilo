#include "app/SingleInstance.h"

#include "app/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

// How long a launch waits for each step. Connecting to a live instance is
// immediate; a launch that is becoming the instance holds the lock for
// well under the lock's wait (7.8).
constexpr int ConnectMs = 1000;
constexpr int ReplyMs = 5000;
constexpr int LockWaitMs = 2000;

QString lockPathFor(const QString &socketPath)
{
    const QFileInfo info(socketPath);
    return info.dir().filePath(info.completeBaseName() + u".lock"_s);
}

} // namespace

SingleInstance::SingleInstance(QString socketPath, QObject *parent)
    : QObject(parent)
    , m_path(std::move(socketPath))
    , m_lock(lockPathFor(m_path))
{}

SingleInstance::~SingleInstance()
{
    // Close the server before the lock is released with the members, so a
    // later launch never takes the lock while the socket is still there.
    delete m_server;
}

QString SingleInstance::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + u"/legilo.sock"_s;
}

SingleInstance::Result SingleInstance::start(const std::optional<IpcRequest> &request,
                                             bool mayBecome)
{
    // Step 1: a running instance answers.
    if (const auto reply = forward(request)) {
        return {Outcome::Forwarded, *reply};
    }
    if (!mayBecome) {
        return {Outcome::NotRunning, {}};
    }
    // Step 2: only one launch at a time may become the instance. One that
    // cannot take the lock waited for another launch, which is likely the
    // instance by now.
    if (!m_lock.tryLock(LockWaitMs)) {
        if (const auto reply = forward(request)) {
            return {Outcome::Forwarded, *reply};
        }
        qCWarning(lcApp) << "Cannot take the lock" << m_lock.fileName() << m_lock.error();
        return {Outcome::Failed, {}};
    }
    // Step 3: another launch may have become the instance just before.
    if (const auto reply = forward(request)) {
        m_lock.unlock();
        return {Outcome::Forwarded, *reply};
    }
    // Step 4.
    if (!listen()) {
        m_lock.unlock();
        return {Outcome::Failed, {}};
    }
    return {Outcome::Became, {}};
}

void SingleInstance::setHandler(Handler handler)
{
    m_handler = std::move(handler);
}

std::optional<IpcReply> SingleInstance::forward(const std::optional<IpcRequest> &request) const
{
    QLocalSocket socket;
    socket.connectToServer(m_path);
    if (!socket.waitForConnected(ConnectMs)) {
        return std::nullopt;
    }
    if (!request) {
        return IpcReply{};
    }
    socket.write(encodeRequest(*request));
    socket.waitForBytesWritten(ReplyMs);
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(ReplyMs)) {
            break;
        }
    }
    auto reply = parseReply(socket.readLine());
    if (!reply) {
        return IpcReply{false, tr("The running instance of Legilo did not answer.")};
    }
    return reply;
}

bool SingleInstance::listen()
{
    // A socket file left by a crash would make listen() fail. With the
    // lock held and no answer on it, nothing is using it.
    QLocalServer::removeServer(m_path);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_path)) {
        qCWarning(lcApp) << "Cannot listen on" << m_path << m_server->errorString();
        delete std::exchange(m_server, nullptr);
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            serve(socket);
        }
    });
    return true;
}

void SingleInstance::serve(QLocalSocket *socket)
{
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
        if (!socket->canReadLine()) {
            return;
        }
        QString error;
        IpcReply reply;
        if (const auto request = parseRequest(socket->readLine(), &error)) {
            reply =
                m_handler ? m_handler(*request) : IpcReply{false, tr("Legilo is still starting.")};
        } else {
            reply = IpcReply{false, error};
        }
        socket->write(encodeReply(reply));
        socket->disconnectFromServer();
    });
}

} // namespace legilo
