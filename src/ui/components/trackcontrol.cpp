#include "trackcontrol.h"
#include <QGridLayout>

TrackControl::TrackControl(QWidget* parent) : QWidget(parent) {
    this->setObjectName("tracksContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel* title = new QLabel("TRACKS", this);
    title->setStyleSheet("font-weight: bold; color: #c4b5fd; letter-spacing: 0.8px; font-size: 10px;");
    layout->addWidget(title);

    trackList = new QListWidget(this);
    trackList->setToolTip("Select a track to edit its clips and effects");
    layout->addWidget(trackList, 1);

    QWidget* toolbar = new QWidget(this);
    QGridLayout* toolbarLayout = new QGridLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);

    QPushButton* newTrackButton = new QPushButton("+ Track", toolbar);
    QPushButton* upTrackButton = new QPushButton("Move Up", toolbar);
    QPushButton* downTrackButton = new QPushButton("Move Down", toolbar);
    QPushButton* deleteTrackButton = new QPushButton("Del Track", toolbar);
    QPushButton* cutClipButton = new QPushButton("Cut Clip", toolbar);
    QPushButton* deleteClipButton = new QPushButton("Del Clip", toolbar);

    newTrackButton->setToolTip("Add new track");
    upTrackButton->setToolTip("Move selected track up");
    downTrackButton->setToolTip("Move selected track down");
    deleteTrackButton->setToolTip("Delete selected track");
    cutClipButton->setToolTip("Cut clip at playhead (C)");
    deleteClipButton->setToolTip("Delete selected clip (Del)");

    toolbarLayout->addWidget(newTrackButton, 0, 0);
    toolbarLayout->addWidget(upTrackButton, 0, 1);
    toolbarLayout->addWidget(downTrackButton, 0, 2);
    toolbarLayout->addWidget(deleteTrackButton, 0, 3);
    toolbarLayout->addWidget(cutClipButton, 1, 0, 1, 2);
    toolbarLayout->addWidget(deleteClipButton, 1, 2, 1, 2);

    layout->addWidget(toolbar);

    connect(trackList, &QListWidget::currentRowChanged, this, &TrackControl::trackSelected);
    connect(newTrackButton, &QPushButton::clicked, this, &TrackControl::newTrackRequested);
    connect(upTrackButton, &QPushButton::clicked, this, &TrackControl::moveUpRequested);
    connect(downTrackButton, &QPushButton::clicked, this, &TrackControl::moveDownRequested);
    connect(deleteTrackButton, &QPushButton::clicked, this, &TrackControl::deleteTrackRequested);
    connect(cutClipButton, &QPushButton::clicked, this, &TrackControl::cutClipRequested);
    connect(deleteClipButton, &QPushButton::clicked, this, &TrackControl::deleteClipRequested);
}

void TrackControl::populateTracks(int count) {
    bool oldState = trackList->signalsBlocked();
    trackList->blockSignals(true);
    trackList->clear();
    for (int i = 0; i < count; ++i) {
        trackList->addItem("Track " + QString::number(i + 1));
    }
    trackList->blockSignals(oldState);
}

void TrackControl::selectTrack(int index) {
    if (index >= 0 && index < trackList->count()) {
        bool oldState = trackList->signalsBlocked();
        trackList->blockSignals(true);
        trackList->setCurrentRow(index);
        trackList->blockSignals(oldState);
    }
}

int TrackControl::getSelectedTrack() const {
    return trackList->currentRow();
}
