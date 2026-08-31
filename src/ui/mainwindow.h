#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QListWidget>
#include <QTabWidget>
#include <QListWidgetItem>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include "core/project.h"
#include "engine/glwidget.h"
#include "ui/components/timeline.h"
#include "ui/components/inspector.h"
#include "ui/components/effectsbrowser.h"
#include "ui/components/mediapool.h"
#include "ui/components/trackcontrol.h"
#include "engine/detector.h"
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QPointer>
#include <thread>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void importVideo();
    void openProject();
    void saveProject();
    void exportVideo();
    void togglePlayback();
    void onPlaybackTimer();

    void onClipSelected(const QString& clipId);
    void onTimelineScrubbed(double time);
    void onEffectSelected(const QString& targetId);
    void onActiveEffectSelected(QListWidgetItem* item);
    void removeSelectedEffect();
    void onParameterChanged(const QString& effectId, const QString& paramName, double value);
    void onPlaybackQualityChanged(int index);
    void onToggleFpsOverlay(bool checked);
    void openPreferences();
    void importMediaFile(const QString& filePath, int targetTrack = -1, double targetTime = -1.0);
    void runDetectionOnCurrentFrame();
    void onDetectionSettingsChanged();
    void clearDetections();
    void chooseYoloModel();
    void downloadRecommendedYoloModel();
    void checkForUpdates();

private:
    QTimer* playbackTimer = nullptr;
    std::thread importThread;
    double currentPlayhead = 0.0;
    bool isPlaying = false;
    bool importInProgress = false;
    bool loopPlayback = true;
    double markIn = -1.0;
    double markOut = -1.0;

    QString selectedClipId;
    QString activeClipId;
    QString activeAudioClipId;
    QString activeFilePath;

    GLWidget* glWidget = nullptr;
    Timeline* timelinePanel = nullptr;
    Inspector* inspectorPanel = nullptr;
    EffectsBrowser* effectsBrowser = nullptr;
    MediaPool* mediaPool = nullptr;
    TrackControl* trackControl = nullptr;
    QListWidget* activeEffectsList = nullptr;
    QTabWidget* bottomTabs = nullptr;

    // Detect / Mask UI
    QListWidget* detectionList = nullptr;
    QSlider* detectSensitivitySlider = nullptr;
    QSlider* detectMinAreaSlider = nullptr;
    QCheckBox* liveDetectCheck = nullptr;
    QCheckBox* showBoxesCheck = nullptr;
    QCheckBox* applyMaskCheck = nullptr;
    QCheckBox* openClCheck = nullptr;
    QComboBox* detectionShapeCombo = nullptr;
    QLineEdit* yoloModelPathEdit = nullptr;
    QLabel* detectionStatusLabel = nullptr;
    QNetworkAccessManager* modelDownloadManager = nullptr;
    QNetworkAccessManager* updateCheckManager = nullptr;
    bool liveDetectEnabled = false;
    QElapsedTimer liveDetectionTimer;
    std::vector<DetectionBox> currentDetections;
    DecodedVideoFrame lastDetectFrame;

    // Transport Bar UI Elements
    QLabel* timecodeLabel = nullptr;
    QPushButton* playPauseBtn = nullptr;
    QPushButton* loopBtn = nullptr;
    QLabel* projectInfoStatusLabel = nullptr;

    // Actions for Dynamic Shortcut Binding
    QAction* importAct = nullptr;
    QAction* openAct = nullptr;
    QAction* saveAct = nullptr;
    QAction* exportAct = nullptr;
    QAction* cutAct = nullptr;
    QAction* deleteAct = nullptr;
    QAction* undoAct = nullptr;
    QAction* redoAct = nullptr;
    QAction* prefAct = nullptr;
    QAction* playPauseAct = nullptr;
    QAction* jumpStartAct = nullptr;
    QAction* jumpEndAct = nullptr;
    QAction* stepFwdAct = nullptr;
    QAction* stepBackAct = nullptr;
    QAction* markInAct = nullptr;
    QAction* markOutAct = nullptr;
    QAction* clearInOutAct = nullptr;
    QAction* zoomInAct = nullptr;
    QAction* zoomOutAct = nullptr;
    QAction* zoomFitAct = nullptr;

    void createActions();
    void createMenus();
    void checkForUpdates(bool interactive);
    void createDocks();
    void createTransportToolbar();
    void applyShortcuts();
    void updateTimecodeDisplay(double time);
    void updateStatusBar();

    void refreshTrackList();
    void updateEffectsState();
    void refreshActiveEffectsList();
    void refreshInspectorForSelectedEffect();
    void syncEffectStackToRenderer();
    void applyEffectsToRenderer(double time, const TimelineTrack* activeTrack, const ProjectClip* activeClip);
    TimelineTrack* currentTrack();
    const TimelineTrack* currentTrack() const;
    ProjectClip* currentClip();
    const ProjectClip* currentClip() const;
    std::vector<AppliedEffect>* activeEffects();
    const std::vector<AppliedEffect>* activeEffects() const;
    const ProjectClip* clipAtTime(const TimelineTrack& track, double time) const;
    ProjectClip* clipAtTime(TimelineTrack& track, double time) const;
    QString effectDisplayNameForId(const QString& effectId) const;
    QString effectIdForDisplayName(const QString& displayName) const;
    bool trackHasEffect(const TimelineTrack& track, const QString& effectId) const;
    AppliedEffect createEffectTemplate(const QString& effectId) const;
    void selectTrackIndex(int index);
    void selectClip(int trackIndex, int clipIndex);
    void sortTrackClips(TimelineTrack& track);
    void moveSelectedTrack(int direction);
    void deleteSelectedTrack();
    void cutClipAtPlayhead();
    void deleteSelectedClip();
    void playPause();
    bool addTransitionAtCut(int trackIndex, double dropTime, const QString& pluginId);
};

#endif 
