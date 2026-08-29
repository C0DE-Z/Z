#include "effectsbrowser.h"
#include "engine/pluginmanager.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QAction>
#include <functional>
#include <QDir>
#include <algorithm>

EffectsBrowser::EffectsBrowser(QWidget* parent) : QWidget(parent) {
    this->setObjectName("effectsContainer");
    this->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("EFFECTS LIBRARY", this);
    title->setStyleSheet("font-weight: bold; color: #c4b5fd; letter-spacing: 0.8px; font-size: 10px;");
    headerLayout->addWidget(title, 1);

    layout->addLayout(headerLayout);

    searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText("Search effects & transitions...");
    searchBar->setClearButtonEnabled(true);
    searchBar->setStyleSheet("QLineEdit { background: #0c0b10; color: #ececf4; border: 1px solid #281d33; padding: 4px 8px; border-radius: 4px; } QLineEdit:focus { border-color: #d946ef; background: #130f1a; }");
    layout->addWidget(searchBar);

    effectsTree = new QTreeWidget(this);
    effectsTree->setHeaderHidden(true);
    effectsTree->setDragEnabled(true);
    effectsTree->setIndentation(16);
    effectsTree->setAnimated(true);
    effectsTree->setToolTip("Drag effect onto clip/timeline, or double-click to apply");
    layout->addWidget(effectsTree, 1);

    connect(searchBar, &QLineEdit::textChanged, this, &EffectsBrowser::onSearchTextChanged);
    connect(effectsTree, &QTreeWidget::itemDoubleClicked, this, &EffectsBrowser::onItemDoubleClicked);

    populateEffects();
}

void EffectsBrowser::populateEffects() {
    effectsTree->clear();

    QTreeWidgetItem* builtinFolder = new QTreeWidgetItem(effectsTree);
    builtinFolder->setText(0, "Core & Glitch Engines");
    builtinFolder->setIcon(0, QIcon());

    QTreeWidgetItem* effectsFolder = new QTreeWidgetItem(effectsTree);
    effectsFolder->setText(0, "Shader Plugins");

    QTreeWidgetItem* transitionsFolder = new QTreeWidgetItem(effectsTree);
    transitionsFolder->setText(0, "Transitions");

    auto addItem = [](QTreeWidgetItem* parent, const QString& label, const QString& id, const QString& tooltip = "") {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, label);
        item->setData(0, Qt::UserRole, id);
        if (!tooltip.isEmpty()) {
            item->setToolTip(0, tooltip);
        } else {
            item->setToolTip(0, QString("Double-click or drag onto timeline to apply %1").arg(label));
        }
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

    addItem(builtinFolder, "Datamoshing", "datamosh", "P-frame motion vector glitch and temporal frame corruption");
    addItem(builtinFolder, "Optical Smear", "optical_smear", "Temporal chromatic smearing based on optical velocity vectors");
    addItem(builtinFolder, "Legacy CPU XOR", "cpu_xor", "Bitwise XOR video blender with vintage binary artifacts");
    addItem(builtinFolder, "Legacy CPU OR", "cpu_or", "Bitwise OR video channel combiner");
    addItem(builtinFolder, "Legacy CPU AND", "cpu_and", "Bitwise AND channel masking effect");
    addItem(builtinFolder, "Legacy CPU XNOR", "cpu_xnor", "Bitwise inverted XOR texture processor");
    addItem(builtinFolder, "Legacy CPU NAND", "cpu_nand", "Bitwise inverted AND synthesizer");

    const auto& plugins = PluginManager::instance().getPlugins();
    std::vector<const ShaderPlugin*> transitionPlugins;
    for (const auto& plugin : plugins) {
        QString cat = QString::fromStdString(plugin.category);
        if (cat.startsWith("Transitions", Qt::CaseInsensitive)) {
            transitionPlugins.push_back(&plugin);
            continue;
        }
        QTreeWidgetItem* parentFolder = getOrCreateFolder(effectsFolder, cat);
        addItem(parentFolder, QString::fromStdString(plugin.name), QString::fromStdString(plugin.id));
    }

    std::sort(transitionPlugins.begin(), transitionPlugins.end(), [](const ShaderPlugin* a, const ShaderPlugin* b) {
        return QString::fromStdString(a->name).toLower() < QString::fromStdString(b->name).toLower();
    });

    for (const auto* plugin : transitionPlugins) {
        addItem(transitionsFolder, QString::fromStdString(plugin->name), QString::fromStdString(plugin->id));
    }

    effectsTree->expandItem(builtinFolder);
    effectsTree->expandItem(effectsFolder);
    effectsTree->expandItem(transitionsFolder);
}

void EffectsBrowser::onSearchTextChanged(const QString& text) {
    QString query = text.trimmed().toLower();

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
            if (anyChildVisible && !query.isEmpty()) {
                item->setExpanded(true);
            }
            return anyChildVisible;
        }
    };

    for (int i = 0; i < effectsTree->topLevelItemCount(); ++i) {
        filterItem(effectsTree->topLevelItem(i));
    }
}

void EffectsBrowser::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item || item->childCount() > 0) return;
    QString pluginId = item->data(0, Qt::UserRole).toString();
    emit effectDoubleClicked(pluginId);
}
