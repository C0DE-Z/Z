#ifndef Z_UI_VECTOR_ICONS_H
#define Z_UI_VECTOR_ICONS_H

#include <QIcon>
#include <QColor>
#include <QSize>

namespace VectorIcon {

enum class Type {
    Play,
    Pause,
    StepForward,
    StepBackward,
    JumpStart,
    JumpEnd,
    Loop,
    MarkIn,
    MarkOut,
    Clear,
    ZoomIn,
    ZoomOut,
    ZoomFit,
    Keyframe,
    KeyframePrev,
    KeyframeNext,
    Settings,
    Import,
    Plus,
    Minus,
    Cut,
    Trash
};

QIcon create(Type type, const QColor& color = QColor(220, 220, 235), const QSize& size = QSize(24, 24));

} // namespace VectorIcon

#endif // Z_UI_VECTOR_ICONS_H
