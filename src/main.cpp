#include <QApplication>
#include "utils/logging.h"
#include "ui/mainwindow.h"
#include "ui/thememanager.h"

#ifndef Z_APP_VERSION
#define Z_APP_VERSION "1.1.20"
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Z");
    app.setApplicationVersion(Z_APP_VERSION);
    app.setOrganizationName("Z-Creative");
    app.setOrganizationDomain("codezey.dev");

    AppLogging::install();
    ThemeManager::instance().initialize(app);

    MainWindow mainWin;
    mainWin.resize(1600, 900);
    mainWin.show();
    return app.exec();
}
