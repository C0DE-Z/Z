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

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Preferences");
    resize(580, 420);
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    tabWidget = new QTabWidget(this);

    QWidget* generalTab = new QWidget(tabWidget);
    QFormLayout* generalLayout = new QFormLayout(generalTab);
    generalLayout->setContentsMargins(12, 12, 12, 12);
    generalLayout->setSpacing(10);

    autoSaveSpin = new QSpinBox(generalTab);
    autoSaveSpin->setRange(0, 60);
    autoSaveSpin->setSuffix(" min (0 = Disabled)");
    generalLayout->addRow("Auto-save interval:", autoSaveSpin);

    promptExitCheck = new QCheckBox("Prompt to save changes before closing", generalTab);
    generalLayout->addRow("", promptExitCheck);
    tabWidget->addTab(generalTab, "General");

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

    audioBufferSizeCombo = new QComboBox(playbackTab);
    audioBufferSizeCombo->addItem("256 frames (Lowest Latency)", 256);
    audioBufferSizeCombo->addItem("512 frames (Balanced)", 512);
    audioBufferSizeCombo->addItem("1024 frames (High Stability)", 1024);
    playbackLayout->addRow("Audio Buffer Size:", audioBufferSizeCombo);

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
    autoSaveSpin->setValue(settings.value("General/AutoSaveMinutes", 5).toInt());
    promptExitCheck->setChecked(settings.value("General/PromptOnExit", true).toBool());

    int qual = settings.value("Playback/QualityScale", 1).toInt();
    int qualIdx = playbackQualityCombo->findData(qual);
    if (qualIdx >= 0) playbackQualityCombo->setCurrentIndex(qualIdx);

    fpsOverlayCheck->setChecked(settings.value("Playback/ShowFpsOverlay", true).toBool());

    int buf = settings.value("Playback/AudioBuffer", 512).toInt();
    int bufIdx = audioBufferSizeCombo->findData(buf);
    if (bufIdx >= 0) audioBufferSizeCombo->setCurrentIndex(bufIdx);

    snapDistanceSpin->setValue(settings.value("Timeline/SnapDistancePx", 15).toInt());
    timecodeFormatCombo->setCurrentIndex(settings.value("Timeline/TimecodeFormat", 0).toInt());

    exportResCombo->setCurrentText(settings.value("Export/Resolution", "Match Project Source").toString());
    int crf = settings.value("Export/Crf", 22).toInt();
    int crfIdx = exportQualityCombo->findData(crf);
    if (crfIdx >= 0) exportQualityCombo->setCurrentIndex(crfIdx);
}

void PreferencesDialog::savePreferences() {
    QSettings settings("Z-Creative", "Z");
    settings.setValue("General/AutoSaveMinutes", autoSaveSpin->value());
    settings.setValue("General/PromptOnExit", promptExitCheck->isChecked());

    settings.setValue("Playback/QualityScale", playbackQualityCombo->currentData().toInt());
    settings.setValue("Playback/ShowFpsOverlay", fpsOverlayCheck->isChecked());
    settings.setValue("Playback/AudioBuffer", audioBufferSizeCombo->currentData().toInt());

    settings.setValue("Timeline/SnapDistancePx", snapDistanceSpin->value());
    settings.setValue("Timeline/TimecodeFormat", timecodeFormatCombo->currentIndex());

    settings.setValue("Export/Resolution", exportResCombo->currentText());
    settings.setValue("Export/Crf", exportQualityCombo->currentData().toInt());

    saveShortcuts();
    accept();
}

void PreferencesDialog::resetToDefaults() {
    autoSaveSpin->setValue(5);
    promptExitCheck->setChecked(true);
    playbackQualityCombo->setCurrentIndex(0);
    fpsOverlayCheck->setChecked(true);
    audioBufferSizeCombo->setCurrentIndex(1);
    snapDistanceSpin->setValue(15);
    timecodeFormatCombo->setCurrentIndex(0);
    exportResCombo->setCurrentIndex(0);
    exportQualityCombo->setCurrentIndex(1);
    ShortcutManager::instance().resetToDefaults();
    populateShortcutsTable();
}

int PreferencesDialog::getAutoSaveIntervalMinutes() {
    return QSettings("Z-Creative", "Z").value("General/AutoSaveMinutes", 5).toInt();
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

QString PreferencesDialog::getDefaultExportResolution() {
    return QSettings("Z-Creative", "Z").value("Export/Resolution", "Match Project Source").toString();
}

int PreferencesDialog::getDefaultExportCrf() {
    return QSettings("Z-Creative", "Z").value("Export/Crf", 22).toInt();
}
