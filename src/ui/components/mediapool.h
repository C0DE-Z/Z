#ifndef MEDIAPOOL_H
#define MEDIAPOOL_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>

class MediaPool : public QWidget {
    Q_OBJECT
public:
    explicit MediaPool(QWidget* parent = nullptr);

    void addMedia(const QString& name);
    void clearMedia();
    void clearSelection();

signals:
    void mediaSelected(const QString& name);

private:
    QListWidget* mediaList;
};

#endif
