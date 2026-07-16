#include "appstate.h"

void AppState::pushUndoState() {
    undoStack.push_back(Project::instance().toJson());
    redoStack.clear();
}

bool AppState::undo() {
    if (undoStack.isEmpty()) return false;
    redoStack.push_back(Project::instance().toJson());
    Project::instance().fromJson(undoStack.takeLast());
    emit stateRestored();
    return true;
}

bool AppState::redo() {
    if (redoStack.isEmpty()) return false;
    undoStack.push_back(Project::instance().toJson());
    Project::instance().fromJson(redoStack.takeLast());
    emit stateRestored();
    return true;
}

void AppState::clear() {
    undoStack.clear();
    redoStack.clear();
    playhead = 0.0;
    playing = false;
}
