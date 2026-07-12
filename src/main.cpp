#include <QApplication>
#include <QStyleFactory>
#include "core/logging.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    AppLogging::install();

    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::WindowText, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::Base, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));
    darkPalette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(255, 85, 0)); 
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 100, 100));

    app.setPalette(darkPalette);

    app.setStyleSheet(
        "QWidget { font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px; }"
        "QMainWindow::separator { background: #121212; width: 4px; height: 4px; }"
        "QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; border: 1px solid #1a1a1a; }"
        "QDockWidget::title { background: #252525; padding-left: 5px; padding-top: 2px; height: 18px; color: #aaaaaa; border-bottom: 1px solid #151515; }"
        "QMenuBar { background: #1c1c1c; border-bottom: 1px solid #121212; }"
        "QMenuBar::item:selected { background: #ff5500; color: white; }"
        "QMenu { background: #1c1c1c; border: 1px solid #121212; }"
        "QMenu::item:selected { background: #ff5500; color: white; }"
        "QScrollBar:vertical { background: #181818; width: 8px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #3e3e3e; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #ff5500; }"
        "QScrollBar:horizontal { background: #181818; height: 8px; margin: 0px; }"
        "QScrollBar::handle:horizontal { background: #3e3e3e; min-width: 20px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: #ff5500; }"
        "QSlider::groove:horizontal { border: 1px solid #151515; height: 4px; background: #121212; }"
        "QSlider::handle:horizontal { background: #3e3e3e; border: 1px solid #121212; width: 10px; height: 16px; margin: -6px 0; border-radius: 2px; }"
        "QSlider::handle:horizontal:hover { background: #ff5500; }"
    );

    MainWindow mainWin;
    mainWin.resize(1600, 900);
    mainWin.show();

    return app.exec();
}
