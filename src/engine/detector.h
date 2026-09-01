#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include "engine/videoframe.h"

#ifdef Z_HAS_OPENCV_DNN
#include <opencv2/dnn.hpp>
#endif

struct DetectionBox {
    std::string label;
    float confidence = 0.0f;
    float x = 0.0f; // normalized 0-1 (left)
    float y = 0.0f; // normalized 0-1 (top)
    float w = 0.0f; // normalized 0-1
    float h = 0.0f; // normalized 0-1
    int trackId = -1;
    struct TrailPoint {
        float x = 0.0f; // normalized centre coordinate
        float y = 0.0f;
    };
    std::vector<TrailPoint> trail;
    // A box-derived guide for people. It is deliberately marked separately
    // from a semantic segmentation contour so callers cannot mistake the two.
    std::vector<TrailPoint> outline;
    bool outlineIsSemantic = false;
};

// CPU-defined geometry used by the detector overlay and generated mask.
// It deliberately lives outside the GLSL plugin system.
enum class DetectionShape {
    Rectangle,
    Ellipse,
    Outline,
    PersonOutline
};

// Lightweight CPU region detector (motion / contrast blobs).
// Pluggable foundation — replace detectFrame body later with ONNX YOLO/etc.
class Detector {
public:
    static Detector& instance();

    Detector() = default;

    static const std::vector<std::string>& cocoClassLabels();

    void setSensitivity(float sensitivity); // 0-1
    float sensitivity() const;

    void setMinArea(float minAreaNorm); // fraction of frame area, e.g. 0.01
    float minArea() const;

    void reset();

    // YOLO is loaded lazily from an ONNX model. The recommended YOLOv5x6 COCO
    // export is verified at a 640px automatic compatibility input with this
    // OpenCV DNN backend; unsupported graphs return an actionable load error
    // and detectFrame falls back to motion regions.
    bool setYoloModel(const std::string& modelPath, std::string* error = nullptr);
    bool yoloReady() const;
    std::string status() const;
    void setPreferOpenCL(bool enabled);
    void setYoloConfidence(float threshold); // 0-1
    float yoloConfidence() const;
    void setYoloNmsThreshold(float threshold); // 0-1
    float yoloNmsThreshold() const;
    // 0 restores the automatic compatibility input size. Valid explicit sizes
    // are rounded down to a YOLO-compatible multiple of 32.
    void setYoloInputSize(int pixels);
    int yoloInputSize() const;
    void setAllowedClasses(std::unordered_set<std::string> classes, bool enabled);
    bool classFilterEnabled() const;

    std::vector<DetectionBox> detectFrame(const DecodedVideoFrame& frame);

    // Build a single-channel (R) mask image from boxes: 255 inside, 0 outside.
    static std::vector<uint8_t> buildMaskFromBoxes(
        int width,
        int height,
        const std::vector<DetectionBox>& boxes,
        float featherPx = 4.0f,
        DetectionShape shape = DetectionShape::Rectangle,
        float outlineWidthPx = 6.0f,
        float paddingPx = 0.0f);

private:
    mutable std::mutex mutex;
    float m_sensitivity = 0.45f;
    float m_minArea = 0.012f;

    int prevW = 0;
    int prevH = 0;
    std::vector<uint8_t> prevGray;
    std::string m_modelPath;
    std::string m_status = "Motion region fallback";
    bool m_preferOpenCL = true;
    bool m_openClFallbackActive = false;
    bool m_yoloReady = false;
    float m_yoloConfidence = 0.25f;
    float m_yoloNmsThreshold = 0.45f;
    int m_defaultYoloInputSize = 640;
    int m_yoloInputSize = 640;
    bool m_classFilterEnabled = false;
    std::unordered_set<std::string> m_allowedClasses;

    struct TrackedObject {
        int id = -1;
        std::string label;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float confidence = 0.0f;
        int missedUpdates = 0;
        std::vector<DetectionBox::TrailPoint> trail;
    };
    std::vector<TrackedObject> m_tracks;
    int m_nextTrackId = 1;

    void updateTracksLocked(std::vector<DetectionBox>& detections);
    void resetTracksLocked();

#ifdef Z_HAS_OPENCV_DNN
    std::vector<DetectionBox> detectYolo(const DecodedVideoFrame& frame);
    cv::dnn::Net m_yoloNet;
#endif
};
