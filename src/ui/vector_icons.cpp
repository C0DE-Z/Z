#include "vector_icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace VectorIcon {

static void renderVectorShape(QPainter& p, Type type, const QRectF& r, const QColor& color) {
    p.setRenderHint(QPainter::Antialiasing, true);
    
    QPen pen(color, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const double cx = r.center().x();
    const double cy = r.center().y();
    const double w = r.width();
    const double h = r.height();

    switch (type) {
    case Type::Play: {
        QPainterPath path;
        path.moveTo(cx - w * 0.22, cy - h * 0.32);
        path.lineTo(cx + w * 0.30, cy);
        path.lineTo(cx - w * 0.22, cy + h * 0.32);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
        break;
    }
    case Type::Pause: {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        double barW = w * 0.16;
        double barH = h * 0.60;
        double spacing = w * 0.12;
        p.drawRoundedRect(QRectF(cx - spacing - barW, cy - barH * 0.5, barW, barH), 1.5, 1.5);
        p.drawRoundedRect(QRectF(cx + spacing, cy - barH * 0.5, barW, barH), 1.5, 1.5);
        break;
    }
    case Type::StepForward: {
        QPainterPath path;
        path.moveTo(cx - w * 0.18, cy - h * 0.28);
        path.lineTo(cx + w * 0.18, cy);
        path.lineTo(cx - w * 0.18, cy + h * 0.28);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
        break;
    }
    case Type::StepBackward: {
        QPainterPath path;
        path.moveTo(cx + w * 0.18, cy - h * 0.28);
        path.lineTo(cx - w * 0.18, cy);
        path.lineTo(cx + w * 0.18, cy + h * 0.28);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
        break;
    }
    case Type::JumpStart: {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(cx - w * 0.32, cy - h * 0.30, w * 0.12, h * 0.60), 1.0, 1.0);
        QPainterPath path;
        path.moveTo(cx + w * 0.30, cy - h * 0.28);
        path.lineTo(cx - w * 0.10, cy);
        path.lineTo(cx + w * 0.30, cy + h * 0.28);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Type::JumpEnd: {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(cx + w * 0.20, cy - h * 0.30, w * 0.12, h * 0.60), 1.0, 1.0);
        QPainterPath path;
        path.moveTo(cx - w * 0.30, cy - h * 0.28);
        path.lineTo(cx + w * 0.10, cy);
        path.lineTo(cx - w * 0.30, cy + h * 0.28);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Type::Loop: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        p.drawArc(QRectF(cx - w * 0.32, cy - h * 0.28, w * 0.64, h * 0.56), 30 * 16, 220 * 16);
        QPainterPath arrow;
        arrow.moveTo(cx + w * 0.18, cy - h * 0.38);
        arrow.lineTo(cx + w * 0.36, cy - h * 0.20);
        arrow.lineTo(cx + w * 0.18, cy - h * 0.08);
        p.drawPath(arrow);
        break;
    }
    case Type::MarkIn: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(cx + w * 0.10, cy - h * 0.32), QPointF(cx - w * 0.18, cy - h * 0.32));
        p.drawLine(QPointF(cx - w * 0.18, cy - h * 0.32), QPointF(cx - w * 0.18, cy + h * 0.32));
        p.drawLine(QPointF(cx - w * 0.18, cy + h * 0.32), QPointF(cx + w * 0.10, cy + h * 0.32));
        break;
    }
    case Type::MarkOut: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.10, cy - h * 0.32), QPointF(cx + w * 0.18, cy - h * 0.32));
        p.drawLine(QPointF(cx + w * 0.18, cy - h * 0.32), QPointF(cx + w * 0.18, cy + h * 0.32));
        p.drawLine(QPointF(cx + w * 0.18, cy + h * 0.32), QPointF(cx - w * 0.10, cy + h * 0.32));
        break;
    }
    case Type::Clear: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        double sz = w * 0.24;
        p.drawLine(QPointF(cx - sz, cy - sz), QPointF(cx + sz, cy + sz));
        p.drawLine(QPointF(cx + sz, cy - sz), QPointF(cx - sz, cy + sz));
        break;
    }
    case Type::ZoomIn: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        double rad = w * 0.22;
        p.drawEllipse(QPointF(cx - w * 0.06, cy - h * 0.06), rad, rad);
        p.drawLine(QPointF(cx + w * 0.10, cy + h * 0.10), QPointF(cx + w * 0.34, cy + h * 0.34));
        p.drawLine(QPointF(cx - w * 0.16, cy - h * 0.06), QPointF(cx + w * 0.04, cy - h * 0.06));
        p.drawLine(QPointF(cx - w * 0.06, cy - h * 0.16), QPointF(cx - w * 0.06, cy + h * 0.04));
        break;
    }
    case Type::ZoomOut: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        double rad = w * 0.22;
        p.drawEllipse(QPointF(cx - w * 0.06, cy - h * 0.06), rad, rad);
        p.drawLine(QPointF(cx + w * 0.10, cy + h * 0.10), QPointF(cx + w * 0.34, cy + h * 0.34));
        p.drawLine(QPointF(cx - w * 0.16, cy - h * 0.06), QPointF(cx + w * 0.04, cy - h * 0.06));
        break;
    }
    case Type::ZoomFit: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.30, cy), QPointF(cx + w * 0.30, cy));
        p.drawLine(QPointF(cx - w * 0.30, cy - h * 0.22), QPointF(cx - w * 0.30, cy + h * 0.22));
        p.drawLine(QPointF(cx + w * 0.30, cy - h * 0.22), QPointF(cx + w * 0.30, cy + h * 0.22));
        p.drawLine(QPointF(cx - w * 0.22, cy - h * 0.12), QPointF(cx - w * 0.30, cy));
        p.drawLine(QPointF(cx - w * 0.22, cy + h * 0.12), QPointF(cx - w * 0.30, cy));
        p.drawLine(QPointF(cx + w * 0.22, cy - h * 0.12), QPointF(cx + w * 0.30, cy));
        p.drawLine(QPointF(cx + w * 0.22, cy + h * 0.12), QPointF(cx + w * 0.30, cy));
        break;
    }
    case Type::Keyframe: {
        QPainterPath path;
        path.moveTo(cx, cy - h * 0.34);
        path.lineTo(cx + w * 0.30, cy);
        path.lineTo(cx, cy + h * 0.34);
        path.lineTo(cx - w * 0.30, cy);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
        break;
    }
    case Type::KeyframePrev: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(cx + w * 0.14, cy - h * 0.26), QPointF(cx - w * 0.14, cy));
        p.drawLine(QPointF(cx - w * 0.14, cy), QPointF(cx + w * 0.14, cy + h * 0.26));
        break;
    }
    case Type::KeyframeNext: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.14, cy - h * 0.26), QPointF(cx + w * 0.14, cy));
        p.drawLine(QPointF(cx + w * 0.14, cy), QPointF(cx - w * 0.14, cy + h * 0.26));
        break;
    }
    case Type::Settings: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        p.drawEllipse(QPointF(cx, cy), w * 0.18, h * 0.18);
        for (int i = 0; i < 6; ++i) {
            double angle = i * (3.1415926535 / 3.0);
            double x1 = cx + std::cos(angle) * (w * 0.24);
            double y1 = cy + std::sin(angle) * (h * 0.24);
            double x2 = cx + std::cos(angle) * (w * 0.36);
            double y2 = cy + std::sin(angle) * (h * 0.36);
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        }
        break;
    }
    case Type::Plus:
    case Type::Import: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.28, cy), QPointF(cx + w * 0.28, cy));
        p.drawLine(QPointF(cx, cy - h * 0.28), QPointF(cx, cy + h * 0.28));
        break;
    }
    case Type::Minus: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.28, cy), QPointF(cx + w * 0.28, cy));
        break;
    }
    case Type::Cut: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.24, cy - h * 0.24), QPointF(cx + w * 0.24, cy + h * 0.24));
        p.drawLine(QPointF(cx + w * 0.24, cy - h * 0.24), QPointF(cx - w * 0.24, cy + h * 0.24));
        p.drawEllipse(QPointF(cx - w * 0.24, cy + h * 0.24), w * 0.08, h * 0.08);
        p.drawEllipse(QPointF(cx + w * 0.24, cy + h * 0.24), w * 0.08, h * 0.08);
        break;
    }
    case Type::Trash: {
        pen.setWidthF(1.75);
        p.setPen(pen);
        p.drawLine(QPointF(cx - w * 0.28, cy - h * 0.24), QPointF(cx + w * 0.28, cy - h * 0.24));
        p.drawLine(QPointF(cx - w * 0.12, cy - h * 0.32), QPointF(cx + w * 0.12, cy - h * 0.32));
        QPainterPath body;
        body.moveTo(cx - w * 0.22, cy - h * 0.20);
        body.lineTo(cx - w * 0.18, cy + h * 0.32);
        body.lineTo(cx + w * 0.18, cy + h * 0.32);
        body.lineTo(cx + w * 0.22, cy - h * 0.20);
        p.drawPath(body);
        break;
    }
    }
}

QIcon create(Type type, const QColor& color, const QSize& size) {
    QIcon icon;
    
    // Normal State Pixmap (HiDPI 2x supersampled)
    QPixmap normalPix(size * 2);
    normalPix.fill(Qt::transparent);
    {
        QPainter p(&normalPix);
        renderVectorShape(p, type, QRectF(0, 0, size.width() * 2, size.height() * 2), color);
    }
    normalPix.setDevicePixelRatio(2.0);
    icon.addPixmap(normalPix, QIcon::Normal, QIcon::Off);

    // Active / Hover State Pixmap
    QPixmap activePix(size * 2);
    activePix.fill(Qt::transparent);
    {
        QPainter p(&activePix);
        QColor hoverColor = (color == QColor(220, 220, 235)) ? QColor(245, 158, 248) : color.lighter(130);
        renderVectorShape(p, type, QRectF(0, 0, size.width() * 2, size.height() * 2), hoverColor);
    }
    activePix.setDevicePixelRatio(2.0);
    icon.addPixmap(activePix, QIcon::Active, QIcon::Off);

    // Disabled State Pixmap
    QPixmap disabledPix(size * 2);
    disabledPix.fill(Qt::transparent);
    {
        QPainter p(&disabledPix);
        renderVectorShape(p, type, QRectF(0, 0, size.width() * 2, size.height() * 2), QColor(80, 80, 95));
    }
    disabledPix.setDevicePixelRatio(2.0);
    icon.addPixmap(disabledPix, QIcon::Disabled, QIcon::Off);

    return icon;
}

} // namespace VectorIcon
