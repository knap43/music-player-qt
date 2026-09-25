#include "Icons.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("amber-player"));
    QApplication::setApplicationName(QStringLiteral("amber-player"));
    QApplication::setApplicationVersion(QStringLiteral(AMBER_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("amber-player"));
    QApplication::setWindowIcon(Icons::appIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Amber — a music player for local MP3, FLAC and WebM files"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("paths"),
                                 QStringLiteral("Files or folders to queue and play."),
                                 QStringLiteral("[paths...]"));
    parser.process(app);

    Theme::apply(app);

    MainWindow window;
    window.show();
    if (!parser.positionalArguments().isEmpty())
        window.openPaths(parser.positionalArguments());

    return app.exec();
}
