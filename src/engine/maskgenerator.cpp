#include "maskgenerator.h"
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <cmath>
#include <algorithm>
#ifdef Z_HAS_OPENCV_DNN
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

std::vector<uint8_t> MaskGenerator::renderMask(
    int width,
    int height,
    const std::vector<ClipMask>& masks,
    double clipLocalTime,
    const std::string& targetEffectId,
    bool targetSourceClip
) {
    if (width <= 0 || height <= 0 || masks.empty()) return {};

    std::vector<const ClipMask*> activeMasks;
    for (const auto& m : masks) {
        if (!m.enabled) continue;
        if (targetSourceClip) {
            if (!m.maskSourceClip) continue;
        } else {
            if (m.maskSourceClip) continue;
        }
        if (!targetSourceClip && !targetEffectId.empty()) {
            if (!m.targetEffectIds.empty() && 
                std::find(m.targetEffectIds.begin(), m.targetEffectIds.end(), targetEffectId) == m.targetEffectIds.end()) {
                continue;
            }
        }
        activeMasks.push_back(&m);
    }

    if (activeMasks.empty()) return {};

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> composite(pixelCount, 0);
    bool firstMask = true;

    for (const auto* mask : activeMasks) {
        double px = mask->evalProp(mask->posXCurve, mask->posX, clipLocalTime);
        double py = mask->evalProp(mask->posYCurve, mask->posY, clipLocalTime);
        double sx = mask->evalProp(mask->scaleXCurve, mask->scaleX, clipLocalTime);
        double sy = mask->evalProp(mask->scaleYCurve, mask->scaleY, clipLocalTime);
        double rot = mask->evalProp(mask->rotationCurve, mask->rotation, clipLocalTime);
        double feather = std::max(0.0, mask->evalProp(mask->featherCurve, mask->feather, clipLocalTime));
        double opacity = std::clamp(mask->evalProp(mask->opacityCurve, mask->opacity, clipLocalTime), 0.0, 1.0);
        double expPx = mask->evalProp(mask->expansionCurve, mask->expansion, clipLocalTime);

        QImage layer(width, height, QImage::Format_Grayscale8);
        layer.fill(0);

        {
            QPainter painter(&layer);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255));

            float centerX = static_cast<float>(px * width);
            float centerY = static_cast<float>(py * height);

            QTransform transform;
            transform.translate(centerX, centerY);
            transform.rotate(rot);
            transform.scale(sx, sy);
            painter.setTransform(transform);

            QPainterPath path;

            if (mask->shapeType == MaskShapeType::Rectangle) {
                float halfW = 0.25f * width;
                float halfH = 0.25f * height;
                path.addRect(-halfW, -halfH, halfW * 2.0f, halfH * 2.0f);
            } else if (mask->shapeType == MaskShapeType::Ellipse) {
                float rx = 0.25f * width;
                float ry = 0.25f * height;
                path.addEllipse(QPointF(0.0, 0.0), rx, ry);
            } else if (mask->shapeType == MaskShapeType::Polygon || mask->shapeType == MaskShapeType::Bezier) {
                if (!mask->points.empty()) {
                    path.moveTo((mask->points[0].x - 0.5f) * width, (mask->points[0].y - 0.5f) * height);
                    for (size_t i = 1; i < mask->points.size(); ++i) {
                        const auto& prev = mask->points[i - 1];
                        const auto& curr = mask->points[i];
                        if (mask->shapeType == MaskShapeType::Bezier &&
                            (prev.outHandleX != 0 || prev.outHandleY != 0 || curr.inHandleX != 0 || curr.inHandleY != 0)) {
                            QPointF c1((prev.x - 0.5f + prev.outHandleX) * width, (prev.y - 0.5f + prev.outHandleY) * height);
                            QPointF c2((curr.x - 0.5f + curr.inHandleX) * width, (curr.y - 0.5f + curr.inHandleY) * height);
                            QPointF end((curr.x - 0.5f) * width, (curr.y - 0.5f) * height);
                            path.cubicTo(c1, c2, end);
                        } else {
                            path.lineTo((curr.x - 0.5f) * width, (curr.y - 0.5f) * height);
                        }
                    }
                    if (mask->closed && mask->points.size() > 2) {
                        const auto& prev = mask->points.back();
                        const auto& curr = mask->points.front();
                        if (mask->shapeType == MaskShapeType::Bezier &&
                            (prev.outHandleX != 0 || prev.outHandleY != 0 || curr.inHandleX != 0 || curr.inHandleY != 0)) {
                            QPointF c1((prev.x - 0.5f + prev.outHandleX) * width, (prev.y - 0.5f + prev.outHandleY) * height);
                            QPointF c2((curr.x - 0.5f + curr.inHandleX) * width, (curr.y - 0.5f + curr.inHandleY) * height);
                            QPointF end((curr.x - 0.5f) * width, (curr.y - 0.5f) * height);
                            path.cubicTo(c1, c2, end);
                        } else {
                            path.closeSubpath();
                        }
                    }
                }
            }

            path = transform.map(path);
            if (std::abs(expPx) > 0.01) {
                QPainterPathStroker stroker;
                stroker.setWidth(std::abs(expPx) * 2.0);
                const QPainterPath border = stroker.createStroke(path);
                path = expPx > 0.0 ? path.united(border) : path.subtracted(border);
            }
            painter.resetTransform();
            painter.drawPath(path);
        }

        // QImage scanlines are padded to a 4-byte boundary for odd widths.
        // Copy rows into packed storage before applying full-frame operations;
        // treating constBits() as width*height contiguous bytes corrupts masks.
        std::vector<uint8_t> layerData(pixelCount);
        for (int y = 0; y < height; ++y) {
            std::copy_n(layer.constScanLine(y), width,
                layerData.begin() + static_cast<size_t>(y) * static_cast<size_t>(width));
        }

        if (feather > 0.5) {
            applyFeather(layerData.data(), width, height, static_cast<float>(feather));
        }

        if (mask->inverted) {
            for (auto& value : layerData) value = static_cast<uint8_t>(255 - value);
        }

        if (opacity < 0.999) {
            const float op = static_cast<float>(opacity);
            for (auto& value : layerData) {
                value = static_cast<uint8_t>(value * op);
            }
        }

        // Use the identity element for the first non-additive operation.
        // In particular, Intersect must start from white rather than zero.
        if (firstMask) {
            if (mask->mode != MaskMode::Add) {
                std::fill(composite.begin(), composite.end(), 255);
            }
            firstMask = false;
        }

        if (mask->mode == MaskMode::Add) {
            for (size_t i = 0; i < pixelCount; ++i) {
                composite[i] = static_cast<uint8_t>(std::min(255, composite[i] + layerData[i]));
            }
        } else if (mask->mode == MaskMode::Subtract) {
            for (size_t i = 0; i < pixelCount; ++i) {
                composite[i] = static_cast<uint8_t>(std::max(0, composite[i] - layerData[i]));
            }
        } else if (mask->mode == MaskMode::Intersect) {
            for (size_t i = 0; i < pixelCount; ++i) {
                composite[i] = static_cast<uint8_t>((static_cast<int>(composite[i]) * layerData[i]) / 255);
            }
        }
    }

    return composite;
}

ClipMask MaskGenerator::createRectangleMask(const std::string& id, const std::string& name) {
    ClipMask m;
    m.id = id;
    m.name = name;
    m.shapeType = MaskShapeType::Rectangle;
    m.posX = 0.5;
    m.posY = 0.5;
    m.scaleX = 1.0;
    m.scaleY = 1.0;
    m.feather = 6.0;
    m.opacity = 1.0;
    m.closed = true;

    // 4 corners normalized
    m.points = {
        { 0.25f, 0.25f, 0, 0, 0, 0 },
        { 0.75f, 0.25f, 0, 0, 0, 0 },
        { 0.75f, 0.75f, 0, 0, 0, 0 },
        { 0.25f, 0.75f, 0, 0, 0, 0 }
    };
    return m;
}

ClipMask MaskGenerator::createEllipseMask(const std::string& id, const std::string& name) {
    ClipMask m;
    m.id = id;
    m.name = name;
    m.shapeType = MaskShapeType::Ellipse;
    m.posX = 0.5;
    m.posY = 0.5;
    m.scaleX = 1.0;
    m.scaleY = 1.0;
    m.feather = 8.0;
    m.opacity = 1.0;
    m.closed = true;
    return m;
}

ClipMask MaskGenerator::createPolygonMask(const std::string& id, const std::vector<MaskPoint>& points, const std::string& name) {
    ClipMask m;
    m.id = id;
    m.name = name;
    m.shapeType = MaskShapeType::Polygon;
    m.posX = 0.5;
    m.posY = 0.5;
    m.scaleX = 1.0;
    m.scaleY = 1.0;
    m.feather = 4.0;
    m.opacity = 1.0;
    m.points = points;
    m.closed = true;
    return m;
}

std::vector<MaskPoint> MaskGenerator::traceObjectOutline(
    const DecodedVideoFrame& frame,
    const DetectionBox& box,
    bool* usedComputerVision) {
    if (usedComputerVision) *usedComputerVision = false;
#ifdef Z_HAS_OPENCV_DNN
    const size_t expectedRgbBytes = static_cast<size_t>(std::max(0, frame.width)) *
        static_cast<size_t>(std::max(0, frame.height)) * 3;
    if (frame.width > 2 && frame.height > 2 && frame.rgbData.size() == expectedRgbBytes &&
        box.w > 0.0f && box.h > 0.0f) {
        try {
            cv::Mat image(frame.height, frame.width, CV_8UC3,
                const_cast<uint8_t*>(frame.rgbData.data()), static_cast<size_t>(frame.width) * 3);
            const int x0 = std::clamp(static_cast<int>(std::floor(box.x * frame.width)), 0, frame.width - 1);
            const int y0 = std::clamp(static_cast<int>(std::floor(box.y * frame.height)), 0, frame.height - 1);
            const int x1 = std::clamp(static_cast<int>(std::ceil((box.x + box.w) * frame.width)), x0 + 1, frame.width);
            const int y1 = std::clamp(static_cast<int>(std::ceil((box.y + box.h) * frame.height)), y0 + 1, frame.height);
            const cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
            if (roi.width > 2 && roi.height > 2) {
                cv::Mat labels(frame.height, frame.width, CV_8UC1, cv::Scalar(cv::GC_BGD));
                cv::Mat backgroundModel, foregroundModel;
                cv::grabCut(image, labels, roi, backgroundModel, foregroundModel, 3, cv::GC_INIT_WITH_RECT);
                cv::Mat foreground = (labels == cv::GC_FGD) | (labels == cv::GC_PR_FGD);
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(foreground, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
                if (!contours.empty()) {
                    const auto largest = std::max_element(contours.begin(), contours.end(), [](const auto& a, const auto& b) {
                        return cv::contourArea(a) < cv::contourArea(b);
                    });
                    if (cv::contourArea(*largest) >= std::max(4.0, roi.area() * 0.01)) {
                        std::vector<cv::Point> simplified;
                        cv::approxPolyDP(*largest, simplified,
                            std::max(1.0, cv::arcLength(*largest, true) * 0.01), true);
                        if (simplified.size() >= 3) {
                            constexpr size_t maxPoints = 128;
                            std::vector<MaskPoint> points;
                            const size_t stride = std::max<size_t>(1, (simplified.size() + maxPoints - 1) / maxPoints);
                            for (size_t i = 0; i < simplified.size(); i += stride) {
                                points.push_back({
                                    std::clamp(static_cast<float>(simplified[i].x) / frame.width, 0.0f, 1.0f),
                                    std::clamp(static_cast<float>(simplified[i].y) / frame.height, 0.0f, 1.0f),
                                    0, 0, 0, 0
                                });
                            }
                            if (points.size() >= 3) {
                                if (usedComputerVision) *usedComputerVision = true;
                                return points;
                            }
                        }
                    }
                }
            }
        } catch (const cv::Exception&) {
            // Segmentation may fail on low-texture or degenerate selections;
            // the editable detector geometry below is the deliberate fallback.
        }
    }
#else
    Q_UNUSED(frame);
#endif

    std::vector<MaskPoint> fallback;
    if (box.outline.size() >= 3) {
        fallback.reserve(box.outline.size());
        for (const auto& point : box.outline) {
            fallback.push_back({point.x, point.y, 0, 0, 0, 0});
        }
        return fallback;
    }
    const float x0 = std::clamp(box.x, 0.0f, 1.0f);
    const float y0 = std::clamp(box.y, 0.0f, 1.0f);
    const float x1 = std::clamp(box.x + box.w, 0.0f, 1.0f);
    const float y1 = std::clamp(box.y + box.h, 0.0f, 1.0f);
    return {{x0, y0, 0, 0, 0, 0}, {x1, y0, 0, 0, 0, 0},
            {x1, y1, 0, 0, 0, 0}, {x0, y1, 0, 0, 0, 0}};
}

void MaskGenerator::applyFeather(uint8_t* data, int width, int height, float featherPx) {
    int radius = static_cast<int>(std::round(featherPx));
    if (radius <= 0 || width <= 0 || height <= 0) return;
    radius = std::min(radius, std::min(width, height) / 2);

    std::vector<uint8_t> temp(width * height);

    // Horizontal pass
    for (int y = 0; y < height; ++y) {
        int rowStart = y * width;
        int sum = 0;
        int count = 0;

        for (int k = -radius; k <= radius; ++k) {
            int x = std::clamp(k, 0, width - 1);
            sum += data[rowStart + x];
            count++;
        }

        for (int x = 0; x < width; ++x) {
            temp[rowStart + x] = static_cast<uint8_t>(sum / count);
            int xRemove = std::clamp(x - radius, 0, width - 1);
            int xAdd = std::clamp(x + radius + 1, 0, width - 1);
            sum += data[rowStart + xAdd] - data[rowStart + xRemove];
        }
    }

    // Vertical pass
    for (int x = 0; x < width; ++x) {
        int sum = 0;
        int count = 0;

        for (int k = -radius; k <= radius; ++k) {
            int y = std::clamp(k, 0, height - 1);
            sum += temp[y * width + x];
            count++;
        }

        for (int y = 0; y < height; ++y) {
            data[y * width + x] = static_cast<uint8_t>(sum / count);
            int yRemove = std::clamp(y - radius, 0, height - 1);
            int yAdd = std::clamp(y + radius + 1, 0, height - 1);
            sum += temp[yAdd * width + x] - temp[yRemove * width + x];
        }
    }
}
