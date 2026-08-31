#include <QApplication>
#include <QStyleFactory>
#include "utils/logging.h"
#include "ui/mainwindow.h"

#ifndef Z_APP_VERSION
#define Z_APP_VERSION "1.1.2"
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Z");
    app.setApplicationVersion(Z_APP_VERSION);
    app.setOrganizationName("Z-Creative");
    app.setOrganizationDomain("codezey.dev");

    AppLogging::install();

    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(14, 14, 18));
    darkPalette.setColor(QPalette::WindowText, QColor(235, 235, 245));
    darkPalette.setColor(QPalette::Base, QColor(10, 10, 14));
    darkPalette.setColor(QPalette::AlternateBase, QColor(18, 18, 24));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(24, 20, 30));
    darkPalette.setColor(QPalette::ToolTipText, QColor(245, 240, 250));
    darkPalette.setColor(QPalette::Text, QColor(235, 235, 245));
    darkPalette.setColor(QPalette::Button, QColor(22, 20, 28));
    darkPalette.setColor(QPalette::ButtonText, QColor(235, 235, 245));
    darkPalette.setColor(QPalette::BrightText, QColor(245, 158, 248));
    darkPalette.setColor(QPalette::Link, QColor(217, 70, 239));
    darkPalette.setColor(QPalette::Highlight, QColor(192, 38, 211)); 
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(90, 85, 100));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(90, 85, 100));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(90, 85, 100));

    app.setPalette(darkPalette);

    app.setStyleSheet(
        "QWidget { font-family: 'Segoe UI', 'SF Pro Text', Roboto, Helvetica, Arial, sans-serif; font-size: 11px; color: #ececf4; }"
        "QMainWindow { background: #0c0c10; }"
        "QMainWindow::separator { background: #1a1622; width: 4px; height: 4px; }"
        "QMainWindow::separator:hover { background: #c026d3; }"
        
        "QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; border: 1px solid #221a2c; background: #111116; }"
        "QDockWidget::title { background: #16131c; padding-left: 8px; padding-top: 3px; height: 20px; font-weight: bold; letter-spacing: 0.8px; color: #c4b5fd; border-bottom: 1px solid #221a2c; }"
        
        "QMenuBar { background: #0e0e12; border-bottom: 1px solid #221a2c; padding: 2px 4px; }"
        "QMenuBar::item { background: transparent; padding: 4px 8px; border-radius: 3px; margin: 1px; }"
        "QMenuBar::item:selected { background: #2e163b; color: #f59ef8; }"
        "QMenuBar::item:pressed { background: #4a1d5e; color: white; }"
        
        "QMenu { background: #131118; border: 1px solid #2e1e3b; padding: 4px; border-radius: 4px; }"
        "QMenu::item { padding: 5px 24px 5px 12px; border-radius: 3px; }"
        "QMenu::item:selected { background: #a855f7; color: white; }"
        "QMenu::separator { height: 1px; background: #281c33; margin: 4px 6px; }"
        
        "QToolBar { background: #0e0e12; border: 1px solid #221a2c; padding: 2px; spacing: 4px; border-radius: 4px; }"
        "QToolButton { background: #1a1622; border: 1px solid #2b1f38; border-radius: 3px; padding: 4px 8px; font-weight: 500; color: #e2e2ec; }"
        "QToolButton:hover { background: #2d1e3c; border-color: #c026d3; color: white; }"
        "QToolButton:pressed { background: #c026d3; color: white; }"
        "QToolButton:checked { background: #3c144c; border-color: #d946ef; color: #f59ef8; }"
        
        "QPushButton { background: #1a1622; border: 1px solid #2b1f38; border-radius: 3px; padding: 4px 10px; color: #e2e2ec; font-weight: 500; }"
        "QPushButton:hover { background: #2d1e3c; border-color: #c026d3; color: white; }"
        "QPushButton:pressed { background: #c026d3; color: white; }"
        "QPushButton:disabled { background: #121016; border-color: #1e1826; color: #585064; }"
        
        "QLineEdit { background: #0a0a0e; border: 1px solid #2a1e36; border-radius: 3px; padding: 4px 8px; color: white; selection-background-color: #c026d3; }"
        "QLineEdit:focus { border: 1px solid #d946ef; background: #120e18; }"
        
        "QComboBox { background: #1a1622; border: 1px solid #2b1f38; border-radius: 3px; padding: 3px 8px; color: #ececf4; }"
        "QComboBox:hover { border-color: #c026d3; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: #131118; border: 1px solid #2e1e3b; selection-background-color: #a855f7; }"
        
        "QTabWidget::pane { border: 1px solid #221a2c; background: #111116; }"
        "QTabBar::tab { background: #14121a; border: 1px solid #221a2c; border-bottom: none; padding: 6px 14px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; color: #a098b2; font-weight: 600; font-size: 10px; letter-spacing: 0.5px; }"
        "QTabBar::tab:selected { background: #111116; border-bottom: 2px solid #d946ef; color: #f59ef8; }"
        "QTabBar::tab:hover:!selected { background: #1e1828; color: #e9d5ff; }"
        
        "QListWidget { background: #0d0c11; border: 1px solid #221a2c; border-radius: 3px; padding: 2px; }"
        "QListWidget::item { padding: 4px 6px; border-radius: 2px; margin: 1px; }"
        "QListWidget::item:hover { background: #20172c; color: white; }"
        "QListWidget::item:selected { background: #3b1548; border: 1px solid #d946ef; color: #f59ef8; }"
        
        "QTreeWidget { background: #0d0c11; border: 1px solid #221a2c; border-radius: 3px; padding: 2px; }"
        "QTreeWidget::item { padding: 3px 4px; border-radius: 2px; }"
        "QTreeWidget::item:hover { background: #20172c; color: white; }"
        "QTreeWidget::item:selected { background: #3b1548; border: 1px solid #d946ef; color: #f59ef8; }"
        
        "QScrollBar:vertical { background: #0c0b10; width: 8px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #2a2036; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #a855f7; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        
        "QScrollBar:horizontal { background: #0c0b10; height: 8px; margin: 0px; }"
        "QScrollBar::handle:horizontal { background: #2a2036; min-width: 20px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: #a855f7; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        
        "QToolTip { background: #1c1524; border: 1px solid #3d2850; color: #f5f0fa; padding: 5px 8px; border-radius: 4px; font-size: 11px; }"
        
        "QStatusBar { background: #0d0c11; border-top: 1px solid #221a2c; color: #9c8eb9; font-size: 10px; }"
        "QStatusBar::item { border: none; padding: 2px 6px; }"
    );

    MainWindow mainWin;
    mainWin.resize(1600, 900);
    mainWin.show();

    return app.exec();
}
