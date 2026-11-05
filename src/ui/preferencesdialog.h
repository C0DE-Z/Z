#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMap>
#include "ui/thememanager.h"

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

    void showTab(const QString& title);

    static int getAutoSaveIntervalMinutes();
    static int getDefaultPlaybackQuality();
    static bool getDefaultFpsOverlay();
    static int getSnappingDistancePx();
    static QString getDefaultExportResolution();
    static int getDefaultExportCrf();

public slots:
    void reject() override;

private slots:
    void savePreferences();
    void resetToDefaults();

private:
    QTabWidget* tabWidget = nullptr;

    // Appearance
    QComboBox* themeCombo = nullptr;
    QMap<QString, QPushButton*> colorSwatches;
    QSpinBox* radiusSpin = nullptr;
    QSpinBox* fontSizeSpin = nullptr;
    QPushButton* duplicateThemeBtn = nullptr;
    QPushButton* deleteThemeBtn = nullptr;
    QPushButton* exportThemeBtn = nullptr;
    Theme editingTheme;
    QString originalThemeName;
    bool syncingAppearance = false;

    void buildAppearanceTab();
    void refreshThemeCombo(const QString& select);
    void selectTheme(const QString& name);
    void syncAppearanceControls();
    void pickColor(const QString& key);
    void previewEditingTheme();

    // General
    QSpinBox* autoSaveSpin = nullptr;
    QCheckBox* promptExitCheck = nullptr;

    // Playback
    QComboBox* playbackQualityCombo = nullptr;
    QCheckBox* fpsOverlayCheck = nullptr;
    QComboBox* audioBufferSizeCombo = nullptr;

    // Timeline
    QSpinBox* snapDistanceSpin = nullptr;
    QComboBox* timecodeFormatCombo = nullptr;

    // Export
    QComboBox* exportResCombo = nullptr;
    QComboBox* exportQualityCombo = nullptr;

    // Shortcuts
    class QTableWidget* shortcutsTable = nullptr;

    void loadPreferences();
    void populateShortcutsTable();
    void saveShortcuts();
};

#endif // PREFERENCESDIALOG_H
