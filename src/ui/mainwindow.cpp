#include "mainwindow.h"
#include "core/project.h"
#include "engine/audioengine.h"
#include "engine/pluginmanager.h"
#include <QFileDialog>
#include <QMenuBar>
#include <QActionGroup>
#include <QInputDialog>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QTreeWidgetItemIterator>
#include <QComboBox>
#include <QToolBar>
#include <QPushButton>
#include <QHeaderView>
#include <QShortcut>
#include <QScrollArea>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QDebug>
#include <QProgressDialog>
#include <QMetaObject>
#include <QProcess>
#include <QDir>
#include <QLineEdit>
#include <QMessageBox>
#include <QStatusBar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QSettings>
#include <QUrl>
#include "ui/preferencesdialog.h"
#include <algorithm>
#include <array>
#include <set>
#include <thread>
#include <limits>
#include <cmath>
#include <future>
#include <chrono>
#include <optional>
#include "utils/logging.h"
#include "engine/videoengine.h"
#include "media/mediaexporter.h"
#include "core/appstate.h"
#include "core/shortcutmanager.h"
#include "ui/vector_icons.h"
#include "media/mediaimporter.h"

namespace {
constexpr auto kLatestReleaseApi = "https://api.github.com/repos/C0DE-Z/Z/releases/latest";

std::optional<std::array<int, 3>> parseSemanticVersion(QString version) {
    version = version.trimmed();
    if (version.startsWith('v', Qt::CaseInsensitive)) version.remove(0, 1);
    const QStringList parts = version.split('.');
    if (parts.size() != 3) return std::nullopt;

    std::array<int, 3> parsed{};
    for (int i = 0; i < 3; ++i) {
        bool valid = false;
        parsed[i] = parts[i].toInt(&valid);
        if (!valid || parsed[i] < 0 || QString::number(parsed[i]) != parts[i]) return std::nullopt;
    }
    return parsed;
}

bool isNewerVersion(const std::array<int, 3>& candidate, const std::array<int, 3>& current) {
    return candidate > current;
}

template <typename Fn>
auto runWithLoader(QWidget* parent, const QString& label, Fn&& fn) {
    (void)parent;
    (void)label;
    using ReturnT = std::invoke_result_t<Fn>;
    auto future = std::async(std::launch::async, std::forward<Fn>(fn));
    if constexpr (std::is_void_v<ReturnT>) {
        future.get();
    } else {
        return future.get();
    }
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setWindowTitle("Z");
    setStyleSheet(R"(
        * {
            font-family: 'Segoe UI', 'JetBrains Mono', 'Consolas', monospace;
            font-size: 11px;
            color: #b0b0c0;
        }
        QMainWindow {
            background: #0b0b0e;
        }
        QDockWidget {
            background: #0b0b0e;
            border: none;
        }
        QWidget#mediaContainer, QWidget#effectsContainer, QWidget#activeContainer, QWidget#tracksContainer {
            background-color: #141418;
            border: 1px solid #23232b;
            border-radius: 4px;
        }
        QWidget#controlContainer {
            background-color: #141418;
            border: 1px solid #23232b;
            border-radius: 4px;
            padding: 4px;
        }
        QListWidget {
            background: transparent;
            border: none;
        }
        QListWidget::item {
            padding: 5px;
            border-bottom: 1px solid #1a1a20;
            color: #b0b0c0;
        }
        QListWidget::item:selected {
            background: #2b1230;
            color: #e855f4;
            font-weight: bold;
        }
        QPushButton {
            background: #1c1c24;
            border: 1px solid #2e2e3a;
            color: #e2e2ea;
            padding: 4px 8px;
            border-radius: 3px;
        }
        QPushButton:hover {
            background: #2b1230;
            border-color: #df42f5;
            color: #e855f4;
        }
    )");

    AudioEngine::instance().init();
    QString pluginsPath = QCoreApplication::applicationDirPath() + "/plugins";
    if (!QDir(pluginsPath).exists() && QDir(QDir::currentPath() + "/plugins").exists()) {
        pluginsPath = QDir::currentPath() + "/plugins";
    }
    PluginManager::instance().createDefaultPlugins(pluginsPath.toStdString());
    PluginManager::instance().scanPluginsDir(pluginsPath.toStdString());

    QWidget* centerWidget = new QWidget(this);
    centerWidget->setAttribute(Qt::WA_TranslucentBackground, false);
    centerWidget->setAttribute(Qt::WA_NoSystemBackground, false);
    centerWidget->setStyleSheet("background: #0b0b0e;");
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    glWidget = new GLWidget(centerWidget);
    centerLayout->addWidget(glWidget, 1);
    modelDownloadManager = new QNetworkAccessManager(this);
    updateCheckManager = new QNetworkAccessManager(this);

    setCentralWidget(centerWidget);

    createActions();
    createMenus();
    createTransportToolbar();
    createDocks();
    applyShortcuts();

    connect(&ShortcutManager::instance(), &ShortcutManager::shortcutsChanged, this, &MainWindow::applyShortcuts);

    playbackTimer = new QTimer(this);
    playbackTimer->setTimerType(Qt::PreciseTimer);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTimer);
    liveDetectionTimer.start();

    statusBar()->showMessage("Ready", 3000);
    updateStatusBar();
    updateEffectsState();
    QTimer::singleShot(1500, this, [this] { checkForUpdates(false); });
}

MainWindow::~MainWindow() {
    if (importThread.joinable()) {
        importThread.join();
    }
    AudioEngine::instance().shutdown();
}

void MainWindow::createTransportToolbar() {
    QWidget* transportWidget = new QWidget(this);
    transportWidget->setObjectName("transportBar");
    transportWidget->setFixedHeight(38);
    transportWidget->setStyleSheet("QWidget#transportBar { background: #0f0d14; border-top: 1px solid #231a2c; border-bottom: 1px solid #231a2c; }");

    QHBoxLayout* layout = new QHBoxLayout(transportWidget);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(5);

    timecodeLabel = new QLabel("00:00:00.00", transportWidget);
    timecodeLabel->setStyleSheet("font-family: 'JetBrains Mono', monospace; font-size: 12px; font-weight: bold; color: #f59ef8; background: #08080c; padding: 3px 8px; border: 1px solid #3b1d4c; border-radius: 3px; min-width: 95px;");
    timecodeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(timecodeLabel);

    layout->addSpacing(8);

    auto makeBtn = [transportWidget](VectorIcon::Type iconType, const QString& tooltip, int width = 30) {
        QPushButton* btn = new QPushButton(transportWidget);
        btn->setIcon(VectorIcon::create(iconType, QColor(220, 215, 235), QSize(16, 16)));
        btn->setIconSize(QSize(16, 16));
        btn->setToolTip(tooltip);
        btn->setFixedSize(width, 26);
        btn->setStyleSheet("QPushButton { background: #181520; border: 1px solid #291e34; border-radius: 3px; } QPushButton:hover { background: #2b1c3a; border-color: #c026d3; } QPushButton:pressed { background: #c026d3; }");
        return btn;
    };

    QPushButton* jumpStartBtn = makeBtn(VectorIcon::Type::JumpStart, "Jump to Start (Home)");
    connect(jumpStartBtn, &QPushButton::clicked, this, [this]() {
        onTimelineScrubbed(0.0);
    });
    layout->addWidget(jumpStartBtn);

    QPushButton* stepBackBtn = makeBtn(VectorIcon::Type::StepBackward, "Previous Frame (Left Arrow)");
    connect(stepBackBtn, &QPushButton::clicked, this, [this]() {
        double newTime = std::max(0.0, currentPlayhead - (1.0 / 30.0));
        onTimelineScrubbed(newTime);
    });
    layout->addWidget(stepBackBtn);

    playPauseBtn = new QPushButton(transportWidget);
    playPauseBtn->setIcon(VectorIcon::create(isPlaying ? VectorIcon::Type::Pause : VectorIcon::Type::Play, QColor(245, 158, 248), QSize(18, 18)));
    playPauseBtn->setIconSize(QSize(18, 18));
    playPauseBtn->setToolTip("Play / Pause (Space)");
    playPauseBtn->setFixedSize(44, 26);
    playPauseBtn->setStyleSheet("QPushButton { background: #3c144c; border: 1px solid #d946ef; border-radius: 3px; } QPushButton:hover { background: #551c6b; }");
    connect(playPauseBtn, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    layout->addWidget(playPauseBtn);

    QPushButton* stepFwdBtn = makeBtn(VectorIcon::Type::StepForward, "Next Frame (Right Arrow)");
    connect(stepFwdBtn, &QPushButton::clicked, this, [this]() {
        double newTime = std::min(Project::instance().getDuration(), currentPlayhead + (1.0 / 30.0));
        onTimelineScrubbed(newTime);
    });
    layout->addWidget(stepFwdBtn);

    QPushButton* jumpEndBtn = makeBtn(VectorIcon::Type::JumpEnd, "Jump to End (End)");
    connect(jumpEndBtn, &QPushButton::clicked, this, [this]() {
        onTimelineScrubbed(Project::instance().getDuration());
    });
    layout->addWidget(jumpEndBtn);

    layout->addSpacing(8);

    loopBtn = makeBtn(VectorIcon::Type::Loop, "Toggle Loop Playback", 32);
    loopBtn->setCheckable(true);
    loopBtn->setChecked(loopPlayback);
    connect(loopBtn, &QPushButton::toggled, this, [this](bool checked) {
        loopPlayback = checked;
    });
    layout->addWidget(loopBtn);

    layout->addSpacing(10);

    QPushButton* markInBtn = makeBtn(VectorIcon::Type::MarkIn, "Set Mark In (I)", 32);
    connect(markInBtn, &QPushButton::clicked, this, [this]() {
        markIn = std::max(0.0, currentPlayhead);
        if (markOut >= 0.0 && markOut < markIn) markOut = markIn;
        if (timelinePanel) timelinePanel->setInOutPoints(markIn, markOut);
    });
    layout->addWidget(markInBtn);

    QPushButton* markOutBtn = makeBtn(VectorIcon::Type::MarkOut, "Set Mark Out (O)", 32);
    connect(markOutBtn, &QPushButton::clicked, this, [this]() {
        markOut = std::max(0.0, currentPlayhead);
        if (markIn >= 0.0 && markOut < markIn) markIn = markOut;
        if (timelinePanel) timelinePanel->setInOutPoints(markIn, markOut);
    });
    layout->addWidget(markOutBtn);

    QPushButton* clearInOutBtn = makeBtn(VectorIcon::Type::Clear, "Clear In/Out Range (Ctrl+Shift+X)", 32);
    connect(clearInOutBtn, &QPushButton::clicked, this, [this]() {
        markIn = -1.0;
        markOut = -1.0;
        if (timelinePanel) timelinePanel->clearInOutPoints();
    });
    layout->addWidget(clearInOutBtn);

    layout->addStretch();

    QPushButton* zoomInBtn = makeBtn(VectorIcon::Type::ZoomIn, "Zoom In Timeline (+)", 30);
    connect(zoomInBtn, &QPushButton::clicked, this, [this]() {
        if (timelinePanel) timelinePanel->zoomIn();
    });
    layout->addWidget(zoomInBtn);

    QPushButton* zoomOutBtn = makeBtn(VectorIcon::Type::ZoomOut, "Zoom Out Timeline (-)", 30);
    connect(zoomOutBtn, &QPushButton::clicked, this, [this]() {
        if (timelinePanel) timelinePanel->zoomOut();
    });
    layout->addWidget(zoomOutBtn);

    QPushButton* zoomFitBtn = makeBtn(VectorIcon::Type::ZoomFit, "Fit Timeline to Window (Ctrl+0)", 30);
    connect(zoomFitBtn, &QPushButton::clicked, this, [this]() {
        if (timelinePanel) timelinePanel->zoomFit(timelinePanel->width());
    });
    layout->addWidget(zoomFitBtn);

    centralWidget()->layout()->addWidget(transportWidget);
}

void MainWindow::updateTimecodeDisplay(double time) {
    if (!timecodeLabel) return;
    int totalMs = static_cast<int>(std::max(0.0, time) * 1000.0);
    int hours = totalMs / 3600000;
    int mins = (totalMs % 3600000) / 60000;
    int secs = (totalMs % 60000) / 1000;
    int frames = static_cast<int>((time - std::floor(time)) * 30.0);
    timecodeLabel->setText(QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(frames, 2, 10, QChar('0')));
}

void MainWindow::updateStatusBar() {
    if (!statusBar()) return;
    double dur = Project::instance().getDuration();
    int mins = static_cast<int>(dur) / 60;
    int secs = static_cast<int>(dur) % 60;
    size_t clipCount = 0;
    for (const auto& t : Project::instance().getTracks()) {
        clipCount += t.clips.size();
    }
    if (!projectInfoStatusLabel) {
        projectInfoStatusLabel = new QLabel(this);
        projectInfoStatusLabel->setStyleSheet("color: #888898; font-size: 10px; padding-right: 12px;");
        statusBar()->addPermanentWidget(projectInfoStatusLabel);
    }
    projectInfoStatusLabel->setText(QString("Project: %1 tracks, %2 clips | Duration: %3:%4 | Undo: %5")
        .arg(Project::instance().getTracks().size())
        .arg(clipCount)
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(AppState::instance().undoStackSize()));
}

void MainWindow::openPreferences() {
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Preferences updated.", 3000);
    }
}

void MainWindow::createActions() {
    importAct = new QAction("&Import Video Clip...", this);
    importAct->setIcon(VectorIcon::create(VectorIcon::Type::Import, QColor(220, 220, 235), QSize(16, 16)));
    connect(importAct, &QAction::triggered, this, &MainWindow::importVideo);

    openAct = new QAction("&Open Project...", this);
    connect(openAct, &QAction::triggered, this, &MainWindow::openProject);

    saveAct = new QAction("&Save Project...", this);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveProject);

    exportAct = new QAction("&Export Video...", this);
    connect(exportAct, &QAction::triggered, this, &MainWindow::exportVideo);

    cutAct = new QAction("Cut Clip at Playhead", this);
    cutAct->setIcon(VectorIcon::create(VectorIcon::Type::Cut, QColor(220, 220, 235), QSize(16, 16)));
    connect(cutAct, &QAction::triggered, this, &MainWindow::cutClipAtPlayhead);

    deleteAct = new QAction("Delete Selected", this);
    deleteAct->setIcon(VectorIcon::create(VectorIcon::Type::Trash, QColor(220, 220, 235), QSize(16, 16)));
    connect(deleteAct, &QAction::triggered, this, &MainWindow::deleteSelectedClip);

    undoAct = new QAction("Undo", this);
    connect(undoAct, &QAction::triggered, this, [this]() {
        if (AppState::instance().undo()) {
            refreshTrackList();
            refreshActiveEffectsList();
            if (timelinePanel) timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
            statusBar()->showMessage("Undo performed.", 2000);
            updateStatusBar();
        }
    });

    redoAct = new QAction("Redo", this);
    connect(redoAct, &QAction::triggered, this, [this]() {
        if (AppState::instance().redo()) {
            refreshTrackList();
            refreshActiveEffectsList();
            if (timelinePanel) timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
            statusBar()->showMessage("Redo performed.", 2000);
            updateStatusBar();
        }
    });

    prefAct = new QAction("Preferences...", this);
    prefAct->setIcon(VectorIcon::create(VectorIcon::Type::Settings, QColor(220, 220, 235), QSize(16, 16)));
    connect(prefAct, &QAction::triggered, this, &MainWindow::openPreferences);

    playPauseAct = new QAction("Toggle Play / Pause", this);
    connect(playPauseAct, &QAction::triggered, this, &MainWindow::togglePlayback);

    jumpStartAct = new QAction("Jump to Start", this);
    connect(jumpStartAct, &QAction::triggered, this, [this]() { onTimelineScrubbed(0.0); });

    jumpEndAct = new QAction("Jump to End", this);
    connect(jumpEndAct, &QAction::triggered, this, [this]() { onTimelineScrubbed(Project::instance().getDuration()); });

    stepFwdAct = new QAction("Step Forward 1 Frame", this);
    connect(stepFwdAct, &QAction::triggered, this, [this]() {
        double newTime = std::min(Project::instance().getDuration(), currentPlayhead + (1.0 / 30.0));
        onTimelineScrubbed(newTime);
    });

    stepBackAct = new QAction("Step Backward 1 Frame", this);
    connect(stepBackAct, &QAction::triggered, this, [this]() {
        double newTime = std::max(0.0, currentPlayhead - (1.0 / 30.0));
        onTimelineScrubbed(newTime);
    });

    markInAct = new QAction("Set Mark In", this);
    connect(markInAct, &QAction::triggered, this, [this]() {
        markIn = std::max(0.0, currentPlayhead);
        if (markOut >= 0.0 && markOut < markIn) markOut = markIn;
        if (timelinePanel) timelinePanel->setInOutPoints(markIn, markOut);
    });

    markOutAct = new QAction("Set Mark Out", this);
    connect(markOutAct, &QAction::triggered, this, [this]() {
        markOut = std::max(0.0, currentPlayhead);
        if (markIn >= 0.0 && markOut < markIn) markIn = markOut;
        if (timelinePanel) timelinePanel->setInOutPoints(markIn, markOut);
    });

    clearInOutAct = new QAction("Clear In/Out Range", this);
    connect(clearInOutAct, &QAction::triggered, this, [this]() {
        markIn = -1.0;
        markOut = -1.0;
        if (timelinePanel) timelinePanel->clearInOutPoints();
    });

    zoomInAct = new QAction("Zoom In Timeline", this);
    connect(zoomInAct, &QAction::triggered, this, [this]() { if (timelinePanel) timelinePanel->zoomIn(); });

    zoomOutAct = new QAction("Zoom Out Timeline", this);
    connect(zoomOutAct, &QAction::triggered, this, [this]() { if (timelinePanel) timelinePanel->zoomOut(); });

    zoomFitAct = new QAction("Fit Timeline to Window", this);
    connect(zoomFitAct, &QAction::triggered, this, [this]() { if (timelinePanel) timelinePanel->zoomFit(timelinePanel->width()); });
}

void MainWindow::applyShortcuts() {
    auto& sm = ShortcutManager::instance();
    if (importAct) importAct->setShortcut(sm.getShortcut("file.import"));
    if (openAct) openAct->setShortcut(sm.getShortcut("file.open_project"));
    if (saveAct) saveAct->setShortcut(sm.getShortcut("file.save_project"));
    if (exportAct) exportAct->setShortcut(sm.getShortcut("file.export_video"));
    if (cutAct) cutAct->setShortcut(sm.getShortcut("edit.cut_clip"));
    if (deleteAct) deleteAct->setShortcut(sm.getShortcut("edit.delete_clip"));
    if (undoAct) undoAct->setShortcut(sm.getShortcut("edit.undo"));
    if (redoAct) redoAct->setShortcut(sm.getShortcut("edit.redo"));
    if (prefAct) prefAct->setShortcut(sm.getShortcut("edit.preferences"));
    if (playPauseAct) playPauseAct->setShortcut(sm.getShortcut("playback.toggle"));
    if (jumpStartAct) jumpStartAct->setShortcut(sm.getShortcut("playback.jump_start"));
    if (jumpEndAct) jumpEndAct->setShortcut(sm.getShortcut("playback.jump_end"));
    if (stepFwdAct) stepFwdAct->setShortcut(sm.getShortcut("playback.step_fwd"));
    if (stepBackAct) stepBackAct->setShortcut(sm.getShortcut("playback.step_back"));
    if (markInAct) markInAct->setShortcut(sm.getShortcut("playback.mark_in"));
    if (markOutAct) markOutAct->setShortcut(sm.getShortcut("playback.mark_out"));
    if (clearInOutAct) clearInOutAct->setShortcut(sm.getShortcut("playback.clear_inout"));
    if (zoomInAct) zoomInAct->setShortcut(sm.getShortcut("view.zoom_in"));
    if (zoomOutAct) zoomOutAct->setShortcut(sm.getShortcut("view.zoom_out"));
    if (zoomFitAct) zoomFitAct->setShortcut(sm.getShortcut("view.zoom_fit"));
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(importAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(exportAct);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(cutAct);
    editMenu->addAction(deleteAct);
    editMenu->addSeparator();
    editMenu->addAction(prefAct);

    QMenu* playbackMenu = menuBar()->addMenu("&Playback");
    playbackMenu->addAction(playPauseAct);
    playbackMenu->addAction(stepBackAct);
    playbackMenu->addAction(stepFwdAct);
    playbackMenu->addAction(jumpStartAct);
    playbackMenu->addAction(jumpEndAct);
    playbackMenu->addSeparator();
    playbackMenu->addAction(markInAct);
    playbackMenu->addAction(markOutAct);
    playbackMenu->addAction(clearInOutAct);

    QMenu* effectsMenu = menuBar()->addMenu("&Effects");
    effectsMenu->addAction("Reload Plugins", this, [this]() {
        QString pluginsPath = QApplication::applicationDirPath() + "/plugins";
        PluginManager::instance().scanPluginsDir(pluginsPath.toStdString());
        updateEffectsState();
        statusBar()->showMessage("Plugins reloaded.", 2000);
    });

    QMenu* viewMenu = menuBar()->addMenu("&View");
    QAction* verboseLogsAct = viewMenu->addAction("Verbose Logs");
    verboseLogsAct->setCheckable(true);
    verboseLogsAct->setChecked(AppLogging::enabled());
    connect(verboseLogsAct, &QAction::toggled, this, [](bool enabled) {
        AppLogging::setEnabled(enabled);
    });

    QAction* asyncDecodeAct = viewMenu->addAction("Async Video Decode");
    asyncDecodeAct->setCheckable(true);
    asyncDecodeAct->setChecked(true);
    connect(asyncDecodeAct, &QAction::toggled, this, [](bool enabled) {
        VideoEngine::instance().setAsyncDecodeEnabled(enabled);
    });

    QMenu* rendererMenu = viewMenu->addMenu("Rendering Engine");
    QActionGroup* rendererGroup = new QActionGroup(rendererMenu);
    rendererGroup->setExclusive(true);
    QAction* compatibilityRenderer = rendererMenu->addAction("Compatibility OpenGL");
    compatibilityRenderer->setCheckable(true);
    rendererGroup->addAction(compatibilityRenderer);
    QAction* pipelinedRenderer = rendererMenu->addAction("Pipelined OpenGL (Experimental)");
    pipelinedRenderer->setCheckable(true);
    pipelinedRenderer->setChecked(true);
    rendererGroup->addAction(pipelinedRenderer);
    connect(compatibilityRenderer, &QAction::triggered, this, [this] {
        glWidget->setRendererBackend(RenderBackendKind::OpenGLCompatibility);
        statusBar()->showMessage("Compatibility OpenGL renderer enabled", 2500);
    });
    connect(pipelinedRenderer, &QAction::triggered, this, [this] {
        glWidget->setRendererBackend(RenderBackendKind::OpenGLPipelined);
        statusBar()->showMessage("Pipelined OpenGL renderer enabled", 2500);
    });

    QMenu* guidesMenu = viewMenu->addMenu("Guide Overlay");
    QActionGroup* guideGroup = new QActionGroup(guidesMenu);
    guideGroup->setExclusive(true);
    const std::vector<std::pair<QString, GLWidget::GuideOverlay>> guides = {
        {"No Guides", GLWidget::GuideOverlay::None},
        {"Center Cross", GLWidget::GuideOverlay::Center},
        {"Rule of Thirds", GLWidget::GuideOverlay::RuleOfThirds},
        {"Title / Action Safe Areas", GLWidget::GuideOverlay::SafeAreas},
        {"8 × 8 Alignment Grid", GLWidget::GuideOverlay::Grid},
    };
    for (const auto& [label, guide] : guides) {
        QAction* action = guidesMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(guide == GLWidget::GuideOverlay::None);
        guideGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, guide] {
            glWidget->setGuideOverlay(guide);
        });
    }

    viewMenu->addSeparator();
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(zoomFitAct);

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("Check for Updates...", this, [this] { checkForUpdates(true); });
    helpMenu->addAction("&About Z...", this, [this]() {
        QMessageBox::about(this, "About Z",
            "<h2>Z - A Video Editor</h2>"
            "<p><b>Version " + QApplication::applicationVersion() + "</b></p>"
            "<p>Made for experimental video art.</p>"
            "<p>Website: <a href='https://z.codezey.dev'>https://z.codezey.dev</a></p>"
        );
    });

    VideoEngine::instance().setAsyncDecodeEnabled(true);
    QToolBar* mainToolBar = addToolBar("Main Toolbar");
    mainToolBar->addWidget(new QLabel(" Playback Quality: ", this));
    QComboBox* qualityCombo = new QComboBox(this);
    qualityCombo->addItem("100% (Full)");
    qualityCombo->addItem("50% (Half)");
    qualityCombo->addItem("25% (Quarter)");
    connect(qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPlaybackQualityChanged);
    mainToolBar->addWidget(qualityCombo);
    mainToolBar->addSeparator();
    QAction* fpsOverlayAct = mainToolBar->addAction("Show FPS Overlay");
    fpsOverlayAct->setCheckable(true);
    fpsOverlayAct->setChecked(true);
    connect(fpsOverlayAct, &QAction::toggled, this, &MainWindow::onToggleFpsOverlay);
}

void MainWindow::refreshTrackList() {
    if (trackControl) {
        trackControl->populateTracks(Project::instance().getTracks());
        if (mediaPool) mediaPool->clearMedia();
    }
}

void MainWindow::sortTrackClips(TimelineTrack& track) {
    std::sort(track.clips.begin(), track.clips.end(), [](const ProjectClip& a, const ProjectClip& b) {
        if (a.timelineStart == b.timelineStart) {
            return a.id < b.id;
        }
        return a.timelineStart < b.timelineStart;
    });
}

void MainWindow::selectTrackIndex(int index) {
    const auto& tracks = Project::instance().getTracks();
    if (tracks.empty()) {
        refreshActiveEffectsList();
        inspectorPanel->clearInspector();
        syncEffectStackToRenderer();
        timelinePanel->update();
        return;
    }

    if (index < 0 || index >= static_cast<int>(tracks.size())) {
        index = 0;
    }

    if (trackControl) trackControl->selectTrack(index);

    refreshActiveEffectsList();
    syncEffectStackToRenderer();
    timelinePanel->update();

    if (activeEffectsList && activeEffectsList->currentItem()) {
        onActiveEffectSelected(activeEffectsList->currentItem());
    } else {
        inspectorPanel->clearInspector();
    }
}

void MainWindow::selectClip(int trackIndex, int clipIndex) {
    auto& tracks = Project::instance().getTracks();
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
    auto& track = tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= static_cast<int>(track.clips.size())) return;

    const auto& clip = track.clips[clipIndex];
    selectedClipId = QString::fromStdString(clip.id);
    activeClipId = QString::fromStdString(clip.id);
    activeFilePath = QString::fromStdString(clip.filePath);
    std::vector<float> audioSamples;
    const std::string mediaId = clip.mediaId.empty() ? clip.id : clip.mediaId;
    if (VideoEngine::instance().getAudioSamples(mediaId, audioSamples)) {
        AudioEngine::instance().loadClipSamples(audioSamples, clip.timelineStart, clip.sourceStart, clip.sourceDuration);
    } else {
        AudioEngine::instance().clearClipSamples();
    }
    if (mediaPool) mediaPool->addMedia(activeClipId);
    refreshActiveEffectsList();
    syncEffectStackToRenderer();
    onTimelineScrubbed(currentPlayhead);


}

void MainWindow::refreshInspectorForSelectedEffect() {
    if (activeEffectsList && activeEffectsList->currentItem()) {
        onActiveEffectSelected(activeEffectsList->currentItem());
    }
}

const ProjectClip* MainWindow::clipAtTime(const TimelineTrack& track, double time) const {
    for (const auto& clip : track.clips) {
        const double endTime = clip.timelineStart + clip.sourceDuration;
        if (time >= clip.timelineStart && time < endTime) {
            return &clip;
        }
    }
    return nullptr;
}

ProjectClip* MainWindow::clipAtTime(TimelineTrack& track, double time) const {
    for (auto& clip : track.clips) {
        const double endTime = clip.timelineStart + clip.sourceDuration;
        if (time >= clip.timelineStart && time < endTime) {
            return &clip;
        }
    }
    return nullptr;
}

void MainWindow::moveSelectedTrack(int direction) {
    auto& tracks = Project::instance().getTracks();
    if (!trackControl || tracks.size() < 2) return;

    int index = trackControl->getSelectedTrack();
    int target = index + direction;
    if (index < 0 || target < 0 || target >= static_cast<int>(tracks.size())) return;

    std::swap(tracks[index], tracks[target]);
    refreshTrackList();
    selectTrackIndex(target);
}

void MainWindow::deleteSelectedTrack() {
    AppState::instance().pushUndoState();
    auto& tracks = Project::instance().getTracks();
    if (!trackControl || tracks.empty()) return;

    int index = trackControl->getSelectedTrack();
    if (index < 0 || index >= static_cast<int>(tracks.size())) return;

    tracks.erase(tracks.begin() + index);
    if (tracks.empty()) {
        activeClipId.clear();
        activeFilePath.clear();
        if (mediaPool) mediaPool->clearMedia();
        inspectorPanel->clearInspector();
        glWidget->setActiveEffects({});
        timelinePanel->update();
    }

    refreshTrackList();
    selectTrackIndex(std::min(index, static_cast<int>(tracks.size()) - 1));
}

void MainWindow::deleteSelectedClip() {
    if (timelinePanel) {
        int tTrack = timelinePanel->getSelectedTransTrackIndex();
        int tIdx = timelinePanel->getSelectedTransIndex();
        if (tTrack >= 0 && tIdx >= 0) {
            auto& tracks = Project::instance().getTracks();
            if (tTrack < static_cast<int>(tracks.size())) {
                auto& transitions = tracks[tTrack].transitions;
                if (tIdx < static_cast<int>(transitions.size())) {
                    AppState::instance().pushUndoState();
                    transitions.erase(transitions.begin() + tIdx);
                    timelinePanel->clearSelection();
                    timelinePanel->update();
                    return;
                }
            }
        }
    }

    if (selectedClipId.isEmpty()) return;
    AppState::instance().pushUndoState();
    auto& tracks = Project::instance().getTracks();
    for (auto& track : tracks) {
        auto it = std::remove_if(track.clips.begin(), track.clips.end(),
                                 [this](const ProjectClip& c) { return c.id == selectedClipId.toStdString(); });
        if (it != track.clips.end()) {
            track.clips.erase(it, track.clips.end());
            break; 
        }
    }
    if (activeClipId == selectedClipId) {
        activeClipId.clear();
        activeFilePath.clear();
    }
    selectedClipId.clear();
    AudioEngine::instance().clearClipSamples();
    if (mediaPool) mediaPool->clearSelection();
    if (timelinePanel) timelinePanel->clearSelection();
    inspectorPanel->clearInspector();
    refreshTrackList();
    timelinePanel->update();
    onTimelineScrubbed(currentPlayhead);
}

void MainWindow::cutClipAtPlayhead() {
    const bool wasPlaying = isPlaying;
    if (wasPlaying) {
        togglePlayback();
    }

    AppState::instance().pushUndoState();
    auto& tracks = Project::instance().getTracks();
    if (tracks.empty()) {
        if (wasPlaying) togglePlayback();
        return;
    }

    int index = trackControl ? trackControl->getSelectedTrack() : 0;
    if (index < 0 || index >= static_cast<int>(tracks.size())) index = 0;

    auto& track = tracks[index];
    ProjectClip* clip = clipAtTime(track, currentPlayhead);
    if (!clip) {
        if (wasPlaying) togglePlayback();
        return;
    }

    const std::string mediaId = clip->mediaId.empty() ? clip->id : clip->mediaId;
    const double fps = std::max(1.0, VideoEngine::instance().getFps(mediaId));
    const double relTime = std::max(0.0, currentPlayhead - clip->timelineStart);
    const int totalFrames = std::max(2, static_cast<int>(std::round(clip->sourceDuration * fps)));
    // Snap to the frame boundary currently on screen; that frame becomes the start of the right clip.
    int cutFrame = static_cast<int>(std::floor((relTime + 1e-5) * fps));
    cutFrame = std::clamp(cutFrame, 1, totalFrames - 1);
    const double leftDuration = static_cast<double>(cutFrame) / fps;
    const double cutTime = clip->timelineStart + leftDuration;
    const double rightDuration = clip->sourceDuration - leftDuration;
    if (leftDuration <= 0.001 || rightDuration <= 0.001) {
        if (wasPlaying) togglePlayback();
        return;
    }

    if (!clip->useClipEffects) {
        clip->effects = track.effects;
        clip->useClipEffects = true;
    }

    ProjectClip rightClip = *clip;
    rightClip.id = clip->id + "_cut_" + QString::number(cutTime, 'f', 3).replace('.', '_').toStdString();
    rightClip.sourceStart = clip->sourceStart + leftDuration;
    rightClip.sourceDuration = rightDuration;
    rightClip.timelineStart = cutTime;
    rightClip.effects.clear();
    rightClip.useClipEffects = true;

    clip->sourceDuration = leftDuration;

    track.clips.insert(std::upper_bound(track.clips.begin(), track.clips.end(), rightClip.timelineStart,
        [](double time, const ProjectClip& c) { return time < c.timelineStart; }), rightClip);

    currentPlayhead = cutTime;

    sortTrackClips(track);
    refreshTrackList();
    refreshActiveEffectsList();
    timelinePanel->update();
    onTimelineScrubbed(currentPlayhead);

    if (wasPlaying) {
        togglePlayback();
    }
}

void MainWindow::updateEffectsState() {
    if (effectsBrowser) {
        effectsBrowser->populateEffects();
    }
    refreshActiveEffectsList();
}

void MainWindow::importVideo() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Import Video Clip",
        activeFilePath.isEmpty() ? QDir::homePath() : QFileInfo(activeFilePath).absolutePath(),
        "Video Files (*.mp4 *.mov *.mkv *.avi *.webm *.flv *.ts);;All Files (*)"
    );
    if (filePath.isEmpty()) return;
    importMediaFile(filePath);
}

void MainWindow::importMediaFile(const QString& filePath, int targetTrack, double targetTime) {
    if (filePath.isEmpty() || !QFile::exists(filePath) || importInProgress) return;

    const bool wasPlaying = isPlaying;
    if (wasPlaying) {
        togglePlayback();
    }
    importInProgress = true;

    auto* loaderDialog = new QProgressDialog("Preparing media for editing...", QString(), 0, 0, this);
    loaderDialog->setWindowTitle("Importing Media");
    loaderDialog->setWindowModality(Qt::WindowModal);
    loaderDialog->setCancelButton(nullptr);
    loaderDialog->setMinimumDuration(0);
    loaderDialog->setAutoClose(false);
    loaderDialog->setAutoReset(false);
    loaderDialog->show();

    const QPointer<MainWindow> window(this);
    const QPointer<QProgressDialog> loader(loaderDialog);
    if (importThread.joinable()) importThread.join();
    importThread = std::thread([window, loader, filePath, targetTrack, targetTime, wasPlaying]() {
        auto updateLoader = [window, loader](const QString& text) {
            if (!window) return;
            QMetaObject::invokeMethod(window.data(), [window, loader, text] {
                if (window && loader) loader->setLabelText(text);
            }, Qt::QueuedConnection);
        };

        updateLoader("Transcoding media for editing...");
        QString standardizedPath = MediaImporter::transcodeToStandardMov(filePath);
        if (standardizedPath.isEmpty()) standardizedPath = filePath;

        const QString clipId = QFileInfo(filePath).baseName();
        updateLoader("Creating H.264 P-frame datamosh proxy...");
        const QString datamoshProxyPath = MediaImporter::transcodeToDatamoshProxy(standardizedPath);
        updateLoader("Loading video and audio streams...");
        const bool loaded = VideoEngine::instance().loadVideo(
            clipId.toStdString(), standardizedPath.toStdString(), datamoshProxyPath.toStdString());

        if (!window) return;
        QMetaObject::invokeMethod(window.data(), [window, loader, filePath, targetTrack, targetTime, wasPlaying, standardizedPath, datamoshProxyPath, clipId, loaded] {
            if (!window) return;
            window->importInProgress = false;
            if (loader) loader->deleteLater();

            if (!loaded) {
                QMessageBox::warning(window, "Import Failed", QString("Could not decode '%1'. Unsupported codec or file error.").arg(QFileInfo(filePath).fileName()));
                if (wasPlaying) window->togglePlayback();
                return;
            }

            AppState::instance().pushUndoState();
            if (window->mediaPool) window->mediaPool->addMedia(clipId);

            ProjectClip videoClip;
            videoClip.id = clipId.toStdString();
            videoClip.mediaId = videoClip.id;
            videoClip.name = videoClip.id;
            videoClip.filePath = standardizedPath.toStdString();
            videoClip.datamoshProxyPath = datamoshProxyPath.toStdString();
            videoClip.sourceStart = 0.0;
            videoClip.sourceDuration = VideoEngine::instance().getDuration(videoClip.mediaId);
            if (videoClip.sourceDuration <= 0.0) videoClip.sourceDuration = 30.0;

            auto& tracks = Project::instance().getTracks();
            int trackIdx = targetTrack;
            const bool droppingOnExistingTrack = trackIdx >= 0 && trackIdx < static_cast<int>(tracks.size());
            if (!droppingOnExistingTrack) {
                TimelineTrack videoTrack;
                videoTrack.id = static_cast<int>(tracks.size()) + 1;
                videoTrack.name = "Video " + std::to_string(videoTrack.id);
                videoTrack.type = TimelineTrackType::Video;
                tracks.push_back(videoTrack);
                trackIdx = static_cast<int>(tracks.size()) - 1;
            }

            videoClip.timelineStart = targetTime >= 0.0 ? targetTime : Project::instance().getDuration();
            if (tracks[trackIdx].type == TimelineTrackType::Video) {
                tracks[trackIdx].clips.push_back(videoClip);
                window->sortTrackClips(tracks[trackIdx]);
            }

            // A normal import creates a separately movable audio clip directly
            // below the video. Both refer to the same decoder/media cache.
            if (!droppingOnExistingTrack || tracks[trackIdx].type == TimelineTrackType::Audio) {
                int audioTrackIndex = trackIdx;
                if (!droppingOnExistingTrack) {
                    TimelineTrack audioTrack;
                    audioTrack.id = static_cast<int>(tracks.size()) + 1;
                    audioTrack.name = "Audio " + std::to_string(audioTrack.id);
                    audioTrack.type = TimelineTrackType::Audio;
                    tracks.push_back(audioTrack);
                    audioTrackIndex = static_cast<int>(tracks.size()) - 1;
                }
                ProjectClip audioClip = videoClip;
                audioClip.id += "_audio";
                audioClip.name += " (Audio)";
                tracks[audioTrackIndex].clips.push_back(audioClip);
                window->sortTrackClips(tracks[audioTrackIndex]);
            }

            window->refreshTrackList();
            window->selectTrackIndex(trackIdx);
            window->activeClipId = clipId;
            window->selectedClipId = clipId;
            window->activeFilePath = standardizedPath;

            if (window->timelinePanel) {
                window->timelinePanel->setDuration(std::max(10.0, Project::instance().getDuration() + 5.0));
                window->timelinePanel->update();
            }
            window->onTimelineScrubbed(videoClip.timelineStart);

            if (window->statusBar()) {
                QString message = QString("Imported: %1 (%2s)").arg(QFileInfo(filePath).fileName()).arg(videoClip.sourceDuration, 0, 'f', 1);
                if (VideoEngine::instance().wasAudioPreloadSkipped(videoClip.mediaId)) {
                    message += " — audio preload skipped for this long clip to keep Z stable";
                }
                window->statusBar()->showMessage(message, 7000);
            }
            window->updateStatusBar();
            window->refreshActiveEffectsList();
            window->syncEffectStackToRenderer();
            if (wasPlaying) window->togglePlayback();
        }, Qt::QueuedConnection);
    });
}

void MainWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, "Open Project", "", "Project Files (*.json)");
    if (!path.isEmpty()) {
        const bool wasPlaying = isPlaying;
        if (wasPlaying) {
            togglePlayback();
        }

        if (Project::instance().load(path.toStdString())) {
            if (mediaPool) mediaPool->clearMedia();
            std::set<std::string> mediaIds;
            for (const auto& track : Project::instance().getTracks()) {
                for (const auto& clip : track.clips) {
                    mediaIds.insert(clip.mediaId.empty() ? clip.id : clip.mediaId);
                }
            }
            const int mediaCount = static_cast<int>(mediaIds.size());
            int idx = 0;
            for (const auto& track : Project::instance().getTracks()) {
                for (const auto& clip : track.clips) {
                    const std::string mediaId = clip.mediaId.empty() ? clip.id : clip.mediaId;
                    if (!mediaIds.erase(mediaId)) continue;
                    const int currentIndex = ++idx;
                    QString label = QString("Loading project media (%1/%2)...").arg(currentIndex).arg(std::max(1, mediaCount));
                    runWithLoader(this, label, [mediaId, clip]() {
                        QString datamoshProxyPath = QString::fromStdString(clip.datamoshProxyPath);
                        if (datamoshProxyPath.isEmpty() || !QFileInfo::exists(datamoshProxyPath)) {
                            // Projects saved before the proxy field existed
                            // are upgraded lazily into the deterministic media
                            // cache rather than silently falling back to a
                            // decoded-pixel imitation of Datamosh.
                            datamoshProxyPath = MediaImporter::transcodeToDatamoshProxy(
                                QString::fromStdString(clip.filePath));
                        }
                        VideoEngine::instance().loadVideo(mediaId, clip.filePath, datamoshProxyPath.toStdString());
                    });
                }
            }
            refreshTrackList();
            const auto& tracks = Project::instance().getTracks();
            if (!tracks.empty() && !tracks.front().clips.empty()) {
                const auto& clip = tracks.front().clips.front();
                activeClipId = QString::fromStdString(clip.id);
                selectedClipId = activeClipId;
                activeFilePath = QString::fromStdString(clip.filePath);
                if (mediaPool) mediaPool->addMedia(activeClipId);
                timelinePanel->setDuration(std::max(10.0, Project::instance().getDuration() + 5.0));
                refreshActiveEffectsList();
                syncEffectStackToRenderer();
                selectTrackIndex(0);
                onTimelineScrubbed(0.0);
            }
            setWindowTitle(QString("Z - %1").arg(QFileInfo(path).fileName()));
            statusBar()->showMessage(QString("Project '%1' opened.").arg(QFileInfo(path).fileName()), 3000);
            updateStatusBar();
        } else {
            QMessageBox::warning(this, "Open Failed", "Could not read project file format.");
        }

        if (wasPlaying) {
            togglePlayback();
        }
    }
}

void MainWindow::saveProject() {
    QString path = QFileDialog::getSaveFileName(this, "Save Project", "", "Project Files (*.json)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".json";
        if (Project::instance().save(path.toStdString())) {
            setWindowTitle(QString("Z - %1").arg(QFileInfo(path).fileName()));
            statusBar()->showMessage("Project saved successfully.", 3000);
            updateStatusBar();
        } else {
            QMessageBox::warning(this, "Save Failed", "Could not save project file to disk.");
        }
    }
}

void MainWindow::togglePlayback() {
    isPlaying = !isPlaying;
    if (playPauseBtn) {
        playPauseBtn->setIcon(VectorIcon::create(isPlaying ? VectorIcon::Type::Pause : VectorIcon::Type::Play, QColor(245, 158, 248), QSize(18, 18)));
    }
    if (isPlaying) {
        // A precise 60 Hz tick leaves enough headroom for UI and GPU work while
        // audio remains the source of truth for the playhead.
        playbackTimer->start(16);
        AudioEngine::instance().setPlayheadTime(currentPlayhead);
        AudioEngine::instance().start();
    } else {
        playbackTimer->stop();
        AudioEngine::instance().stop();
    }
}

void MainWindow::onPlaybackTimer() {
    currentPlayhead = AudioEngine::instance().getPlayheadTime();

    double maxDuration = std::max(10.0, Project::instance().getDuration());
    double loopStart = (markIn >= 0.0 && markOut > markIn) ? markIn : 0.0;
    double loopEnd = (markIn >= 0.0 && markOut > markIn) ? markOut : maxDuration;

    if (currentPlayhead >= loopEnd) {
        if (loopPlayback) {
            currentPlayhead = loopStart; 
            AudioEngine::instance().setPlayheadTime(loopStart);
        } else {
            togglePlayback();
            currentPlayhead = loopEnd;
            onTimelineScrubbed(currentPlayhead);
            return;
        }
    }

    onTimelineScrubbed(currentPlayhead);

}

void MainWindow::onTimelineScrubbed(double time) {
    currentPlayhead = time;
    updateTimecodeDisplay(time);
    timelinePanel->setPlayhead(time);
    inspectorPanel->setCurrentTime(time);
    glWidget->setPlaybackTime(time);

    // Audio lanes are independent from the visible clip stack. Existing
    // projects without audio lanes retain their original video-attached audio.
    const auto& allTracks = Project::instance().getTracks();
    const bool hasDedicatedAudioTracks = std::any_of(allTracks.begin(), allTracks.end(), [](const TimelineTrack& track) {
        return track.type == TimelineTrackType::Audio;
    });
    const ProjectClip* activeAudioClip = nullptr;
    for (auto it = allTracks.rbegin(); it != allTracks.rend() && !activeAudioClip; ++it) {
        if (it->type != TimelineTrackType::Audio && hasDedicatedAudioTracks) continue;
        for (const auto& clip : it->clips) {
            if (time >= clip.timelineStart && time < clip.timelineStart + clip.sourceDuration) {
                activeAudioClip = &clip;
                break;
            }
        }
    }
    if (activeAudioClip) {
        const QString audioClipId = QString::fromStdString(activeAudioClip->id);
        if (activeAudioClipId != audioClipId) {
            const std::string mediaId = activeAudioClip->mediaId.empty() ? activeAudioClip->id : activeAudioClip->mediaId;
            std::vector<float> audioSamples;
            if (VideoEngine::instance().getAudioSamples(mediaId, audioSamples)) {
                AudioEngine::instance().loadClipSamples(audioSamples, activeAudioClip->timelineStart, activeAudioClip->sourceStart, activeAudioClip->sourceDuration);
            } else {
                AudioEngine::instance().clearClipSamples();
            }
            activeAudioClipId = audioClipId;
        }
    } else if (!activeAudioClipId.isEmpty()) {
        AudioEngine::instance().clearClipSamples();
        activeAudioClipId.clear();
    }

    const ProjectClip* topClip = nullptr;
    const TimelineTrack* topTrack = nullptr;
    const ProjectTransition* activeTrans = nullptr;
    const ProjectClip* transLeftClip = nullptr;
    const ProjectClip* transRightClip = nullptr;
    const auto& tracks = Project::instance().getTracks();
    for (auto it = tracks.rbegin(); it != tracks.rend(); ++it) {
        if (it->type == TimelineTrackType::Audio) continue;
            for (const auto& trans : it->transitions) {
            const ProjectClip* left = nullptr;
            const ProjectClip* right = nullptr;
            for (const auto& c : it->clips) {
                if (c.id == trans.leftClipId) left = &c;
                if (c.id == trans.rightClipId) right = &c;
            }

            if (!left || !right) {
                const ProjectClip* nearestLeft = nullptr;
                const ProjectClip* nearestRight = nullptr;
                double bestLeftDist = std::numeric_limits<double>::max();
                double bestRightDist = std::numeric_limits<double>::max();
                for (const auto& c : it->clips) {
                    const double clipStart = c.timelineStart;
                    const double clipEnd = c.timelineStart + c.sourceDuration;
                    if (clipEnd <= trans.cutTime + 0.25) {
                        double d = std::abs(trans.cutTime - clipEnd);
                        if (d < bestLeftDist) {
                            bestLeftDist = d;
                            nearestLeft = &c;
                        }
                    }
                    if (clipStart >= trans.cutTime - 0.25) {
                        double d = std::abs(clipStart - trans.cutTime);
                        if (d < bestRightDist) {
                            bestRightDist = d;
                            nearestRight = &c;
                        }
                    }
                }
                if (!left) left = nearestLeft;
                if (!right) right = nearestRight;
            }

            if (left && right) {
                    double cutPoint = trans.cutTime;
                double transStart = cutPoint - trans.duration / 2.0;
                double transEnd = cutPoint + trans.duration / 2.0;
                if (time >= transStart && time < transEnd) {
                    activeTrans = &trans;
                    transLeftClip = left;
                    transRightClip = right;
                    topTrack = &(*it);
                    break;
                }
            }
        }
        if (activeTrans) break;

        for (const auto& c : it->clips) {
            if (time >= c.timelineStart && time < c.timelineStart + c.sourceDuration) {
                topClip = &c;
                topTrack = &(*it);
                break;
            }
        }
        if (topClip) break;
    }

    if (activeTrans) {
        double cutPoint = activeTrans->cutTime;
        double transStart = cutPoint - activeTrans->duration / 2.0;
        double progress = (time - transStart) / activeTrans->duration;
        progress = std::clamp(progress, 0.0, 1.0);

        double localTime1 = transLeftClip->sourceStart + std::max(0.0, time - transLeftClip->timelineStart);
        double localTime2 = transRightClip->sourceStart + std::max(0.0, time - transRightClip->timelineStart);
        AudioEngine::instance().setPlayheadTime(time);

        DecodedVideoFrame frame1, frame2;
        const ProjectClip* effectClip = progress < 0.5 ? transLeftClip : transRightClip;
        applyEffectsToRenderer(time, topTrack, effectClip);
        const std::string mediaId1 = transLeftClip->mediaId.empty() ? transLeftClip->id : transLeftClip->mediaId;
        const std::string mediaId2 = transRightClip->mediaId.empty() ? transRightClip->id : transRightClip->mediaId;
        bool gotFrame1 = VideoEngine::instance().getFrame(mediaId1, localTime1, frame1);
        bool gotFrame2 = VideoEngine::instance().getFrame(mediaId2, localTime2, frame2);

        if (gotFrame1 && gotFrame2 && !frame1.rgbData.empty() && !frame2.rgbData.empty()) {
            glWidget->updateTransitionFrames(frame1, frame2, progress, activeTrans->pluginId);
        } else if (gotFrame1 && !frame1.rgbData.empty()) {
            glWidget->updateFrame(frame1);
        } else if (gotFrame2 && !frame2.rgbData.empty()) {
            glWidget->updateFrame(frame2);
        } else {
            // Keep the last valid frame while async decoding catches up.
            // Clearing here produced intermittent blank frames during playback.
        }
    } else if (topClip) {
        activeClipId = QString::fromStdString(topClip->id);
        activeFilePath = QString::fromStdString(topClip->filePath);

        double localTime = topClip->sourceStart + std::max(0.0, time - topClip->timelineStart);
        if (localTime < 0.0) localTime = 0.0;
        AudioEngine::instance().setPlayheadTime(time);

        DecodedVideoFrame frame;
        applyEffectsToRenderer(time, topTrack, topClip);
        const std::string clipKey = topClip->mediaId.empty() ? topClip->id : topClip->mediaId;
        bool gotFrame = false;
        const bool asyncPlayback = isPlaying && VideoEngine::instance().isAsyncDecodeEnabled();
        if (asyncPlayback) {
            // Never block the GUI behind async decode lag. Prefer the nearest
            // cached frame in either direction, then request a small lead ahead.
            gotFrame = VideoEngine::instance().tryGetCachedFrame(clipKey, localTime, frame);
            if (!gotFrame) {
                gotFrame = VideoEngine::instance().tryGetNearestCachedFrame(clipKey, localTime, frame, 0.25);
            }
            VideoEngine::instance().requestFrameAsync(clipKey, localTime + 0.10);
        } else {
            gotFrame = VideoEngine::instance().getFrame(clipKey, localTime, frame);
        }

        if (gotFrame && frame.width > 0 && frame.height > 0 && !frame.rgbData.empty()) {
            lastDetectFrame = frame;
            glWidget->updateFrame(std::move(frame));
            // Detection is intentionally decoupled from the 60 Hz renderer;
            // CPU fallback and YOLO are both expensive enough to cause jitter.
            if (liveDetectEnabled && liveDetectionTimer.elapsed() >= 150) {
                liveDetectionTimer.restart();
                runDetectionOnCurrentFrame();
            }
        } else {
            // Keep the last valid frame while async decoding catches up.
        }
    } else {
        glWidget->clearFrame();
        AudioEngine::instance().setPlayheadTime(time);
        activeClipId.clear();
        activeFilePath.clear();
        glWidget->setActiveEffects({});
    }
}

void MainWindow::onPlaybackQualityChanged(int index) {
    int scale = 1;
    if (index == 1) scale = 2; 
    else if (index == 2) scale = 4; 
    VideoEngine::instance().setPlaybackQuality(scale);
    onTimelineScrubbed(currentPlayhead);
}

void MainWindow::onToggleFpsOverlay(bool checked) {
    glWidget->setShowOverlay(checked);
}

void MainWindow::onDetectionSettingsChanged() {
    if (!detectSensitivitySlider || !detectMinAreaSlider) return;
    Detector::instance().setSensitivity(detectSensitivitySlider->value() / 100.0f);
    Detector::instance().setMinArea(detectMinAreaSlider->value() / 1000.0f);
    if (!currentDetections.empty() || liveDetectEnabled) {
        runDetectionOnCurrentFrame();
    }
}

void MainWindow::clearDetections() {
    currentDetections.clear();
    Detector::instance().reset();
    if (detectionList) detectionList->clear();
    if (glWidget) {
        glWidget->setDetections({});
        glWidget->setMaskData(0, 0, {});
        glWidget->setMaskEnabled(false);
    }
    if (applyMaskCheck) applyMaskCheck->setChecked(false);
    if (statusBar()) statusBar()->showMessage("Detections cleared", 2000);
}

void MainWindow::chooseYoloModel() {
    const QString path = QFileDialog::getOpenFileName(this, "Load YOLO ONNX model", QString(), "ONNX model (*.onnx)");
    if (path.isEmpty()) return;
    std::string error;
    const bool ready = Detector::instance().setYoloModel(path.toStdString(), &error);
    if (yoloModelPathEdit) yoloModelPathEdit->setText(path);
    if (detectionStatusLabel) detectionStatusLabel->setText(QString::fromStdString(ready ? "YOLO model loaded" : error));
    if (ready) runDetectionOnCurrentFrame();
}

void MainWindow::downloadRecommendedYoloModel() {
    if (!modelDownloadManager) return;

    // Official Ultralytics YOLO11 nano COCO detector: small enough for local
    // editing, with general-purpose labels such as person, pet, vehicle, etc.
    const QUrl modelUrl("https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n.onnx");
    const QString modelDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/models";
    QDir().mkpath(modelDir);
    const QString modelPath = modelDir + "/yolo11n.onnx";

    auto* progress = new QProgressDialog("Downloading recommended YOLO11n object model...", "Cancel", 0, 100, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(true);
    progress->show();

    QNetworkRequest request(modelUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = modelDownloadManager->get(request);
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::downloadProgress, progress, [progress](qint64 received, qint64 total) {
        if (total > 0) progress->setValue(static_cast<int>(received * 100 / total));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, progress, modelPath] {
        progress->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = "Model download failed: " + reply->errorString();
            if (detectionStatusLabel) detectionStatusLabel->setText(message);
            if (statusBar()) statusBar()->showMessage(message, 5000);
            reply->deleteLater();
            return;
        }

        QSaveFile modelFile(modelPath);
        if (!modelFile.open(QIODevice::WriteOnly) || modelFile.write(reply->readAll()) < 0 || !modelFile.commit()) {
            const QString message = "Could not save the downloaded YOLO model.";
            if (detectionStatusLabel) detectionStatusLabel->setText(message);
            if (statusBar()) statusBar()->showMessage(message, 5000);
            reply->deleteLater();
            return;
        }

        std::string error;
        const bool ready = Detector::instance().setYoloModel(modelPath.toStdString(), &error);
        if (yoloModelPathEdit) yoloModelPathEdit->setText(modelPath);
        const QString message = ready ? "YOLO11n downloaded and ready" : QString::fromStdString(error);
        if (detectionStatusLabel) detectionStatusLabel->setText(message);
        if (statusBar()) statusBar()->showMessage(message, 5000);
        if (ready) runDetectionOnCurrentFrame();
        reply->deleteLater();
    });
}

void MainWindow::checkForUpdates() {
    checkForUpdates(true);
}

void MainWindow::checkForUpdates(bool interactive) {
    if (!updateCheckManager) return;

    QNetworkRequest request(QUrl(QString::fromLatin1(kLatestReleaseApi)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Z-VideoEditor/" + QApplication::applicationVersion());
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply* reply = updateCheckManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] {
        const auto finish = [reply] { reply->deleteLater(); };
        if (reply->error() != QNetworkReply::NoError) {
            if (interactive) {
                QMessageBox::information(this, "Z Updates", "Could not check for updates right now.\n\n" + reply->errorString());
            }
            finish();
            return;
        }

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value("tag_name").toString();
        const QString releaseUrl = release.value("html_url").toString();
        const auto current = parseSemanticVersion(QApplication::applicationVersion());
        const auto available = parseSemanticVersion(tag);
        if (!current || !available) {
            if (interactive) {
                QMessageBox::information(this, "Z Updates", "The latest release does not use a supported integer version number.");
            }
            finish();
            return;
        }

        if (!isNewerVersion(*available, *current)) {
            if (interactive) {
                QMessageBox::information(this, "Z Updates", "Z is up to date (version " + QApplication::applicationVersion() + ").");
            }
            finish();
            return;
        }

        QSettings settings;
        const QString ignoredVersion = settings.value("updates/ignoredVersion").toString();
        if (!interactive && ignoredVersion == tag) {
            finish();
            return;
        }

        QMessageBox dialog(this);
        dialog.setWindowTitle("Update Available");
        dialog.setIcon(QMessageBox::Information);
        dialog.setText("Z " + tag + " is available. You are using " + QApplication::applicationVersion() + ".");
        dialog.setInformativeText("Download the new installer from the Z GitHub release page?");
        QPushButton* downloadButton = dialog.addButton("Open Download Page", QMessageBox::AcceptRole);
        QPushButton* laterButton = dialog.addButton("Not Now", QMessageBox::RejectRole);
        Q_UNUSED(laterButton);
        dialog.exec();
        if (dialog.clickedButton() == downloadButton) {
            QDesktopServices::openUrl(QUrl(releaseUrl));
        } else {
            settings.setValue("updates/ignoredVersion", tag);
        }
        finish();
    });
}

void MainWindow::runDetectionOnCurrentFrame() {
    DecodedVideoFrame frame = lastDetectFrame;

    if (frame.rgbData.empty() || frame.width <= 0 || frame.height <= 0) {
        // Fetch a frame for the active clip at the playhead
        const ProjectClip* clip = currentClip();
        if (!clip) {
            auto* track = currentTrack();
            if (track) clip = clipAtTime(*track, currentPlayhead);
        }
        if (!clip) {
            if (statusBar()) statusBar()->showMessage("No clip under playhead to detect", 2500);
            return;
        }
        const double localTime = clip->sourceStart + std::max(0.0, currentPlayhead - clip->timelineStart);
        const std::string mediaId = clip->mediaId.empty() ? clip->id : clip->mediaId;
        if (!VideoEngine::instance().getFrame(mediaId, localTime, frame) || frame.rgbData.empty()) {
            if (statusBar()) statusBar()->showMessage("Could not decode frame for detection", 2500);
            return;
        }
        lastDetectFrame = frame;
    }

    if (detectSensitivitySlider && detectMinAreaSlider) {
        Detector::instance().setSensitivity(detectSensitivitySlider->value() / 100.0f);
        Detector::instance().setMinArea(detectMinAreaSlider->value() / 1000.0f);
    }

    currentDetections = Detector::instance().detectFrame(frame);
    if (detectionStatusLabel) detectionStatusLabel->setText(QString::fromStdString(Detector::instance().status()));

    if (detectionList) {
        detectionList->clear();
        for (const auto& box : currentDetections) {
            detectionList->addItem(QString("%1  (%2%)  [%3x%4]")
                .arg(QString::fromStdString(box.label))
                .arg(box.confidence * 100.0f, 0, 'f', 0)
                .arg(box.w * frame.width, 0, 'f', 0)
                .arg(box.h * frame.height, 0, 'f', 0));
        }
    }

    if (glWidget) {
        glWidget->setDetections(currentDetections);
        if (showBoxesCheck) glWidget->setShowDetections(showBoxesCheck->isChecked());
        const DetectionShape shape = detectionShapeCombo
            ? static_cast<DetectionShape>(detectionShapeCombo->currentData().toInt())
            : DetectionShape::Rectangle;
        glWidget->setDetectionShape(shape);

        const bool maskOn = applyMaskCheck && applyMaskCheck->isChecked();
        glWidget->setMaskEnabled(maskOn);
        if (maskOn) {
            auto mask = Detector::buildMaskFromBoxes(frame.width, frame.height, currentDetections, 6.0f, shape);
            glWidget->setMaskData(frame.width, frame.height, mask);
        }
    }

    if (statusBar()) {
        statusBar()->showMessage(QString("Detected %1 region(s)").arg(currentDetections.size()), 2500);
    }
}

void MainWindow::applyEffectsToRenderer(double time, const TimelineTrack* activeTrack, const ProjectClip* activeClip) {
    Q_UNUSED(activeTrack);
    if (!activeTrack) {
        glWidget->clearFrame();
        glWidget->setActiveEffects({});
        return;
    }
    auto evalParam = [time](const ShaderParameter& p) {
        return p.curve.getKeyframes().empty() ? p.currentVal : p.curve.evaluate(time);
    };
    if (!activeClip) {
        glWidget->setActiveEffects({});
        glWidget->update();
        return;
    }

    activeClipId = QString::fromStdString(activeClip->id);
    activeFilePath = QString::fromStdString(activeClip->filePath);
    const std::string mediaId = activeClip->mediaId.empty() ? activeClip->id : activeClip->mediaId;
    const double clipTime = std::clamp(time - activeClip->timelineStart, 0.0, activeClip->sourceDuration);
    glWidget->setPlaybackTime(clipTime);

    bool datamoshEnabled = false;
    double iDrop = 0.0, pDup = 0.0, pDrop = 0.0;
    int pDupCount = 1;
    bool smearEnabled = false;
    double frameMerge = 0.0, frameSmear = 0.0, colorBleed = 0.0, lumaBias = 0.0;
    bool xorEnabled = false, orEnabled = false, andEnabled = false, xnorEnabled = false, nandEnabled = false;
    double xorValue = 0.5, xorIntensity = 1.0, orValue = 0.5, orIntensity = 1.0;
    double andValue = 1.0, andIntensity = 1.0, xnorValue = 0.5, xnorIntensity = 1.0;
    double nandValue = 0.5, nandIntensity = 1.0;
    std::vector<AppliedEffect> shaderEffects;

    const auto& effects = activeClip->effects;
    for (const auto& eff : effects) {
        if (clipTime + 1e-5 < eff.startOffset) continue;
        if (auto* pluginMeta = PluginManager::instance().findPlugin(eff.pluginId)) {
            const QString cat = QString::fromStdString(pluginMeta->category);
            if (cat.startsWith("Transitions", Qt::CaseInsensitive)) {
                continue;
            }
        }

        AppliedEffect evaluatedEffect = eff;
        for (auto& param : evaluatedEffect.parameters) {
            param.currentVal = evalParam(param);
            // The GPU receives the evaluated value. Keeping project curves out
            // of this transient copy prevents stale global-time evaluation.
            param.curve = AnimationCurve(param.currentVal);
        }
        auto readParam = [&](size_t index, double fallback) {
            return index < evaluatedEffect.parameters.size() ? evaluatedEffect.parameters[index].currentVal : fallback;
        };
        if (eff.pluginId == "datamosh") {
            iDrop = readParam(0, 0.0);
            pDup = readParam(1, 0.0);
            pDupCount = std::max(1, static_cast<int>(readParam(2, 4.0)));
            pDrop = readParam(3, 0.0);
            datamoshEnabled = iDrop >= 0.5 || pDup > 0.0 || pDrop > 0.0;
        } else if (eff.pluginId == "optical_smear") {
            frameMerge = readParam(0, 0.25);
            frameSmear = readParam(1, 0.1);
            colorBleed = readParam(2, 0.25);
            lumaBias = readParam(3, 0.2);
            smearEnabled = frameMerge > 0.0 || frameSmear > 0.0 || colorBleed > 0.0 || lumaBias > 0.0;
        } else if (eff.pluginId == "cpu_xor") {
            xorValue = readParam(0, 0.5);
            xorIntensity = readParam(1, 1.0);
            xorEnabled = true;
        } else if (eff.pluginId == "cpu_or") {
            orValue = readParam(0, 0.5);
            orIntensity = readParam(1, 1.0);
            orEnabled = true;
        } else if (eff.pluginId == "cpu_and") {
            andValue = readParam(0, 1.0);
            andIntensity = readParam(1, 1.0);
            andEnabled = true;
        } else if (eff.pluginId == "cpu_xnor") {
            xnorValue = readParam(0, 0.5);
            xnorIntensity = readParam(1, 1.0);
            xnorEnabled = true;
        } else if (eff.pluginId == "cpu_nand") {
            nandValue = readParam(0, 0.5);
            nandIntensity = readParam(1, 1.0);
            nandEnabled = true;
        } else {
            shaderEffects.push_back(std::move(evaluatedEffect));
        }
    }

    VideoEngine::instance().setDatamoshing(mediaId, datamoshEnabled, iDrop >= 0.5 ? 1.0 : 0.0, pDup, pDupCount, pDrop);
    VideoEngine::instance().setOpticalSmear(mediaId, smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
    VideoEngine::instance().setCpuXor(mediaId, xorEnabled, xorValue, xorIntensity);
    VideoEngine::instance().setCpuOr(mediaId, orEnabled, orValue, orIntensity);
    VideoEngine::instance().setCpuAnd(mediaId, andEnabled, andValue, andIntensity);
    VideoEngine::instance().setCpuXnor(mediaId, xnorEnabled, xnorValue, xnorIntensity);
    VideoEngine::instance().setCpuNand(mediaId, nandEnabled, nandValue, nandIntensity);
    glWidget->setActiveEffects(shaderEffects);
    glWidget->update();
}

void MainWindow::onClipSelected(const QString& clipId) {
    if (clipId.isEmpty()) return;

    const std::string wanted = clipId.toStdString();
    auto& tracks = Project::instance().getTracks();
    for (int t = 0; t < static_cast<int>(tracks.size()); ++t) {
        for (int c = 0; c < static_cast<int>(tracks[t].clips.size()); ++c) {
            if (tracks[t].clips[c].id == wanted) {
                selectTrackIndex(t);
                selectClip(t, c);
                return;
            }
        }
    }
}

void MainWindow::onEffectSelected(const QString& targetId) {
    auto* track = currentTrack();
    if (!track) return;
    if (track->type == TimelineTrackType::Audio) return;

    if (auto* plugin = PluginManager::instance().findPlugin(targetId.toStdString())) {
        const QString cat = QString::fromStdString(plugin->category);
        if (cat.startsWith("Transitions", Qt::CaseInsensitive)) {
            int trackIndex = trackControl ? trackControl->getSelectedTrack() : 0;
            if (addTransitionAtCut(trackIndex, currentPlayhead, targetId)) {
                refreshActiveEffectsList();
                syncEffectStackToRenderer();
            }
            return;
        }
    }

    ProjectClip* clip = nullptr;
    auto& tracks = Project::instance().getTracks();
    for (auto it = tracks.rbegin(); it != tracks.rend() && !clip; ++it) {
        if (it->type == TimelineTrackType::Audio) continue;
        clip = clipAtTime(*it, currentPlayhead);
    }
    if (!clip) clip = currentClip();
    if (!clip) return;
    selectedClipId = QString::fromStdString(clip->id);
    clip->useClipEffects = true;

    auto* effects = &clip->effects;

    if (std::none_of(effects->begin(), effects->end(), [&](const AppliedEffect& eff) {
            return QString::fromStdString(eff.pluginId) == targetId;
        })) {
        AppState::instance().pushUndoState();
        AppliedEffect effect = createEffectTemplate(targetId);
        effect.startOffset = std::clamp(currentPlayhead - clip->timelineStart, 0.0, clip->sourceDuration);
        effects->push_back(std::move(effect));
    }

    refreshActiveEffectsList();
    syncEffectStackToRenderer();
    onTimelineScrubbed(currentPlayhead);

    for (int i = 0; i < activeEffectsList->count(); ++i) {
        if (activeEffectsList->item(i)->data(Qt::UserRole).toString() == targetId) {
            activeEffectsList->setCurrentRow(i);
            onActiveEffectSelected(activeEffectsList->item(i));
            break;
        }
    }
}

void MainWindow::onActiveEffectSelected(QListWidgetItem* item) {
    if (!item || Project::instance().getTracks().empty()) return;

    QString effectId = item->data(Qt::UserRole).toString();
    auto* effects = activeEffects();
    if (!effects) return;
    for (auto& eff : *effects) {
        if (QString::fromStdString(eff.pluginId) == effectId) {
            inspectorPanel->loadEffect(effectId, eff.parameters, currentPlayhead);
            syncEffectStackToRenderer();
            return;
        }
    }
}

void MainWindow::removeSelectedEffect() {
    if (!activeEffectsList || !activeEffectsList->currentItem()) return;
    if (Project::instance().getTracks().empty()) return;

    QString effectId = activeEffectsList->currentItem()->data(Qt::UserRole).toString();
    auto* effects = activeEffects();
    if (!effects) return;
    AppState::instance().pushUndoState();
    auto it = std::remove_if(effects->begin(), effects->end(), [&](const AppliedEffect& eff) {
        return QString::fromStdString(eff.pluginId) == effectId;
    });
    if (it != effects->end()) {
        effects->erase(it, effects->end());
        inspectorPanel->clearInspector();
        refreshActiveEffectsList();
        syncEffectStackToRenderer();
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    }
}

TimelineTrack* MainWindow::currentTrack() {
    auto& tracks = Project::instance().getTracks();
    if (tracks.empty()) return nullptr;
    int index = trackControl ? trackControl->getSelectedTrack() : 0;
    if (index < 0 || index >= static_cast<int>(tracks.size())) index = 0;
    return &tracks[index];
}

const TimelineTrack* MainWindow::currentTrack() const {
    const auto& tracks = Project::instance().getTracks();
    if (tracks.empty()) return nullptr;
    int index = trackControl ? trackControl->getSelectedTrack() : 0;
    if (index < 0 || index >= static_cast<int>(tracks.size())) index = 0;
    return &tracks[index];
}

ProjectClip* MainWindow::currentClip() {
    auto& tracks = Project::instance().getTracks();
    if (!selectedClipId.isEmpty()) {
        const std::string wantedId = selectedClipId.toStdString();
        for (auto& track : tracks) {
            for (auto& clip : track.clips) {
                if (clip.id == wantedId) {
                    return &clip;
                }
            }
        }
    }

    auto* track = currentTrack();
    if (!track) return nullptr;
    return clipAtTime(*track, currentPlayhead);
}

const ProjectClip* MainWindow::currentClip() const {
    const auto& tracks = Project::instance().getTracks();
    if (!selectedClipId.isEmpty()) {
        const std::string wantedId = selectedClipId.toStdString();
        for (const auto& track : tracks) {
            for (const auto& clip : track.clips) {
                if (clip.id == wantedId) {
                    return &clip;
                }
            }
        }
    }

    const auto* track = currentTrack();
    if (!track) return nullptr;
    return clipAtTime(*track, currentPlayhead);
}

std::vector<AppliedEffect>* MainWindow::activeEffects() {
    if (auto* clip = currentClip()) {
        clip->useClipEffects = true;
        return &clip->effects;
    }
    return nullptr;
}

const std::vector<AppliedEffect>* MainWindow::activeEffects() const {
    if (const auto* clip = currentClip()) {
        return &clip->effects;
    }
    return nullptr;
}

QString MainWindow::effectDisplayNameForId(const QString& effectId) const {
    if (effectId == "vhs") return "VHS Degradation";
    if (effectId == "crt") return "CRT Simulation";
    if (effectId == "bent") return "Circuit Bent Camera";
    if (effectId == "feedback") return "Feedback Loop";
    if (effectId == "xor") return "XOR Blend";
    if (effectId == "datamosh") return "Datamoshing";
    if (effectId == "optical_smear") return "Optical Smear";
    if (effectId == "cpu_xor") return "Legacy CPU XOR";
    if (effectId == "cpu_or") return "Legacy CPU OR";
    if (effectId == "cpu_and") return "Legacy CPU AND";
    if (effectId == "cpu_xnor") return "Legacy CPU XNOR";
    if (effectId == "cpu_nand") return "Legacy CPU NAND";
    if (effectId == "milkdrop") return "Milkdrop Visualizer";

    if (auto* plugin = PluginManager::instance().findPlugin(effectId.toStdString())) {
        return QString::fromStdString(plugin->name);
    }
    return effectId;
}

QString MainWindow::effectIdForDisplayName(const QString& displayName) const {
    if (displayName == "VHS Degradation") return "vhs";
    if (displayName == "CRT Simulation") return "crt";
    if (displayName == "Circuit Bent Camera") return "bent";
    if (displayName == "Feedback Loop") return "feedback";
    if (displayName == "XOR Blend") return "xor";
    if (displayName == "Datamoshing") return "datamosh";
    if (displayName == "Optical Smear") return "optical_smear";
    if (displayName == "Legacy CPU XOR") return "cpu_xor";
    if (displayName == "Legacy CPU OR") return "cpu_or";
    if (displayName == "Legacy CPU AND") return "cpu_and";
    if (displayName == "Legacy CPU XNOR") return "cpu_xnor";
    if (displayName == "Legacy CPU NAND") return "cpu_nand";
    if (displayName == "Milkdrop Visualizer") return "milkdrop";

    const auto& plugins = PluginManager::instance().getPlugins();
    for (const auto& plugin : plugins) {
        if (QString::fromStdString(plugin.name) == displayName) {
            return QString::fromStdString(plugin.id);
        }
    }
    return displayName;
}

bool MainWindow::trackHasEffect(const TimelineTrack& track, const QString& effectId) const {
    for (const auto& eff : track.effects) {
        if (QString::fromStdString(eff.pluginId) == effectId) {
            return true;
        }
    }
    return false;
}

AppliedEffect MainWindow::createEffectTemplate(const QString& effectId) const {
    AppliedEffect effect;
    effect.pluginId = effectId.toStdString();
    if (effectId == "datamosh") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("iDrop", "Remove I-Frames", 0.0, 1.0, 1.0, true),
            make("pDup", "P-Frame Repeat Chance", 0.0, 1.0, 0.0),
            make("pDupCount", "P-Frame Repeat Count", 1.0, 20.0, 2.0),
            make("pDrop", "P-Frame Drop Chance", 0.0, 1.0, 0.0)
        };
    } else if (effectId == "optical_smear") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("frameMerge", "Frame Merge", 0.0, 1.0, 0.25),
            make("frameSmear", "Frame Smear", 0.0, 1.0, 0.1),
            make("colorBleed", "Color Bleed", 0.0, 1.0, 0.25),
            make("lumaBias", "Luma Bias", 0.0, 1.0, 0.2)
        };
    } else if (effectId == "cpu_xor") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("xorValue", "XOR Bitmask", 0.0, 1.0, 0.5),
            make("intensity", "XOR Blend", 0.0, 1.0, 1.0)
        };
    } else if (effectId == "cpu_or") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("orValue", "OR Bitmask", 0.0, 1.0, 0.5),
            make("intensity", "OR Blend", 0.0, 1.0, 1.0)
        };
    } else if (effectId == "cpu_and") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("andValue", "AND Bitmask", 0.0, 1.0, 1.0),
            make("intensity", "AND Blend", 0.0, 1.0, 1.0)
        };
    } else if (effectId == "cpu_xnor") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("xnorValue", "XNOR Bitmask", 0.0, 1.0, 0.5),
            make("intensity", "XNOR Blend", 0.0, 1.0, 1.0)
        };
    } else if (effectId == "cpu_nand") {
        auto make = [](const std::string& name, const std::string& label, double minV, double maxV, double defV, bool isBool = false) {
            return ShaderParameter{name, label, minV, maxV, defV, defV, isBool, AnimationCurve(defV)};
        };
        effect.parameters = {
            make("nandValue", "NAND Bitmask", 0.0, 1.0, 0.5),
            make("intensity", "NAND Blend", 0.0, 1.0, 1.0)
        };
    } else {
        auto* plugin = PluginManager::instance().findPlugin(effectId.toStdString());
        if (plugin) {
            effect.pluginId = plugin->id;
            effect.parameters = plugin->parameters;
        } else {
        }
    }
    return effect;
}

bool MainWindow::addTransitionAtCut(int trackIndex, double dropTime, const QString& pluginId) {
    auto& tracks = Project::instance().getTracks();
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return false;
    auto& track = tracks[trackIndex];
    if (track.clips.size() < 2) return false;

    double bestDist = 0.6;
    double cutPoint = dropTime;

    for (const auto& c : track.clips) {
        const double clipStart = c.timelineStart;
        const double clipEnd = c.timelineStart + c.sourceDuration;
        double distStart = std::abs(clipStart - dropTime);
        if (distStart < bestDist) {
            bestDist = distStart;
            cutPoint = clipStart;
        }
        double distEnd = std::abs(clipEnd - dropTime);
        if (distEnd < bestDist) {
            bestDist = distEnd;
            cutPoint = clipEnd;
        }
    }

    const ProjectClip* leftClip = nullptr;
    const ProjectClip* rightClip = nullptr;
    double bestPairDist = std::numeric_limits<double>::max();

    for (const auto& l : track.clips) {
        const double lEnd = l.timelineStart + l.sourceDuration;
        for (const auto& r : track.clips) {
            if (l.id == r.id) continue;
            const double gap = std::abs(lEnd - r.timelineStart);
            if (gap > 0.35) continue;

            const double pairCut = r.timelineStart;
            const double pairDist = std::abs(pairCut - cutPoint);
            if (pairDist < bestPairDist) {
                bestPairDist = pairDist;
                leftClip = &l;
                rightClip = &r;
                cutPoint = pairCut;
            }
        }
    }

    if (!leftClip || !rightClip) {
        return false;
    }

    AppState::instance().pushUndoState();
    ProjectTransition trans;
    trans.id = "trans_" + std::to_string(rand());
    trans.pluginId = pluginId.toStdString();
    trans.leftClipId = leftClip->id;
    trans.rightClipId = rightClip->id;
    trans.duration = 1.0;
    trans.cutTime = cutPoint;
    trans.alignment = "center";

    auto* plugin = PluginManager::instance().findPlugin(trans.pluginId);
    if (plugin) {
        trans.parameters = plugin->parameters;
    }

    track.transitions.erase(std::remove_if(track.transitions.begin(), track.transitions.end(),
        [&](const ProjectTransition& t) { return std::abs(t.cutTime - cutPoint) < 0.3; }), track.transitions.end());

    track.transitions.push_back(trans);
    timelinePanel->update();
    onTimelineScrubbed(currentPlayhead);
    return true;
}

void MainWindow::refreshActiveEffectsList() {
    if (!activeEffectsList) return;

    QString selectedId;
    if (activeEffectsList->currentItem()) {
        selectedId = activeEffectsList->currentItem()->data(Qt::UserRole).toString();
    }

    activeEffectsList->clear();

    const auto* effects = activeEffects();
    if (!effects) return;

    int restoreRow = -1;
    for (const auto& eff : *effects) {
        QString id = QString::fromStdString(eff.pluginId);
        if (id == "object_mask") {
            continue;
        }
        if (auto* pluginMeta = PluginManager::instance().findPlugin(eff.pluginId)) {
            const QString cat = QString::fromStdString(pluginMeta->category);
            if (cat.startsWith("Transitions", Qt::CaseInsensitive)) {
                continue;
            }
        }
        auto* item = new QListWidgetItem(effectDisplayNameForId(id), activeEffectsList);
        item->setData(Qt::UserRole, id);
        if (!selectedId.isEmpty() && id == selectedId) {
            restoreRow = activeEffectsList->count() - 1;
        }
    }

    if (restoreRow >= 0) {
        activeEffectsList->setCurrentRow(restoreRow);
    }
}

void MainWindow::syncEffectStackToRenderer() {
    std::vector<AppliedEffect> customPlugins;
    const ProjectClip* clip = currentClip();
    if (!clip) {
        glWidget->setActiveEffects(customPlugins);
        return;
    }
    const auto& effects = clip->effects;
    for (const auto& eff : effects) {
        if (eff.pluginId == "object_mask") {
            continue;
        }
        if (auto* pluginMeta = PluginManager::instance().findPlugin(eff.pluginId)) {
            const QString cat = QString::fromStdString(pluginMeta->category);
            if (cat.startsWith("Transitions", Qt::CaseInsensitive)) {
                continue;
            }
        }

        if (eff.pluginId != "datamosh" && eff.pluginId != "optical_smear" && 
            eff.pluginId != "cpu_xor" && eff.pluginId != "cpu_or" && 
            eff.pluginId != "cpu_and" && eff.pluginId != "cpu_xnor" &&
            eff.pluginId != "cpu_nand") {
            customPlugins.push_back(eff);
        }
    }
    glWidget->setActiveEffects(customPlugins);
}

void MainWindow::onParameterChanged(const QString& effectId, const QString& paramName, double value) {
    auto* effects = activeEffects();
    if (!effects) return;

    for (auto& eff : *effects) {
        if (QString::fromStdString(eff.pluginId) == effectId) {
            for (auto& param : eff.parameters) {
                if (QString::fromStdString(param.name) == paramName) {
                    param.currentVal = value;
                    param.curve.setDefaultValue(value);
                    if (!param.curve.getKeyframes().empty()) {
                        param.curve.insertKeyframe(currentPlayhead, value);
                        if (timelinePanel) timelinePanel->update(); 
                    }
                    if (inspectorPanel) {
                        inspectorPanel->syncParameters(eff.parameters);
                    }
                    syncEffectStackToRenderer();
                    onTimelineScrubbed(currentPlayhead);
                    return;
                }
            }
        }
    }
}

void MainWindow::exportVideo() {
    MediaExporter::exportVideo(
        this,
        activeClipId,
        activeFilePath,
        glWidget,
        [this](double time) { this->onTimelineScrubbed(time); },
        [this]() { this->togglePlayback(); },
        isPlaying,
        markIn,
        markOut
    );
}
