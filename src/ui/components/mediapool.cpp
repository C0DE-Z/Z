#include "mediapool.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QUrl>
#include <QFileInfo>

MediaPool::MediaPool(QWidget* parent) : QWidget(parent) {
    this->setObjectName("mediaContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    setAcceptDrops(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("MEDIA POOL", this);
    title->setStyleSheet("font-weight: bold; color: #FF72AA; letter-spacing: 0.8px; font-size: 10px;");
    headerLayout->addWidget(title, 1);

    QPushButton* importBtn = new QPushButton("+ Import", this);
    importBtn->setToolTip("Import video clip (Ctrl+I)");
    importBtn->setStyleSheet("QPushButton { background: #32101F; border: 1px solid #80324F; color: #FFB8D2; padding: 2px 8px; font-size: 10px; font-weight: bold; border-radius: 3px; } QPushButton:hover { background: #5C1E38; border-color: #FF4F91; color: white; }");
    connect(importBtn, &QPushButton::clicked, this, &MediaPool::importRequested);
    headerLayout->addWidget(importBtn);

    layout->addLayout(headerLayout);

    mediaList = new QListWidget(this);
    mediaList->setToolTip("Imported clips in current project\nDrag and drop video files here from Explorer");
    layout->addWidget(mediaList, 1);

    emptyLabel = new QLabel("No media loaded.\nDrop video files here or click + Import", this);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #918B92; font-size: 10px; padding: 20px;");
    layout->addWidget(emptyLabel);

    connect(mediaList, &QListWidget::currentTextChanged, this, &MediaPool::mediaSelected);

    updateEmptyState();
}

void MediaPool::updateEmptyState() {
    bool hasItems = (mediaList && mediaList->count() > 0);
    if (emptyLabel) emptyLabel->setVisible(!hasItems);
    if (mediaList) mediaList->setVisible(hasItems);
}

void MediaPool::addMedia(const QString& name) {
    if (name.isEmpty()) return;
    for (int i = 0; i < mediaList->count(); ++i) {
        if (mediaList->item(i)->text() == name) {
            bool oldState = mediaList->signalsBlocked();
            mediaList->blockSignals(true);
            mediaList->setCurrentRow(i);
            mediaList->blockSignals(oldState);
            updateEmptyState();
            return;
        }
    }
    mediaList->addItem(name);
    bool oldState = mediaList->signalsBlocked();
    mediaList->blockSignals(true);
    mediaList->setCurrentRow(mediaList->count() - 1);
    mediaList->blockSignals(oldState);
    updateEmptyState();
}

void MediaPool::clearMedia() {
    mediaList->clear();
    updateEmptyState();
}

void MediaPool::clearSelection() {
    mediaList->clearSelection();
}

void MediaPool::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MediaPool::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MediaPool::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                QString ext = QFileInfo(path).suffix().toLower();
                if (ext == "mp4" || ext == "m4v" || ext == "mov" || ext == "avi" || ext == "mkv" || ext == "webm" || ext == "flv" || ext == "ts") {
                    emit fileDropped(path);
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
}
