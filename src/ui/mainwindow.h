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
#include "engine/detectionworker.h"
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QPointer>
#include <thread>
#include <set>
#include <map>
#include <memory>
#include <cstdint>

class QNetworkReply;
class QProgressDialog;
class QSaveFile;

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
    void detectEntireActiveClip();
    void cancelDetectionScan();
    void checkForUpdates();

private:
    QTimer* playbackTimer = nullptr;
    std::thread importThread;
    std::thread datamoshProxyThread;
    std::set<std::string> datamoshProxyInProgress;
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
    QSlider* yoloConfidenceSlider = nullptr;
    QSlider* yoloNmsSlider = nullptr;
    QComboBox* yoloInputSizeCombo = nullptr;
    QSlider* liveDetectionIntervalSlider = nullptr;
    QSlider* detectionScanIntervalSlider = nullptr;
    QSlider* detectionLineWidthSlider = nullptr;
    QSlider* detectionFillOpacitySlider = nullptr;
    QSlider* detectionLabelSizeSlider = nullptr;
    QSlider* detectionTrailLengthSlider = nullptr;
    QSlider* detectionTrailWidthSlider = nullptr;
    QSlider* detectionTrailOpacitySlider = nullptr;
    QSlider* detectionLinkDistanceSlider = nullptr;
    QSlider* maskFeatherSlider = nullptr;
    QSlider* maskPaddingSlider = nullptr;
    QSlider* maskOutlineWidthSlider = nullptr;
    QCheckBox* liveDetectCheck = nullptr;
    QCheckBox* showBoxesCheck = nullptr;
    QCheckBox* showDetectionLabelsCheck = nullptr;
    QCheckBox* showDetectionConfidenceCheck = nullptr;
    QCheckBox* showDetectionTrackIdsCheck = nullptr;
    QCheckBox* showDetectionTrailsCheck = nullptr;
    QCheckBox* showDetectionLinksCheck = nullptr;
    QCheckBox* showDetectionCentersCheck = nullptr;
    QCheckBox* applyMaskCheck = nullptr;
    QCheckBox* invertMaskCheck = nullptr;
    QCheckBox* openClCheck = nullptr;
    QCheckBox* showPersonOutlineCheck = nullptr;
    QCheckBox* replacePersonBoxesCheck = nullptr;
    QComboBox* detectionShapeCombo = nullptr;
    QComboBox* detectionOverlayStyleCombo = nullptr;
    QComboBox* detectionColorModeCombo = nullptr;
    QLineEdit* yoloModelPathEdit = nullptr;
    QLineEdit* detectionClassFilterEdit = nullptr;
    QListWidget* detectionClassList = nullptr;
    QPushButton* detectEntireClipButton = nullptr;
    QPushButton* cancelDetectionScanButton = nullptr;
    QLabel* detectionStatusLabel = nullptr;
    QNetworkAccessManager* modelDownloadManager = nullptr;
    QNetworkAccessManager* updateCheckManager = nullptr;
    bool liveDetectEnabled = false;
    QElapsedTimer liveDetectionTimer;
    QTimer* detectionSettingsTimer = nullptr;
    std::unique_ptr<DetectionWorker> detectionWorker;
    DetectionWorkerSettings detectionWorkerSettings;
    bool yoloModelReady = false;
    uint64_t modelLoadGeneration = 0;
    uint64_t detectionGeneration = 0;
    uint64_t maskGeneration = 0;
    uint64_t detectionScanGeneration = 0;
    bool detectionScanInProgress = false;
    QString detectionWorkerTrackClipId;
    double detectionWorkerTrackSourceTime = -1.0;
    std::vector<DetectionBox> currentDetections;
    std::vector<DetectionBox> rawCurrentDetections;
    std::shared_ptr<const DecodedVideoFrame> latestDetectionFrame;
    QString latestDetectionFrameClipId;
    double latestDetectionFrameSourceTime = -1.0;
    int currentDetectionFrameWidth = 0;
    int currentDetectionFrameHeight = 0;
    QString detectionSourceClipId;
    double lastDetectionPlayhead = -1.0;
    std::set<std::string> allowedDetectionClasses;
    bool detectionClassFilterEnabled = false;
    std::map<std::string, std::set<int>> rejectedDetectionTracks;

    struct CachedDetectionSample {
        double sourceTime = 0.0;
        int width = 0;
        int height = 0;
        std::vector<DetectionBox> detections;
    };
    struct ClipDetectionCache {
        double sampleInterval = 1.0;
        bool complete = false;
        std::vector<CachedDetectionSample> samples;
    };
    std::map<std::string, ClipDetectionCache> clipDetectionCaches;

    QPointer<QNetworkReply> modelDownloadReply;
    QPointer<QProgressDialog> modelDownloadProgress;
    std::unique_ptr<QSaveFile> modelDownloadFile;
    qsizetype modelDownloadBytes = 0;
    bool modelDownloadInProgress = false;

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
    void applyDetectionOverlayOptions();
    void refreshDetectionMask();
    DetectionWorkerSettings currentDetectionSettings() const;
    void queueDetectionForCurrentFrame(bool forceTrackingReset = false);
    void queueDetectionForSource(
        const QString& sourcePath,
        const QString& sourceClipId,
        double sourceTime,
        bool forceTrackingReset = false);
    void scheduleDetectionSettingsRefresh();
    void postDetectionWorkerResult(DetectionWorkerResult&& result);
    void handleDetectionWorkerResult(const std::shared_ptr<DetectionWorkerResult>& result);
    void applyDetectionResults(
        std::vector<DetectionBox> detections,
        const QString& sourceClipId,
        double sourceTime,
        int width,
        int height,
        bool fromPrecomputedScan = false);
    void applyCurrentDetectionFilters();
    void refreshDetectionList();
    void updateClassFilterFromUi();
    bool applyPrecomputedDetectionsForClip(const ProjectClip& clip, double sourceTime);
    void beginYoloModelLoad(const QString& path);
    void drainModelDownload(QNetworkReply* reply);
    void updateTimecodeDisplay(double time);
    void updateStatusBar();

    void refreshTrackList();
    void updateEffectsState();
    void refreshActiveEffectsList();
    void refreshInspectorForSelectedEffect();
    void syncEffectStackToRenderer();
    void applyEffectsToRenderer(double time, const TimelineTrack* activeTrack, const ProjectClip* activeClip);
    void createDatamoshProxyAsync(const ProjectClip& clip);
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
