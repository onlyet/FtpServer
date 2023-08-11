#include "mainwindow.h"
#include <log.h>

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed for QSettings.
    app.setOrganizationName("TaGuoJiang");
    app.setApplicationName("FtpServer");

    app.setWindowIcon(QIcon(":/logo1.png"));

    // 初始化日志
    QString logDir = QString("%1/log").arg(app.applicationDirPath());
    QDir dir;
    dir.mkpath(logDir);
    LogInit(logDir, app.applicationVersion());
    setLogLevel(QtDebugMsg);

    // Show the main window.
    MainWindow mainWindow;
    mainWindow.setOrientation(MainWindow::ScreenOrientationAuto);
    mainWindow.showExpanded();

    return app.exec();
}
