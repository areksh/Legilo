#include <QCommandLineParser>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("legilo"));
    QCoreApplication::setApplicationVersion(QStringLiteral(LEGILO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline text-to-speech reader"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    // Nothing else is implemented yet, so a bare start shows the help.
    parser.showHelp(0);
}
