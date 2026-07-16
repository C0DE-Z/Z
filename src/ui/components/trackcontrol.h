#ifndef TRACKCONTROL_H
#define TRACKCONTROL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class TrackControl : public QWidget {
    Q_OBJECT
public:
    explicit TrackControl(QWidget* parent = nullptr);

    void populateTracks(int count);
    void selectTrack(int index);
    int getSelectedTrack() const;

signals:
    void trackSelected(int index);
    
    void newTrackRequested();
    void moveUpRequested();
    void moveDownRequested();
    void deleteTrackRequested();
    void cutClipRequested();
    void deleteClipRequested();

private:
    QListWidget* trackList;
};

#endif
