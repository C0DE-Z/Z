#include "detector.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <numeric>
#include <sstream>

#ifdef Z_HAS_OPENCV_DNN
#include <opencv2/core/ocl.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#endif

Detector& Detector::instance() {
    static Detector inst;
    return inst;
}

namespace {
constexpr size_t kMaxTrackTrailPoints = 120;
constexpr int kMaxMissedTrackUpdates = 12;
constexpr size_t kMaxYoloResults = 128;

float intersectionOverUnion(const DetectionBox& box, float trackX, float trackY, float trackW, float trackH) {
    const float left = std::max(box.x, trackX);
    const float top = std::max(box.y, trackY);
    const float right = std::min(box.x + box.w, trackX + trackW);
    const float bottom = std::min(box.y + box.h, trackY + trackH);
    const float intersection = std::max(0.0f, right - left) * std::max(0.0f, bottom - top);
    const float unionArea = box.w * box.h + trackW * trackH - intersection;
    return unionArea > 0.000001f ? intersection / unionArea : 0.0f;
}

bool isGenericRegionLabel(const std::string& label) {
    return label.empty() || label.rfind("region_", 0) == 0 || label.rfind("object_", 0) == 0;
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string compactErrorDetail(std::string detail) {
    std::replace(detail.begin(), detail.end(), '\n', ' ');
    std::replace(detail.begin(), detail.end(), '\r', ' ');
    constexpr size_t kMaxDetailLength = 360;
    if (detail.size() > kMaxDetailLength) {
        detail.resize(kMaxDetailLength);
        detail += "...";
    }
    return detail;
}

std::string describeYoloLoadFailure(const std::string& modelPath, const std::string& detail) {
    const size_t separator = modelPath.find_last_of("\\/");
    const std::string fileName = lowerAscii(separator == std::string::npos
        ? modelPath : modelPath.substr(separator + 1));
    std::string message;
    if (fileName.find("yolo11") != std::string::npos) {
        message = "This YOLO11 ONNX export is not supported by this OpenCV DNN importer. "
                  "Use 'Download Recommended YOLOv5x6 Model' instead.";
    } else {
        message = "OpenCV DNN could not import this ONNX model. Use a static YOLOv5-style COCO ONNX model "
                  "or 'Download Recommended YOLOv5x6 Model'.";
    }
    if (!detail.empty()) {
        message += " Details: " + compactErrorDetail(detail);
    }
    return message;
}

int automaticInputSizeForModel(const std::filesystem::path&) {
    // Most supported YOLO exports use 640px. The recommended P6/x6 checkpoint
    // is physically trained for 1280px, but OpenCV 4.12's ONNX importer cannot
    // evaluate that graph at 1280 (its Reshape node fails). It is verified at
    // 640px, so this is the automatic compatibility input for all models.
    return 640;
}

std::vector<DetectionBox::TrailPoint> personOutlineGuide(const DetectionBox& box) {
    const auto point = [&box](float x, float y) {
        return DetectionBox::TrailPoint {
            std::clamp(box.x + box.w * x, 0.0f, 1.0f),
            std::clamp(box.y + box.h * y, 0.0f, 1.0f)
        };
    };

    // A legible human-shaped guide is more useful than another rectangle for
    // a person detection. It is intentionally not advertised as a real mask:
    // a bounding-box detector cannot know the subject's exact silhouette.
    return {
        point(0.50f, 0.02f), point(0.38f, 0.05f), point(0.31f, 0.15f),
        point(0.32f, 0.26f), point(0.22f, 0.32f), point(0.16f, 0.47f),
        point(0.24f, 0.53f), point(0.31f, 0.42f), point(0.34f, 0.62f),
        point(0.25f, 0.97f), point(0.40f, 0.98f), point(0.50f, 0.72f),
        point(0.60f, 0.98f), point(0.75f, 0.97f), point(0.66f, 0.62f),
        point(0.69f, 0.42f), point(0.76f, 0.53f), point(0.84f, 0.47f),
        point(0.78f, 0.32f), point(0.68f, 0.26f), point(0.69f, 0.15f),
        point(0.62f, 0.05f)
    };
}

bool pointInPolygon(float x, float y, const std::vector<DetectionBox::TrailPoint>& polygon) {
    bool inside = false;
    for (size_t current = 0, previous = polygon.size() - 1; current < polygon.size(); previous = current++) {
        const auto& a = polygon[current];
        const auto& b = polygon[previous];
        const bool spansY = (a.y > y) != (b.y > y);
        if (spansY) {
            const float intersectionX = (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x;
            if (x < intersectionX) inside = !inside;
        }
    }
    return inside;
}

float distanceToSegmentPixels(
    float px, float py,
    const DetectionBox::TrailPoint& a,
    const DetectionBox::TrailPoint& b,
    int width, int height) {
    const float ax = a.x * width;
    const float ay = a.y * height;
    const float bx = b.x * width;
    const float by = b.y * height;
    const float dx = bx - ax;
    const float dy = by - ay;
    const float lengthSquared = dx * dx + dy * dy;
    const float projection = lengthSquared > 0.000001f
        ? std::clamp(((px - ax) * dx + (py - ay) * dy) / lengthSquared, 0.0f, 1.0f)
        : 0.0f;
    return std::hypot(px - (ax + projection * dx), py - (ay + projection * dy));
}
}

const std::vector<std::string>& Detector::cocoClassLabels() {
    static const std::vector<std::string> labels = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
        "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed",
        "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven",
        "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
    };
    return labels;
}

void Detector::setSensitivity(float sensitivity) {
    std::lock_guard<std::mutex> lock(mutex);
    m_sensitivity = std::clamp(sensitivity, 0.05f, 0.95f);
}

float Detector::sensitivity() const {
    std::lock_guard<std::mutex> lock(mutex);
    return m_sensitivity;
}

void Detector::setMinArea(float minAreaNorm) {
    std::lock_guard<std::mutex> lock(mutex);
    m_minArea = std::clamp(minAreaNorm, 0.001f, 0.5f);
}

float Detector::minArea() const {
    std::lock_guard<std::mutex> lock(mutex);
    return m_minArea;
}

void Detector::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    prevW = 0;
    prevH = 0;
    prevGray.clear();
    resetTracksLocked();
}

bool Detector::setYoloModel(const std::string& modelPath, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex);
    m_modelPath.clear();
    m_yoloReady = false;
    m_openClFallbackActive = false;
    resetTracksLocked();
#ifdef Z_HAS_OPENCV_DNN
    m_yoloNet = cv::dnn::Net();
    if (modelPath.empty()) {
        m_status = "YOLO ONNX model path is empty.";
        if (error) *error = m_status;
        return false;
    }

    const std::filesystem::path modelFile(modelPath);
    std::error_code filesystemError;
    const bool modelExists = std::filesystem::exists(modelFile, filesystemError);
    if (filesystemError) {
        m_status = "Could not access the YOLO ONNX model: " + filesystemError.message();
        if (error) *error = m_status;
        return false;
    }
    if (!modelExists) {
        m_status = "YOLO ONNX model file was not found: " + modelPath;
        if (error) *error = m_status;
        return false;
    }
    if (!std::filesystem::is_regular_file(modelFile, filesystemError)) {
        m_status = filesystemError
            ? "Could not access the YOLO ONNX model: " + filesystemError.message()
            : "YOLO ONNX model path is not a regular file: " + modelPath;
        if (error) *error = m_status;
        return false;
    }
    try {
        m_defaultYoloInputSize = automaticInputSizeForModel(modelFile);
        m_yoloInputSize = m_defaultYoloInputSize;
        m_yoloNet = cv::dnn::readNetFromONNX(modelPath);
        if (m_yoloNet.empty()) {
            m_status = "OpenCV DNN imported an empty YOLO network. Use 'Download Recommended YOLOv5x6 Model'.";
            if (error) *error = m_status;
            return false;
        }
        m_modelPath = modelPath;
        m_yoloReady = true;
        m_status = "YOLO ONNX ready: " + modelFile.filename().string() +
            "; automatic compatibility input: " + std::to_string(m_yoloInputSize) + " px.";
        return true;
    } catch (const cv::Exception& e) {
        m_status = describeYoloLoadFailure(modelPath, e.what());
    } catch (const std::exception& e) {
        m_status = describeYoloLoadFailure(modelPath, e.what());
    }
#else
    m_status = "YOLO unavailable: build Z with OpenCV DNN support";
#endif
    if (error) *error = m_status;
    return false;
}

bool Detector::yoloReady() const { std::lock_guard<std::mutex> lock(mutex); return m_yoloReady; }
std::string Detector::status() const { std::lock_guard<std::mutex> lock(mutex); return m_status; }
void Detector::setPreferOpenCL(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    if (enabled && !m_preferOpenCL) m_openClFallbackActive = false;
    m_preferOpenCL = enabled;
}

void Detector::setYoloConfidence(float threshold) {
    std::lock_guard<std::mutex> lock(mutex);
    m_yoloConfidence = std::clamp(threshold, 0.05f, 0.95f);
}

float Detector::yoloConfidence() const {
    std::lock_guard<std::mutex> lock(mutex);
    return m_yoloConfidence;
}

void Detector::setYoloNmsThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex);
    m_yoloNmsThreshold = std::clamp(threshold, 0.05f, 0.95f);
}

float Detector::yoloNmsThreshold() const {
    std::lock_guard<std::mutex> lock(mutex);
    return m_yoloNmsThreshold;
}

void Detector::setYoloInputSize(int pixels) {
    std::lock_guard<std::mutex> lock(mutex);
    if (pixels <= 0) {
        m_yoloInputSize = m_defaultYoloInputSize;
        return;
    }
    const int clamped = std::clamp(pixels, 320, 1536);
    m_yoloInputSize = std::max(320, (clamped / 32) * 32);
}

int Detector::yoloInputSize() const {
    std::lock_guard<std::mutex> lock(mutex);
    return m_yoloInputSize;
}

void Detector::setAllowedClasses(std::unordered_set<std::string> classes, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    m_allowedClasses = std::move(classes);
    m_classFilterEnabled = enabled;
}

bool Detector::classFilterEnabled() const {
    std::lock_guard<std::mutex> lock(mutex);
    return m_classFilterEnabled;
}

std::vector<DetectionBox> Detector::detectFrame(const DecodedVideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex);

#ifdef Z_HAS_OPENCV_DNN
    if (m_yoloReady) {
        auto yolo = detectYolo(frame);
        // Do not silently substitute contrast blobs for an active YOLO model.
        // It made model inference failures look like unlabelled "region_N"
        // detections, hiding the actual class-label problem from the editor.
        updateTracksLocked(yolo);
        return yolo;
    }
#endif

    std::vector<DetectionBox> results;
    if (frame.width <= 0 || frame.height <= 0 || frame.rgbData.empty()) {
        updateTracksLocked(results);
        return results;
    }

    const int w = frame.width;
    const int h = frame.height;
    const size_t pxCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (frame.rgbData.size() < pxCount * 3) {
        updateTracksLocked(results);
        return results;
    }

    std::vector<uint8_t> gray(pxCount);
    for (size_t i = 0; i < pxCount; ++i) {
        const size_t o = i * 3;
        gray[i] = static_cast<uint8_t>(
            (77 * frame.rgbData[o] + 150 * frame.rgbData[o + 1] + 29 * frame.rgbData[o + 2]) >> 8);
    }

    const int cell = std::max(8, std::min(w, h) / 48);
    const int gw = (w + cell - 1) / cell;
    const int gh = (h + cell - 1) / cell;
    std::vector<uint8_t> active(static_cast<size_t>(gw * gh), 0);

    const bool hasPrev = (prevW == w && prevH == h && prevGray.size() == gray.size());
    const int motionThresh = static_cast<int>(8.0f + (1.0f - m_sensitivity) * 40.0f);
    const int contrastThresh = static_cast<int>(18.0f + (1.0f - m_sensitivity) * 50.0f);

    for (int cy = 0; cy < gh; ++cy) {
        for (int cx = 0; cx < gw; ++cx) {
            int x0 = cx * cell;
            int y0 = cy * cell;
            int x1 = std::min(w, x0 + cell);
            int y1 = std::min(h, y0 + cell);

            int sum = 0;
            int sumSq = 0;
            int motionSum = 0;
            int count = 0;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const int idx = y * w + x;
                    const int g = gray[static_cast<size_t>(idx)];
                    sum += g;
                    sumSq += g * g;
                    if (hasPrev) {
                        motionSum += std::abs(g - static_cast<int>(prevGray[static_cast<size_t>(idx)]));
                    }
                    ++count;
                }
            }
            if (count <= 0) continue;

            const float mean = static_cast<float>(sum) / static_cast<float>(count);
            const float var = static_cast<float>(sumSq) / static_cast<float>(count) - mean * mean;
            const float motion = hasPrev ? (static_cast<float>(motionSum) / static_cast<float>(count)) : 0.0f;

            const bool motionHit = hasPrev && motion >= static_cast<float>(motionThresh);
            const bool contrastHit = var >= static_cast<float>(contrastThresh * contrastThresh);
            // A static high-contrast object should remain detectable when a
            // user adjusts a detection or mask control on the same frame.
            // The former motion-gated condition made a second manual pass
            // erase otherwise valid fallback regions.
            if (motionHit || contrastHit) {
                active[static_cast<size_t>(cy * gw + cx)] = 1;
            }
        }
    }

    prevGray = std::move(gray);
    prevW = w;
    prevH = h;

    // Connected components on the grid
    std::vector<int> labels(static_cast<size_t>(gw * gh), -1);
    int nextLabel = 0;
    struct Comp {
        int minX = 0, minY = 0, maxX = 0, maxY = 0;
        int cells = 0;
        float score = 0.0f;
    };
    std::vector<Comp> comps;

    auto flood = [&](int startX, int startY, int labelId) {
        Comp comp;
        comp.minX = startX;
        comp.maxX = startX;
        comp.minY = startY;
        comp.maxY = startY;
        std::vector<std::pair<int, int>> stack;
        stack.emplace_back(startX, startY);
        labels[static_cast<size_t>(startY * gw + startX)] = labelId;

        while (!stack.empty()) {
            auto [x, y] = stack.back();
            stack.pop_back();
            comp.minX = std::min(comp.minX, x);
            comp.maxX = std::max(comp.maxX, x);
            comp.minY = std::min(comp.minY, y);
            comp.maxY = std::max(comp.maxY, y);
            ++comp.cells;

            const int nbs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : nbs) {
                const int nx = x + d[0];
                const int ny = y + d[1];
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                const size_t ni = static_cast<size_t>(ny * gw + nx);
                if (!active[ni] || labels[ni] >= 0) continue;
                labels[ni] = labelId;
                stack.emplace_back(nx, ny);
            }
        }
        comp.score = static_cast<float>(comp.cells);
        comps.push_back(comp);
    };

    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            const size_t i = static_cast<size_t>(y * gw + x);
            if (active[i] && labels[i] < 0) {
                flood(x, y, nextLabel++);
            }
        }
    }

    const float frameArea = static_cast<float>(w * h);
    int objIndex = 1;
    for (const auto& c : comps) {
        const int px0 = c.minX * cell;
        const int py0 = c.minY * cell;
        const int px1 = std::min(w, (c.maxX + 1) * cell);
        const int py1 = std::min(h, (c.maxY + 1) * cell);
        const float area = static_cast<float>((px1 - px0) * (py1 - py0));
        if (area / frameArea < m_minArea) continue;

        DetectionBox box;
        box.x = static_cast<float>(px0) / static_cast<float>(w);
        box.y = static_cast<float>(py0) / static_cast<float>(h);
        box.w = static_cast<float>(px1 - px0) / static_cast<float>(w);
        box.h = static_cast<float>(py1 - py0) / static_cast<float>(h);
        box.confidence = std::clamp(0.35f + c.score / static_cast<float>(gw * gh) * 2.0f, 0.0f, 0.99f);
        box.label = "region_" + std::to_string(objIndex++);
        
        results.push_back(box);
    }

    std::sort(results.begin(), results.end(), [](const DetectionBox& a, const DetectionBox& b) {
        return (a.w * a.h) > (b.w * b.h);
    });
    if (m_classFilterEnabled) {
        std::erase_if(results, [this](const DetectionBox& box) {
            return !m_allowedClasses.contains(box.label);
        });
    }
    if (results.size() > 12) {
        results.resize(12);
    }
    updateTracksLocked(results);
    return results;
}

void Detector::resetTracksLocked() {
    m_tracks.clear();
    m_nextTrackId = 1;
}

void Detector::updateTracksLocked(std::vector<DetectionBox>& detections) {
    std::vector<bool> trackMatched(m_tracks.size(), false);

    for (auto& detection : detections) {
        const float centerX = detection.x + detection.w * 0.5f;
        const float centerY = detection.y + detection.h * 0.5f;
        int bestTrack = -1;
        float bestScore = -1.0f;

        for (size_t index = 0; index < m_tracks.size(); ++index) {
            const auto& track = m_tracks[index];
            if (trackMatched[index]) continue;
            if (!isGenericRegionLabel(detection.label) && !isGenericRegionLabel(track.label) &&
                detection.label != track.label) {
                continue;
            }

            const float trackCenterX = track.x + track.w * 0.5f;
            const float trackCenterY = track.y + track.h * 0.5f;
            const float centerDistance = std::hypot(centerX - trackCenterX, centerY - trackCenterY);
            const float iou = intersectionOverUnion(detection, track.x, track.y, track.w, track.h);
            const float allowedDistance = std::max(0.12f, 0.75f * std::max(track.w, track.h));
            if (iou < 0.08f && centerDistance > allowedDistance) continue;

            const float score = iou + std::max(0.0f, 1.0f - centerDistance / allowedDistance) * 0.2f;
            if (score > bestScore) {
                bestScore = score;
                bestTrack = static_cast<int>(index);
            }
        }

        if (bestTrack < 0) {
            TrackedObject track;
            track.id = m_nextTrackId++;
            track.label = detection.label;
            track.x = detection.x;
            track.y = detection.y;
            track.w = detection.w;
            track.h = detection.h;
            track.confidence = detection.confidence;
            track.trail.push_back({centerX, centerY});
            m_tracks.push_back(std::move(track));
            bestTrack = static_cast<int>(m_tracks.size() - 1);
            trackMatched.push_back(true);
        } else {
            auto& track = m_tracks[static_cast<size_t>(bestTrack)];
            trackMatched[static_cast<size_t>(bestTrack)] = true;
            track.label = isGenericRegionLabel(detection.label) && !track.label.empty()
                ? track.label : detection.label;
            track.x = detection.x;
            track.y = detection.y;
            track.w = detection.w;
            track.h = detection.h;
            track.confidence = detection.confidence;
            track.missedUpdates = 0;
            if (track.trail.empty() || std::hypot(track.trail.back().x - centerX, track.trail.back().y - centerY) > 0.002f) {
                track.trail.push_back({centerX, centerY});
                if (track.trail.size() > kMaxTrackTrailPoints) {
                    track.trail.erase(track.trail.begin(), track.trail.begin() +
                        static_cast<std::ptrdiff_t>(track.trail.size() - kMaxTrackTrailPoints));
                }
            }
        }

        const auto& matchedTrack = m_tracks[static_cast<size_t>(bestTrack)];
        detection.trackId = matchedTrack.id;
        detection.label = matchedTrack.label.empty() ? detection.label : matchedTrack.label;
        detection.trail = matchedTrack.trail;
    }

    for (size_t index = 0; index < m_tracks.size(); ++index) {
        if (!trackMatched[index]) {
            ++m_tracks[index].missedUpdates;
        }
    }
    std::erase_if(m_tracks, [](const TrackedObject& track) {
        return track.missedUpdates > kMaxMissedTrackUpdates;
    });
}

#ifdef Z_HAS_OPENCV_DNN
std::vector<DetectionBox> Detector::detectYolo(const DecodedVideoFrame& frame) {
    const auto& labels = cocoClassLabels();
    try {
        cv::dnn::Net& net = m_yoloNet;
        bool usedOpenCL = false;
        bool fellBackToCpu = false;
        if (m_preferOpenCL && !m_openClFallbackActive && cv::ocl::haveOpenCL()) {
            cv::ocl::setUseOpenCL(true);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
            usedOpenCL = true;
        } else {
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
        if (frame.width <= 0 || frame.height <= 0 || frame.rgbData.size() <
            static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * 3) {
            m_status = "YOLO: no usable RGB frame";
            return {};
        }

        const int input = m_yoloInputSize;
        cv::Mat rgb(frame.height, frame.width, CV_8UC3, const_cast<uint8_t*>(frame.rgbData.data()));
        const float scale = std::min(
            static_cast<float>(input) / static_cast<float>(frame.width),
            static_cast<float>(input) / static_cast<float>(frame.height));
        const int resizedW = std::max(1, static_cast<int>(std::round(frame.width * scale)));
        const int resizedH = std::max(1, static_cast<int>(std::round(frame.height * scale)));
        const int padX = (input - resizedW) / 2;
        const int padY = (input - resizedH) / 2;
        cv::Mat letterboxed(input, input, CV_8UC3, cv::Scalar(114, 114, 114));
        cv::Mat resized;
        cv::resize(rgb, resized, cv::Size(resizedW, resizedH), 0.0, 0.0, cv::INTER_LINEAR);
        resized.copyTo(letterboxed(cv::Rect(padX, padY, resizedW, resizedH)));

        // DecodedVideoFrame is RGB already. Passing swapRB=true would turn it
        // into BGR and weaken classification for both YOLOv5 and YOLO11 models.
        cv::Mat blob = cv::dnn::blobFromImage(letterboxed, 1.0 / 255.0, cv::Size(), cv::Scalar(), false, false);
        net.setInput(blob);
        cv::Mat out;
        try {
            out = net.forward();
        } catch (const cv::Exception&) {
            if (!usedOpenCL) throw;

            // A model that renders no boxes because an OpenCL driver or layer
            // fails is worse than a slower CPU pass. Remember the fallback so
            // subsequent live detections do not repeatedly throw on the GPU.
            cv::ocl::setUseOpenCL(false);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            net.setInput(blob);
            out = net.forward();
            usedOpenCL = false;
            fellBackToCpu = true;
            m_openClFallbackActive = true;
        }

        if (out.type() != CV_32F) { m_status = "YOLO output is not float data"; return {}; }
        if (!out.isContinuous()) out = out.clone();

        cv::Mat rows;
        if (out.dims == 3) {
            const int first = out.size[1];
            const int second = out.size[2];
            if (first < 5 || second < 1) { m_status = "YOLO output has no classes"; return {}; }

            // YOLO11 commonly returns [1, 84, 8400], while some OpenCV and
            // ONNX variants return [1, 8400, 84]. Normalize both to one
            // candidate per row before reading classes.
            if (first <= second && first <= 256) {
                rows = cv::Mat(first, second, CV_32F, out.ptr<float>()).t();
            } else {
                rows = cv::Mat(first, second, CV_32F, out.ptr<float>()).clone();
            }
        } else if (out.dims == 2 && out.rows > 0 && out.cols >= 5) {
            rows = out;
        } else {
            m_status = "YOLO output format is not supported";
            return {};
        }

        const int attributes = rows.cols;
        if (attributes < 5) { m_status = "YOLO output has no classes"; return {}; }
        const bool hasObjectness = attributes == static_cast<int>(labels.size()) + 5;
        const int classOffset = hasObjectness ? 5 : 4;
        if (attributes <= classOffset) { m_status = "YOLO output has no class scores"; return {}; }

        std::vector<cv::Rect> rects; std::vector<float> scores; std::vector<int> classIds;
        for (int i = 0; i < rows.rows; ++i) {
            const float* d = rows.ptr<float>(i);
            const float objectness = hasObjectness ? d[4] : 1.0f;
            if (objectness < m_yoloConfidence) continue;
            int bestClass = -1; float bestScore = 0.0f;
            for (int c = classOffset; c < attributes; ++c) {
                const float score = d[c];
                if (score > bestScore) { bestScore = score; bestClass = c - classOffset; }
            }
            const float score = objectness * bestScore;
            if (bestClass < 0 || score < m_yoloConfidence) continue;
            const float cx = (d[0] - padX) / scale;
            const float cy = (d[1] - padY) / scale;
            const float bw = d[2] / scale;
            const float bh = d[3] / scale;
            if (bw <= 1.0f || bh <= 1.0f) continue;
            rects.emplace_back(static_cast<int>(cx - bw / 2), static_cast<int>(cy - bh / 2), static_cast<int>(bw), static_cast<int>(bh));
            scores.push_back(score); classIds.push_back(bestClass);
        }
        std::vector<int> kept;
        cv::dnn::NMSBoxesBatched(rects, scores, classIds, m_yoloConfidence, m_yoloNmsThreshold, kept);
        std::vector<DetectionBox> results;
        for (int i : kept) {
            const cv::Rect r = rects[i] & cv::Rect(0, 0, frame.width, frame.height);
            if (r.empty()) continue;
            DetectionBox b; b.x = float(r.x) / frame.width; b.y = float(r.y) / frame.height;
            b.w = float(r.width) / frame.width; b.h = float(r.height) / frame.height; b.confidence = scores[i];
            b.label = classIds[i] < static_cast<int>(labels.size()) ? labels[classIds[i]] : "object_" + std::to_string(classIds[i]);
            if (m_classFilterEnabled && !m_allowedClasses.contains(b.label)) continue;
            if (b.label == "person") b.outline = personOutlineGuide(b);
            results.push_back(b);
        }
        std::sort(results.begin(), results.end(), [](const DetectionBox& a, const DetectionBox& b) {
            return a.confidence > b.confidence;
        });
        if (results.size() > kMaxYoloResults) results.resize(kMaxYoloResults);
        std::ostringstream status;
         status << "YOLO " << (usedOpenCL ? "OpenCL" : "CPU")
             << (fellBackToCpu ? " (OpenCL fallback)" : (m_openClFallbackActive ? " (OpenCL unavailable)" : ""))
               << ": " << results.size() << (results.size() == 1 ? " object" : " objects");
        m_status = status.str();
        return results;
    } catch (const cv::Exception& e) {
        const std::string modelName = lowerAscii(std::filesystem::path(m_modelPath).filename().string());
        m_status = "YOLO inference failed at " + std::to_string(m_yoloInputSize) +
            "px; the model remains loaded. " +
            (modelName.find("x6") != std::string::npos && m_yoloInputSize > 640
                ? "YOLOv5x6 is verified at 640 px with this OpenCV build; select 640 px or Model-safe automatic. "
                : "") +
            "Details: " + compactErrorDetail(e.what());
        return {};
    }
}
#endif

std::vector<uint8_t> Detector::buildMaskFromBoxes(
    int width,
    int height,
    const std::vector<DetectionBox>& boxes,
    float featherPx,
    DetectionShape shape,
    float outlineWidthPx,
    float paddingPx) {
    std::vector<uint8_t> mask(static_cast<size_t>(std::max(0, width) * std::max(0, height)), 0);
    if (width <= 0 || height <= 0 || boxes.empty()) return mask;

    const float feather = std::max(0.0f, featherPx);
    const float padding = std::max(0.0f, paddingPx);
    for (const auto& box : boxes) {
        const int x0 = std::clamp(static_cast<int>(std::floor(box.x * width - padding)), 0, width - 1);
        const int y0 = std::clamp(static_cast<int>(std::floor(box.y * height - padding)), 0, height - 1);
        const int x1 = std::clamp(static_cast<int>(std::ceil((box.x + box.w) * width + padding)), 0, width);
        const int y1 = std::clamp(static_cast<int>(std::ceil((box.y + box.h) * height + padding)), 0, height);
        if (x1 <= x0 || y1 <= y0) continue;
        const float centerX = (x0 + x1 - 1) * 0.5f;
        const float centerY = (y0 + y1 - 1) * 0.5f;
        const float radiusX = std::max(0.5f, (x1 - x0) * 0.5f);
        const float radiusY = std::max(0.5f, (y1 - y0) * 0.5f);
        const bool hasPersonOutline = shape == DetectionShape::PersonOutline && box.outline.size() >= 3;

        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                float alpha = 1.0f;
                if (hasPersonOutline) {
                    const float pointX = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
                    const float pointY = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
                    if (!pointInPolygon(pointX, pointY, box.outline)) continue;
                    if (feather > 0.0f) {
                        float edgeDistance = std::numeric_limits<float>::max();
                        for (size_t current = 0, previous = box.outline.size() - 1;
                             current < box.outline.size(); previous = current++) {
                            edgeDistance = std::min(edgeDistance, distanceToSegmentPixels(
                                static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f,
                                box.outline[previous], box.outline[current], width, height));
                        }
                        alpha = std::clamp(edgeDistance / feather, 0.0f, 1.0f);
                    }
                } else if (shape == DetectionShape::Ellipse) {
                    const float dx = (x - centerX) / radiusX;
                    const float dy = (y - centerY) / radiusY;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance > 1.0f) continue;
                    if (feather > 0.0f) {
                        const float featherNorm = feather / std::min(radiusX, radiusY);
                        alpha = std::clamp((1.0f - distance) / std::max(0.0001f, featherNorm), 0.0f, 1.0f);
                    }
                }
                if (!hasPersonOutline && feather > 0.0f) {
                    const float dx = std::min(static_cast<float>(x - x0), static_cast<float>(x1 - 1 - x));
                    const float dy = std::min(static_cast<float>(y - y0), static_cast<float>(y1 - 1 - y));
                    const float d = std::min(dx, dy);
                    alpha = std::min(alpha, std::clamp(d / feather, 0.0f, 1.0f));
                }
                if (shape == DetectionShape::Outline) {
                    const float edgeDistance = std::min(
                        std::min(static_cast<float>(x - x0), static_cast<float>(x1 - 1 - x)),
                        std::min(static_cast<float>(y - y0), static_cast<float>(y1 - 1 - y)));
                    const float outerFeather = feather > 0.0f ? std::clamp(edgeDistance / feather, 0.0f, 1.0f) : 1.0f;
                    const float innerEdge = std::max(0.0f, edgeDistance - outlineWidthPx);
                    const float innerFeather = feather > 0.0f ? std::clamp(1.0f - innerEdge / feather, 0.0f, 1.0f) : (edgeDistance <= outlineWidthPx ? 1.0f : 0.0f);
                    alpha = std::min(outerFeather, innerFeather);
                }
                const size_t idx = static_cast<size_t>(y * width + x);
                mask[idx] = static_cast<uint8_t>(std::max<int>(mask[idx], static_cast<int>(alpha * 255.0f)));
            }
        }
    }
    return mask;
}
