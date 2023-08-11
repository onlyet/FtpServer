#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed for QSettings.
    app.setOrganizationName("TaGuoJiang");
    app.setApplicationName("FtpServer");

    app.setWindowIcon(QIcon(":/logo1.png"));

    // Show the main window.
    MainWindow mainWindow;
    mainWindow.setOrientation(MainWindow::ScreenOrientationAuto);
    mainWindow.showExpanded();

    return app.exec();
}
