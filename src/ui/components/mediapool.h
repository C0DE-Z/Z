#ifndef MEDIAPOOL_H
#define MEDIAPOOL_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

class MediaPool : public QWidget {
    Q_OBJECT
public:
    explicit MediaPool(QWidget* parent = nullptr);

    void addMedia(const QString& name);
    void clearMedia();
    void clearSelection();

signals:
    void mediaSelected(const QString& name);
    void fileDropped(const QString& filePath);
    void importRequested();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QListWidget* mediaList = nullptr;
    QLabel* emptyLabel = nullptr;

    void updateEmptyState();
};

#endif
