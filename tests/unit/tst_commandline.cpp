#include "app/CommandLine.h"

#include <QTest>

using legilo::CommandLineAction;
using legilo::IpcRequest;
using namespace Qt::StringLiterals;
using Kind = CommandLineAction::Kind;

Q_DECLARE_METATYPE(std::optional<IpcRequest>)

namespace {

const auto WorkingDirectory = u"/home/user/notes"_s;

CommandLineAction parse(QStringList arguments)
{
    arguments.prepend(u"legilo"_s);
    return legilo::parseCommandLine(arguments, WorkingDirectory);
}

IpcRequest command(const QString &name)
{
    return {name, {}, {}};
}

} // namespace

class TestCommandLine : public QObject
{
    Q_OBJECT

private slots:
    void instanceCommandsBecomeRequests_data();
    void instanceCommandsBecomeRequests();
    void readMakesItsPathAbsolute();
    void readWithoutAFileShowsTheReader();
    void localCommandsRunInThisProcess_data();
    void localCommandsRunInThisProcess();
    void helpAndVersionAreRecognised();
    void invalidArgumentsAreUsageErrors_data();
    void invalidArgumentsAreUsageErrors();
    void helpListsEveryCommand();
};

void TestCommandLine::instanceCommandsBecomeRequests_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<std::optional<IpcRequest>>("request");
    QTest::addColumn<bool>("mayBecome");

    QTest::newRow("bare start shows") << QStringList() << std::optional(command(u"show"_s)) << true;
    QTest::newRow("tray") << QStringList{u"--tray"_s} << std::optional<IpcRequest>() << true;
    QTest::newRow("speak-selection") << QStringList{u"--speak-selection"_s}
                                     << std::optional(command(u"speak-selection"_s)) << true;
    QTest::newRow("speak-clipboard") << QStringList{u"--speak-clipboard"_s}
                                     << std::optional(command(u"speak-clipboard"_s)) << true;
    QTest::newRow("speak-pdf-text") << QStringList{u"--speak-pdf-text"_s}
                                    << std::optional(command(u"speak-pdf-text"_s)) << true;
    QTest::newRow("speak-text") << QStringList{u"--speak-text"_s, u"Hello there."_s}
                                << std::optional(IpcRequest{u"speak-text"_s, {}, u"Hello there."_s})
                                << true;
    QTest::newRow("show") << QStringList{u"--show"_s} << std::optional(command(u"show"_s)) << true;
    QTest::newRow("pause") << QStringList{u"--pause"_s} << std::optional(command(u"pause"_s))
                           << false;
    QTest::newRow("stop") << QStringList{u"--stop"_s} << std::optional(command(u"stop"_s)) << false;
    QTest::newRow("faster") << QStringList{u"--faster"_s} << std::optional(command(u"faster"_s))
                            << false;
    QTest::newRow("slower") << QStringList{u"--slower"_s} << std::optional(command(u"slower"_s))
                            << false;
}

void TestCommandLine::instanceCommandsBecomeRequests()
{
    QFETCH(QStringList, arguments);
    QFETCH(std::optional<IpcRequest>, request);
    QFETCH(bool, mayBecome);

    const CommandLineAction action = parse(arguments);

    QCOMPARE(action.kind, Kind::Instance);
    QCOMPARE(action.request, request);
    QCOMPARE(action.mayBecome, mayBecome);
}

void TestCommandLine::readMakesItsPathAbsolute()
{
    const CommandLineAction relative = parse({u"--read"_s, u"chapter.txt"_s});
    const CommandLineAction absolute = parse({u"--read"_s, u"/tmp/other.txt"_s});

    QCOMPARE(relative.kind, Kind::Instance);
    QCOMPARE(relative.request,
             std::optional(IpcRequest{u"read-file"_s, u"/home/user/notes/chapter.txt"_s, {}}));
    QVERIFY(relative.mayBecome);
    QCOMPARE(absolute.request, std::optional(IpcRequest{u"read-file"_s, u"/tmp/other.txt"_s, {}}));
}

void TestCommandLine::readWithoutAFileShowsTheReader()
{
    const CommandLineAction action = parse({u"--read"_s});

    QCOMPARE(action.kind, Kind::Instance);
    QCOMPARE(action.request, std::optional(command(u"show"_s)));
}

void TestCommandLine::localCommandsRunInThisProcess_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<Kind>("kind");
    QTest::addColumn<QStringList>("files");
    QTest::addColumn<QString>("archive");

    QTest::newRow("clean-only from stdin")
        << QStringList{u"--clean-only"_s} << Kind::CleanOnly << QStringList() << QString();
    QTest::newRow("clean-only a file")
        << QStringList{u"--clean-only"_s, u"copied.txt"_s} << Kind::CleanOnly
        << QStringList{u"copied.txt"_s} << QString();
    QTest::newRow("explain a file") << QStringList{u"--explain"_s, u"copied.txt"_s} << Kind::Explain
                                    << QStringList{u"copied.txt"_s} << QString();
    QTest::newRow("list-voices") << QStringList{u"--list-voices"_s} << Kind::ListVoices
                                 << QStringList() << QString();
    QTest::newRow("install-voice") << QStringList{u"--install-voice"_s, u"voice.tar.bz2"_s}
                                   << Kind::InstallVoice << QStringList() << u"voice.tar.bz2"_s;
}

void TestCommandLine::localCommandsRunInThisProcess()
{
    QFETCH(QStringList, arguments);
    QFETCH(Kind, kind);
    QFETCH(QStringList, files);
    QFETCH(QString, archive);

    const CommandLineAction action = parse(arguments);

    QCOMPARE(action.kind, kind);
    QCOMPARE(action.files, files);
    QCOMPARE(action.archive, archive);
}

void TestCommandLine::helpAndVersionAreRecognised()
{
    QCOMPARE(parse({u"--help"_s}).kind, Kind::Help);
    QCOMPARE(parse({u"-h"_s}).kind, Kind::Help);
    QCOMPARE(parse({u"--version"_s}).kind, Kind::Version);
}

void TestCommandLine::invalidArgumentsAreUsageErrors_data()
{
    QTest::addColumn<QStringList>("arguments");

    QTest::newRow("unknown option") << QStringList{u"--shout"_s};
    QTest::newRow("two commands") << QStringList{u"--stop"_s, u"--speak-selection"_s};
    QTest::newRow("a file without --read") << QStringList{u"notes.txt"_s};
    QTest::newRow("two files to read") << QStringList{u"--read"_s, u"a.txt"_s, u"b.txt"_s};
    QTest::newRow("two files to clean") << QStringList{u"--clean-only"_s, u"a.txt"_s, u"b.txt"_s};
    QTest::newRow("a file with a command that takes none") << QStringList{u"--stop"_s, u"a.txt"_s};
    QTest::newRow("empty text") << QStringList{u"--speak-text"_s, QString()};
    QTest::newRow("missing archive") << QStringList{u"--install-voice"_s};
}

void TestCommandLine::invalidArgumentsAreUsageErrors()
{
    QFETCH(QStringList, arguments);

    const CommandLineAction action = parse(arguments);

    QCOMPARE(action.kind, Kind::Usage);
    QVERIFY(!action.error.isEmpty());
}

void TestCommandLine::helpListsEveryCommand()
{
    const QString help = legilo::commandLineHelp();

    for (const auto *option :
         {"--tray", "--speak-selection", "--speak-clipboard", "--speak-pdf-text", "--speak-text",
          "--pause", "--stop", "--faster", "--slower", "--read", "--show", "--clean-only",
          "--explain", "--list-voices", "--install-voice", "--version", "--help"}) {
        QVERIFY2(help.contains(QLatin1StringView(option)), option);
    }
}

QTEST_GUILESS_MAIN(TestCommandLine)
#include "tst_commandline.moc"
