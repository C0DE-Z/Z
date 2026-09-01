#include "detectionworker.h"

#include "engine/videodecoder.h"

#include <algorithm>
#include <cmath>
#include <utility>

DetectionWorker::DetectionWorker()
    : m_thread(&DetectionWorker::workerLoop, this) {
}

DetectionWorker::~DetectionWorker() {
    stop();
}

void DetectionWorker::requestModelLoad(
    std::string modelPath,
    DetectionWorkerSettings settings,
    uint64_t generation,
    ResultCallback callback) {
    Job job;
    job.type = JobType::ModelLoad;
    job.cancelled = std::make_shared<std::atomic_bool>(false);
    job.settings = std::move(settings);
    job.generation = generation;
    job.callback = std::move(callback);
    job.sourcePath = std::move(modelPath);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) return;
    if (m_activeModelCancellation) m_activeModelCancellation->store(true);
    if (m_pendingModel && m_pendingModel->cancelled) m_pendingModel->cancelled->store(true);
    m_activeModelCancellation = job.cancelled;
    m_pendingModel = std::move(job);
    m_cv.notify_one();
}

void DetectionWorker::requestFrameDetection(
    std::shared_ptr<const DecodedVideoFrame> frame,
    std::string sourceClipId,
    double sourceTime,
    DetectionWorkerSettings settings,
    bool resetTracking,
    uint64_t generation,
    ResultCallback callback) {
    if (!frame) return;

    Job job;
    job.type = JobType::FrameDetection;
    job.cancelled = std::make_shared<std::atomic_bool>(false);
    job.settings = std::move(settings);
    job.generation = generation;
    job.callback = std::move(callback);
    job.frame = std::move(frame);
    job.sourceClipId = std::move(sourceClipId);
    job.sourceTime = sourceTime;
    job.resetTracking = resetTracking;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) return;
    if (m_activeFrameCancellation) m_activeFrameCancellation->store(true);
    if (m_pendingFrame && m_pendingFrame->cancelled) m_pendingFrame->cancelled->store(true);
    m_activeFrameCancellation = job.cancelled;
    m_pendingFrame = std::move(job);
    m_cv.notify_one();
}

void DetectionWorker::requestSourceFrameDetection(
    std::string sourcePath,
    std::string sourceClipId,
    double sourceTime,
    DetectionWorkerSettings settings,
    bool resetTracking,
    uint64_t generation,
    ResultCallback callback) {
    Job job;
    job.type = JobType::SourceFrameDetection;
    job.cancelled = std::make_shared<std::atomic_bool>(false);
    job.settings = std::move(settings);
    job.generation = generation;
    job.callback = std::move(callback);
    job.sourcePath = std::move(sourcePath);
    job.sourceClipId = std::move(sourceClipId);
    job.sourceTime = sourceTime;
    job.resetTracking = resetTracking;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) return;
    if (m_activeFrameCancellation) m_activeFrameCancellation->store(true);
    if (m_pendingFrame && m_pendingFrame->cancelled) m_pendingFrame->cancelled->store(true);
    m_activeFrameCancellation = job.cancelled;
    m_pendingFrame = std::move(job);
    m_cv.notify_one();
}

void DetectionWorker::requestClipScan(
    std::string sourcePath,
    std::string sourceClipId,
    double sourceStart,
    double sourceDuration,
    double sampleInterval,
    DetectionWorkerSettings settings,
    uint64_t generation,
    ResultCallback callback) {
    Job job;
    job.type = JobType::ClipScan;
    job.cancelled = std::make_shared<std::atomic_bool>(false);
    job.settings = std::move(settings);
    job.generation = generation;
    job.callback = std::move(callback);
    job.sourcePath = std::move(sourcePath);
    job.sourceClipId = std::move(sourceClipId);
    job.sourceTime = std::max(0.0, sourceStart);
    job.sourceDuration = std::max(0.0, sourceDuration);
    job.sampleInterval = std::max(0.10, sampleInterval);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) return;
    if (m_activeScanCancellation) m_activeScanCancellation->store(true);
    if (m_pendingScan && m_pendingScan->cancelled) m_pendingScan->cancelled->store(true);
    m_activeScanCancellation = job.cancelled;
    m_pendingScan = std::move(job);
    m_cv.notify_one();
}

void DetectionWorker::requestMaskBuild(
    int width,
    int height,
    std::vector<DetectionBox> boxes,
    float featherPx,
    DetectionShape shape,
    float outlineWidthPx,
    float paddingPx,
    uint64_t generation,
    ResultCallback callback) {
    Job job;
    job.type = JobType::MaskBuild;
    job.cancelled = std::make_shared<std::atomic_bool>(false);
    job.generation = generation;
    job.callback = std::move(callback);
    job.maskWidth = width;
    job.maskHeight = height;
    job.boxes = std::move(boxes);
    job.featherPx = featherPx;
    job.maskShape = shape;
    job.outlineWidthPx = outlineWidthPx;
    job.paddingPx = paddingPx;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) return;
    if (m_activeMaskCancellation) m_activeMaskCancellation->store(true);
    if (m_pendingMask && m_pendingMask->cancelled) m_pendingMask->cancelled->store(true);
    m_activeMaskCancellation = job.cancelled;
    m_pendingMask = std::move(job);
    m_cv.notify_one();
}

void DetectionWorker::cancelScan() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeScanCancellation) m_activeScanCancellation->store(true);
    if (m_pendingScan && m_pendingScan->cancelled) m_pendingScan->cancelled->store(true);
    m_pendingScan.reset();
}

void DetectionWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping) return;
        m_stopping = true;
        for (const auto& cancellation : {
                 m_activeModelCancellation,
                 m_activeFrameCancellation,
                 m_activeMaskCancellation,
                 m_activeScanCancellation }) {
            if (cancellation) cancellation->store(true);
        }
        m_pendingModel.reset();
        m_pendingFrame.reset();
        m_pendingMask.reset();
        m_pendingScan.reset();
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

std::optional<DetectionWorker::Job> DetectionWorker::takeNextJobLocked() {
    if (m_pendingModel) {
        auto job = std::move(m_pendingModel);
        m_pendingModel.reset();
        return job;
    }
    if (m_pendingFrame) {
        auto job = std::move(m_pendingFrame);
        m_pendingFrame.reset();
        return job;
    }
    if (m_pendingMask) {
        auto job = std::move(m_pendingMask);
        m_pendingMask.reset();
        return job;
    }
    if (m_pendingScan) {
        auto job = std::move(m_pendingScan);
        m_pendingScan.reset();
        return job;
    }
    return std::nullopt;
}

bool DetectionWorker::isStoppingOrCancelled(const Job& job) const {
    if (job.cancelled && job.cancelled->load()) return true;
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping;
}

bool DetectionWorker::hasPendingInteractiveWork() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Mask rebuilding can safely wait for a scan sample batch; model changes
    // and frame requests cannot. This avoids interrupting a scan after every
    // sample merely because its preview mask was refreshed.
    return m_stopping || m_pendingModel.has_value() || m_pendingFrame.has_value();
}

void DetectionWorker::applySettings(const DetectionWorkerSettings& settings) {
    m_detector.setSensitivity(settings.sensitivity);
    m_detector.setMinArea(settings.minArea);
    m_detector.setYoloConfidence(settings.yoloConfidence);
    m_detector.setYoloNmsThreshold(settings.yoloNmsThreshold);
    m_detector.setPreferOpenCL(settings.preferOpenCL);
    // Zero is meaningful: restore the current model's automatic compatibility
    // input size. Omitting it here retained an earlier manual resolution.
    m_detector.setYoloInputSize(settings.yoloInputSize);
    m_detector.setAllowedClasses(settings.allowedClasses, settings.classFilterEnabled);
}

void DetectionWorker::deliver(const Job& job, DetectionWorkerResult&& result) const {
    if (job.callback) job.callback(std::move(result));
}

void DetectionWorker::processModelLoad(const Job& job) {
    if (isStoppingOrCancelled(job)) return;

    std::string error;
    const bool loaded = m_detector.setYoloModel(job.sourcePath, &error);
    if (loaded) applySettings(job.settings);
    if (isStoppingOrCancelled(job)) return;

    DetectionWorkerResult result;
    result.type = DetectionWorkerResultType::ModelLoaded;
    result.generation = job.generation;
    result.success = loaded;
    result.modelReady = loaded;
    result.status = loaded ? m_detector.status() : error;
    deliver(job, std::move(result));
}

void DetectionWorker::processFrameDetection(const Job& job, bool decodeSourceFrame) {
    if (isStoppingOrCancelled(job)) return;

    std::shared_ptr<const DecodedVideoFrame> frame = job.frame;
    std::shared_ptr<DecodedVideoFrame> decodedFrame;
    if (decodeSourceFrame) {
        VideoDecoder decoder;
        decodedFrame = std::make_shared<DecodedVideoFrame>();
        if (!decoder.openFile(job.sourcePath, false, false) ||
            !decoder.decodeFrameAt(job.sourceTime, *decodedFrame)) {
            if (isStoppingOrCancelled(job)) return;
            DetectionWorkerResult result;
            result.type = DetectionWorkerResultType::FrameDetected;
            result.generation = job.generation;
            result.sourceClipId = job.sourceClipId;
            result.sourceTime = job.sourceTime;
            result.success = false;
            result.status = "Could not decode a frame for object detection.";
            deliver(job, std::move(result));
            return;
        }
        frame = decodedFrame;
    }

    if (!frame || frame->rgbData.empty() || frame->width <= 0 || frame->height <= 0) {
        DetectionWorkerResult result;
        result.type = DetectionWorkerResultType::FrameDetected;
        result.generation = job.generation;
        result.sourceClipId = job.sourceClipId;
        result.sourceTime = job.sourceTime;
        result.success = false;
        result.status = "No usable RGB frame is available for object detection.";
        deliver(job, std::move(result));
        return;
    }

    if (job.resetTracking || m_scanInterruptedForInteractiveWork) {
        m_detector.reset();
        m_scanInterruptedForInteractiveWork = false;
    }
    applySettings(job.settings);
    auto detections = m_detector.detectFrame(*frame);
    if (isStoppingOrCancelled(job)) return;

    DetectionWorkerResult result;
    result.type = DetectionWorkerResultType::FrameDetected;
    result.generation = job.generation;
    result.sourceClipId = job.sourceClipId;
    result.sourceTime = job.sourceTime;
    result.width = frame->width;
    result.height = frame->height;
    result.status = m_detector.status();
    result.detections = std::move(detections);
    deliver(job, std::move(result));
}

void DetectionWorker::processMaskBuild(const Job& job) {
    if (isStoppingOrCancelled(job)) return;
    auto mask = Detector::buildMaskFromBoxes(
        job.maskWidth, job.maskHeight, job.boxes, job.featherPx,
        job.maskShape, job.outlineWidthPx, job.paddingPx);
    if (isStoppingOrCancelled(job)) return;

    DetectionWorkerResult result;
    result.type = DetectionWorkerResultType::MaskBuilt;
    result.generation = job.generation;
    result.width = job.maskWidth;
    result.height = job.maskHeight;
    result.mask = std::move(mask);
    deliver(job, std::move(result));
}

void DetectionWorker::processClipScan(const Job& job) {
    m_scanInterruptedForInteractiveWork = false;
    DetectionWorkerResult started;
    started.type = DetectionWorkerResultType::ScanStarted;
    started.generation = job.generation;
    started.sourceClipId = job.sourceClipId;
    started.status = "Opening clip for background detection…";
    deliver(job, std::move(started));

    VideoDecoder decoder;
    if (!decoder.openFile(job.sourcePath, false, false)) {
        if (isStoppingOrCancelled(job)) return;
        DetectionWorkerResult failed;
        failed.type = DetectionWorkerResultType::ScanFinished;
        failed.generation = job.generation;
        failed.sourceClipId = job.sourceClipId;
        failed.success = false;
        failed.status = "Could not open this clip for background detection.";
        deliver(job, std::move(failed));
        return;
    }

    if (isStoppingOrCancelled(job)) return;
    m_detector.reset();
    applySettings(job.settings);

    const double firstTime = std::clamp(job.sourceTime, 0.0, decoder.getDuration());
    const double lastTime = std::clamp(
        job.sourceTime + job.sourceDuration, firstTime, decoder.getDuration());
    const int sampleCount = std::max(1, static_cast<int>(std::floor(
        (lastTime - firstTime) / job.sampleInterval + 0.0001)) + 1);

    bool cancelled = false;
    bool preemptedForInteractiveWork = false;
    int completedSamples = 0;
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        if (isStoppingOrCancelled(job)) {
            cancelled = true;
            break;
        }
        if (hasPendingInteractiveWork()) {
            // A single model inference cannot be forcibly interrupted by
            // OpenCV, but every clip sample is a cancellation boundary.
            // Stop here and let current-frame work run next rather than
            // making playback wait for an entire long scan.
            cancelled = true;
            preemptedForInteractiveWork = true;
            m_scanInterruptedForInteractiveWork = true;
            break;
        }

        const double time = std::min(lastTime, firstTime + sampleIndex * job.sampleInterval);
        DecodedVideoFrame frame;
        if (decoder.decodeFrameAt(time, frame) && !frame.rgbData.empty()) {
            auto detections = m_detector.detectFrame(frame);
            if (isStoppingOrCancelled(job)) {
                cancelled = true;
                break;
            }
            DetectionWorkerResult sample;
            sample.type = DetectionWorkerResultType::ScanSample;
            sample.generation = job.generation;
            sample.sourceClipId = job.sourceClipId;
            sample.sourceTime = time;
            sample.width = frame.width;
            sample.height = frame.height;
            sample.progress = static_cast<float>(sampleIndex + 1) / static_cast<float>(sampleCount);
            sample.sampleIndex = sampleIndex + 1;
            sample.sampleCount = sampleCount;
            sample.status = m_detector.status();
            sample.detections = std::move(detections);
            deliver(job, std::move(sample));
        }
        ++completedSamples;
    }

    if (isStoppingOrCancelled(job) && !cancelled) return;
    DetectionWorkerResult finished;
    finished.type = DetectionWorkerResultType::ScanFinished;
    finished.generation = job.generation;
    finished.sourceClipId = job.sourceClipId;
    finished.progress = sampleCount > 0
        ? static_cast<float>(completedSamples) / static_cast<float>(sampleCount) : 0.0f;
    finished.sampleIndex = completedSamples;
    finished.sampleCount = sampleCount;
    finished.success = !cancelled;
    finished.cancelled = cancelled;
    finished.status = !cancelled
        ? "Background clip detection complete."
        : (preemptedForInteractiveWork
            ? "Background clip detection stopped to prioritize current-frame detection."
            : "Background clip detection cancelled.");
    deliver(job, std::move(finished));
}

void DetectionWorker::workerLoop() {
    while (true) {
        std::optional<Job> job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] {
                return m_stopping || m_pendingModel || m_pendingFrame || m_pendingMask || m_pendingScan;
            });
            if (m_stopping) return;
            job = takeNextJobLocked();
        }
        if (!job || isStoppingOrCancelled(*job)) continue;

        switch (job->type) {
        case JobType::ModelLoad:
            processModelLoad(*job);
            break;
        case JobType::FrameDetection:
            processFrameDetection(*job, false);
            break;
        case JobType::SourceFrameDetection:
            processFrameDetection(*job, true);
            break;
        case JobType::MaskBuild:
            processMaskBuild(*job);
            break;
        case JobType::ClipScan:
            processClipScan(*job);
            break;
        }
    }
}