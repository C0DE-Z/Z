#include "mainwindow.h"
#include "core/appstate.h"
#include "core/project.h"
#include "engine/audioengine.h"
#include "engine/pluginmanager.h"
#include "engine/videoengine.h"

#include <QDockWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QInputDialog>
#include <QLineEdit>
#include <QTreeWidgetItemIterator>

void MainWindow::createDocks() {
    QDockWidget* sidebarDock = new QDockWidget("Library", this);
    sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sidebarDock->setTitleBarWidget(new QWidget());

    QWidget* sidebarWidget = new QWidget(sidebarDock);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(2, 2, 2, 2);
    sidebarLayout->setSpacing(2);

    QTabWidget* sidebarTabs = new QTabWidget(sidebarWidget);
    sidebarTabs->setTabPosition(QTabWidget::North);

    // Tab 1: Project (Media + Tracks)
    QWidget* projectTab = new QWidget(sidebarTabs);
    QVBoxLayout* projectLayout = new QVBoxLayout(projectTab);
    projectLayout->setContentsMargins(4, 4, 4, 4);
    projectLayout->setSpacing(6);

    mediaPool = new MediaPool(projectTab);
    connect(mediaPool, &MediaPool::mediaSelected, this, &MainWindow::onClipSelected);
    connect(mediaPool, &MediaPool::fileDropped, this, [this](const QString& filePath) {
        importMediaFile(filePath);
    });
    connect(mediaPool, &MediaPool::importRequested, this, &MainWindow::importVideo);
    projectLayout->addWidget(mediaPool, 1);

    trackControl = new TrackControl(projectTab);
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
    projectLayout->addWidget(trackControl, 1);

    sidebarTabs->addTab(projectTab, "Project");

    // Tab 2: Effects Library
    effectsBrowser = new EffectsBrowser(sidebarTabs);
    connect(effectsBrowser, &EffectsBrowser::effectDoubleClicked, this, &MainWindow::onEffectSelected);
    sidebarTabs->addTab(effectsBrowser, "Effects");

    // Tab 3: Active Effects Stack
    QWidget* activeContainer = new QWidget(sidebarTabs);
    activeContainer->setObjectName("activeContainer");
    QVBoxLayout* activeLayout = new QVBoxLayout(activeContainer);
    activeLayout->setContentsMargins(8, 8, 8, 8);
    QLabel* activeTitle = new QLabel("ACTIVE EFFECTS", activeContainer);
    activeTitle->setStyleSheet("font-weight: bold; color: #c4b5fd; font-size: 11px; letter-spacing: 0.5px;");
    activeLayout->addWidget(activeTitle);
    activeEffectsList = new QListWidget(activeContainer);
    connect(activeEffectsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onActiveEffectSelected);
    activeLayout->addWidget(activeEffectsList, 1);

    QPushButton* removeEffectButton = new QPushButton("Remove Selected Effect", activeContainer);
    removeEffectButton->setStyleSheet("QPushButton { background: #251020; color: #f59ef8; border: 1px solid #551d45; padding: 6px; font-size: 11px; font-weight: bold; border-radius: 3px; } QPushButton:hover { background: #45153c; color: white; border-color: #d946ef; }");
    connect(removeEffectButton, &QPushButton::clicked, this, &MainWindow::removeSelectedEffect);
    activeLayout->addWidget(removeEffectButton);

    sidebarTabs->addTab(activeContainer, "Active FX");

    sidebarLayout->addWidget(sidebarTabs);
    sidebarWidget->setLayout(sidebarLayout);
    sidebarDock->setWidget(sidebarWidget);
    addDockWidget(Qt::LeftDockWidgetArea, sidebarDock);

    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setTitleBarWidget(new QWidget());

    QWidget* controlContainer = new QWidget(inspectorDock);
    controlContainer->setObjectName("controlContainer");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlContainer);
    controlLayout->setContentsMargins(8, 8, 8, 8);
    QLabel* controlTitle = new QLabel("INSPECTOR", controlContainer);
    controlTitle->setStyleSheet("font-weight: bold; color: #c4b5fd; font-size: 11px; letter-spacing: 0.5px;");
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
        auto* effects = activeEffects();
        if (!effects) return;
        auto it = std::remove_if(effects->begin(), effects->end(), [&](const AppliedEffect& eff) {
            return QString::fromStdString(eff.pluginId) == effectId;
        });
        if (it != effects->end()) {
            AppState::instance().pushUndoState();
            effects->erase(it, effects->end());
            inspectorPanel->clearInspector();
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
            timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
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
    connect(timelinePanel, &Timeline::transitionApplyRequested, this, [this](int trackIndex, double cutTime, const QString& transitionId) {
        if (addTransitionAtCut(trackIndex, cutTime, transitionId)) {
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
        }
    });
    connect(timelinePanel, &Timeline::clipSelected, this, [this](int trackIndex, int clipIndex) {
        selectTrackIndex(trackIndex);
        selectClip(trackIndex, clipIndex);
    });
    connect(timelinePanel, &Timeline::fileDropped, this, [this](int trackIndex, double dropTime, const QString& filePath) {
        importMediaFile(filePath, trackIndex, dropTime);
    });
    connect(timelinePanel, &Timeline::effectDropped, this, [this](int trackIndex, double dropTime, const QString& effectName) {
        QString pluginId;
        bool isTransition = false;
        if (effectsBrowser) {
            auto* tree = effectsBrowser->getTreeWidget();
            QTreeWidgetItemIterator it(tree);
            while (*it) {
                if ((*it)->text(0) == effectName || (*it)->data(0, Qt::UserRole).toString() == effectName) {
                    pluginId = (*it)->data(0, Qt::UserRole).toString();
                    QTreeWidgetItem* p = (*it)->parent();
                    while (p) {
                        if (p->text(0) == "Transitions") {
                            isTransition = true;
                            break;
                        }
                        p = p->parent();
                    }
                    break;
                }
                ++it;
            }
        }
        if (pluginId.isEmpty()) return;

        if (isTransition) {
            if (addTransitionAtCut(trackIndex, dropTime, pluginId)) {
                refreshActiveEffectsList();
                syncEffectStackToRenderer();
            }
        } else {
            auto& tracks = Project::instance().getTracks();
            if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
            auto& track = tracks[trackIndex];
            ProjectClip* targetClip = nullptr;
            for (auto& c : track.clips) {
                if (dropTime >= c.timelineStart && dropTime < c.timelineStart + c.sourceDuration) {
                    targetClip = &c;
                    break;
                }
            }
            if (!targetClip) return;

            AppState::instance().pushUndoState();
            if (!targetClip->useClipEffects) {
                targetClip->effects = track.effects;
                targetClip->useClipEffects = true;
            }
            targetClip->effects.push_back(createEffectTemplate(pluginId));
            selectedClipId = QString::fromStdString(targetClip->id);
            timelinePanel->update();
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
        }
    });
    connect(timelinePanel, &Timeline::clipMoveStarted, this, [this]() {
        AppState::instance().pushUndoState();
    });
    connect(timelinePanel, &Timeline::clipMoveFinished, this, [this]() {
        auto& tracks = Project::instance().getTracks();
        for (auto& track : tracks) {
            sortTrackClips(track);
        }
        refreshTrackList();
        timelinePanel->update();
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
                AudioEngine::instance().loadClipSamples(audioSamples, clip.timelineStart, clip.sourceStart, clip.sourceDuration);
            }
        }
        timelinePanel->setDuration(std::max(10.0, Project::instance().getDuration() + 5.0));
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    });
    connect(timelinePanel, &Timeline::clipTrackChangeRequested, this, [this](int fromTrack, int fromClip, int toTrack, double newTimelineStart) {
        auto& tracks = Project::instance().getTracks();
        if (fromTrack < 0 || fromTrack >= static_cast<int>(tracks.size())) return;
        if (toTrack < 0 || toTrack > static_cast<int>(tracks.size())) return;

        if (toTrack == static_cast<int>(tracks.size())) {
            TimelineTrack newT;
            newT.id = tracks.size() + 1;
            newT.name = "Track " + std::to_string(newT.id);
            tracks.push_back(newT);
        }
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
                AudioEngine::instance().loadClipSamples(audioSamples, clipCopy.timelineStart, clipCopy.sourceStart, clipCopy.sourceDuration);
            }
        }
        sortTrackClips(srcTrack);
        sortTrackClips(dstTrack);
        int newClipIndex = -1;
        for (int i = 0; i < static_cast<int>(dstTrack.clips.size()); ++i) {
            if (dstTrack.clips[i].id == clipCopy.id) {
                newClipIndex = i;
                break;
            }
        }
        if (newClipIndex != -1) timelinePanel->updateDragIndices(toTrack, newClipIndex);
        timelinePanel->setDuration(std::max(10.0, Project::instance().getDuration() + 5.0));
        refreshTrackList();
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    });

    connect(timelinePanel, &Timeline::deleteClipRequested, this, [this](int trackIndex, int clipIndex) {
        AppState::instance().pushUndoState();
        auto& tracks = Project::instance().getTracks();
        if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size())) {
            if (clipIndex >= 0 && clipIndex < static_cast<int>(tracks[trackIndex].clips.size())) {
                const std::string removedId = tracks[trackIndex].clips[clipIndex].id;
                tracks[trackIndex].clips.erase(tracks[trackIndex].clips.begin() + clipIndex);
                if (selectedClipId == QString::fromStdString(removedId)) {
                    selectedClipId.clear();
                }
                refreshTrackList();
                timelinePanel->update();
            }
        }
    });

    connect(timelinePanel, &Timeline::renameClipRequested, this, [this](int trackIndex, int clipIndex) {
        auto& tracks = Project::instance().getTracks();
        if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size())) {
            if (clipIndex >= 0 && clipIndex < static_cast<int>(tracks[trackIndex].clips.size())) {
                QString currentName = QString::fromStdString(tracks[trackIndex].clips[clipIndex].name);
                bool ok;
                QString newName = QInputDialog::getText(this, "Rename Clip", "New name:", QLineEdit::Normal, currentName, &ok);
                if (ok && !newName.isEmpty()) {
                    AppState::instance().pushUndoState();
                    tracks[trackIndex].clips[clipIndex].name = newName.toStdString();
                    timelinePanel->update();
                }
            }
        }
    });

    connect(timelinePanel, &Timeline::deleteTransitionRequested, this, [this](int trackIndex, int transIndex) {
        AppState::instance().pushUndoState();
        auto& tracks = Project::instance().getTracks();
        if (trackIndex >= 0 && trackIndex < (int)tracks.size()) {
            auto& transitions = tracks[trackIndex].transitions;
            if (transIndex >= 0 && transIndex < (int)transitions.size()) {
                transitions.erase(transitions.begin() + transIndex);
                timelinePanel->update();
            }
        }
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
