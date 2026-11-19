#include "appstate.h"

namespace {
constexpr int MAX_UNDO_STATES = 50;
}

void AppState::pushUndoState() {
    undoStack.push_back(Project::instance().toJson());
    while (undoStack.size() > MAX_UNDO_STATES) {
        undoStack.removeFirst();
    }
    redoStack.clear();
    emit stateChanged();
}

bool AppState::undo() {
    if (undoStack.isEmpty()) return false;
    redoStack.push_back(Project::instance().toJson());
    while (redoStack.size() > MAX_UNDO_STATES) {
        redoStack.removeFirst();
    }
    Project::instance().fromJson(undoStack.takeLast());
    emit stateRestored();
    emit stateChanged();
    return true;
}

bool AppState::redo() {
    if (redoStack.isEmpty()) return false;
    undoStack.push_back(Project::instance().toJson());
    while (undoStack.size() > MAX_UNDO_STATES) {
        undoStack.removeFirst();
    }
    Project::instance().fromJson(redoStack.takeLast());
    emit stateRestored();
    emit stateChanged();
    return true;
}

void AppState::clear() {
    undoStack.clear();
    redoStack.clear();
    playhead = 0.0;
    playing = false;
    emit stateChanged();
}
