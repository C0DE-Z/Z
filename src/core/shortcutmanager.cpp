#include "shortcutmanager.h"
#include <QSettings>

ShortcutManager::ShortcutManager() {
    initialize();
    loadFromSettings();
}

void ShortcutManager::initialize() {
    definitions = {
        // Playback
        {"playback.toggle", "Playback", "Play / Pause", "Space", "Space"},
        {"playback.step_fwd", "Playback", "Step Forward 1 Frame", "Right", "Right"},
        {"playback.step_back", "Playback", "Step Backward 1 Frame", "Left", "Left"},
        {"playback.jump_start", "Playback", "Jump to Start", "Home", "Home"},
        {"playback.jump_end", "Playback", "Jump to End", "End", "End"},
        {"playback.mark_in", "Playback", "Set Mark In", "I", "I"},
        {"playback.mark_out", "Playback", "Set Mark Out", "O", "O"},
        {"playback.clear_inout", "Playback", "Clear In/Out Range", "Ctrl+Shift+X", "Ctrl+Shift+X"},

        // Edit
        {"edit.cut_clip", "Edit", "Cut Clip at Playhead", "Ctrl+K", "Ctrl+K"},
        {"edit.delete_clip", "Edit", "Delete Selected", "Delete", "Delete"},
        {"edit.undo", "Edit", "Undo", "Ctrl+Z", "Ctrl+Z"},
        {"edit.redo", "Edit", "Redo", "Ctrl+Shift+Z", "Ctrl+Shift+Z"},
        {"edit.preferences", "Edit", "Preferences", "Ctrl+,", "Ctrl+,"},

        // File
        {"file.import", "File", "Import Media Clip", "Ctrl+I", "Ctrl+I"},
        {"file.open_project", "File", "Open Project", "Ctrl+O", "Ctrl+O"},
        {"file.save_project", "File", "Save Project", "Ctrl+S", "Ctrl+S"},
        {"file.export_video", "File", "Export Video", "Ctrl+E", "Ctrl+E"},

        // View & Navigation
        {"view.zoom_in", "View", "Zoom In Timeline", "+", "+"},
        {"view.zoom_out", "View", "Zoom Out Timeline", "-", "-"},
        {"view.zoom_fit", "View", "Fit Timeline to Window", "Ctrl+0", "Ctrl+0"},
    };

    idToIndex.clear();
    for (size_t i = 0; i < definitions.size(); ++i) {
        idToIndex[definitions[i].id] = i;
    }
}

void ShortcutManager::loadFromSettings() {
    QSettings settings("Z-Creative", "Z");
    settings.beginGroup("Shortcuts");
    for (auto& def : definitions) {
        if (settings.contains(def.id)) {
            def.currentKey = settings.value(def.id).toString();
        }
    }
    settings.endGroup();
}

void ShortcutManager::saveToSettings() {
    QSettings settings("Z-Creative", "Z");
    settings.beginGroup("Shortcuts");
    for (const auto& def : definitions) {
        settings.setValue(def.id, def.currentKey);
    }
    settings.endGroup();
    emit shortcutsChanged();
}

void ShortcutManager::resetToDefaults() {
    for (auto& def : definitions) {
        def.currentKey = def.defaultKey;
    }
    saveToSettings();
}

QKeySequence ShortcutManager::getShortcut(const QString& id) const {
    auto it = idToIndex.find(id);
    if (it != idToIndex.end()) {
        const QString& keyStr = definitions[it.value()].currentKey;
        if (!keyStr.isEmpty()) {
            return QKeySequence(keyStr, QKeySequence::PortableText);
        }
    }
    return QKeySequence();
}

void ShortcutManager::setShortcut(const QString& id, const QKeySequence& sequence) {
    auto it = idToIndex.find(id);
    if (it != idToIndex.end()) {
        definitions[it.value()].currentKey = sequence.toString(QKeySequence::PortableText);
    }
}
