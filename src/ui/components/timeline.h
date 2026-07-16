#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

class Timeline : public QWidget {
    Q_OBJECT
public:
    explicit Timeline(QWidget* parent = nullptr);

    void setPlayhead(double time);
    void setDuration(double duration);
    void selectParameter(const QString& effectId, const QString& paramName);

    void zoomIn();
    void zoomOut();

    void updateDragIndices(int newTrack, int newClip);

signals:
    void scrubbed(double time);
    void keyframeAdded(double time, double value);
    void clipMoveStarted();
    void clipMoveRequested(int trackIndex, int clipIndex, double newTimelineStart);
    void clipTrackChangeRequested(int fromTrack, int fromClip, int toTrack, double newTimelineStart);
    void clipSelected(int trackIndex, int clipIndex);
    void effectDropped(int trackIndex, double time, const QString& effectName);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    double playheadTime = 0.0;
    double totalDuration = 30.0; 
    double pixelsPerSecond = 20.0; 
    QString activeEffectId;
    QString activeParamName;

    bool draggingClip = false;
    int dragTrackIndex = -1;
    int dragClipIndex = -1;
    double dragClipOffsetTime = 0.0;

    int timeToX(double time) const;
    double xToTime(int x) const;
    bool hitTestClip(const QPoint& pos, int& trackIndex, int& clipIndex, double& clipStart, double& clipDuration) const;
};

#endif 
