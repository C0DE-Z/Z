#pragma once

#include "engine/detector.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

// All values needed to run detection are copied into a job. The GUI can safely
// change a slider or class filter while a previous inference is still running.
struct DetectionWorkerSettings {
    float sensitivity = 0.45f;
    float minArea = 0.012f;
    float yoloConfidence = 0.25f;
    float yoloNmsThreshold = 0.45f;
    bool preferOpenCL = true;
    // 0 restores the model's automatic compatibility input size.
    int yoloInputSize = 0;
    bool classFilterEnabled = false;
    std::unordered_set<std::string> allowedClasses;
};

enum class DetectionWorkerResultType {
    ModelLoaded,
    FrameDetected,
    MaskBuilt,
    ScanStarted,
    ScanSample,
    ScanFinished
};

struct DetectionWorkerResult {
    DetectionWorkerResultType type = DetectionWorkerResultType::FrameDetected;
    uint64_t generation = 0;
    std::string sourceClipId;
    double sourceTime = 0.0;
    int width = 0;
    int height = 0;
    float progress = 0.0f;
    int sampleIndex = 0;
    int sampleCount = 0;
    bool success = true;
    bool cancelled = false;
    bool modelReady = false;
    std::string status;
    std::vector<DetectionBox> detections;
    std::vector<uint8_t> mask;
};

// A single persistent worker prevents ONNX/OpenCV state from crossing threads
// and ensures expensive image work never executes from a Qt GUI callback.
class DetectionWorker {
public:
    using ResultCallback = std::function<void(DetectionWorkerResult&&)>;

    DetectionWorker();
    ~DetectionWorker();

    DetectionWorker(const DetectionWorker&) = delete;
    DetectionWorker& operator=(const DetectionWorker&) = delete;

    void requestModelLoad(
        std::string modelPath,
        DetectionWorkerSettings settings,
        uint64_t generation,
        ResultCallback callback);

    void requestFrameDetection(
        std::shared_ptr<const DecodedVideoFrame> frame,
        std::string sourceClipId,
        double sourceTime,
        DetectionWorkerSettings settings,
        bool resetTracking,
        uint64_t generation,
        ResultCallback callback);

    // Use when the preview does not currently own a decoded frame. Decoding
    // happens in the worker, never from the button-click handler.
    void requestSourceFrameDetection(
        std::string sourcePath,
        std::string sourceClipId,
        double sourceTime,
        DetectionWorkerSettings settings,
        bool resetTracking,
        uint64_t generation,
        ResultCallback callback);

    void requestClipScan(
        std::string sourcePath,
        std::string sourceClipId,
        double sourceStart,
        double sourceDuration,
        double sampleInterval,
        DetectionWorkerSettings settings,
        uint64_t generation,
        ResultCallback callback);

    void requestMaskBuild(
        int width,
        int height,
        std::vector<DetectionBox> boxes,
        float featherPx,
        DetectionShape shape,
        float outlineWidthPx,
        float paddingPx,
        uint64_t generation,
        ResultCallback callback);

    void cancelScan();
    void stop();

private:
    enum class JobType {
        ModelLoad,
        FrameDetection,
        SourceFrameDetection,
        ClipScan,
        MaskBuild
    };

    struct Job {
        JobType type = JobType::FrameDetection;
        std::shared_ptr<std::atomic_bool> cancelled;
        DetectionWorkerSettings settings;
        uint64_t generation = 0;
        ResultCallback callback;

        std::shared_ptr<const DecodedVideoFrame> frame;
        std::string sourcePath;
        std::string sourceClipId;
        double sourceTime = 0.0;
        bool resetTracking = false;

        double sourceDuration = 0.0;
        double sampleInterval = 1.0;

        int maskWidth = 0;
        int maskHeight = 0;
        std::vector<DetectionBox> boxes;
        float featherPx = 0.0f;
        DetectionShape maskShape = DetectionShape::Rectangle;
        float outlineWidthPx = 6.0f;
        float paddingPx = 0.0f;
    };

    Detector m_detector;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stopping = false;

    // Latest interactive requests replace stale work. A long scan remains low
    // priority so model loads and manual/live preview requests are not queued
    // behind it before the scan starts.
    std::optional<Job> m_pendingModel;
    std::optional<Job> m_pendingFrame;
    std::optional<Job> m_pendingMask;
    std::optional<Job> m_pendingScan;
    std::shared_ptr<std::atomic_bool> m_activeModelCancellation;
    std::shared_ptr<std::atomic_bool> m_activeFrameCancellation;
    std::shared_ptr<std::atomic_bool> m_activeMaskCancellation;
    std::shared_ptr<std::atomic_bool> m_activeScanCancellation;
    bool m_scanInterruptedForInteractiveWork = false;

    void workerLoop();
    std::optional<Job> takeNextJobLocked();
    bool isStoppingOrCancelled(const Job& job) const;
    bool hasPendingInteractiveWork() const;
    void applySettings(const DetectionWorkerSettings& settings);
    void deliver(const Job& job, DetectionWorkerResult&& result) const;
    void processModelLoad(const Job& job);
    void processFrameDetection(const Job& job, bool decodeSourceFrame);
    void processMaskBuild(const Job& job);
    void processClipScan(const Job& job);
};