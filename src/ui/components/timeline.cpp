#include "timeline.h"
#include "core/project.h"
#include <algorithm>
#include <QTime>
#include <QDebug>

Timeline::Timeline(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAcceptDrops(true);
}

void Timeline::setPlayhead(double time) {
    if (playheadTime != time) {
        playheadTime = time;
        update();
    }
}

void Timeline::setDuration(double duration) {
    if (totalDuration != duration) {
        totalDuration = duration;
        setMinimumWidth(80 + static_cast<int>(totalDuration * pixelsPerSecond) + 50);
        update();
    }
}

void Timeline::zoomIn() {
    double old = pixelsPerSecond;
    pixelsPerSecond = std::clamp(pixelsPerSecond * 1.2, 1.0, 1000.0);
    if (pixelsPerSecond != old) {
        setMinimumWidth(80 + static_cast<int>(totalDuration * pixelsPerSecond) + 50);
        update();
    }
}

void Timeline::zoomOut() {
    double old = pixelsPerSecond;
    pixelsPerSecond = std::clamp(pixelsPerSecond / 1.2, 1.0, 1000.0);
    if (pixelsPerSecond != old) {
        setMinimumWidth(80 + static_cast<int>(totalDuration * pixelsPerSecond) + 50);
        update();
    }
}

void Timeline::selectParameter(const QString& effectId, const QString& paramName) {
    activeEffectId = effectId;
    activeParamName = paramName;
    update();
}

int Timeline::timeToX(double time) const {
    return 80 + static_cast<int>(time * pixelsPerSecond);
}

double Timeline::xToTime(int x) const {
    if (x < 80) return 0.0;
    return std::clamp(static_cast<double>(x - 80) / pixelsPerSecond, 0.0, totalDuration);
}

void Timeline::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::AltModifier) {
        double oldPixelsPerSecond = pixelsPerSecond;
        if (event->angleDelta().y() > 0) {
            pixelsPerSecond *= 1.2;
        } else {
            pixelsPerSecond /= 1.2;
        }
        pixelsPerSecond = std::clamp(pixelsPerSecond, 1.0, 1000.0);
        if (pixelsPerSecond != oldPixelsPerSecond) {
            setMinimumWidth(80 + static_cast<int>(totalDuration * pixelsPerSecond) + 50);
            update();
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

bool Timeline::hitTestClip(const QPoint& pos, int& trackIndex, int& clipIndex, double& clipStart, double& clipDuration) const {
    const auto& tracks = Project::instance().getTracks();
    const int trackHeight = 35;
    const int yOffset = 25;

    if (pos.x() < 80 || pos.y() < yOffset) {
        return false;
    }

    int idx = (pos.y() - yOffset) / trackHeight;
    if (idx < 0 || idx >= static_cast<int>(tracks.size())) {
        return false;
    }

    const auto& track = tracks[idx];
    for (int i = 0; i < static_cast<int>(track.clips.size()); ++i) {
        const auto& clip = track.clips[i];
        int startX = timeToX(clip.timelineStart);
        int endX = timeToX(clip.timelineStart + clip.sourceDuration);
        QRect clipRect(startX, yOffset + idx * trackHeight + 3, endX - startX, trackHeight - 6);
        if (clipRect.contains(pos)) {
            trackIndex = idx;
            clipIndex = i;
            clipStart = clip.timelineStart;
            clipDuration = clip.sourceDuration;
            return true;
        }
    }

    return false;
}

void Timeline::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), QColor(13, 13, 13));

    painter.fillRect(0, 0, 80, height(), QColor(18, 18, 18));
    painter.setPen(QColor(40, 40, 40));
    painter.drawLine(80, 0, 80, height());

    int trackHeight = 35;
    const auto& tracks = Project::instance().getTracks();
    int numTracks = std::max(1, static_cast<int>(tracks.size()));
    for (int i = 0; i < numTracks; ++i) {
        int y = 25 + i * trackHeight;
        painter.setPen(QColor(30, 30, 30));
        painter.drawLine(0, y + trackHeight, width(), y + trackHeight);
        painter.setPen(QColor(180, 180, 180));
        QString title = (i < static_cast<int>(tracks.size()))
            ? QString::fromStdString(tracks[i].name)
            : QString("Track %1").arg(i + 1);
        QString uppercaseTitle = title.toUpper();
        painter.setFont(QFont("JetBrains Mono", 9));
        painter.drawText(10, y + 20, uppercaseTitle);
    }

    painter.fillRect(80, 0, width() - 80, 25, QColor(24, 24, 24));
    painter.setPen(QColor(40, 40, 40));
    painter.drawLine(80, 25, width(), 25);

    painter.setPen(QColor(120, 120, 120));
    painter.setFont(QFont("JetBrains Mono", 8));
    double minTickSpacingPx = 80.0;
    double minTickSecs = minTickSpacingPx / pixelsPerSecond;
    double niceSteps[] = {0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0};
    double tickStep = niceSteps[0];
    for (double step : niceSteps) {
        if (step >= minTickSecs) {
            tickStep = step;
            break;
        }
    }

    for (double t = 0; t <= totalDuration; t += tickStep) {
        int x = timeToX(t);
        painter.drawLine(x, 15, x, 25);
        int totalMs = static_cast<int>(t * 1000.0);
        int m = (totalMs / 60000);
        int s = (totalMs / 1000) % 60;
        int ms = totalMs % 1000;
        QString timeStr;
        if (tickStep < 1.0) {
            timeStr = QString("%1:%2.%3").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')).arg(ms / 10, 2, 10, QChar('0'));
        } else {
            timeStr = QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        }
        painter.drawText(x - 15, 12, timeStr);
    }

    for (size_t tIdx = 0; tIdx < tracks.size() && tIdx < (size_t)numTracks; ++tIdx) {
        int y = 25 + tIdx * trackHeight;
        for (const auto& clip : tracks[tIdx].clips) {
            int startX = timeToX(clip.timelineStart);
            int endX = timeToX(clip.timelineStart + clip.sourceDuration);
            QRect clipRect(startX, y + 3, endX - startX, trackHeight - 6);
            painter.fillRect(clipRect, QColor(59, 20, 66, 180));
            painter.setPen(QPen(QColor(232, 85, 244), 1.5));
            painter.drawRect(clipRect);
            painter.setPen(Qt::white);
            QString clipName = QString::fromStdString(clip.id);
            painter.drawText(clipRect.adjusted(5, 0, -30, 0), Qt::AlignLeft | Qt::AlignVCenter, clipName);

            if (!clip.effects.empty() || clip.useClipEffects) {
                painter.setPen(QColor(232, 85, 244));
                painter.drawText(clipRect.adjusted(0, 0, -5, 0), Qt::AlignRight | Qt::AlignVCenter, "[FX]");
            }
        }
        for (const auto& trans : tracks[tIdx].transitions) {
            double cutPoint = 0.0;
            for (const auto& clip : tracks[tIdx].clips) {
                if (clip.id == trans.rightClipId) {
                    cutPoint = clip.timelineStart;
                    break;
                }
            }
            if (cutPoint > 0.0) {
                int startX = timeToX(cutPoint - trans.duration / 2.0);
                int endX = timeToX(cutPoint + trans.duration / 2.0);
                QRect transRect(startX, y + 15, endX - startX, trackHeight - 30);
                painter.fillRect(transRect, QColor(200, 80, 80, 180));
                painter.setPen(QPen(QColor(255, 120, 120), 1.0));
                painter.drawRect(transRect);
                painter.setPen(Qt::black);
                painter.drawText(transRect, Qt::AlignCenter, "TR");
            }
        }
    }

    if (!activeEffectId.isEmpty() && !activeParamName.isEmpty()) {
        int kfLaneY = 25 + numTracks * trackHeight + 10;
        int kfLaneH = height() - kfLaneY - 10;
        if (kfLaneH > 15) {
            painter.fillRect(80, kfLaneY, width() - 80, kfLaneH, QColor(18, 18, 18));
            painter.setPen(QColor(40, 40, 40));
            painter.drawRect(80, kfLaneY, width() - 80, kfLaneH);

            painter.setPen(QColor(100, 100, 100));
            QString laneLabel = QString("Keyframes: %1 - %2").arg(activeEffectId, activeParamName);
            painter.drawText(90, kfLaneY + 15, laneLabel);

            QList<QPointF> kfPoints;
            const auto& tracks = Project::instance().getTracks();
            for (const auto& track : tracks) {
                for (const auto& eff : track.effects) {
                    if (QString::fromStdString(eff.pluginId) == activeEffectId) {
                        for (const auto& param : eff.parameters) {
                            if (QString::fromStdString(param.name) == activeParamName) {
                                for (const auto& kf : param.curve.getKeyframes()) {
                                    int kfX = timeToX(kf.time);
                                    double normVal = (param.maxVal > param.minVal) 
                                        ? (kf.value - param.minVal) / (param.maxVal - param.minVal)
                                        : 0.5;
                                    int kfY = kfLaneY + kfLaneH - 10 - static_cast<int>(normVal * (kfLaneH - 20));
                                    kfPoints.append(QPointF(kfX, kfY));
                                }
                            }
                        }
                    }
                }
            }

            if (kfPoints.size() > 1) {
                painter.setPen(QPen(QColor(232, 85, 244), 1.0, Qt::DashLine));
                for (int i = 0; i < kfPoints.size() - 1; ++i) {
                    painter.drawLine(kfPoints[i], kfPoints[i+1]);
                }
            }

            for (const auto& pt : kfPoints) {
                int kfX = static_cast<int>(pt.x());
                int kfY = static_cast<int>(pt.y());
                QPolygon diamond;
                diamond << QPoint(kfX, kfY - 4)
                        << QPoint(kfX + 4, kfY)
                        << QPoint(kfX, kfY + 4)
                        << QPoint(kfX - 4, kfY);
                painter.setBrush(QColor(232, 85, 244));
                painter.drawPolygon(diamond);
                painter.setBrush(Qt::NoBrush);
            }
        }
    }

    int phX = timeToX(playheadTime);
    if (phX >= 80) {
        painter.setPen(QPen(QColor(232, 85, 244), 1.5));
        painter.drawLine(phX, 0, phX, height());
        QPolygon playheadHead;
        playheadHead << QPoint(phX - 6, 0)
                     << QPoint(phX + 6, 0)
                     << QPoint(phX + 6, 12)
                     << QPoint(phX, 18)
                     << QPoint(phX - 6, 12);
        painter.setBrush(QColor(232, 85, 244));
        painter.drawPolygon(playheadHead);
    }
}

void Timeline::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int trackIndex = -1;
        int clipIndex = -1;
        double clipStart = 0.0;
        double clipDuration = 0.0;
        if (hitTestClip(event->pos(), trackIndex, clipIndex, clipStart, clipDuration)) {
            draggingClip = true;
            dragTrackIndex = trackIndex;
            dragClipIndex = clipIndex;
            dragClipOffsetTime = xToTime(event->pos().x()) - clipStart;
            emit clipMoveStarted();
            emit clipSelected(trackIndex, clipIndex);
            update();
            return;
        }

        double clickedTime = xToTime(event->pos().x());
        emit scrubbed(clickedTime);
    }
}

void Timeline::mouseMoveEvent(QMouseEvent* event) {
    if (draggingClip && (event->buttons() & Qt::LeftButton)) {
        double targetStart = xToTime(event->pos().x()) - dragClipOffsetTime;
        if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
            const double snapThreshold = 15.0 / pixelsPerSecond;
            double bestSnapDiff = snapThreshold;
            double snappedStart = targetStart;
            if (std::abs(targetStart - playheadTime) < bestSnapDiff) {
                bestSnapDiff = std::abs(targetStart - playheadTime);
                snappedStart = playheadTime;
            }
            const auto& tracks = Project::instance().getTracks();
            if (dragTrackIndex >= 0 && dragTrackIndex < (int)tracks.size()) {
                const auto& track = tracks[dragTrackIndex];
                if (dragClipIndex >= 0 && dragClipIndex < (int)track.clips.size()) {
                    double duration = track.clips[dragClipIndex].sourceDuration;
                    if (std::abs((targetStart + duration) - playheadTime) < bestSnapDiff) {
                        bestSnapDiff = std::abs((targetStart + duration) - playheadTime);
                        snappedStart = playheadTime - duration;
                    }
                    for (int tIdx = 0; tIdx < (int)tracks.size(); ++tIdx) {
                        for (int cIdx = 0; cIdx < (int)tracks[tIdx].clips.size(); ++cIdx) {
                            if (tIdx == dragTrackIndex && cIdx == dragClipIndex) continue;
                            const auto& other = tracks[tIdx].clips[cIdx];
                            double otherStart = other.timelineStart;
                            double otherEnd = otherStart + other.sourceDuration;
                            if (std::abs(targetStart - otherStart) < bestSnapDiff) {
                                bestSnapDiff = std::abs(targetStart - otherStart);
                                snappedStart = otherStart;
                            }
                            if (std::abs(targetStart - otherEnd) < bestSnapDiff) {
                                bestSnapDiff = std::abs(targetStart - otherEnd);
                                snappedStart = otherEnd;
                            }
                            if (std::abs((targetStart + duration) - otherStart) < bestSnapDiff) {
                                bestSnapDiff = std::abs((targetStart + duration) - otherStart);
                                snappedStart = otherStart - duration;
                            }
                            if (std::abs((targetStart + duration) - otherEnd) < bestSnapDiff) {
                                bestSnapDiff = std::abs((targetStart + duration) - otherEnd);
                                snappedStart = otherEnd - duration;
                            }
                        }
                    }
                }
            }
            targetStart = snappedStart;
        }

        int targetTrackIndex = dragTrackIndex;
        if (event->pos().y() >= 25) {
            int idx = (event->pos().y() - 25) / 35;
            const auto& tracks = Project::instance().getTracks();
            if (idx >= 0 && idx < static_cast<int>(tracks.size())) {
                targetTrackIndex = idx;
            }
        }

        if (targetTrackIndex != dragTrackIndex) {
            emit clipTrackChangeRequested(dragTrackIndex, dragClipIndex, targetTrackIndex, targetStart);
        } else {
            emit clipMoveRequested(dragTrackIndex, dragClipIndex, targetStart);
        }
        update();
        return;
    }

    if (event->buttons() & Qt::LeftButton) {
        double clickedTime = xToTime(event->pos().x());
        emit scrubbed(clickedTime);
    }
}

void Timeline::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    draggingClip = false;
    dragTrackIndex = -1;
    dragClipIndex = -1;
    dragClipOffsetTime = 0.0;
}

void Timeline::updateDragIndices(int newTrack, int newClip) {
    dragTrackIndex = newTrack;
    dragClipIndex = newClip;
}

void Timeline::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText() || event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
    }
}

void Timeline::dragMoveEvent(QDragMoveEvent* event) {
    event->acceptProposedAction();
}

void Timeline::dropEvent(QDropEvent* event) {
    int y = event->pos().y();
    if (y < 25) return;
    int trackIndex = (y - 25) / 35; 
    double dropTime = (event->pos().x() - 80) / pixelsPerSecond; 
    if (dropTime < 0) dropTime = 0;
    QString effectName;
    if (event->mimeData()->hasText()) {
        effectName = event->mimeData()->text();
    }
    emit effectDropped(trackIndex, dropTime, effectName);
    event->acceptProposedAction();
}
