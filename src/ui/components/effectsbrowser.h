#ifndef EFFECTSBROWSER_H
#define EFFECTSBROWSER_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>

class EffectsBrowser : public QWidget {
    Q_OBJECT
public:
    explicit EffectsBrowser(QWidget* parent = nullptr);
    void populateEffects();
    QTreeWidget* getTreeWidget() const { return effectsTree; }

signals:
    void effectDoubleClicked(const QString& pluginId);

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    QTreeWidget* effectsTree;
    QLineEdit* searchBar;
};

#endif
