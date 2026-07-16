#include "trackcontrol.h"

TrackControl::TrackControl(QWidget* parent) : QWidget(parent) {
    this->setObjectName("tracksContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel* title = new QLabel("T R A C K S", this);
    title->setStyleSheet("font-weight: bold; color: white;");
    layout->addWidget(title);

    trackList = new QListWidget(this);
    trackList->setToolTip("Select a track to edit its clips and effects");
    layout->addWidget(trackList, 1);

    QWidget* toolbar = new QWidget(this);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    QPushButton* newTrackButton = new QPushButton("New Track", toolbar);
    QPushButton* upTrackButton = new QPushButton("Move Up", toolbar);
    QPushButton* downTrackButton = new QPushButton("Move Down", toolbar);
    QPushButton* deleteTrackButton = new QPushButton("Delete Track", toolbar);
    QPushButton* cutClipButton = new QPushButton("Cut Clip", toolbar);
    QPushButton* deleteClipButton = new QPushButton("Delete Clip", toolbar);

    toolbarLayout->addWidget(newTrackButton);
    toolbarLayout->addWidget(upTrackButton);
    toolbarLayout->addWidget(downTrackButton);
    toolbarLayout->addWidget(deleteTrackButton);
    toolbarLayout->addWidget(cutClipButton);
    toolbarLayout->addWidget(deleteClipButton);
    toolbarLayout->addStretch();

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
