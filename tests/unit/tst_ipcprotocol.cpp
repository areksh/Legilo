#include "app/IpcProtocol.h"

#include <QTest>

using legilo::IpcReply;
using legilo::IpcRequest;
using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(IpcRequest)

class TestIpcProtocol : public QObject
{
    Q_OBJECT

private slots:
    void requestSurvivesEncodingAndParsing_data();
    void requestSurvivesEncodingAndParsing();
    void encodedRequestIsOneLine();
    void invalidRequestIsRefusedWithAReason_data();
    void invalidRequestIsRefusedWithAReason();
    void replySurvivesEncodingAndParsing_data();
    void replySurvivesEncodingAndParsing();
    void malformedReplyIsRefused();
};

void TestIpcProtocol::requestSurvivesEncodingAndParsing_data()
{
    QTest::addColumn<IpcRequest>("request");
    for (const char *command : {"speak-selection", "speak-clipboard", "speak-pdf-text", "pause",
                                "stop", "faster", "slower", "show"}) {
        QTest::newRow(command) << IpcRequest{QString::fromLatin1(command), {}, {}};
    }
    QTest::newRow("read-file") << IpcRequest{u"read-file"_s, u"/home/user/notes.txt"_s, {}};
    QTest::newRow("speak-text") << IpcRequest{u"speak-text"_s, {}, u"Line one\nline \"two\""_s};
}

void TestIpcProtocol::requestSurvivesEncodingAndParsing()
{
    QFETCH(IpcRequest, request);

    const auto parsed = legilo::parseRequest(legilo::encodeRequest(request));

    QVERIFY(parsed.has_value());
    QCOMPARE(parsed.value_or(IpcRequest{}), request);
}

void TestIpcProtocol::encodedRequestIsOneLine()
{
    const QByteArray line =
        legilo::encodeRequest({u"speak-text"_s, {}, u"First\nsecond\r\nthird"_s});

    QVERIFY(line.endsWith('\n'));
    QCOMPARE(line.count('\n'), 1);
}

void TestIpcProtocol::invalidRequestIsRefusedWithAReason_data()
{
    QTest::addColumn<QByteArray>("line");
    QTest::newRow("not JSON") << QByteArray("speak-selection\n");
    QTest::newRow("not an object") << QByteArray("[\"speak-selection\"]\n");
    QTest::newRow("no command") << QByteArray("{\"path\": \"/a.txt\"}\n");
    QTest::newRow("unknown command") << QByteArray("{\"cmd\": \"format-disk\"}\n");
    QTest::newRow("read-file without a path") << QByteArray("{\"cmd\": \"read-file\"}\n");
    QTest::newRow("relative path") << QByteArray("{\"cmd\": \"read-file\", \"path\": \"a.txt\"}\n");
    QTest::newRow("speak-text without text") << QByteArray("{\"cmd\": \"speak-text\"}\n");
}

void TestIpcProtocol::invalidRequestIsRefusedWithAReason()
{
    QFETCH(QByteArray, line);
    QString error;

    QVERIFY(!legilo::parseRequest(line, &error));
    QVERIFY(!error.isEmpty());
}

void TestIpcProtocol::replySurvivesEncodingAndParsing_data()
{
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QString>("error");
    QTest::newRow("ok") << true << QString();
    QTest::newRow("refused") << false << u"Speaking text sent by other programs is turned off."_s;
}

void TestIpcProtocol::replySurvivesEncodingAndParsing()
{
    QFETCH(bool, ok);
    QFETCH(QString, error);
    const IpcReply reply{ok, error};

    const QByteArray line = legilo::encodeReply(reply);

    QVERIFY(line.endsWith('\n'));
    QCOMPARE(legilo::parseReply(line), std::optional(reply));
}

void TestIpcProtocol::malformedReplyIsRefused()
{
    QVERIFY(!legilo::parseReply("yes\n"));
    QVERIFY(!legilo::parseReply("{\"error\": \"no ok field\"}\n"));
}

QTEST_GUILESS_MAIN(TestIpcProtocol)
#include "tst_ipcprotocol.moc"
