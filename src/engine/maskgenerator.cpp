#include "maskgenerator.h"
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <algorithm>
#include <cstring>

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
        } else if (!targetEffectId.empty()) {
            if (!m.targetEffectIds.empty() && 
                std::find(m.targetEffectIds.begin(), m.targetEffectIds.end(), targetEffectId) == m.targetEffectIds.end()) {
                continue;
            }
        }
        activeMasks.push_back(&m);
    }

    if (activeMasks.empty()) return {};

    QImage composite(width, height, QImage::Format_Grayscale8);
    composite.fill(0);

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
            transform.translate(-centerX, -centerY);
            painter.setTransform(transform);

            QPainterPath path;

            if (mask->shapeType == MaskShapeType::Rectangle) {
                float halfW = (0.25f * width) + static_cast<float>(expPx);
                float halfH = (0.25f * height) + static_cast<float>(expPx);
                path.addRect(centerX - halfW, centerY - halfH, halfW * 2.0f, halfH * 2.0f);
            } else if (mask->shapeType == MaskShapeType::Ellipse) {
                float rx = (0.25f * width) + static_cast<float>(expPx);
                float ry = (0.25f * height) + static_cast<float>(expPx);
                path.addEllipse(QPointF(centerX, centerY), rx, ry);
            } else if (mask->shapeType == MaskShapeType::Polygon || mask->shapeType == MaskShapeType::Bezier) {
                if (!mask->points.empty()) {
                    path.moveTo(mask->points[0].x * width, mask->points[0].y * height);
                    for (size_t i = 1; i < mask->points.size(); ++i) {
                        const auto& prev = mask->points[i - 1];
                        const auto& curr = mask->points[i];
                        if (mask->shapeType == MaskShapeType::Bezier &&
                            (prev.outHandleX != 0 || prev.outHandleY != 0 || curr.inHandleX != 0 || curr.inHandleY != 0)) {
                            QPointF c1(prev.x * width + prev.outHandleX * width, prev.y * height + prev.outHandleY * height);
                            QPointF c2(curr.x * width + curr.inHandleX * width, curr.y * height + curr.inHandleY * height);
                            QPointF end(curr.x * width, curr.y * height);
                            path.cubicTo(c1, c2, end);
                        } else {
                            path.lineTo(curr.x * width, curr.y * height);
                        }
                    }
                    if (mask->closed && mask->points.size() > 2) {
                        const auto& prev = mask->points.back();
                        const auto& curr = mask->points.front();
                        if (mask->shapeType == MaskShapeType::Bezier &&
                            (prev.outHandleX != 0 || prev.outHandleY != 0 || curr.inHandleX != 0 || curr.inHandleY != 0)) {
                            QPointF c1(prev.x * width + prev.outHandleX * width, prev.y * height + prev.outHandleY * height);
                            QPointF c2(curr.x * width + curr.inHandleX * width, curr.y * height + curr.inHandleY * height);
                            QPointF end(curr.x * width, curr.y * height);
                            path.cubicTo(c1, c2, end);
                        } else {
                            path.closeSubpath();
                        }
                    }
                }
            }

            painter.drawPath(path);
        }

        if (feather > 0.5) {
            applyFeather(layer.bits(), width, height, static_cast<float>(feather));
        }

        if (mask->inverted) {
            uint8_t* bits = layer.bits();
            int total = width * height;
            for (int i = 0; i < total; ++i) {
                bits[i] = 255 - bits[i];
            }
        }

        if (opacity < 0.999) {
            uint8_t* bits = layer.bits();
            int total = width * height;
            float op = static_cast<float>(opacity);
            for (int i = 0; i < total; ++i) {
                bits[i] = static_cast<uint8_t>(bits[i] * op);
            }
        }

        uint8_t* compBits = composite.bits();
        const uint8_t* layerBits = layer.constBits();
        int total = width * height;

        if (mask->mode == MaskMode::Add) {
            for (int i = 0; i < total; ++i) {
                compBits[i] = static_cast<uint8_t>(std::min(255, compBits[i] + layerBits[i]));
            }
        } else if (mask->mode == MaskMode::Subtract) {
            for (int i = 0; i < total; ++i) {
                compBits[i] = static_cast<uint8_t>(std::max(0, compBits[i] - layerBits[i]));
            }
        } else if (mask->mode == MaskMode::Intersect) {
            for (int i = 0; i < total; ++i) {
                compBits[i] = static_cast<uint8_t>((static_cast<int>(compBits[i]) * layerBits[i]) / 255);
            }
        }
    }

    std::vector<uint8_t> result(width * height);
    std::memcpy(result.data(), composite.constBits(), width * height);
    return result;
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
