#include "mediapool.h"

MediaPool::MediaPool(QWidget* parent) : QWidget(parent) {
    this->setObjectName("mediaContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel* title = new QLabel("M E D I A", this);
    title->setStyleSheet("font-weight: bold; color: white;");
    layout->addWidget(title);

    mediaList = new QListWidget(this);
    layout->addWidget(mediaList, 1);

    connect(mediaList, &QListWidget::currentTextChanged, this, &MediaPool::mediaSelected);
}

void MediaPool::addMedia(const QString& name) {
    mediaList->addItem(name);
    mediaList->setCurrentRow(mediaList->count() - 1);
}

void MediaPool::clearMedia() {
    mediaList->clear();
}

void MediaPool::clearSelection() {
    mediaList->clearSelection();
}
