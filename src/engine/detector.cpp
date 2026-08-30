#include "detector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#ifdef Z_HAS_OPENCV_DNN
#include <opencv2/core/ocl.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#endif

Detector& Detector::instance() {
    static Detector inst;
    return inst;
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
}

bool Detector::setYoloModel(const std::string& modelPath, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex);
    m_modelPath = modelPath;
    m_yoloReady = false;
#ifdef Z_HAS_OPENCV_DNN
    try {
        m_yoloNet = cv::dnn::readNetFromONNX(modelPath);
        m_yoloReady = true;
        m_status = "YOLO ONNX ready";
        return true;
    } catch (const cv::Exception& e) {
        m_status = std::string("YOLO unavailable: ") + e.what();
    }
#else
    m_status = "YOLO unavailable: build Z with OpenCV DNN support";
#endif
    if (error) *error = m_status;
    return false;
}

bool Detector::yoloReady() const { std::lock_guard<std::mutex> lock(mutex); return m_yoloReady; }
std::string Detector::status() const { std::lock_guard<std::mutex> lock(mutex); return m_status; }
void Detector::setPreferOpenCL(bool enabled) { std::lock_guard<std::mutex> lock(mutex); m_preferOpenCL = enabled; }

std::vector<DetectionBox> Detector::detectFrame(const DecodedVideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex);

#ifdef Z_HAS_OPENCV_DNN
    if (m_yoloReady) {
        auto yolo = detectYolo(frame);
        if (!yolo.empty()) return yolo;
    }
#endif

    std::vector<DetectionBox> results;
    if (frame.width <= 0 || frame.height <= 0 || frame.rgbData.empty()) {
        return results;
    }

    const int w = frame.width;
    const int h = frame.height;
    const size_t pxCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (frame.rgbData.size() < pxCount * 3) {
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
            if (motionHit || (!hasPrev && contrastHit) || (contrastHit && motion > motionThresh * 0.35f)) {
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
    if (results.size() > 12) {
        results.resize(12);
    }
    return results;
}

#ifdef Z_HAS_OPENCV_DNN
std::vector<DetectionBox> Detector::detectYolo(const DecodedVideoFrame& frame) {
    static const std::vector<std::string> labels = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow"
    };
    try {
        cv::dnn::Net& net = m_yoloNet;
        if (m_preferOpenCL && cv::ocl::haveOpenCL()) {
            cv::ocl::setUseOpenCL(true);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
        } else {
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
        const int input = 640;
        cv::Mat rgb(frame.height, frame.width, CV_8UC3, const_cast<uint8_t*>(frame.rgbData.data()));
        cv::Mat blob = cv::dnn::blobFromImage(rgb, 1.0 / 255.0, cv::Size(input, input), cv::Scalar(), true, false);
        net.setInput(blob);
        cv::Mat out = net.forward();
        if (out.dims != 3) { m_status = "YOLO output format is not supported"; return {}; }
        const int attributes = out.size[1];
        const int candidates = out.size[2];
        if (attributes < 5) { m_status = "YOLO output has no classes"; return {}; }
        std::vector<cv::Rect> rects; std::vector<float> scores; std::vector<int> classIds;
        const float sx = static_cast<float>(frame.width) / input, sy = static_cast<float>(frame.height) / input;
        for (int i = 0; i < candidates; ++i) {
            const float* d = out.ptr<float>() + i;
            int bestClass = -1; float bestScore = 0.0f;
            for (int c = 4; c < attributes; ++c) { float score = out.ptr<float>(0, c)[i]; if (score > bestScore) { bestScore = score; bestClass = c - 4; } }
            if (bestScore < 0.35f) continue;
            const float cx = out.ptr<float>(0, 0)[i] * sx, cy = out.ptr<float>(0, 1)[i] * sy;
            const float bw = out.ptr<float>(0, 2)[i] * sx, bh = out.ptr<float>(0, 3)[i] * sy;
            rects.emplace_back(static_cast<int>(cx - bw / 2), static_cast<int>(cy - bh / 2), static_cast<int>(bw), static_cast<int>(bh));
            scores.push_back(bestScore); classIds.push_back(bestClass);
        }
        std::vector<int> kept; cv::dnn::NMSBoxes(rects, scores, 0.35f, 0.45f, kept);
        std::vector<DetectionBox> results;
        for (int i : kept) {
            const cv::Rect r = rects[i] & cv::Rect(0, 0, frame.width, frame.height);
            if (r.empty()) continue;
            DetectionBox b; b.x = float(r.x) / frame.width; b.y = float(r.y) / frame.height;
            b.w = float(r.width) / frame.width; b.h = float(r.height) / frame.height; b.confidence = scores[i];
            b.label = classIds[i] < static_cast<int>(labels.size()) ? labels[classIds[i]] : "object_" + std::to_string(classIds[i]);
            results.push_back(b);
        }
        m_status = m_preferOpenCL && cv::ocl::useOpenCL() ? "YOLO · OpenCL" : "YOLO · CPU";
        return results;
    } catch (const cv::Exception& e) { m_status = std::string("YOLO inference failed: ") + e.what(); return {}; }
}
#endif

std::vector<uint8_t> Detector::buildMaskFromBoxes(
    int width,
    int height,
    const std::vector<DetectionBox>& boxes,
    float featherPx) {
    std::vector<uint8_t> mask(static_cast<size_t>(std::max(0, width) * std::max(0, height)), 0);
    if (width <= 0 || height <= 0 || boxes.empty()) return mask;

    const float feather = std::max(0.0f, featherPx);
    for (const auto& box : boxes) {
        const int x0 = std::clamp(static_cast<int>(box.x * width), 0, width - 1);
        const int y0 = std::clamp(static_cast<int>(box.y * height), 0, height - 1);
        const int x1 = std::clamp(static_cast<int>((box.x + box.w) * width), 0, width);
        const int y1 = std::clamp(static_cast<int>((box.y + box.h) * height), 0, height);

        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                float alpha = 1.0f;
                if (feather > 0.0f) {
                    const float dx = std::min(static_cast<float>(x - x0), static_cast<float>(x1 - 1 - x));
                    const float dy = std::min(static_cast<float>(y - y0), static_cast<float>(y1 - 1 - y));
                    const float d = std::min(dx, dy);
                    alpha = std::clamp(d / feather, 0.0f, 1.0f);
                }
                const size_t idx = static_cast<size_t>(y * width + x);
                mask[idx] = static_cast<uint8_t>(std::max<int>(mask[idx], static_cast<int>(alpha * 255.0f)));
            }
        }
    }
    return mask;
}
