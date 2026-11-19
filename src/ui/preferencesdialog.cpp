#include "preferencesdialog.h"
#include "core/shortcutmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QColorDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QDir>

namespace {
struct ColorField {
    const char* key;
    const char* label;
    QColor Theme::*member;
};
const ColorField kColorFields[] = {
    {"canvas", "Background", &Theme::canvas},
    {"surface", "Panels", &Theme::surface},
    {"raised", "Controls", &Theme::raised},
    {"line", "Dividers", &Theme::line},
    {"lineStrong", "Control borders", &Theme::lineStrong},
    {"ink", "Text", &Theme::ink},
    {"inkMuted", "Secondary text", &Theme::inkMuted},
    {"inkSubtle", "Hints", &Theme::inkSubtle},
    {"accent", "Accent", &Theme::accent},
};
}

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Preferences");
    resize(640, 520);
    setModal(true);
    originalThemeName = ThemeManager::instance().current().name;
    editingTheme = ThemeManager::instance().current();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    tabWidget = new QTabWidget(this);

    QWidget* generalTab = new QWidget(tabWidget);
    QFormLayout* generalLayout = new QFormLayout(generalTab);
    generalLayout->setContentsMargins(12, 12, 12, 12);
    generalLayout->setSpacing(10);

    QLabel* projectSafetyHint = new QLabel(
        "Z asks before closing or replacing a project with unsaved changes. Use Save or Save As to keep a project on disk.",
        generalTab);
    projectSafetyHint->setWordWrap(true);
    generalLayout->addRow(projectSafetyHint);
    tabWidget->addTab(generalTab, "General");
    buildAppearanceTab();

    QWidget* playbackTab = new QWidget(tabWidget);
    QFormLayout* playbackLayout = new QFormLayout(playbackTab);
    playbackLayout->setContentsMargins(12, 12, 12, 12);
    playbackLayout->setSpacing(10);

    playbackQualityCombo = new QComboBox(playbackTab);
    playbackQualityCombo->addItem("100% (Full Quality)", 1);
    playbackQualityCombo->addItem("50% (Half Quality - Fast)", 2);
    playbackQualityCombo->addItem("25% (Quarter Quality - Fastest)", 4);
    playbackLayout->addRow("Default Playback Quality:", playbackQualityCombo);

    fpsOverlayCheck = new QCheckBox("Show HUD overlay by default", playbackTab);
    playbackLayout->addRow("", fpsOverlayCheck);

    tabWidget->addTab(playbackTab, "Playback & Audio");

    QWidget* timelineTab = new QWidget(tabWidget);
    QFormLayout* timelineLayout = new QFormLayout(timelineTab);
    timelineLayout->setContentsMargins(12, 12, 12, 12);
    timelineLayout->setSpacing(10);

    snapDistanceSpin = new QSpinBox(timelineTab);
    snapDistanceSpin->setRange(5, 50);
    snapDistanceSpin->setSuffix(" px");
    timelineLayout->addRow("Snapping Magnetism:", snapDistanceSpin);

    timecodeFormatCombo = new QComboBox(timelineTab);
    timecodeFormatCombo->addItem("Standard Timecode (MM:SS:FF)");
    timecodeFormatCombo->addItem("Milliseconds (MM:SS.mmm)");
    timecodeFormatCombo->addItem("Total Seconds");
    timelineLayout->addRow("Timecode Display Format:", timecodeFormatCombo);

    tabWidget->addTab(timelineTab, "Timeline & Snapping");

    QWidget* exportTab = new QWidget(tabWidget);
    QFormLayout* exportLayout = new QFormLayout(exportTab);
    exportLayout->setContentsMargins(12, 12, 12, 12);
    exportLayout->setSpacing(10);

    exportResCombo = new QComboBox(exportTab);
    exportResCombo->addItem("Match Project Source");
    exportResCombo->addItem("1080p Full HD (1920x1080)");
    exportResCombo->addItem("4K UHD (3840x2160)");
    exportResCombo->addItem("720p HD (1280x720)");
    exportLayout->addRow("Default Export Resolution:", exportResCombo);

    exportQualityCombo = new QComboBox(exportTab);
    exportQualityCombo->addItem("High Quality (CRF 18 - Archival)", 18);
    exportQualityCombo->addItem("Balanced (CRF 22 - Recommended)", 22);
    exportQualityCombo->addItem("Fast Web (CRF 26 - Smallest)", 26);
    exportLayout->addRow("Default Video Quality:", exportQualityCombo);

    tabWidget->addTab(exportTab, "Export");

    QWidget* shortcutsTab = new QWidget(tabWidget);
    QVBoxLayout* shortcutsLayout = new QVBoxLayout(shortcutsTab);
    shortcutsLayout->setContentsMargins(8, 8, 8, 8);
    shortcutsLayout->setSpacing(6);

    QLabel* shortcutsHint = new QLabel("Click on any key sequence to record a custom shortcut binding.", shortcutsTab);
    shortcutsHint->setStyleSheet("color: #9090a0; font-size: 11px;");
    shortcutsLayout->addWidget(shortcutsHint);

    shortcutsTable = new QTableWidget(shortcutsTab);
    shortcutsTable->setColumnCount(3);
    shortcutsTable->setHorizontalHeaderLabels({"Action", "Category", "Shortcut"});
    shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    shortcutsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    shortcutsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    shortcutsTable->verticalHeader()->setVisible(false);
    shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    shortcutsTable->setStyleSheet("QTableWidget { background: #08080A; border: 1px solid #303036; gridline-color: #303036; } QTableWidget::item { padding: 4px; } QTableWidget::item:selected { background: #3D1226; color: #FFB8D2; } QHeaderView::section { background: #19191F; color: #FF72AA; border: 1px solid #303036; font-weight: bold; padding: 4px; }");
    shortcutsLayout->addWidget(shortcutsTable, 1);

    QPushButton* resetShortcutsBtn = new QPushButton("Reset All Shortcuts to Defaults", shortcutsTab);
    connect(resetShortcutsBtn, &QPushButton::clicked, this, [this]() {
        ShortcutManager::instance().resetToDefaults();
        populateShortcutsTable();
    });
    shortcutsLayout->addWidget(resetShortcutsBtn, 0, Qt::AlignRight);

    tabWidget->addTab(shortcutsTab, "Shortcuts");

    mainLayout->addWidget(tabWidget);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton("Restore All Defaults", this);
    connect(resetBtn, &QPushButton::clicked, this, &PreferencesDialog::resetToDefaults);
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &PreferencesDialog::savePreferences);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    btnLayout->addWidget(bbox);

    mainLayout->addLayout(btnLayout);

    loadPreferences();
    populateShortcutsTable();
}

void PreferencesDialog::populateShortcutsTable() {
    if (!shortcutsTable) return;
    const auto& defs = ShortcutManager::instance().getDefinitions();
    shortcutsTable->setRowCount(static_cast<int>(defs.size()));

    for (int row = 0; row < static_cast<int>(defs.size()); ++row) {
        const auto& def = defs[row];

        QTableWidgetItem* actionItem = new QTableWidgetItem(def.name);
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        shortcutsTable->setItem(row, 0, actionItem);

        QTableWidgetItem* catItem = new QTableWidgetItem(def.category);
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsEditable);
        shortcutsTable->setItem(row, 1, catItem);

        QKeySequenceEdit* keyEdit = new QKeySequenceEdit(QKeySequence(def.currentKey), shortcutsTable);
        keyEdit->setStyleSheet("QKeySequenceEdit { background: #19191F; color: #FFB8D2; border: 1px solid #4E4E58; border-radius: 3px; padding: 2px 6px; font-weight: bold; } QKeySequenceEdit:focus { border-color: #FF4F91; }");
        shortcutsTable->setCellWidget(row, 2, keyEdit);
    }
}

void PreferencesDialog::saveShortcuts() {
    if (!shortcutsTable) return;
    const auto& defs = ShortcutManager::instance().getDefinitions();
    for (int row = 0; row < static_cast<int>(defs.size()) && row < shortcutsTable->rowCount(); ++row) {
        QKeySequenceEdit* keyEdit = qobject_cast<QKeySequenceEdit*>(shortcutsTable->cellWidget(row, 2));
        if (keyEdit) {
            ShortcutManager::instance().setShortcut(defs[row].id, keyEdit->keySequence());
        }
    }
    ShortcutManager::instance().saveToSettings();
}

void PreferencesDialog::loadPreferences() {
    QSettings settings("Z-Creative", "Z");
    int qual = settings.value("Playback/QualityScale", 1).toInt();
    int qualIdx = playbackQualityCombo->findData(qual);
    if (qualIdx >= 0) playbackQualityCombo->setCurrentIndex(qualIdx);

    fpsOverlayCheck->setChecked(settings.value("Playback/ShowFpsOverlay", true).toBool());

    snapDistanceSpin->setValue(settings.value("Timeline/SnapDistancePx", 15).toInt());
    timecodeFormatCombo->setCurrentIndex(settings.value("Timeline/TimecodeFormat", 0).toInt());

    exportResCombo->setCurrentText(settings.value("Export/Resolution", "Match Project Source").toString());
    int crf = settings.value("Export/Crf", 22).toInt();
    int crfIdx = exportQualityCombo->findData(crf);
    if (crfIdx >= 0) exportQualityCombo->setCurrentIndex(crfIdx);
}

void PreferencesDialog::savePreferences() {
    QSettings settings("Z-Creative", "Z");
    settings.remove("General/AutoSaveMinutes");
    settings.remove("General/PromptOnExit");

    settings.setValue("Playback/QualityScale", playbackQualityCombo->currentData().toInt());
    settings.setValue("Playback/ShowFpsOverlay", fpsOverlayCheck->isChecked());
    settings.remove("Playback/AudioBuffer");

    settings.setValue("Timeline/SnapDistancePx", snapDistanceSpin->value());
    settings.setValue("Timeline/TimecodeFormat", timecodeFormatCombo->currentIndex());

    settings.setValue("Export/Resolution", exportResCombo->currentText());
    settings.setValue("Export/Crf", exportQualityCombo->currentData().toInt());

    saveShortcuts();

    if (!editingTheme.builtIn) {
        QString error;
        if (!ThemeManager::instance().saveCustomTheme(editingTheme, &error)) {
            QMessageBox::warning(this, "Theme", error);
            return;
        }
    }
    ThemeManager::instance().setCurrent(editingTheme.name, true);
    accept();
}

void PreferencesDialog::reject() {
    // Colour edits are previewed live, so cancelling must put the saved theme back.
    if (!ThemeManager::instance().setCurrent(originalThemeName, false)) {
        ThemeManager::instance().setCurrent("Z Pink", false);
    }
    QDialog::reject();
}

void PreferencesDialog::showTab(const QString& title) {
    for (int i = 0; i < tabWidget->count(); ++i) {
        if (tabWidget->tabText(i) == title) {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }
}

void PreferencesDialog::buildAppearanceTab() {
    QWidget* tab = new QWidget(tabWidget);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QHBoxLayout* themeRow = new QHBoxLayout();
    themeRow->addWidget(new QLabel("Theme:", tab));
    themeCombo = new QComboBox(tab);
    themeRow->addWidget(themeCombo, 1);
    layout->addLayout(themeRow);

    QHBoxLayout* actionRow = new QHBoxLayout();
    duplicateThemeBtn = new QPushButton("Duplicate", tab);
    deleteThemeBtn = new QPushButton("Delete", tab);
    QPushButton* importThemeBtn = new QPushButton("Import...", tab);
    exportThemeBtn = new QPushButton("Export...", tab);
    for (QPushButton* button : {duplicateThemeBtn, deleteThemeBtn, importThemeBtn, exportThemeBtn}) actionRow->addWidget(button);
    actionRow->addStretch();
    layout->addLayout(actionRow);

    QLabel* hint = new QLabel("Built-in themes are read-only. Duplicate one to make your own, or import a theme file.", tab);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #918B92; font-size: 10px;");
    layout->addWidget(hint);

    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    int index = 0;
    for (const ColorField& field : kColorFields) {
        const QString key = field.key;
        QPushButton* swatch = new QPushButton(tab);
        swatch->setMinimumWidth(110);
        connect(swatch, &QPushButton::clicked, this, [this, key] { pickColor(key); });
        colorSwatches.insert(key, swatch);
        const int row = index / 2;
        const int column = (index % 2) * 2;
        grid->addWidget(new QLabel(field.label, tab), row, column);
        grid->addWidget(swatch, row, column + 1);
        ++index;
    }
    layout->addLayout(grid);

    QFormLayout* metrics = new QFormLayout();
    radiusSpin = new QSpinBox(tab);
    radiusSpin->setRange(0, 12);
    radiusSpin->setSuffix(" px");
    metrics->addRow("Corner radius:", radiusSpin);
    fontSizeSpin = new QSpinBox(tab);
    fontSizeSpin->setRange(9, 16);
    fontSizeSpin->setSuffix(" px");
    metrics->addRow("Interface text size:", fontSizeSpin);
    layout->addLayout(metrics);
    layout->addStretch();

    connect(themeCombo, &QComboBox::currentTextChanged, this, [this](const QString& name) {
        if (!syncingAppearance && !name.isEmpty()) selectTheme(name);
    });
    connect(radiusSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (syncingAppearance) return;
        editingTheme.cornerRadius = value;
        previewEditingTheme();
    });
    connect(fontSizeSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (syncingAppearance) return;
        editingTheme.fontSize = value;
        previewEditingTheme();
    });
    connect(duplicateThemeBtn, &QPushButton::clicked, this, [this] {
        Theme copy = editingTheme;
        copy.name = ThemeManager::instance().uniqueName(editingTheme.name + " Copy");
        copy.builtIn = false;
        copy.fixedPalette.reset();
        QString error;
        if (!ThemeManager::instance().saveCustomTheme(copy, &error)) {
            QMessageBox::warning(this, "Theme", error);
            return;
        }
        refreshThemeCombo(copy.name);
    });
    connect(deleteThemeBtn, &QPushButton::clicked, this, [this] {
        const QString name = editingTheme.name;
        if (QMessageBox::question(this, "Delete Theme", QString("Delete the theme \"%1\"?").arg(name)) != QMessageBox::Yes) return;
        if (originalThemeName == name) originalThemeName = "Z Pink";
        ThemeManager::instance().deleteCustomTheme(name);
        refreshThemeCombo(ThemeManager::instance().current().name);
    });
    connect(importThemeBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Import Theme", QDir::homePath(), "Z themes (*.json)");
        if (path.isEmpty()) return;
        Theme imported;
        QString error;
        if (!ThemeManager::instance().importTheme(path, &imported, &error)) {
            QMessageBox::warning(this, "Import Theme", error);
            return;
        }
        refreshThemeCombo(imported.name);
    });
    connect(exportThemeBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, "Export Theme", QDir::homePath() + "/" + editingTheme.name + ".json", "Z themes (*.json)");
        if (path.isEmpty()) return;
        QString error;
        if (!ThemeManager::instance().exportTheme(editingTheme, path, &error)) QMessageBox::warning(this, "Export Theme", error);
    });

    tabWidget->addTab(tab, "Appearance");
    refreshThemeCombo(editingTheme.name);
}

void PreferencesDialog::refreshThemeCombo(const QString& select) {
    syncingAppearance = true;
    themeCombo->clear();
    themeCombo->addItems(ThemeManager::instance().themeNames());
    themeCombo->setCurrentText(select);
    syncingAppearance = false;
    selectTheme(themeCombo->currentText());
}

void PreferencesDialog::selectTheme(const QString& name) {
    if (!ThemeManager::instance().setCurrent(name, false)) return;
    editingTheme = ThemeManager::instance().current();
    syncAppearanceControls();
}

void PreferencesDialog::syncAppearanceControls() {
    syncingAppearance = true;
    const bool editable = !editingTheme.builtIn;
    for (const ColorField& field : kColorFields) {
        QPushButton* swatch = colorSwatches.value(field.key);
        const QColor color = editingTheme.*(field.member);
        QPixmap pixmap(28, 14);
        pixmap.fill(color);
        swatch->setIcon(QIcon(pixmap));
        swatch->setText(color.name().toUpper());
        swatch->setEnabled(editable);
    }
    radiusSpin->setValue(editingTheme.cornerRadius);
    fontSizeSpin->setValue(editingTheme.fontSize);
    radiusSpin->setEnabled(editable);
    fontSizeSpin->setEnabled(editable);
    deleteThemeBtn->setEnabled(editable);
    syncingAppearance = false;
}

void PreferencesDialog::pickColor(const QString& key) {
    for (const ColorField& field : kColorFields) {
        if (key != field.key) continue;
        const QColor chosen = QColorDialog::getColor(editingTheme.*(field.member), this, QString("Choose %1 colour").arg(field.label));
        if (!chosen.isValid()) return;
        editingTheme.*(field.member) = chosen;
        previewEditingTheme();
        syncAppearanceControls();
        return;
    }
}

void PreferencesDialog::previewEditingTheme() {
    ThemeManager::instance().preview(editingTheme);
}

void PreferencesDialog::resetToDefaults() {
    playbackQualityCombo->setCurrentIndex(0);
    fpsOverlayCheck->setChecked(true);
    snapDistanceSpin->setValue(15);
    timecodeFormatCombo->setCurrentIndex(0);
    exportResCombo->setCurrentIndex(0);
    exportQualityCombo->setCurrentIndex(1);
    refreshThemeCombo("Z Pink");
    ShortcutManager::instance().resetToDefaults();
    populateShortcutsTable();
}

int PreferencesDialog::getDefaultPlaybackQuality() {
    return QSettings("Z-Creative", "Z").value("Playback/QualityScale", 1).toInt();
}

bool PreferencesDialog::getDefaultFpsOverlay() {
    return QSettings("Z-Creative", "Z").value("Playback/ShowFpsOverlay", true).toBool();
}

int PreferencesDialog::getSnappingDistancePx() {
    return QSettings("Z-Creative", "Z").value("Timeline/SnapDistancePx", 15).toInt();
}

int PreferencesDialog::getTimecodeFormat() {
    return QSettings("Z-Creative", "Z").value("Timeline/TimecodeFormat", 0).toInt();
}

QString PreferencesDialog::getDefaultExportResolution() {
    return QSettings("Z-Creative", "Z").value("Export/Resolution", "Match Project Source").toString();
}

int PreferencesDialog::getDefaultExportCrf() {
    return QSettings("Z-Creative", "Z").value("Export/Crf", 22).toInt();
}
