#include "trackcontrol.h"
#include <QGridLayout>

TrackControl::TrackControl(QWidget* parent) : QWidget(parent) {
    this->setObjectName("tracksContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel* title = new QLabel("TRACKS", this);
    title->setStyleSheet("font-weight: bold; color: #FF72AA; letter-spacing: 0.8px; font-size: 10px;");
    layout->addWidget(title);

    trackList = new QListWidget(this);
    trackList->setToolTip("Select a track to edit its clips and effects");
    layout->addWidget(trackList, 1);

    QWidget* toolbar = new QWidget(this);
    QGridLayout* toolbarLayout = new QGridLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);

    QPushButton* newVideoTrackButton = new QPushButton("+ Video", toolbar);
    QPushButton* newAudioTrackButton = new QPushButton("+ Audio", toolbar);
    QPushButton* upTrackButton = new QPushButton("Move Up", toolbar);
    QPushButton* downTrackButton = new QPushButton("Move Down", toolbar);
    QPushButton* deleteTrackButton = new QPushButton("Del Track", toolbar);
    QPushButton* cutClipButton = new QPushButton("Cut Clip", toolbar);
    QPushButton* deleteClipButton = new QPushButton("Del Clip", toolbar);

    newVideoTrackButton->setToolTip("Add a video track");
    newAudioTrackButton->setToolTip("Add an audio track");
    upTrackButton->setToolTip("Move selected track up");
    downTrackButton->setToolTip("Move selected track down");
    deleteTrackButton->setToolTip("Delete selected track");
    cutClipButton->setToolTip("Cut clip at playhead (C)");
    deleteClipButton->setToolTip("Delete selected clip (Del)");

    toolbarLayout->addWidget(newVideoTrackButton, 0, 0);
    toolbarLayout->addWidget(newAudioTrackButton, 0, 1);
    toolbarLayout->addWidget(upTrackButton, 0, 2);
    toolbarLayout->addWidget(downTrackButton, 0, 3);
    toolbarLayout->addWidget(deleteTrackButton, 0, 4);
    toolbarLayout->addWidget(cutClipButton, 1, 0, 1, 2);
    toolbarLayout->addWidget(deleteClipButton, 1, 2, 1, 2);

    layout->addWidget(toolbar);

    connect(trackList, &QListWidget::currentRowChanged, this, &TrackControl::trackSelected);
    connect(newVideoTrackButton, &QPushButton::clicked, this, &TrackControl::newVideoTrackRequested);
    connect(newAudioTrackButton, &QPushButton::clicked, this, &TrackControl::newAudioTrackRequested);
    connect(upTrackButton, &QPushButton::clicked, this, &TrackControl::moveUpRequested);
    connect(downTrackButton, &QPushButton::clicked, this, &TrackControl::moveDownRequested);
    connect(deleteTrackButton, &QPushButton::clicked, this, &TrackControl::deleteTrackRequested);
    connect(cutClipButton, &QPushButton::clicked, this, &TrackControl::cutClipRequested);
    connect(deleteClipButton, &QPushButton::clicked, this, &TrackControl::deleteClipRequested);
}

void TrackControl::populateTracks(const std::vector<TimelineTrack>& tracks) {
    bool oldState = trackList->signalsBlocked();
    trackList->blockSignals(true);
    trackList->clear();
    int videoNumber = 0;
    int audioNumber = 0;
    for (const auto& track : tracks) {
        const bool isAudio = track.type == TimelineTrackType::Audio;
        const int number = isAudio ? ++audioNumber : ++videoNumber;
        const QString prefix = isAudio ? "A" : "V";
        const QString fallback = isAudio ? "Audio" : "Video";
        const QString name = track.name.empty() ? fallback : QString::fromStdString(track.name);
        trackList->addItem(QString("%1%2  %3").arg(prefix).arg(number).arg(name));
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
