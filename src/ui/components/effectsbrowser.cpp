#include "effectsbrowser.h"
#include "engine/pluginmanager.h"

EffectsBrowser::EffectsBrowser(QWidget* parent) : QWidget(parent) {
    this->setObjectName("effectsContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel* title = new QLabel("F U C K   I T   U P", this);
    title->setStyleSheet("font-weight: bold; color: white;");
    layout->addWidget(title);

    searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText("Search effects...");
    searchBar->setStyleSheet("QLineEdit { background: #1a1a1a; color: white; border: 1px solid #333; padding: 4px; border-radius: 4px; }");
    layout->addWidget(searchBar);

    effectsTree = new QTreeWidget(this);
    effectsTree->setHeaderHidden(true);
    effectsTree->setDragEnabled(true);
    layout->addWidget(effectsTree, 1);

    connect(searchBar, &QLineEdit::textChanged, this, &EffectsBrowser::onSearchTextChanged);
    connect(effectsTree, &QTreeWidget::itemDoubleClicked, this, &EffectsBrowser::onItemDoubleClicked);

    populateEffects();
}

#include <functional>
#include <QDir>

void EffectsBrowser::populateEffects() {
    effectsTree->clear();
    QTreeWidgetItem* transitionsFolder = new QTreeWidgetItem(effectsTree);
    transitionsFolder->setText(0, "Transitions");
    QTreeWidgetItem* effectsFolder = new QTreeWidgetItem(effectsTree);
    effectsFolder->setText(0, "Plugins");
    QTreeWidgetItem* builtinFolder = new QTreeWidgetItem(effectsTree);
    builtinFolder->setText(0, "Builtin");

    auto addItem = [](QTreeWidgetItem* parent, const QString& label, const QString& id) {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, label);
        item->setData(0, Qt::UserRole, id);
    };

    auto getOrCreateFolder = [](QTreeWidgetItem* root, const QString& categoryPath) -> QTreeWidgetItem* {
        if (categoryPath.isEmpty()) return root;
        QString normalized = QString(categoryPath).replace('\\', '/');
        QStringList parts = normalized.split('/', Qt::SkipEmptyParts);
        QTreeWidgetItem* current = root;
        for (const QString& part : parts) {
            QTreeWidgetItem* found = nullptr;
            for (int i = 0; i < current->childCount(); ++i) {
                if (current->child(i)->text(0) == part) {
                    found = current->child(i);
                    break;
                }
            }
            if (!found) {
                found = new QTreeWidgetItem(current);
                found->setText(0, part);
            }
            current = found;
        }
        return current;
    };

    addItem(transitionsFolder, "Cross Dissolve", "cross_dissolve");
    addItem(transitionsFolder, "Datamosh Transition", "datamosh_transition");
    
    addItem(builtinFolder, "Datamoshing", "datamosh");
    addItem(builtinFolder, "Optical Smear", "optical_smear");
    addItem(builtinFolder, "Legacy CPU XOR", "cpu_xor");
    addItem(builtinFolder, "Legacy CPU OR", "cpu_or");
    addItem(builtinFolder, "Legacy CPU AND", "cpu_and");
    addItem(builtinFolder, "Legacy CPU XNOR", "cpu_xnor");
    addItem(builtinFolder, "Legacy CPU NAND", "cpu_nand");

    const auto& plugins = PluginManager::instance().getPlugins();
    for (const auto& plugin : plugins) {
        QString cat = QString::fromStdString(plugin.category);
        QTreeWidgetItem* parentFolder = getOrCreateFolder(effectsFolder, cat);
        addItem(parentFolder, QString::fromStdString(plugin.name), QString::fromStdString(plugin.id));
    }

    effectsTree->expandAll();
}

void EffectsBrowser::onSearchTextChanged(const QString& text) {
    QString query = text.toLower();

    std::function<bool(QTreeWidgetItem*)> filterItem = [&](QTreeWidgetItem* item) -> bool {
        if (item->childCount() == 0) {
            bool matches = item->text(0).toLower().contains(query) || query.isEmpty();
            item->setHidden(!matches);
            return matches;
        } else {
            bool anyChildVisible = false;
            for (int i = 0; i < item->childCount(); ++i) {
                if (filterItem(item->child(i))) {
                    anyChildVisible = true;
                }
            }
            item->setHidden(!anyChildVisible && !query.isEmpty());
            return anyChildVisible;
        }
    };

    for (int i = 0; i < effectsTree->topLevelItemCount(); ++i) {
        filterItem(effectsTree->topLevelItem(i));
    }
}

void EffectsBrowser::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    if (!item || item->childCount() > 0) return; // Do not apply folders
    QString pluginId = item->data(0, Qt::UserRole).toString();
    emit effectDoubleClicked(pluginId);
}
