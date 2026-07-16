#include "mainwindow.h"
#include "core/project.h"
#include "engine/audioengine.h"
#include "engine/pluginmanager.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
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
#include <QProcess>
#include <QDir>
#include <QLineEdit>
#include <algorithm>
#include "utils/logging.h"
#include "engine/videoengine.h"
#include "media/mediaexporter.h"
#include "core/appstate.h"

#include "media/mediaimporter.h" 

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Z");
    setStyleSheet(R"(
        * {
            font-family: 'JetBrains Mono', 'Consolas', 'Courier New', monospace;
            font-size: 11px;
            color: #b0b0b0;
        }
        QMainWindow {
            background-color: #0b0b0b;
        }
        QDockWidget {
            background: #0b0b0b;
            border: none;
        }
        QWidget#mediaContainer, QWidget#effectsContainer, QWidget#activeContainer, QWidget#tracksContainer {
            background-color: #141414;
            border-left: 3px solid #df42f5;
            border-top: 1px solid #222;
            border-right: 1px solid #222;
            border-bottom: 1px solid #222;
            border-radius: 4px;
            margin-bottom: 6px;
        }
        QWidget#controlContainer {
            background-color: #141414;
            border-right: 3px solid #df42f5;
            border-top: 1px solid #222;
            border-left: 1px solid #222;
            border-bottom: 1px solid #222;
            border-radius: 4px;
            padding: 8px;
        }
        QListWidget {
            background: transparent;
            border: none;
        }
        QListWidget::item {
            padding: 5px;
            border-bottom: 1px solid #1a1a1a;
            color: #b0b0b0;
        }
        QListWidget::item:selected {
            background: #2b1230;
            color: #e855f4;
            font-weight: bold;
        }
        QPushButton {
            background: #1c1c1c;
            border: 1px solid #333;
            color: #fff;
            padding: 4px 8px;
            border-radius: 3px;
        }
        QPushButton:hover {
            background: #2b1230;
            border-color: #df42f5;
            color: #e855f4;
        }
        QScrollBar:vertical {
            border: none;
            background: #0d0d0d;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #333;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #df42f5;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QTabWidget::pane {
            border: 1px solid #222;
            background: #121212;
        }
        QTabBar::tab {
            background: #161616;
            color: #aaa;
            padding: 6px 12px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: #2b1230;
            color: #e855f4;
            border: 1px solid #df42f5;
            border-bottom-color: #2b1230;
            font-weight: bold;
        }
        QLabel {
            color: #e0e0e0;
            font-weight: bold;
            letter-spacing: 1px;
        }
    )");

    AudioEngine::instance().init();
    QString pluginsPath = QDir::currentPath() + "/plugins";
    PluginManager::instance().createDefaultPlugins(pluginsPath.toStdString());
    PluginManager::instance().scanPluginsDir(pluginsPath.toStdString());

    glWidget = new GLWidget(this);
    setCentralWidget(glWidget);

    createActions();
    createMenus();
    createDocks();

    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTimer);

    updateEffectsState();
}

MainWindow::~MainWindow() {
    AudioEngine::instance().shutdown();
}

void MainWindow::createActions() {
    QShortcut* cutShortcut = new QShortcut(QKeySequence("C"), this);
    connect(cutShortcut, &QShortcut::activated, this, &MainWindow::cutClipAtPlayhead);

    QShortcut* undoShortcut = new QShortcut(QKeySequence("Ctrl+Z"), this);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        if (AppState::instance().undo()) {
            refreshTrackList();
            refreshActiveEffectsList();
            if (timelinePanel) timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
        }
    });

    QShortcut* redoShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Z"), this);
    connect(redoShortcut, &QShortcut::activated, this, [this]() {
        if (AppState::instance().redo()) {
            refreshTrackList();
            refreshActiveEffectsList();
            if (timelinePanel) timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
        }
    });
    QShortcut* redoShortcut2 = new QShortcut(QKeySequence("Ctrl+Y"), this);
    connect(redoShortcut2, &QShortcut::activated, this, [this]() {
        if (AppState::instance().redo()) {
            refreshTrackList();
            refreshActiveEffectsList();
            if (timelinePanel) timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
        }
    });
    QShortcut* deleteShortcut = new QShortcut(QKeySequence("Delete"), this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelectedClip);
    QShortcut* backspaceShortcut = new QShortcut(QKeySequence("Backspace"), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::deleteSelectedClip);

    QShortcut* zoomInShortcut = new QShortcut(QKeySequence("="), this);
    connect(zoomInShortcut, &QShortcut::activated, this, [this]() {
        if (timelinePanel) timelinePanel->zoomIn();
    });

    QShortcut* zoomInAltShortcut = new QShortcut(QKeySequence("+"), this);
    connect(zoomInAltShortcut, &QShortcut::activated, this, [this]() {
        if (timelinePanel) timelinePanel->zoomIn();
    });

    QShortcut* zoomOutShortcut = new QShortcut(QKeySequence("-"), this);
    connect(zoomOutShortcut, &QShortcut::activated, this, [this]() {
        if (timelinePanel) timelinePanel->zoomOut();
    });
    QShortcut* zoomOutAltShortcut = new QShortcut(QKeySequence("_"), this);
    connect(zoomOutAltShortcut, &QShortcut::activated, this, [this]() {
        if (timelinePanel) timelinePanel->zoomOut();
    });

    QShortcut* stepForwardShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(stepForwardShortcut, &QShortcut::activated, this, [this]() {
        double newTime = std::min(Project::instance().getDuration(), currentPlayhead + (1.0 / 30.0));
        timelinePanel->setPlayhead(newTime);
        onTimelineScrubbed(newTime);
    });

    QShortcut* stepBackwardShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(stepBackwardShortcut, &QShortcut::activated, this, [this]() {
        double newTime = std::max(0.0, currentPlayhead - (1.0 / 30.0));
        timelinePanel->setPlayhead(newTime);
        onTimelineScrubbed(newTime);
    });
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* importAct = fileMenu->addAction("&Import Video Clip...", this, &MainWindow::importVideo);
    importAct->setShortcut(QKeySequence("Ctrl+I"));

    QAction* openAct = fileMenu->addAction("&Open Project...", this, &MainWindow::openProject);
    openAct->setShortcut(QKeySequence("Ctrl+O"));

    QAction* saveAct = fileMenu->addAction("&Save Project...", this, &MainWindow::saveProject);
    saveAct->setShortcut(QKeySequence("Ctrl+S"));

    QAction* exportAct = fileMenu->addAction("&Export Video...", this, &MainWindow::exportVideo);
    exportAct->setShortcut(QKeySequence("Ctrl+E"));

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("Toggle Playback", this, &MainWindow::togglePlayback)->setShortcut(QKeySequence("Space"));

    QMenu* effectsMenu = menuBar()->addMenu("&Effects");
    effectsMenu->addAction("Reload Plugins", this, [this]() {
        QString pluginsPath = QApplication::applicationDirPath() + "/plugins";
        PluginManager::instance().scanPluginsDir(pluginsPath.toStdString());
        updateEffectsState();
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
    asyncDecodeAct->setChecked(false);
    connect(asyncDecodeAct, &QAction::toggled, this, [](bool enabled) {
        VideoEngine::instance().setAsyncDecodeEnabled(enabled);
    });

    VideoEngine::instance().setAsyncDecodeEnabled(false);
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

void MainWindow::createDocks() {
    QDockWidget* sidebarDock = new QDockWidget("Library", this);
    sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sidebarDock->setTitleBarWidget(new QWidget()); 

    QWidget* sidebarWidget = new QWidget(sidebarDock);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(4, 4, 4, 4);
    sidebarLayout->setSpacing(6);

    mediaPool = new MediaPool(sidebarWidget);
    connect(mediaPool, &MediaPool::mediaSelected, this, &MainWindow::onClipSelected);
    sidebarLayout->addWidget(mediaPool, 1);

    trackControl = new TrackControl(sidebarWidget);
    connect(trackControl, &TrackControl::trackSelected, this, [this](int row) { selectTrackIndex(row); });
    connect(trackControl, &TrackControl::newTrackRequested, this, [this]() {
        TimelineTrack track;
        track.id = static_cast<int>(Project::instance().getTracks().size()) + 1;
        track.name = "Track " + std::to_string(track.id);
        Project::instance().getTracks().push_back(track);
        refreshTrackList();
        selectTrackIndex(static_cast<int>(Project::instance().getTracks().size()) - 1);
    });
    connect(trackControl, &TrackControl::moveUpRequested, this, [this]() { moveSelectedTrack(-1); });
    connect(trackControl, &TrackControl::moveDownRequested, this, [this]() { moveSelectedTrack(1); });
    connect(trackControl, &TrackControl::deleteTrackRequested, this, [this]() { deleteSelectedTrack(); });
    connect(trackControl, &TrackControl::cutClipRequested, this, [this]() { cutClipAtPlayhead(); });
    connect(trackControl, &TrackControl::deleteClipRequested, this, [this]() { deleteSelectedClip(); });
    sidebarLayout->addWidget(trackControl, 1);

    effectsBrowser = new EffectsBrowser(sidebarWidget);
    connect(effectsBrowser, &EffectsBrowser::effectDoubleClicked, this, &MainWindow::onEffectSelected);
    sidebarLayout->addWidget(effectsBrowser, 2);

    QWidget* activeContainer = new QWidget(sidebarWidget);
    activeContainer->setObjectName("activeContainer");
    QVBoxLayout* activeLayout = new QVBoxLayout(activeContainer);
    activeLayout->setContentsMargins(8, 8, 8, 8);
    QLabel* activeTitle = new QLabel("A C T I V E", activeContainer);
    activeTitle->setStyleSheet("font-weight: bold; color: white;");
    activeLayout->addWidget(activeTitle);
    activeEffectsList = new QListWidget(activeContainer);
    connect(activeEffectsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onActiveEffectSelected);
    activeLayout->addWidget(activeEffectsList, 2);

    QPushButton* removeEffectButton = new QPushButton("Remove Selected Effect", activeContainer);
    removeEffectButton->setStyleSheet("QPushButton { background: #321818; color: #ffb0b0; border: 1px solid #6b2b2b; padding: 4px; font-size: 10px; } QPushButton:hover { background: #8b2f2f; color: white; }");
    connect(removeEffectButton, &QPushButton::clicked, this, &MainWindow::removeSelectedEffect);
    activeLayout->addWidget(removeEffectButton);
    sidebarLayout->addWidget(activeContainer, 2);

    sidebarWidget->setLayout(sidebarLayout);
    sidebarDock->setWidget(sidebarWidget);
    addDockWidget(Qt::LeftDockWidgetArea, sidebarDock);

    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setTitleBarWidget(new QWidget()); 

    QWidget* controlContainer = new QWidget(inspectorDock);
    controlContainer->setObjectName("controlContainer");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlContainer);
    controlLayout->setContentsMargins(8, 8, 8, 8);
    QLabel* controlTitle = new QLabel("C O N T R O L L", controlContainer);
    controlTitle->setStyleSheet("font-weight: bold; color: white;");
    controlLayout->addWidget(controlTitle);

    inspectorPanel = new Inspector(controlContainer);
    inspectorPanel->setStyleSheet("background: transparent; border: none;");
    controlLayout->addWidget(inspectorPanel, 1);
    connect(inspectorPanel, &Inspector::parameterChanged, this, &MainWindow::onParameterChanged);
    connect(inspectorPanel, &Inspector::scrubRequested, this, &MainWindow::onTimelineScrubbed);
    connect(inspectorPanel, &Inspector::keyframeRemoveRequested, this, [this](const QString& effId, const QString& paramName, double time) {
        auto* effects = activeEffects();
        if (!effects) return;
        for (auto& eff : *effects) {
            if (QString::fromStdString(eff.pluginId) == effId) {
                for (auto& param : eff.parameters) {
                    if (QString::fromStdString(param.name) == paramName) {
                        param.curve.removeKeyframeAt(time, 0.05);
                        timelinePanel->update();
                        refreshActiveEffectsList();
                        onTimelineScrubbed(currentPlayhead);
                        refreshInspectorForSelectedEffect();
                        return;
                    }
                }
            }
        }
    });
    connect(inspectorPanel, &Inspector::keyframeRequested, this, [this](const QString& effId, const QString& paramName, double time, double value) {
        auto* effects = activeEffects();
        if (!effects) return;
        for (auto& eff : *effects) {
            if (QString::fromStdString(eff.pluginId) == effId) {
                for (auto& param : eff.parameters) {
                    if (QString::fromStdString(param.name) == paramName) {
                        param.curve.insertKeyframe(time, value);
                        timelinePanel->update();
                        refreshActiveEffectsList();
                        inspectorPanel->setCurrentTime(currentPlayhead);
                        onTimelineScrubbed(currentPlayhead);
                        refreshInspectorForSelectedEffect();
                        return;
                    }
                }
            }
        }
    });

    connect(inspectorPanel, &Inspector::removeEffectRequested, this, [this](const QString& effectId) {
        if (Project::instance().getTracks().empty()) return;
        auto& effects = Project::instance().getTracks().front().effects;
        auto it = std::remove_if(effects.begin(), effects.end(), [&](const AppliedEffect& eff) {
            return QString::fromStdString(eff.pluginId) == effectId;
        });
        if (it != effects.end()) {
            effects.erase(it, effects.end());
            inspectorPanel->clearInspector();
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
            timelinePanel->update();
        }
    });

    connect(inspectorPanel, &Inspector::parameterSelected, this, [this](const QString& effId, const QString& paramName) {
        timelinePanel->selectParameter(effId, paramName);
    });

    inspectorDock->setWidget(controlContainer);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    QDockWidget* bottomDock = new QDockWidget("Workspaces", this);
    bottomDock->setTitleBarWidget(new QWidget()); 
    QWidget* bottomContainer = new QWidget(bottomDock);
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(4, 4, 4, 4);
    bottomLayout->setSpacing(4);


    bottomTabs = new QTabWidget(bottomContainer);
    bottomTabs->setTabPosition(QTabWidget::South);

    timelinePanel = new Timeline(this);
    connect(timelinePanel, &Timeline::scrubbed, this, &MainWindow::onTimelineScrubbed);
    connect(timelinePanel, &Timeline::clipSelected, this, [this](int trackIndex, int clipIndex) {
        selectTrackIndex(trackIndex);
        selectClip(trackIndex, clipIndex);
    });
    connect(timelinePanel, &Timeline::effectDropped, this, [this](int trackIndex, double dropTime, const QString& effectName) {
        QString pluginId;
        bool isTransition = false;
        if (effectsBrowser) {
            auto* tree = effectsBrowser->getTreeWidget();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                QTreeWidgetItem* parent = tree->topLevelItem(i);
                for (int j = 0; j < parent->childCount(); ++j) {
                    if (parent->child(j)->text(0) == effectName) {
                        pluginId = parent->child(j)->data(0, Qt::UserRole).toString();
                        if (parent->text(0) == "Transitions") isTransition = true;
                        break;
                    }
                }
                if (!pluginId.isEmpty()) break;
            }
        }
        if (pluginId.isEmpty()) return;

        auto& tracks = Project::instance().getTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
        auto& track = tracks[trackIndex];

        if (isTransition) {
            double bestDist = 1.0; 
            double cutPoint = -1.0;
            const ProjectClip* leftClip = nullptr;
            const ProjectClip* rightClip = nullptr;

            for (const auto& c : track.clips) {
                double dist = std::abs(c.timelineStart - dropTime);
                if (dist < bestDist) {
                    bestDist = dist;
                    cutPoint = c.timelineStart;
                    rightClip = &c;
                }
            }

            if (rightClip) {
                for (const auto& c : track.clips) {
                    if (std::abs((c.timelineStart + c.sourceDuration) - cutPoint) < 0.1) {
                        leftClip = &c;
                        break;
                    }
                }
            }

            if (leftClip && rightClip) {
                AppState::instance().pushUndoState();
                ProjectTransition trans;
                trans.id = "trans_" + std::to_string(rand());
                trans.pluginId = pluginId.toStdString();
                trans.leftClipId = leftClip->id;
                trans.rightClipId = rightClip->id;
                trans.duration = 1.0; 

                auto* plugin = PluginManager::instance().findPlugin(trans.pluginId);
                if (plugin) {
                    trans.parameters = plugin->parameters;
                }

                track.transitions.erase(std::remove_if(track.transitions.begin(), track.transitions.end(),
                    [&](const ProjectTransition& t) { return t.rightClipId == rightClip->id; }), track.transitions.end());

                track.transitions.push_back(trans);
                timelinePanel->update();
            }
        } else {
            ProjectClip* targetClip = nullptr;
            for (auto& c : track.clips) {
                if (dropTime >= c.timelineStart && dropTime < c.timelineStart + c.sourceDuration) {
                    targetClip = &c;
                    break;
                }
            }
            if (targetClip) {
                AppState::instance().pushUndoState();
                if (!targetClip->useClipEffects) {
                    targetClip->effects = track.effects;
                    targetClip->useClipEffects = true;
                }
                targetClip->effects.push_back(createEffectTemplate(pluginId));
                timelinePanel->update();
            } else {
                AppState::instance().pushUndoState();
                track.effects.push_back(createEffectTemplate(pluginId));
                timelinePanel->update();
            }
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
        }
    });
    connect(timelinePanel, &Timeline::clipMoveStarted, this, [this]() {
        AppState::instance().pushUndoState();
    });
    connect(timelinePanel, &Timeline::clipMoveRequested, this, [this](int trackIndex, int clipIndex, double newTimelineStart) {
        auto& tracks = Project::instance().getTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
        auto& track = tracks[trackIndex];
        if (clipIndex < 0 || clipIndex >= static_cast<int>(track.clips.size())) return;

        auto& clip = track.clips[clipIndex];
        clip.timelineStart = std::max(0.0, newTimelineStart);
        if (clip.id == activeClipId.toStdString()) {
            std::vector<float> audioSamples;
            if (VideoEngine::instance().getAudioSamples(clip.id, audioSamples)) {
                AudioEngine::instance().loadClipSamples(audioSamples, clip.timelineStart);
            }
        }
        sortTrackClips(track);
        int newClipIndex = -1;
        for (int i = 0; i < static_cast<int>(track.clips.size()); ++i) {
            if (track.clips[i].id == activeClipId.toStdString()) {
                newClipIndex = i;
                break;
            }
        }
        if (newClipIndex != -1) timelinePanel->updateDragIndices(trackIndex, newClipIndex);
        refreshTrackList();
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    });
    connect(timelinePanel, &Timeline::clipTrackChangeRequested, this, [this](int fromTrack, int fromClip, int toTrack, double newTimelineStart) {
        auto& tracks = Project::instance().getTracks();
        if (fromTrack < 0 || fromTrack >= static_cast<int>(tracks.size())) return;
        if (toTrack < 0 || toTrack >= static_cast<int>(tracks.size())) return;
        auto& srcTrack = tracks[fromTrack];
        if (fromClip < 0 || fromClip >= static_cast<int>(srcTrack.clips.size())) return;
        auto clipCopy = srcTrack.clips[fromClip];
        clipCopy.timelineStart = std::max(0.0, newTimelineStart);
        srcTrack.clips.erase(srcTrack.clips.begin() + fromClip);
        auto& dstTrack = tracks[toTrack];
        dstTrack.clips.push_back(clipCopy);
        if (clipCopy.id == activeClipId.toStdString()) {
            std::vector<float> audioSamples;
            if (VideoEngine::instance().getAudioSamples(clipCopy.id, audioSamples)) {
                AudioEngine::instance().loadClipSamples(audioSamples, clipCopy.timelineStart);
            }
        }
        sortTrackClips(srcTrack);
        sortTrackClips(dstTrack);
        int newClipIndex = -1;
        for (int i = 0; i < static_cast<int>(dstTrack.clips.size()); ++i) {
            if (dstTrack.clips[i].id == activeClipId.toStdString()) {
                newClipIndex = i;
                break;
            }
        }
        if (newClipIndex != -1) timelinePanel->updateDragIndices(toTrack, newClipIndex);
        refreshTrackList();
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    });
    QScrollArea* timelineScroll = new QScrollArea(this);
    timelineScroll->setWidget(timelinePanel);
    timelineScroll->setWidgetResizable(true);
    timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    timelineScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    bottomTabs->addTab(timelineScroll, "Timeline Editor");

    bottomLayout->addWidget(bottomTabs);
    bottomDock->setWidget(bottomContainer);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);

    sidebarDock->setMinimumWidth(220);
    inspectorDock->setMinimumWidth(260);

    QList<QDockWidget*> hDocks;
    hDocks << sidebarDock << inspectorDock;
    QList<int> hSizes;
    hSizes << 240 << 300;
    resizeDocks(hDocks, hSizes, Qt::Horizontal);

    bottomDock->setMinimumHeight(240);
}

void MainWindow::refreshTrackList() {
    if (trackControl) {
        trackControl->populateTracks(Project::instance().getTracks().size());
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
    activeClipId = QString::fromStdString(clip.id);
    activeFilePath = QString::fromStdString(clip.filePath);
    std::vector<float> audioSamples;
    if (VideoEngine::instance().getAudioSamples(clip.id, audioSamples)) {
        AudioEngine::instance().loadClipSamples(audioSamples, clip.timelineStart);
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
    return track.clips.empty() ? nullptr : &track.clips.front();
}

ProjectClip* MainWindow::clipAtTime(TimelineTrack& track, double time) const {
    for (auto& clip : track.clips) {
        const double endTime = clip.timelineStart + clip.sourceDuration;
        if (time >= clip.timelineStart && time < endTime) {
            return &clip;
        }
    }
    return track.clips.empty() ? nullptr : &track.clips.front();
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
    if (activeClipId.isEmpty()) return;
    AppState::instance().pushUndoState();
    auto& tracks = Project::instance().getTracks();
    for (auto& track : tracks) {
        auto it = std::remove_if(track.clips.begin(), track.clips.end(),
                                 [this](const ProjectClip& c) { return c.id == activeClipId.toStdString(); });
        if (it != track.clips.end()) {
            track.clips.erase(it, track.clips.end());
            break; 
        }
    }
    activeClipId.clear();
    activeFilePath.clear();
    AudioEngine::instance().clearClipSamples();
    if (mediaPool) mediaPool->clearSelection();
    inspectorPanel->clearInspector();
    refreshTrackList();
    timelinePanel->update();
    onTimelineScrubbed(currentPlayhead);
}

void MainWindow::cutClipAtPlayhead() {
    AppState::instance().pushUndoState();
    auto& tracks = Project::instance().getTracks();
    if (tracks.empty()) return;

    int index = trackControl ? trackControl->getSelectedTrack() : 0;
    if (index < 0 || index >= static_cast<int>(tracks.size())) index = 0;

    auto& track = tracks[index];
    ProjectClip* clip = clipAtTime(track, currentPlayhead);
    if (!clip) return;

    double cutTime = std::clamp(currentPlayhead, clip->timelineStart + 0.01, clip->timelineStart + clip->sourceDuration - 0.01);
    double leftDuration = cutTime - clip->timelineStart;
    double rightDuration = clip->sourceDuration - leftDuration;
    if (leftDuration <= 0.01 || rightDuration <= 0.01) return;

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

    if (VideoEngine::instance().loadVideo(rightClip.id, rightClip.filePath)) {
        track.clips.insert(std::upper_bound(track.clips.begin(), track.clips.end(), rightClip.timelineStart,
            [](double time, const ProjectClip& c) { return time < c.timelineStart; }), rightClip);
    } else {
        qWarning() << "MainWindow Trace: Failed to load split clip" << QString::fromStdString(rightClip.id);
    }

    sortTrackClips(track);
    refreshTrackList();
    refreshActiveEffectsList();
    timelinePanel->update();
    onTimelineScrubbed(currentPlayhead);
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
        "Video Files (*.mp4 *.mov *.mkv *.avi *.webm);;All Files (*)"
    );
    if (filePath.isEmpty()) return;

    QString standardizedPath = MediaImporter::transcodeToStandardMp4(filePath);
    if (standardizedPath.isEmpty()) {
        qWarning() << "Import: Falling back to original file because MP4 conversion failed.";
        standardizedPath = filePath;
    }

    QString clipId = QFileInfo(filePath).baseName();
    if (VideoEngine::instance().loadVideo(clipId.toStdString(), standardizedPath.toStdString())) {
        AppState::instance().pushUndoState();
        if (mediaPool) mediaPool->addMedia(clipId);

        double maxDuration = 10.0;
        for (const auto& t : Project::instance().getTracks()) {
            for (const auto& c : t.clips) {
                maxDuration = std::max(maxDuration, c.timelineStart + c.sourceDuration);
            }
        }
        std::vector<float> audioSamples;
        if (VideoEngine::instance().getAudioSamples(clipId.toStdString(), audioSamples)) {
            AudioEngine::instance().loadClipSamples(audioSamples, maxDuration);
        } else {
            AudioEngine::instance().clearClipSamples();
        }
        ProjectClip clip;
        clip.id = clipId.toStdString();
        clip.filePath = standardizedPath.toStdString();
        clip.sourceStart = 0.0;
        DecodedVideoFrame dummy;
        VideoEngine::instance().getFrame(clip.id, 0.0, dummy);
        clip.sourceDuration = VideoEngine::instance().getDuration(clip.id);
        if (clip.sourceDuration <= 0.0) clip.sourceDuration = 30.0; 
        clip.timelineStart = maxDuration;

        TimelineTrack track;
        track.id = Project::instance().getTracks().size() + 1;
        track.name = "Track " + std::to_string(track.id);
        track.clips.push_back(clip);

        Project::instance().getTracks().push_back(track);
        refreshTrackList();
        selectTrackIndex(static_cast<int>(Project::instance().getTracks().size()) - 1);

        activeClipId = clipId;
        activeFilePath = standardizedPath;


        timelinePanel->setDuration(maxDuration);
        onTimelineScrubbed(0.0);
        refreshActiveEffectsList();
        syncEffectStackToRenderer();

    }
}

void MainWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, "Open Project", "", "Project Files (*.json)");
    if (!path.isEmpty()) {
        if (Project::instance().load(path.toStdString())) {
            if (mediaPool) mediaPool->clearMedia();
            for (const auto& track : Project::instance().getTracks()) {
                for (const auto& clip : track.clips) {
                    VideoEngine::instance().loadVideo(clip.id, clip.filePath);
                }
            }
            refreshTrackList();
            const auto& tracks = Project::instance().getTracks();
            if (!tracks.empty() && !tracks.front().clips.empty()) {
                const auto& clip = tracks.front().clips.front();
                activeClipId = QString::fromStdString(clip.id);
                activeFilePath = QString::fromStdString(clip.filePath);
                if (mediaPool) mediaPool->addMedia(activeClipId);
                timelinePanel->setDuration(Project::instance().getDuration());
                refreshActiveEffectsList();
                syncEffectStackToRenderer();
                selectTrackIndex(0);
                onTimelineScrubbed(0.0);
            }
        }
    }
}

void MainWindow::saveProject() {
    QString path = QFileDialog::getSaveFileName(this, "Save Project", "", "Project Files (*.json)");
    if (!path.isEmpty()) {
        if (Project::instance().save(path.toStdString())) {
        }
    }
}

void MainWindow::togglePlayback() {
    isPlaying = !isPlaying;
    if (isPlaying) {
        playbackTimer->start(16); 
        AudioEngine::instance().start();
    } else {
        playbackTimer->stop();
        AudioEngine::instance().stop();
    }
}

void MainWindow::onPlaybackTimer() {
    currentPlayhead = AudioEngine::instance().getPlayheadTime();
    double maxDuration = 10.0;
    for (const auto& t : Project::instance().getTracks()) {
        for (const auto& c : t.clips) {
            maxDuration = std::max(maxDuration, c.timelineStart + c.sourceDuration);
        }
    }
    if (currentPlayhead >= maxDuration) {
        currentPlayhead = 0.0; 
        AudioEngine::instance().setPlayheadTime(0.0);
    }

    onTimelineScrubbed(currentPlayhead);

    if (isPlaying && !activeClipId.isEmpty() && VideoEngine::instance().isAsyncDecodeEnabled()) {
        const double clipFps = std::max(1.0, VideoEngine::instance().getFps(activeClipId.toStdString()));
        const double prefetchTime = std::min(currentPlayhead + (1.0 / clipFps), maxDuration);
        VideoEngine::instance().requestFrameAsync(activeClipId.toStdString(), prefetchTime);
    }
}

void MainWindow::onTimelineScrubbed(double time) {
    currentPlayhead = time;
    timelinePanel->setPlayhead(time);
    inspectorPanel->setCurrentTime(time);
    glWidget->setPlaybackTime(time);

    const ProjectClip* topClip = nullptr;
    const TimelineTrack* topTrack = nullptr;
    const ProjectTransition* activeTrans = nullptr;
    const ProjectClip* transLeftClip = nullptr;
    const ProjectClip* transRightClip = nullptr;
    const auto& tracks = Project::instance().getTracks();
    for (auto it = tracks.rbegin(); it != tracks.rend(); ++it) {
        for (const auto& trans : it->transitions) {
            const ProjectClip* left = nullptr;
            const ProjectClip* right = nullptr;
            for (const auto& c : it->clips) {
                if (c.id == trans.leftClipId) left = &c;
                if (c.id == trans.rightClipId) right = &c;
            }
            if (left && right) {
                double cutPoint = right->timelineStart;
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
        double cutPoint = transRightClip->timelineStart;
        double transStart = cutPoint - activeTrans->duration / 2.0;
        double progress = (time - transStart) / activeTrans->duration;
        progress = std::clamp(progress, 0.0, 1.0);

        double localTime1 = std::max(0.0, time - transLeftClip->timelineStart);
        double localTime2 = std::max(0.0, time - transRightClip->timelineStart);
        AudioEngine::instance().setPlayheadTime(time);

        DecodedVideoFrame frame1, frame2;
        bool gotFrame1 = VideoEngine::instance().getFrame(transLeftClip->id, localTime1, frame1);
        bool gotFrame2 = VideoEngine::instance().getFrame(transRightClip->id, localTime2, frame2);

        if (gotFrame1 && gotFrame2 && !frame1.rgbData.empty() && !frame2.rgbData.empty()) {
            glWidget->updateTransitionFrames(frame1, frame2, progress, activeTrans->pluginId);
        } else if (gotFrame1 && !frame1.rgbData.empty()) {
            glWidget->updateFrame(frame1);
        } else if (gotFrame2 && !frame2.rgbData.empty()) {
            glWidget->updateFrame(frame2);
        } else {
            glWidget->clearFrame();
        }
        applyEffectsToRenderer(time, topTrack, nullptr);
    } else if (topClip) {
        double localTime = time - topClip->timelineStart;
        if (localTime < 0.0) localTime = 0.0;
        AudioEngine::instance().setPlayheadTime(time);

        DecodedVideoFrame frame;
        const std::string clipKey = topClip->id;
        bool gotFrame = false;
        if (isPlaying && VideoEngine::instance().isAsyncDecodeEnabled()) {
            VideoEngine::instance().requestFrameAsync(clipKey, localTime);
            gotFrame = VideoEngine::instance().tryGetCachedFrame(clipKey, localTime, frame);
        } else {
            gotFrame = VideoEngine::instance().getFrame(clipKey, localTime, frame);
            if (!isPlaying && VideoEngine::instance().isAsyncDecodeEnabled()) {
                VideoEngine::instance().requestFrameAsync(clipKey, localTime);
            }
        }

        if (gotFrame && frame.width > 0 && frame.height > 0 && !frame.rgbData.empty()) {
            glWidget->updateFrame(frame);
        } else {
            glWidget->clearFrame();
        }
        applyEffectsToRenderer(time, topTrack, topClip);
    } else {
        glWidget->clearFrame();
        AudioEngine::instance().setPlayheadTime(0.0);
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

void MainWindow::applyEffectsToRenderer(double time, const TimelineTrack* activeTrack, const ProjectClip* activeClip) {
    if (!activeTrack) {
        glWidget->clearFrame();
        glWidget->setActiveEffects({});
        return;
    }
    auto evalParam = [time](const ShaderParameter& p) {
        return p.curve.getKeyframes().empty() ? p.currentVal : p.curve.evaluate(time);
    };
    if (activeClip && (activeClipId != QString::fromStdString(activeClip->id))) {
        activeClipId = QString::fromStdString(activeClip->id);
        activeFilePath = QString::fromStdString(activeClip->filePath);
    }
    VideoEngine::instance().setDatamoshing(activeClipId.toStdString(), false, false, 0.0, 1, 0.0);
    VideoEngine::instance().setOpticalSmear(activeClipId.toStdString(), false, 0.0, 0.0, 0.0, 0.0);
    VideoEngine::instance().setCpuXor(activeClipId.toStdString(), false, 0.5, 1.0);
    VideoEngine::instance().setCpuOr(activeClipId.toStdString(), false, 0.5, 1.0);
    VideoEngine::instance().setCpuAnd(activeClipId.toStdString(), false, 1.0, 1.0);
    VideoEngine::instance().setCpuXnor(activeClipId.toStdString(), false, 0.5, 1.0);
    VideoEngine::instance().setCpuNand(activeClipId.toStdString(), false, 0.5, 1.0);

    const auto& effects = (activeClip && !activeClip->effects.empty()) ? activeClip->effects : activeTrack->effects;
    bool hasDatamoshEffect = false;
    for (const auto& eff : effects) {
        if (eff.pluginId == "datamosh") {
            hasDatamoshEffect = true;
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double iDrop = readParam(0, 0.0), pDup = readParam(1, 0.0), pDupCount = readParam(2, 4.0), pDrop = readParam(3, 0.0);
            bool datamoshEnabled = (iDrop >= 0.5) || (pDup > 0.0) || (pDrop > 0.0);
            VideoEngine::instance().setDatamoshing(activeClipId.toStdString(), datamoshEnabled, (iDrop >= 0.5), pDup, static_cast<int>(pDupCount), pDrop);
        } else if (eff.pluginId == "optical_smear") {
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double frameMerge = readParam(0, 0.25), frameSmear = readParam(1, 0.1), colorBleed = readParam(2, 0.25), lumaBias = readParam(3, 0.2);
            bool smearEnabled = (frameMerge > 0.0) || (frameSmear > 0.0) || (colorBleed > 0.0) || (lumaBias > 0.0);
            VideoEngine::instance().setOpticalSmear(activeClipId.toStdString(), smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
        } else if (eff.pluginId == "cpu_xor") {
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double xorValue = readParam(0, 0.5);
            double intensity = readParam(1, 1.0);
            VideoEngine::instance().setCpuXor(activeClipId.toStdString(), true, xorValue, intensity);
        } else if (eff.pluginId == "cpu_or") {
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double orValue = readParam(0, 0.5);
            double intensity = readParam(1, 1.0);
            VideoEngine::instance().setCpuOr(activeClipId.toStdString(), true, orValue, intensity);
        } else if (eff.pluginId == "cpu_and") {
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double andValue = readParam(0, 1.0);
            double intensity = readParam(1, 1.0);
            VideoEngine::instance().setCpuAnd(activeClipId.toStdString(), true, andValue, intensity);
        } else if (eff.pluginId == "cpu_xnor") {
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double xnorValue = readParam(0, 0.5);
            double intensity = readParam(1, 1.0);
            VideoEngine::instance().setCpuXnor(activeClipId.toStdString(), true, xnorValue, intensity);
        } else if (eff.pluginId == "cpu_nand") {
            auto readParam = [&](size_t index, double fallback) { return index < eff.parameters.size() ? evalParam(eff.parameters[index]) : fallback; };
            double nandValue = readParam(0, 0.5);
            double intensity = readParam(1, 1.0);
            VideoEngine::instance().setCpuNand(activeClipId.toStdString(), true, nandValue, intensity);
        } else {
            ShaderPlugin* plugin = PluginManager::instance().findPlugin(eff.pluginId);
            if (plugin) {
                for (size_t p = 0; p < eff.parameters.size() && p < plugin->parameters.size(); ++p) {
                    plugin->parameters[p].currentVal = evalParam(eff.parameters[p]);
                }
            }
        }
    }
    glWidget->update();
}

void MainWindow::onClipSelected(const QString& clipId) {
    Q_UNUSED(clipId);
}

void MainWindow::onEffectSelected(const QString& targetId) {
    auto* track = currentTrack();
    if (!track) return;

    auto* effects = activeEffects();
    if (!effects) return;

    if (std::none_of(effects->begin(), effects->end(), [&](const AppliedEffect& eff) {
            return QString::fromStdString(eff.pluginId) == targetId;
        })) {
        AppState::instance().pushUndoState();
        effects->push_back(createEffectTemplate(targetId));
    }

    refreshActiveEffectsList();
    syncEffectStackToRenderer();

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
    auto* track = currentTrack();
    if (!track) return nullptr;
    return clipAtTime(*track, currentPlayhead);
}

const ProjectClip* MainWindow::currentClip() const {
    const auto* track = currentTrack();
    if (!track) return nullptr;
    return clipAtTime(*track, currentPlayhead);
}

std::vector<AppliedEffect>* MainWindow::activeEffects() {
    if (auto* clip = currentClip()) {
        if (clip->useClipEffects) return &clip->effects;
    }
    if (auto* track = currentTrack()) return &track->effects;
    return nullptr;
}

const std::vector<AppliedEffect>* MainWindow::activeEffects() const {
    if (const auto* clip = currentClip()) {
        if (clip->useClipEffects) return &clip->effects;
    }
    if (const auto* track = currentTrack()) return &track->effects;
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
            make("iDrop", "I-Frame Drop Toggle", 0.0, 1.0, 0.0, true),
            make("pDup", "P-Frame Bloom Duplicate", 0.0, 1.0, 0.0),
            make("pDupCount", "P-Frame Duplicate Count", 1.0, 20.0, 4.0),
            make("pDrop", "P-Frame Stutter Drop", 0.0, 1.0, 0.0)
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

void MainWindow::refreshActiveEffectsList() {
    if (!activeEffectsList) return;
    activeEffectsList->clear();

    const auto* effects = activeEffects();
    if (!effects) return;

    for (const auto& eff : *effects) {
        QString id = QString::fromStdString(eff.pluginId);
        auto* item = new QListWidgetItem(effectDisplayNameForId(id), activeEffectsList);
        item->setData(Qt::UserRole, id);
    }
}

void MainWindow::syncEffectStackToRenderer() {
    std::vector<AppliedEffect> customPlugins;
    auto* track = currentTrack();
    if (!track) {
        glWidget->setActiveEffects(customPlugins);
        return;
    }
    const ProjectClip* clip = currentClip();
    const auto& effects = (clip && !clip->effects.empty()) ? clip->effects : track->effects;
    for (const auto& eff : effects) {
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
                    if (!param.curve.getKeyframes().empty()) {
                        param.curve.insertKeyframe(currentPlayhead, value);
                        timelinePanel->update(); 
                    }
                    syncEffectStackToRenderer();
                    if (!isPlaying) {
                        onTimelineScrubbed(currentPlayhead);
                    }
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
        isPlaying
    );
}
