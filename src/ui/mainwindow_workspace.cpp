#include "mainwindow.h"
#include "ui/preferencesdialog.h"
#include "ui/thememanager.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDockWidget>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>

namespace {
constexpr int kLayoutVersion = 2;
const QStringList kPresetWorkspaces = {"Editing", "Effects", "Timeline Focus"};

QString sanitizeWorkspaceName(QString name) {
    name.replace('/', '-').replace('\\', '-');
    return name.trimmed();
}

QSettings appSettings() { return QSettings("Z-Creative", "Z"); }
}

QDockWidget* MainWindow::panelByName(const QString& objectName) const {
    for (QDockWidget* dock : panelDocks) {
        if (dock->objectName() == objectName) return dock;
    }
    return nullptr;
}

void MainWindow::createWindowMenu() {
    QMenu* windowMenu = menuBar()->addMenu("&Window");

    workspaceMenu = windowMenu->addMenu("Workspaces");
    connect(workspaceMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildWorkspaceMenu);

    QMenu* panelsMenu = windowMenu->addMenu("Panels");
    connect(panelsMenu, &QMenu::aboutToShow, this, [this, panelsMenu] {
        panelsMenu->clear();
        for (QDockWidget* dock : panelDocks) panelsMenu->addAction(dock->toggleViewAction());
    });

    windowMenu->addSeparator();
    themeMenu = windowMenu->addMenu("Theme");
    connect(themeMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildThemeMenu);
    windowMenu->addAction("Customize Theme...", this, [this] { openPreferences("Appearance"); });

    QTimer::singleShot(0, this, &MainWindow::initializeWorkspaces);
}

void MainWindow::rebuildThemeMenu() {
    themeMenu->clear();
    auto* group = new QActionGroup(themeMenu);
    group->setExclusive(true);
    auto& themes = ThemeManager::instance();
    for (const QString& name : themes.themeNames()) {
        QAction* action = themeMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(name == themes.current().name);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [name] { ThemeManager::instance().setCurrent(name); });
    }
}

void MainWindow::initializeWorkspaces() {
    defaultWorkspaceState = saveState(kLayoutVersion);
    defaultWorkspaceGeometry = saveGeometry();

    QSettings settings = appSettings();
    if (settings.contains("Layout/Geometry")) restoreGeometry(settings.value("Layout/Geometry").toByteArray());
    if (settings.contains("Layout/State")) restoreState(settings.value("Layout/State").toByteArray(), kLayoutVersion);
}

void MainWindow::rebuildWorkspaceMenu() {
    workspaceMenu->clear();
    for (const QString& preset : kPresetWorkspaces) {
        workspaceMenu->addAction(preset, this, [this, preset] { applyWorkspacePreset(preset); });
    }

    QSettings settings = appSettings();
    settings.beginGroup("Workspaces");
    const QStringList saved = settings.childGroups();
    settings.endGroup();

    if (!saved.isEmpty()) {
        workspaceMenu->addSeparator();
        for (const QString& name : saved) {
            workspaceMenu->addAction(name, this, [this, name] { applyWorkspace(name); });
        }
    }

    workspaceMenu->addSeparator();
    workspaceMenu->addAction("Save Current Layout As...", this, &MainWindow::saveCurrentLayout);
    if (!saved.isEmpty()) {
        QMenu* deleteMenu = workspaceMenu->addMenu("Delete Saved Layout");
        for (const QString& name : saved) {
            deleteMenu->addAction(name, this, [this, name] {
                if (QMessageBox::question(this, "Delete Layout", QString("Delete the saved layout \"%1\"?").arg(name)) != QMessageBox::Yes) return;
                QSettings s = appSettings();
                s.remove("Workspaces/" + name);
            });
        }
    }
    workspaceMenu->addAction("Reset to Default Layout", this, [this] { applyWorkspacePreset("Editing"); });
}

void MainWindow::saveCurrentLayout() {
    bool accepted = false;
    const QString name = sanitizeWorkspaceName(QInputDialog::getText(this, "Save Layout", "Layout name:", QLineEdit::Normal, QString(), &accepted));
    if (!accepted || name.isEmpty()) return;
    if (kPresetWorkspaces.contains(name, Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Save Layout", "That name is used by a built-in workspace. Choose another name.");
        return;
    }
    QSettings settings = appSettings();
    if (settings.contains("Workspaces/" + name + "/State") &&
        QMessageBox::question(this, "Save Layout", QString("Replace the saved layout \"%1\"?").arg(name)) != QMessageBox::Yes) {
        return;
    }
    settings.setValue("Workspaces/" + name + "/State", saveState(kLayoutVersion));
    statusBar()->showMessage(QString("Saved layout \"%1\".").arg(name), 3000);
}

bool MainWindow::applyWorkspace(const QString& name) {
    const QByteArray state = appSettings().value("Workspaces/" + name + "/State").toByteArray();
    if (state.isEmpty() || !restoreState(state, kLayoutVersion)) {
        statusBar()->showMessage(QString("Could not load layout \"%1\".").arg(name), 3000);
        return false;
    }
    statusBar()->showMessage(QString("Layout \"%1\" loaded.").arg(name), 2000);
    return true;
}

void MainWindow::applyWorkspacePreset(const QString& preset) {
    if (!defaultWorkspaceState.isEmpty()) restoreState(defaultWorkspaceState, kLayoutVersion);
    for (QDockWidget* dock : panelDocks) {
        dock->setFloating(false);
        dock->show();
    }

    QDockWidget* inspector = panelByName("panelInspector");
    QDockWidget* project = panelByName("panelProject");
    QDockWidget* timeline = panelByName("panelTimeline");

    if (preset == "Effects" && inspector && project) {
        if (QDockWidget* effects = panelByName("panelEffects")) effects->raise();
        resizeDocks({project, inspector}, {320, 420}, Qt::Horizontal);
    } else if (preset == "Timeline Focus" && timeline) {
        for (QDockWidget* dock : panelDocks) {
            if (dock != timeline) dock->hide();
        }
        resizeDocks({timeline}, {height() / 2}, Qt::Vertical);
    }
    statusBar()->showMessage(QString("Workspace \"%1\" applied.").arg(preset), 2000);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (importInProgress) {
        statusBar()->showMessage("Wait for the current media import to finish before closing Z.", 5000);
        event->ignore();
        return;
    }
    if (!maybeSaveProject()) {
        event->ignore();
        return;
    }
    QSettings settings = appSettings();
    settings.setValue("Layout/Geometry", saveGeometry());
    settings.setValue("Layout/State", saveState(kLayoutVersion));
    QMainWindow::closeEvent(event);
}
