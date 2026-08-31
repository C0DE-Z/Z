#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
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
};

// CPU-defined geometry used by the detector overlay and generated mask.
// It deliberately lives outside the GLSL plugin system.
enum class DetectionShape {
    Rectangle,
    Ellipse,
    Outline
};

// Lightweight CPU region detector (motion / contrast blobs).
// Pluggable foundation — replace detectFrame body later with ONNX YOLO/etc.
class Detector {
public:
    static Detector& instance();

    void setSensitivity(float sensitivity); // 0-1
    float sensitivity() const;

    void setMinArea(float minAreaNorm); // fraction of frame area, e.g. 0.01
    float minArea() const;

    void reset();

    // YOLO is loaded lazily from an ONNX model. If OpenCV DNN or a model is not
    // available, detectFrame transparently falls back to motion regions.
    bool setYoloModel(const std::string& modelPath, std::string* error = nullptr);
    bool yoloReady() const;
    std::string status() const;
    void setPreferOpenCL(bool enabled);

    std::vector<DetectionBox> detectFrame(const DecodedVideoFrame& frame);

    // Build a single-channel (R) mask image from boxes: 255 inside, 0 outside.
    static std::vector<uint8_t> buildMaskFromBoxes(
        int width,
        int height,
        const std::vector<DetectionBox>& boxes,
        float featherPx = 4.0f,
        DetectionShape shape = DetectionShape::Rectangle,
        float outlineWidthPx = 6.0f);

private:
    Detector() = default;

    mutable std::mutex mutex;
    float m_sensitivity = 0.45f;
    float m_minArea = 0.012f;

    int prevW = 0;
    int prevH = 0;
    std::vector<uint8_t> prevGray;
    std::string m_modelPath;
    std::string m_status = "Motion region fallback";
    bool m_preferOpenCL = true;
    bool m_yoloReady = false;

#ifdef Z_HAS_OPENCV_DNN
    std::vector<DetectionBox> detectYolo(const DecodedVideoFrame& frame);
    cv::dnn::Net m_yoloNet;
#endif
};
