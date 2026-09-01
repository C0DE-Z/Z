#include "engine/detector.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef Z_HAS_OPENCV_DNN
#include <QImage>
#include <QString>
#endif

namespace {

bool expect(bool condition, const std::string& message) {
    if (condition) return true;
    std::cerr << "[FAIL] " << message << '\n';
    return false;
}

size_t coveredPixels(const std::vector<uint8_t>& mask) {
    return static_cast<size_t>(std::count_if(mask.begin(), mask.end(), [](uint8_t value) {
        return value != 0;
    }));
}

DecodedVideoFrame makePatternFrame(int leftCell, bool inverted) {
    constexpr int kWidth = 128;
    constexpr int kHeight = 128;
    constexpr int kCellSize = 8;

    DecodedVideoFrame frame;
    frame.width = kWidth;
    frame.height = kHeight;
    frame.rgbData.assign(static_cast<size_t>(kWidth) * kHeight * 3, 0);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int cellX = x / kCellSize;
            const int cellY = y / kCellSize;
            if (cellX < leftCell || cellX > leftCell + 1 || cellY < 6 || cellY > 7) continue;

            const bool bright = ((x + y + (inverted ? 1 : 0)) % 2) == 0;
            const uint8_t value = bright ? 255 : 0;
            const size_t index = (static_cast<size_t>(y) * kWidth + x) * 3;
            frame.rgbData[index] = value;
            frame.rgbData[index + 1] = value;
            frame.rgbData[index + 2] = value;
        }
    }
    return frame;
}

bool testMaskGeometry() {
    const DetectionBox box{.label = "person", .confidence = 0.9f, .x = 0.25f, .y = 0.25f, .w = 0.25f, .h = 0.25f};
    const std::vector<DetectionBox> boxes{box};

    const auto rectangle = Detector::buildMaskFromBoxes(64, 64, boxes, 0.0f, DetectionShape::Rectangle);
    const auto padded = Detector::buildMaskFromBoxes(64, 64, boxes, 0.0f, DetectionShape::Rectangle, 6.0f, 5.0f);
    if (!expect(rectangle.size() == 64u * 64u, "rectangle mask should match frame dimensions")) return false;
    if (!expect(coveredPixels(padded) > coveredPixels(rectangle), "padding should expand mask coverage")) return false;

    const auto ellipse = Detector::buildMaskFromBoxes(64, 64, boxes, 0.0f, DetectionShape::Ellipse);
    if (!expect(ellipse[24u * 64u + 24u] == 255, "ellipse mask should cover its centre")) return false;
    if (!expect(ellipse[16u * 64u + 16u] == 0, "ellipse mask should not cover the rectangle corner")) return false;

    const auto outline = Detector::buildMaskFromBoxes(64, 64, boxes, 0.0f, DetectionShape::Outline, 3.0f);
    if (!expect(outline[16u * 64u + 16u] == 255, "outline mask should cover the detection boundary")) return false;
    if (!expect(outline[24u * 64u + 24u] == 0, "outline mask should leave the detection centre clear")) return false;

    std::cout << "[PASS] testMaskGeometry\n";
    return true;
}

bool testFallbackTracksAndTrails() {
    Detector& detector = Detector::instance();
    detector.reset();
    detector.setSensitivity(0.45f);
    detector.setMinArea(0.01f);

    auto detections = detector.detectFrame(makePatternFrame(1, false));
    if (!expect(detections.size() == 1, "initial high-contrast region should be detected")) return false;
    const int trackId = detections.front().trackId;
    if (!expect(trackId > 0, "detected region should receive a track ID")) return false;

    int cell = 1;
    int direction = 1;
    for (int update = 0; update < 320; ++update) {
        cell += direction;
        if (cell >= 13 || cell <= 1) direction = -direction;
        detections = detector.detectFrame(makePatternFrame(cell, (update % 2) != 0));
        if (!expect(detections.size() == 1, "moving high-contrast region should remain a single detection")) return false;
        if (!expect(detections.front().trackId == trackId, "moving region should retain its track ID")) return false;
    }

    if (!expect(detections.front().trail.size() == 120, "trail history should be capped at 120 points")) return false;
    std::cout << "[PASS] testFallbackTracksAndTrails\n";
    return true;
}

#ifdef Z_HAS_OPENCV_DNN
DecodedVideoFrame loadRgbImage(const char* imagePath) {
    QImage image(QString::fromLocal8Bit(imagePath));
    if (image.isNull()) return {};

    image = image.convertToFormat(QImage::Format_RGB888);
    DecodedVideoFrame frame;
    frame.width = image.width();
    frame.height = image.height();
    frame.rgbData.resize(static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * 3);
    for (int y = 0; y < frame.height; ++y) {
        const auto* source = image.constScanLine(y);
        auto* destination = frame.rgbData.data() + static_cast<size_t>(y) * static_cast<size_t>(frame.width) * 3;
        std::copy_n(source, static_cast<size_t>(frame.width) * 3, destination);
    }
    return frame;
}

bool testMissingYoloModelReportsActionableError() {
    Detector& detector = Detector::instance();
    detector.reset();

    std::string error;
    const bool loaded = detector.setYoloModel("missing-z-detector-model.onnx", &error);
    if (!expect(!loaded, "missing YOLO model should not load")) return false;
    if (!expect(error.find("not found") != std::string::npos,
            "missing YOLO model should report a file-not-found error, got: " + error)) {
        return false;
    }

    std::cout << "[PASS] testMissingYoloModelReportsActionableError\n";
    return true;
}

bool testOptionalYoloModel(
    const char* modelPath,
    const char* imagePath = nullptr,
    const char* expectedLabel = nullptr,
    int expectedAutomaticInputSize = 0,
    int inferenceInputSize = 0,
    int recoveryInputSize = 0) {
    Detector& detector = Detector::instance();
    detector.reset();
    detector.setPreferOpenCL(false);

    std::string error;
    const bool loaded = detector.setYoloModel(modelPath, &error);
    if (!expect(loaded, "YOLO model should load: " + error)) return false;
    const int automaticInputSize = detector.yoloInputSize();
    if (expectedAutomaticInputSize > 0 &&
        !expect(automaticInputSize == expectedAutomaticInputSize,
            "YOLO automatic compatibility input should be " + std::to_string(expectedAutomaticInputSize) +
                ", got " + std::to_string(automaticInputSize))) {
        return false;
    }
    std::cout << "[INFO] YOLO automatic compatibility input: " << automaticInputSize << '\n';
    detector.setYoloInputSize(640);
    if (!expect(detector.yoloInputSize() == 640, "explicit YOLO input size should be applied")) return false;
    detector.setYoloInputSize(0);
        if (!expect(detector.yoloInputSize() == automaticInputSize,
            "automatic YOLO compatibility input should be restored after an explicit override")) {
        return false;
    }
    if (inferenceInputSize > 0) {
        detector.setYoloInputSize(inferenceInputSize);
        if (!expect(detector.yoloInputSize() == inferenceInputSize,
                "explicit YOLO inference input size should be applied")) {
            return false;
        }
    }
    std::cout << "[INFO] YOLO inference input: " << detector.yoloInputSize() << '\n';
    detector.detectFrame(makePatternFrame(1, false));
    if (recoveryInputSize > 0) {
        if (!expect(detector.yoloReady(),
                "YOLO model should remain loaded after a recoverable inference failure")) {
            return false;
        }
        if (!expect(detector.status().rfind("YOLO inference failed at", 0) == 0,
                "advanced-resolution probe should report an actionable inference failure, got: " + detector.status())) {
            return false;
        }
        detector.setYoloInputSize(recoveryInputSize);
        std::cout << "[INFO] YOLO recovery input: " << detector.yoloInputSize() << '\n';
        detector.detectFrame(makePatternFrame(1, false));
        if (!expect(detector.status().rfind("YOLO CPU:", 0) == 0,
                "YOLO model should recover at the safe input size, got: " + detector.status())) {
            return false;
        }
    } else if (!expect(detector.status().rfind("YOLO CPU:", 0) == 0,
                   "YOLO model should complete a CPU inference pass, got: " + detector.status())) {
        return false;
    }

    detector.setPreferOpenCL(true);
    detector.detectFrame(makePatternFrame(1, false));
    const std::string automaticBackendStatus = detector.status();
    if (!expect(detector.yoloReady(),
            "YOLO should remain usable when OpenCL is preferred and CPU fallback is needed")) {
        return false;
    }
    if (!expect(automaticBackendStatus.rfind("YOLO ", 0) == 0,
            "automatic OpenCL or CPU fallback inference should complete, got: " + automaticBackendStatus)) {
        return false;
    }
    std::cout << "[INFO] YOLO automatic backend: " << automaticBackendStatus << '\n';
    detector.setPreferOpenCL(false);

    if (imagePath) {
        const DecodedVideoFrame referenceFrame = loadRgbImage(imagePath);
        if (!expect(!referenceFrame.rgbData.empty(), "reference image should decode: " + std::string(imagePath))) return false;

        const auto detections = detector.detectFrame(referenceFrame);
        if (!expect(!detections.empty(), "reference image should produce named YOLO detections")) return false;
        const bool hasNamedDetection = std::any_of(detections.begin(), detections.end(), [](const DetectionBox& box) {
            return !box.label.empty() && box.label.rfind("region_", 0) != 0 && box.label.rfind("object_", 0) != 0;
        });
        if (!expect(hasNamedDetection, "YOLO detections should expose class names")) return false;
        if (expectedLabel) {
            const bool foundExpectedLabel = std::any_of(detections.begin(), detections.end(), [expectedLabel](const DetectionBox& box) {
                return box.label == expectedLabel;
            });
            if (!expect(foundExpectedLabel, "reference image should contain the expected label: " + std::string(expectedLabel))) return false;
        }

        std::cout << "[INFO] YOLO labels:";
        for (const auto& box : detections) std::cout << ' ' << box.label;
        std::cout << '\n';
    }

    std::cout << "[PASS] testOptionalYoloModel\n";
    return true;
}
#endif

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "=== Running Detector Unit Tests ===\n";
    const bool passed = testMaskGeometry() && testFallbackTracksAndTrails();
#ifdef Z_HAS_OPENCV_DNN
    if (!passed) return 1;
    if (!testMissingYoloModelReportsActionableError()) return 1;
    // Optional arguments: model, image, expected label, expected automatic
    // input, explicit probe input, then recovery input after a failed probe.
    if (argc > 1 && !testOptionalYoloModel(argv[1], argc > 2 ? argv[2] : nullptr,
                                            argc > 3 ? argv[3] : nullptr,
                                            argc > 4 ? std::atoi(argv[4]) : 0,
                                            argc > 5 ? std::atoi(argv[5]) : 0,
                                            argc > 6 ? std::atoi(argv[6]) : 0)) return 1;
#else
    (void)argc;
    (void)argv;
#endif
    if (!passed) return 1;
    std::cout << "=== All Detector Tests Passed ===\n";
    return 0;
}
