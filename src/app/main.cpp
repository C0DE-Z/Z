#include <QApplication>
#include <QStyleFactory>
#include "core/logging.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Z");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Z-Creative");
    app.setOrganizationDomain("codezey.dev");

    AppLogging::install();

    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(18, 18, 20));
    darkPalette.setColor(QPalette::WindowText, QColor(230, 230, 235));
    darkPalette.setColor(QPalette::Base, QColor(14, 14, 16));
    darkPalette.setColor(QPalette::AlternateBase, QColor(24, 24, 28));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(30, 30, 35));
    darkPalette.setColor(QPalette::ToolTipText, QColor(240, 240, 245));
    darkPalette.setColor(QPalette::Text, QColor(230, 230, 235));
    darkPalette.setColor(QPalette::Button, QColor(32, 32, 38));
    darkPalette.setColor(QPalette::ButtonText, QColor(230, 230, 235));
    darkPalette.setColor(QPalette::BrightText, QColor(255, 75, 75));
    darkPalette.setColor(QPalette::Link, QColor(0, 180, 255));
    darkPalette.setColor(QPalette::Highlight, QColor(232, 85, 244)); 
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 120));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 110, 120));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 110, 120));

    app.setPalette(darkPalette);

    app.setStyleSheet(
        "QWidget { font-family: 'Segoe UI', 'SF Pro Text', Roboto, Helvetica, Arial, sans-serif; font-size: 11px; color: #e2e2e8; }"
        "QMainWindow { background: #101012; }"
        "QMainWindow::separator { background: #1c1c22; width: 4px; height: 4px; }"
        "QMainWindow::separator:hover { background: #e855f4; }"
        
        "QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; border: 1px solid #23232b; background: #16161a; }"
        "QDockWidget::title { background: #1c1c22; padding-left: 8px; padding-top: 3px; height: 20px; font-weight: bold; letter-spacing: 1px; color: #a0a0b0; border-bottom: 1px solid #23232b; }"
        
        "QMenuBar { background: #141418; border-bottom: 1px solid #23232b; padding: 2px 4px; }"
        "QMenuBar::item { background: transparent; padding: 4px 8px; border-radius: 3px; margin: 1px; }"
        "QMenuBar::item:selected { background: #2a1e30; color: #f59ef8; }"
        "QMenuBar::item:pressed { background: #3d2348; color: white; }"
        
        "QMenu { background: #1a1a20; border: 1px solid #2e2e38; padding: 4px; border-radius: 4px; }"
        "QMenu::item { padding: 5px 24px 5px 12px; border-radius: 3px; }"
        "QMenu::item:selected { background: #e855f4; color: white; }"
        "QMenu::separator { height: 1px; background: #2a2a34; margin: 4px 6px; }"
        
        "QToolBar { background: #141418; border: 1px solid #23232b; padding: 2px; spacing: 4px; border-radius: 4px; }"
        "QToolButton { background: #202026; border: 1px solid #2c2c36; border-radius: 3px; padding: 4px 8px; font-weight: 500; }"
        "QToolButton:hover { background: #2f2f3c; border-color: #e855f4; color: white; }"
        "QToolButton:pressed { background: #e855f4; color: white; }"
        "QToolButton:checked { background: #381b40; border-color: #e855f4; color: #f59ef8; }"
        
        "QPushButton { background: #22222a; border: 1px solid #30303c; border-radius: 3px; padding: 4px 10px; color: #e2e2e8; font-weight: 500; }"
        "QPushButton:hover { background: #30303d; border-color: #e855f4; color: white; }"
        "QPushButton:pressed { background: #e855f4; color: white; }"
        "QPushButton:disabled { background: #18181e; border-color: #24242e; color: #60606e; }"
        
        "QLineEdit { background: #121216; border: 1px solid #2c2c36; border-radius: 3px; padding: 4px 8px; color: white; selection-background-color: #e855f4; }"
        "QLineEdit:focus { border: 1px solid #e855f4; background: #16161c; }"
        
        "QComboBox { background: #202026; border: 1px solid #30303c; border-radius: 3px; padding: 3px 8px; color: #e2e2e8; }"
        "QComboBox:hover { border-color: #e855f4; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: #1a1a20; border: 1px solid #2e2e38; selection-background-color: #e855f4; }"
        
        "QTabWidget::pane { border: 1px solid #23232b; background: #16161a; }"
        "QTabBar::tab { background: #18181e; border: 1px solid #23232b; border-bottom: none; padding: 6px 14px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; color: #9090a0; font-weight: 600; font-size: 10px; letter-spacing: 0.5px; }"
        "QTabBar::tab:selected { background: #16161a; border-bottom: 2px solid #e855f4; color: white; }"
        "QTabBar::tab:hover:!selected { background: #22222a; color: #d0d0dc; }"
        
        "QListWidget { background: #141418; border: 1px solid #23232b; border-radius: 3px; padding: 2px; }"
        "QListWidget::item { padding: 4px 6px; border-radius: 2px; margin: 1px; }"
        "QListWidget::item:hover { background: #22222c; color: white; }"
        "QListWidget::item:selected { background: #3c1c44; border: 1px solid #e855f4; color: white; }"
        
        "QTreeWidget { background: #141418; border: 1px solid #23232b; border-radius: 3px; padding: 2px; }"
        "QTreeWidget::item { padding: 3px 4px; border-radius: 2px; }"
        "QTreeWidget::item:hover { background: #22222c; color: white; }"
        "QTreeWidget::item:selected { background: #3c1c44; border: 1px solid #e855f4; color: white; }"
        
        "QScrollBar:vertical { background: #121216; width: 8px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #2e2e3a; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #e855f4; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        
        "QScrollBar:horizontal { background: #121216; height: 8px; margin: 0px; }"
        "QScrollBar::handle:horizontal { background: #2e2e3a; min-width: 20px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: #e855f4; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        
        "QToolTip { background: #22222a; border: 1px solid #3d3d4e; color: #f0f0f5; padding: 5px 8px; border-radius: 4px; font-size: 11px; }"
        
        "QStatusBar { background: #121216; border-top: 1px solid #23232b; color: #888898; font-size: 10px; }"
        "QStatusBar::item { border: none; padding: 2px 6px; }"
    );

    MainWindow mainWin;
    mainWin.resize(1600, 900);
    mainWin.show();

    return app.exec();
}
