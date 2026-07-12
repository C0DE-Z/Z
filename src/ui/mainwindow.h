#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QListWidget>
#include <QTabWidget>
#include <QListWidgetItem>
#include <QTreeWidget>
#include "core/project.h"
#include "engine/glwidget.h"
#include "ui/timeline.h"
#include "ui/inspector.h"
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
    void onEffectSelected(QTreeWidgetItem* item, int column);
    void onActiveEffectSelected(QListWidgetItem* item);
    void removeSelectedEffect();
    void onParameterChanged(const QString& effectId, const QString& paramName, double value);
    void onPlaybackQualityChanged(int index);
    void onToggleFpsOverlay(bool checked);

private:
    QTimer* playbackTimer = nullptr;
    double currentPlayhead = 0.0;
    bool isPlaying = false;

    QString activeClipId;
    QString activeFilePath;

    GLWidget* glWidget = nullptr;
    Timeline* timelinePanel = nullptr;
    Inspector* inspectorPanel = nullptr;
    QListWidget* trackList = nullptr;
    QTreeWidget* effectsTree = nullptr;
    QListWidget* activeEffectsList = nullptr;
    QListWidget* mediaList = nullptr;

    QTabWidget* bottomTabs = nullptr;

    void createActions();
    void createMenus();

    void pushUndoState();

private slots:
    void undo();
    void redo();

private:
    QList<QJsonObject> undoStack;
    QList<QJsonObject> redoStack;
    void createDocks();
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
};

#endif 
