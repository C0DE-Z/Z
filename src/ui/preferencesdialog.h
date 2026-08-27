#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

    static int getAutoSaveIntervalMinutes();
    static int getDefaultPlaybackQuality();
    static bool getDefaultFpsOverlay();
    static int getSnappingDistancePx();
    static QString getDefaultExportResolution();
    static int getDefaultExportCrf();

private slots:
    void savePreferences();
    void resetToDefaults();

private:
    QTabWidget* tabWidget = nullptr;

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
