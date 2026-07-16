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

void EffectsBrowser::populateEffects() {
    effectsTree->clear();
    QTreeWidgetItem* transitionsFolder = new QTreeWidgetItem(effectsTree);
    transitionsFolder->setText(0, "Transitions");
    QTreeWidgetItem* effectsFolder = new QTreeWidgetItem(effectsTree);
    effectsFolder->setText(0, "Effects");

    auto addItem = [](QTreeWidgetItem* parent, const QString& label, const QString& id) {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, label);
        item->setData(0, Qt::UserRole, id);
    };

    addItem(transitionsFolder, "Cross Dissolve", "cross_dissolve");
    addItem(transitionsFolder, "Datamosh Transition", "datamosh_transition");
    
    addItem(effectsFolder, "Datamoshing", "datamosh");
    addItem(effectsFolder, "Optical Smear", "optical_smear");
    addItem(effectsFolder, "Legacy CPU XOR", "cpu_xor");
    addItem(effectsFolder, "Legacy CPU OR", "cpu_or");
    addItem(effectsFolder, "Legacy CPU AND", "cpu_and");
    addItem(effectsFolder, "Legacy CPU XNOR", "cpu_xnor");
    addItem(effectsFolder, "Legacy CPU NAND", "cpu_nand");

    const auto& plugins = PluginManager::instance().getPlugins();
    for (const auto& plugin : plugins) {
        addItem(effectsFolder, QString::fromStdString(plugin.name), QString::fromStdString(plugin.id));
    }

    effectsTree->expandAll();
}

void EffectsBrowser::onSearchTextChanged(const QString& text) {
    QString query = text.toLower();

    for (int i = 0; i < effectsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* folder = effectsTree->topLevelItem(i);
        bool folderHasVisibleChildren = false;

        for (int j = 0; j < folder->childCount(); ++j) {
            QTreeWidgetItem* child = folder->child(j);
            QString itemName = child->text(0).toLower();

            if (itemName.contains(query) || query.isEmpty()) {
                child->setHidden(false);
                folderHasVisibleChildren = true;
            } else {
                child->setHidden(true);
            }
        }

        folder->setHidden(!folderHasVisibleChildren && !query.isEmpty());
    }
}

void EffectsBrowser::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    if (!item || item->childCount() > 0) return; // Do not apply folders
    QString pluginId = item->data(0, Qt::UserRole).toString();
    emit effectDoubleClicked(pluginId);
}
