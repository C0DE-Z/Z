#include "core/appstate.h"
#include "core/project.h"
#include <cassert>
#include <iostream>

void testUndoRedoStackBounds() {
    auto& state = AppState::instance();
    state.clear();
    assert(state.undoStackSize() == 0);

    // Push 60 states
    for (int i = 0; i < 60; ++i) {
        TimelineTrack t;
        t.id = i + 1;
        t.name = "Track " + std::to_string(i + 1);
        Project::instance().getTracks().push_back(t);
        state.pushUndoState();
    }

    // Must be capped at max 50
    assert(state.undoStackSize() <= 50);

    // Test undo
    bool undoOk = state.undo();
    assert(undoOk);

    // Test redo
    bool redoOk = state.redo();
    assert(redoOk);

    state.clear();
    std::cout << "[PASS] testUndoRedoStackBounds\n";
}

int main() {
    std::cout << "=== Running AppState Unit Tests ===\n";
    testUndoRedoStackBounds();
    std::cout << "=== All AppState Tests Passed ===\n";
    return 0;
}
