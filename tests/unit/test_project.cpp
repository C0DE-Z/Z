#include "core/project.h"
#include <cassert>
#include <iostream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

void testProjectSerialization() {
    auto& proj = Project::instance();
    proj.clear();

    TimelineTrack t1;
    t1.id = 1;
    t1.name = "Video 1";

    ProjectClip c1;
    c1.id = "clip_01";
    c1.mediaId = "source_01";
    c1.name = "Intro";
    c1.timelineStart = 0.0;
    c1.sourceDuration = 15.0;
    AppliedEffect effect;
    effect.pluginId = "pixel_sorter";
    effect.startOffset = 3.5;
    c1.effects.push_back(effect);
    t1.clips.push_back(c1);

    ProjectTransition tr;
    tr.id = "trans_01";
    tr.pluginId = "cross_dissolve";
    tr.cutTime = 5.0;
    tr.duration = 1.0;
    t1.transitions.push_back(tr);

    proj.getTracks().push_back(t1);

    const std::string tmpPath = "test_project_temp.json";
    bool saveOk = proj.save(tmpPath);
    assert(saveOk);

    // Clear and reload
    proj.clear();
    assert(proj.getTracks().empty());

    bool loadOk = proj.load(tmpPath);
    assert(loadOk);
    assert(proj.getTracks().size() == 1);
    assert(proj.getTracks()[0].clips.size() == 1);
    assert(proj.getTracks()[0].clips[0].id == "clip_01");
    assert(proj.getTracks()[0].clips[0].mediaId == "source_01");
    assert(proj.getTracks()[0].clips[0].effects[0].startOffset == 3.5);
    assert(proj.getTracks()[0].type == TimelineTrackType::Video);
    assert(proj.getTracks()[0].transitions.size() == 1);
    assert(proj.getTracks()[0].transitions[0].pluginId == "cross_dissolve");

    QFile::remove(QString::fromStdString(tmpPath));
    std::cout << "[PASS] testProjectSerialization\n";
}

void testAudioTrackAndLegacyCompatibility() {
    auto& proj = Project::instance();
    proj.clear();

    TimelineTrack audioTrack;
    audioTrack.id = 2;
    audioTrack.name = "Audio 1";
    audioTrack.type = TimelineTrackType::Audio;
    ProjectClip audioClip;
    audioClip.id = "clip_01_audio";
    audioClip.mediaId = "source_01";
    audioClip.sourceDuration = 5.0;
    audioTrack.clips.push_back(audioClip);
    proj.getTracks().push_back(audioTrack);

    const std::string typedPath = "test_project_audio_temp.json";
    assert(proj.save(typedPath));
    proj.clear();
    assert(proj.load(typedPath));
    assert(proj.getTracks().size() == 1);
    assert(proj.getTracks()[0].type == TimelineTrackType::Audio);
    assert(proj.getTracks()[0].clips[0].mediaId == "source_01");
    QFile::remove(QString::fromStdString(typedPath));

    QJsonObject legacyTrack;
    legacyTrack["id"] = 1;
    legacyTrack["name"] = "Old Track";
    legacyTrack["clips"] = QJsonArray();
    legacyTrack["effects"] = QJsonArray();
    legacyTrack["transitions"] = QJsonArray();
    QJsonObject root;
    root["tracks"] = QJsonArray { legacyTrack };
    const std::string legacyPath = "test_project_legacy_temp.json";
    QFile legacyFile(QString::fromStdString(legacyPath));
    assert(legacyFile.open(QIODevice::WriteOnly));
    legacyFile.write(QJsonDocument(root).toJson());
    legacyFile.close();

    proj.clear();
    assert(proj.load(legacyPath));
    assert(proj.getTracks().size() == 1);
    assert(proj.getTracks()[0].type == TimelineTrackType::Video);
    QFile::remove(QString::fromStdString(legacyPath));
    std::cout << "[PASS] testAudioTrackAndLegacyCompatibility\n";
}

void testCutPrecisionMath() {
    // Test 30 FPS cutting: playhead pausing anywhere inside frame 60 [2.000s, 2.0333s)
    const double fps = 30.0;
    const double clipTimelineStart = 0.0;
    const double clipSourceStart = 0.0;
    const double clipSourceDuration = 10.0;

    auto simulateCut = [](double currentPlayhead, double timelineStart, double sourceStart, double sourceDuration, double fps) {
        const double relTime = std::max(0.0, currentPlayhead - timelineStart);
        const int totalFrames = std::max(2, static_cast<int>(std::round(sourceDuration * fps)));
        int cutFrame = static_cast<int>(std::floor((relTime + 1e-5) * fps));
        cutFrame = std::clamp(cutFrame, 1, totalFrames - 1);

        const double leftDuration = static_cast<double>(cutFrame) / fps;
        const double cutTime = timelineStart + leftDuration;
        const double rightDuration = sourceDuration - leftDuration;
        const double rightSourceStart = sourceStart + leftDuration;

        return std::make_tuple(cutFrame, cutTime, leftDuration, rightSourceStart, rightDuration);
    };

    // Paused at beginning of frame 60 (2.000s)
    {
        auto [frame, cutTime, leftDur, rightSrc, rightDur] = simulateCut(2.000, clipTimelineStart, clipSourceStart, clipSourceDuration, fps);
        assert(frame == 60);
        assert(std::abs(cutTime - 2.000) < 1e-5);
        assert(std::abs(leftDur - 2.000) < 1e-5);
        assert(std::abs(rightSrc - 2.000) < 1e-5);
        assert(std::abs(rightDur - 8.000) < 1e-5);
    }

    // Paused in the middle of frame 60 (2.015s) - must still cut exactly at frame 60 (2.000s) without advancing to frame 61!
    {
        auto [frame, cutTime, leftDur, rightSrc, rightDur] = simulateCut(2.015, clipTimelineStart, clipSourceStart, clipSourceDuration, fps);
        assert(frame == 60);
        assert(std::abs(cutTime - 2.000) < 1e-5);
        assert(std::abs(leftDur - 2.000) < 1e-5);
        assert(std::abs(rightSrc - 2.000) < 1e-5);
        assert(std::abs(rightDur - 8.000) < 1e-5);
    }

    // Paused near the end of frame 60 (2.030s) - must still cut exactly at frame 60 (2.000s)
    {
        auto [frame, cutTime, leftDur, rightSrc, rightDur] = simulateCut(2.030, clipTimelineStart, clipSourceStart, clipSourceDuration, fps);
        assert(frame == 60);
        assert(std::abs(cutTime - 2.000) < 1e-5);
        assert(std::abs(leftDur - 2.000) < 1e-5);
        assert(std::abs(rightSrc - 2.000) < 1e-5);
    }

    // Stepped forward to frame 61 (2.0334s) - now cuts at frame 61
    {
        auto [frame, cutTime, leftDur, rightSrc, rightDur] = simulateCut(2.0334, clipTimelineStart, clipSourceStart, clipSourceDuration, fps);
        assert(frame == 61);
        assert(std::abs(cutTime - (61.0 / 30.0)) < 1e-5);
        assert(std::abs(rightSrc - (61.0 / 30.0)) < 1e-5);
    }

    // Second cut on an already cut clip with sourceStart > 0
    {
        const double cut1RightSrc = 2.000;
        const double cut1TimelineStart = 2.000;
        const double cut1SourceDuration = 8.000;

        // Cut at 5.000s on timeline (frame 90 relative to source start 2.000s = 3.0s rel)
        auto [frame, cutTime, leftDur, rightSrc, rightDur] = simulateCut(5.000, cut1TimelineStart, cut1RightSrc, cut1SourceDuration, fps);
        assert(frame == 90);
        assert(std::abs(cutTime - 5.000) < 1e-5);
        assert(std::abs(leftDur - 3.000) < 1e-5);
        assert(std::abs(rightSrc - 5.000) < 1e-5);
        assert(std::abs(rightDur - 5.000) < 1e-5);
    }

    std::cout << "[PASS] testCutPrecisionMath\n";
}

int main() {
    std::cout << "=== Running Project Serialization and Cut Precision Unit Tests ===\n";
    testProjectSerialization();
    testAudioTrackAndLegacyCompatibility();
    testCutPrecisionMath();
    std::cout << "=== All Project Tests Passed ===\n";
    return 0;
}
