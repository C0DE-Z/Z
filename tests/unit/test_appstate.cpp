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

void testLinkedSplitUndoRedo() {
    auto& state = AppState::instance();
    auto& proj = Project::instance();
    state.clear();
    proj.clear();

    TimelineTrack video; video.id = 1; video.type = TimelineTrackType::Video;
    TimelineTrack audio; audio.id = 2; audio.type = TimelineTrackType::Audio;
    ProjectClip v; v.id = "m"; v.mediaId = "m"; v.linkId = "m_link";
    v.sourceStart = 0.0; v.sourceDuration = 10.0; v.timelineStart = 0.0;
    ProjectClip a = v; a.id = "m_audio";
    video.clips.push_back(v);
    audio.clips.push_back(a);
    proj.getTracks() = { video, audio };

    state.pushUndoState();
    assert(splitLinkedClips(proj.getTracks(), "m", 4.0));
    assert(proj.getTracks()[0].clips.size() == 2 && proj.getTracks()[1].clips.size() == 2);

    assert(state.undo());
    assert(proj.getTracks()[0].clips.size() == 1 && proj.getTracks()[1].clips.size() == 1);
    assert(proj.getTracks()[0].clips[0].linkId == "m_link");
    assert(proj.getTracks()[1].clips[0].linkId == "m_link");
    assert(proj.getTracks()[1].clips[0].sourceDuration == 10.0);

    assert(state.redo());
    assert(proj.getTracks()[0].clips.size() == 2 && proj.getTracks()[1].clips.size() == 2);
    assert(proj.getTracks()[0].clips[1].linkId == proj.getTracks()[1].clips[1].linkId);
    assert(proj.getTracks()[0].clips[1].linkId != "m_link");

    state.clear();
    proj.clear();
    std::cout << "[PASS] testLinkedSplitUndoRedo\n";
}

int main() {
    std::cout << "=== Running AppState Unit Tests ===\n";
    testUndoRedoStackBounds();
    testLinkedSplitUndoRedo();
    std::cout << "=== All AppState Tests Passed ===\n";
    return 0;
}
