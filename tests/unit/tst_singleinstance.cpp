#include "app/SingleInstance.h"

#include <QFile>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <memory>

using legilo::IpcReply;
using legilo::IpcRequest;
using legilo::SingleInstance;
using namespace Qt::StringLiterals;
using Outcome = SingleInstance::Outcome;

namespace {

// Runs work on a thread of its own, as a second launch would be a second
// process, while this thread's event loop serves the instance. Returns
// false on timing out.
template <typename Work> bool runElsewhere(Work work, int timeoutMs = 10000)
{
    std::unique_ptr<QThread> thread(QThread::create(std::move(work)));
    thread->start();
    const bool finished = QTest::qWaitFor([&] { return thread->isFinished(); }, timeoutMs);
    if (!finished) {
        thread->wait();
    }
    return finished;
}

} // namespace

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void firstLaunchBecomesTheInstance();
    void laterLaunchForwardsItsRequestAndGetsTheReply();
    void laterLaunchWithoutARequestOnlyFindsTheInstance();
    void requestsAreRefusedUntilThereIsAHandler();
    void invalidRequestGetsAnErrorReply();
    void launchThatMustNotBecomeTheInstanceDoesNot();
    void socketLeftByACrashIsReplaced();
    void addressIsFreeOnceTheInstanceEnds();

private:
    QString socketPath() const { return m_dir->filePath(u"legilo.sock"_s); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestSingleInstance::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void TestSingleInstance::firstLaunchBecomesTheInstance()
{
    SingleInstance instance(socketPath());

    const auto result = instance.start(std::nullopt, true);

    QCOMPARE(result.outcome, Outcome::Became);
    QVERIFY(QFile::exists(socketPath()));
}

void TestSingleInstance::laterLaunchForwardsItsRequestAndGetsTheReply()
{
    SingleInstance first(socketPath());
    QCOMPARE(first.start(std::nullopt, true).outcome, Outcome::Became);
    QList<IpcRequest> received;
    first.setHandler([&](const IpcRequest &request) {
        received.append(request);
        return IpcReply{false, u"Not today."_s};
    });

    SingleInstance::Result result;
    QVERIFY(runElsewhere([&] {
        SingleInstance second(socketPath());
        result = second.start(IpcRequest{u"speak-selection"_s, {}, {}}, true);
    }));

    QCOMPARE(result.outcome, Outcome::Forwarded);
    QCOMPARE(result.reply, (IpcReply{false, u"Not today."_s}));
    QCOMPARE(received, QList<IpcRequest>({IpcRequest{u"speak-selection"_s, {}, {}}}));
}

void TestSingleInstance::laterLaunchWithoutARequestOnlyFindsTheInstance()
{
    SingleInstance first(socketPath());
    QCOMPARE(first.start(std::nullopt, true).outcome, Outcome::Became);
    int handled = 0;
    first.setHandler([&](const IpcRequest &) {
        ++handled;
        return IpcReply{};
    });

    SingleInstance::Result result;
    QVERIFY(runElsewhere([&] { result = SingleInstance(socketPath()).start(std::nullopt, true); }));

    QCOMPARE(result.outcome, Outcome::Forwarded);
    QVERIFY(result.reply.ok);
    QCOMPARE(handled, 0);
}

void TestSingleInstance::requestsAreRefusedUntilThereIsAHandler()
{
    SingleInstance first(socketPath());
    QCOMPARE(first.start(std::nullopt, true).outcome, Outcome::Became);

    SingleInstance::Result result;
    QVERIFY(runElsewhere(
        [&] { result = SingleInstance(socketPath()).start(IpcRequest{u"stop"_s, {}, {}}, true); }));

    QCOMPARE(result.outcome, Outcome::Forwarded);
    QVERIFY(!result.reply.ok);
    QVERIFY(!result.reply.error.isEmpty());
}

void TestSingleInstance::invalidRequestGetsAnErrorReply()
{
    SingleInstance first(socketPath());
    QCOMPARE(first.start(std::nullopt, true).outcome, Outcome::Became);
    first.setHandler([](const IpcRequest &) { return IpcReply{}; });

    QByteArray reply;
    QVERIFY(runElsewhere([&] {
        QLocalSocket socket;
        socket.connectToServer(socketPath());
        if (socket.waitForConnected(2000)) {
            socket.write("{\"cmd\": \"format-disk\"}\n");
            socket.waitForBytesWritten(2000);
            while (!socket.canReadLine() && socket.waitForReadyRead(2000)) {
            }
            reply = socket.readLine();
        }
    }));

    const auto parsed = legilo::parseReply(reply).value_or(IpcReply{true, {}});
    QVERIFY(!parsed.ok);
    QVERIFY(parsed.error.contains(u"format-disk"_s));
}

void TestSingleInstance::launchThatMustNotBecomeTheInstanceDoesNot()
{
    SingleInstance instance(socketPath());

    const auto result = instance.start(IpcRequest{u"pause"_s, {}, {}}, false);

    QCOMPARE(result.outcome, Outcome::NotRunning);
    QVERIFY(!QFile::exists(socketPath()));
}

void TestSingleInstance::socketLeftByACrashIsReplaced()
{
    QFile stale(socketPath());
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.close();

    SingleInstance instance(socketPath());

    QCOMPARE(instance.start(std::nullopt, true).outcome, Outcome::Became);
}

void TestSingleInstance::addressIsFreeOnceTheInstanceEnds()
{
    {
        SingleInstance first(socketPath());
        QCOMPARE(first.start(std::nullopt, true).outcome, Outcome::Became);
    }

    SingleInstance second(socketPath());

    QCOMPARE(second.start(std::nullopt, true).outcome, Outcome::Became);
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
