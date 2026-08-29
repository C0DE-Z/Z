#ifndef Z_CORE_SHORTCUTMANAGER_H
#define Z_CORE_SHORTCUTMANAGER_H

#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QMap>
#include <vector>

struct ShortcutDefinition {
    QString id;
    QString category;
    QString name;
    QString defaultKey;
    QString currentKey;
};

class ShortcutManager : public QObject {
    Q_OBJECT

public:
    static ShortcutManager& instance() {
        static ShortcutManager inst;
        return inst;
    }

    void initialize();
    void loadFromSettings();
    void saveToSettings();
    void resetToDefaults();

    QKeySequence getShortcut(const QString& id) const;
    void setShortcut(const QString& id, const QKeySequence& sequence);

    const std::vector<ShortcutDefinition>& getDefinitions() const { return definitions; }

signals:
    void shortcutsChanged();

private:
    ShortcutManager();
    std::vector<ShortcutDefinition> definitions;
    QMap<QString, size_t> idToIndex;
};

#endif // Z_CORE_SHORTCUTMANAGER_H
