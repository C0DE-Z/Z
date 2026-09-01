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

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#08080A"));
    palette.setColor(QPalette::WindowText, QColor("#F7F4F6"));
    palette.setColor(QPalette::Base, QColor("#08080A"));
    palette.setColor(QPalette::AlternateBase, QColor("#19191F"));
    palette.setColor(QPalette::ToolTipBase, QColor("#19191F"));
    palette.setColor(QPalette::ToolTipText, QColor("#F7F4F6"));
    palette.setColor(QPalette::Text, QColor("#C3BEC3"));
    palette.setColor(QPalette::Button, QColor("#19191F"));
    palette.setColor(QPalette::ButtonText, QColor("#F7F4F6"));
    palette.setColor(QPalette::BrightText, QColor("#FF72AA"));
    palette.setColor(QPalette::Link, QColor("#FF4F91"));
    palette.setColor(QPalette::Highlight, QColor("#FF4F91"));
    palette.setColor(QPalette::HighlightedText, QColor("#2A0715"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#706B72"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#706B72"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#706B72"));
    app.setPalette(palette);

    app.setStyleSheet(R"(
        QWidget { background: #08080A; color: #C3BEC3; font-family: 'Segoe UI', 'Inter', sans-serif; font-size: 11px; }
        QMainWindow { background: #08080A; }
        QMainWindow::separator { background: #303036; width: 3px; height: 3px; }
        QMainWindow::separator:hover { background: #FF4F91; }
        QDockWidget { border: 1px solid #303036; background: #111116; }
        QDockWidget::title { height: 22px; padding-left: 8px; background: #111116; border-bottom: 1px solid #303036; color: #918B92; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        QMenuBar { background: #08080A; border-bottom: 1px solid #303036; padding: 2px 4px; }
        QMenuBar::item { padding: 4px 8px; margin: 1px; }
        QMenuBar::item:selected { background: #19191F; color: #FF72AA; }
        QMenu { background: #111116; border: 1px solid #4E4E58; padding: 4px; }
        QMenu::item { padding: 5px 24px 5px 12px; }
        QMenu::item:selected { background: #FF4F91; color: #2A0715; }
        QMenu::separator { height: 1px; margin: 4px 6px; background: #303036; }
        QToolBar { background: #111116; border: 0; border-bottom: 1px solid #303036; padding: 3px; spacing: 4px; }
        QToolButton, QPushButton { padding: 5px 9px; border: 1px solid #4E4E58; border-radius: 4px; background: #19191F; color: #F7F4F6; font-weight: 600; }
        QToolButton:hover, QPushButton:hover { border-color: #FF4F91; background: #24242C; }
        QToolButton:pressed, QPushButton:pressed { background: #FF4F91; color: #2A0715; }
        QToolButton:checked { border-color: #FF4F91; background: #3D1226; color: #FFB8D2; }
        QPushButton:disabled { border-color: #25252B; background: #111116; color: #706B72; }
        QLineEdit, QComboBox, QAbstractSpinBox { padding: 4px 7px; border: 1px solid #4E4E58; border-radius: 4px; background: #08080A; color: #F7F4F6; selection-background-color: #FF4F91; selection-color: #2A0715; }
        QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border-color: #FF4F91; }
        QComboBox::drop-down { border: 0; width: 18px; }
        QComboBox QAbstractItemView { background: #111116; border: 1px solid #4E4E58; selection-background-color: #FF4F91; selection-color: #2A0715; }
        QTabWidget::pane { border: 1px solid #303036; background: #111116; }
        QTabBar::tab { padding: 6px 12px; margin-right: 2px; border: 1px solid #303036; border-bottom: 0; background: #111116; color: #918B92; font-size: 10px; font-weight: 700; }
        QTabBar::tab:selected { border-bottom: 2px solid #FF4F91; background: #19191F; color: #F7F4F6; }
        QTabBar::tab:hover:!selected { color: #FF72AA; }
        QListWidget, QTreeWidget, QTableWidget { padding: 2px; border: 1px solid #303036; border-radius: 3px; background: #08080A; }
        QListWidget::item, QTreeWidget::item { padding: 4px 6px; }
        QListWidget::item:hover, QTreeWidget::item:hover { background: #19191F; }
        QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected { border: 1px solid #FF4F91; background: #3D1226; color: #F7F4F6; }
        QHeaderView::section { padding: 4px; border: 1px solid #303036; background: #19191F; color: #C3BEC3; font-weight: 700; }
        QCheckBox { spacing: 6px; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #4E4E58; border-radius: 3px; background: #08080A; }
        QCheckBox::indicator:checked { border-color: #FF4F91; background: #FF4F91; }
        QSlider::groove:horizontal { height: 3px; background: #303036; }
        QSlider::handle:horizontal { width: 12px; margin: -5px 0; border-radius: 6px; background: #FF4F91; }
        QScrollBar:vertical { width: 8px; background: #08080A; }
        QScrollBar:horizontal { height: 8px; background: #08080A; }
        QScrollBar::handle { min-height: 20px; min-width: 20px; border-radius: 3px; background: #4E4E58; }
        QScrollBar::handle:hover { background: #FF4F91; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QToolTip { padding: 5px 8px; border: 1px solid #4E4E58; background: #19191F; color: #F7F4F6; }
        QStatusBar { border-top: 1px solid #303036; background: #08080A; color: #918B92; font-size: 10px; }
        QStatusBar::item { border: 0; padding: 2px 6px; }
        QWidget#mediaContainer, QWidget#effectsContainer, QWidget#activeContainer, QWidget#tracksContainer, QWidget#controlContainer { border: 1px solid #303036; border-radius: 4px; background: #111116; }
    )");

    MainWindow mainWin;
    mainWin.resize(1600, 900);
    mainWin.show();
    return app.exec();
}
