#include "timeline.h"
#include "core/project.h"
#include <algorithm>
#include <limits>
#include <QTime>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QFontMetrics>
#include <QMap>
#include <QFileInfo>
#include <QUrl>

namespace {
std::vector<std::pair<QString, QString>> getTransitionChoices() {
    std::vector<std::pair<QString, QString>> choices;
    const auto& plugins = PluginManager::instance().getPlugins();
    for (const auto& p : plugins) {
        const QString category = QString::fromStdString(p.category);
        if (category.startsWith("Transitions", Qt::CaseInsensitive)) {
            choices.emplace_back(QString::fromStdString(p.name), QString::fromStdString(p.id));
        }
    }

    std::sort(choices.begin(), choices.end(), [](const auto& a, const auto& b) {
        return a.first.toLower() < b.first.toLower();
    });

    if (choices.empty()) {
        choices.emplace_back("Cross Dissolve", "cross_dissolve");
        choices.emplace_back("Datamosh Transition", "datamosh_transition");
        choices.emplace_back("Glitch Slide Transition", "glitch_slide");
    }

    return choices;
}

QMap<QAction*, QString> addTransitionActions(QMenu* menu) {
    QMap<QAction*, QString> actionToId;
    const auto choices = getTransitionChoices();
    for (const auto& [label, id] : choices) {
        QAction* action = menu->addAction(label);
        actionToId.insert(action, id);
    }
    return actionToId;
}
}

Timeline::Timeline(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAcceptDrops(true);
}

void Timeline::clearSelection() {
    selectedTrackIndex = -1;
    selectedClipIndex = -1;
    selectedTransTrackIndex = -1;
    selectedTransIndex = -1;
    update();
}

void Timeline::setInOutPoints(double inPt, double outPt) {
    inPoint = inPt;
    outPoint = outPt;
    update();
}

void Timeline::clearInOutPoints() {
    inPoint = -1.0;
    outPoint = -1.0;
    update();
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

void Timeline::zoomFit(int viewWidth) {
    if (totalDuration <= 0.0) return;
    int availableWidth = std::max(200, viewWidth - 140);
    pixelsPerSecond = std::clamp(static_cast<double>(availableWidth) / totalDuration, 1.0, 1000.0);
    setMinimumWidth(80 + static_cast<int>(totalDuration * pixelsPerSecond) + 50);
    update();
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
        QRect clipRect(startX, yOffset + idx * trackHeight + 3, endX - startX, trackHeight - 14);
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

    int trackHeight = 35;
    const auto& tracks = Project::instance().getTracks();
    int numTracks = std::max(1, static_cast<int>(tracks.size()));
    int neededHeight = 25 + numTracks * trackHeight + (!activeEffectId.isEmpty() ? 100 : 25);
    if (minimumHeight() != neededHeight) {
        setMinimumHeight(neededHeight);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), QColor("#08080A"));

    painter.fillRect(0, 0, 80, height(), QColor("#111116"));
    painter.setPen(QColor("#303036"));
    painter.drawLine(80, 0, 80, height());

    for (int i = 0; i < numTracks; ++i) {
        int y = 25 + i * trackHeight;
        painter.setPen(QColor("#303036"));
        painter.drawLine(0, y + trackHeight, width(), y + trackHeight);
        painter.setPen(QColor("#C3BEC3"));
        QString title = (i < static_cast<int>(tracks.size()))
            ? QString::fromStdString(tracks[i].name)
            : QString("Track %1").arg(i + 1);
        if (i < static_cast<int>(tracks.size())) {
            title = (tracks[i].type == TimelineTrackType::Audio ? "A  " : "V  ") + title;
        }
        QString uppercaseTitle = title.toUpper();
        painter.setFont(QFont("JetBrains Mono", 9));
        painter.drawText(10, y + 20, uppercaseTitle);
    }

    painter.fillRect(80, 0, width() - 80, 25, QColor("#111116"));
    painter.setPen(QColor("#303036"));
    painter.drawLine(80, 25, width(), 25);

    painter.setPen(QColor("#918B92"));
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

    for (const auto& track : tracks) {
        if (track.type == TimelineTrackType::Audio) continue;
        for (const auto& trans : track.transitions) {
            int cutX = timeToX(trans.cutTime);
            int startX = timeToX(trans.cutTime - trans.duration / 2.0);
            int endX = timeToX(trans.cutTime + trans.duration / 2.0);

            QRect spanRect(startX, 19, std::max(2, endX - startX), 5);
            painter.fillRect(spanRect, QColor(255, 79, 145, 220));

            QPolygon marker;
            marker << QPoint(cutX - 4, 2)
                   << QPoint(cutX + 4, 2)
                   << QPoint(cutX, 8);
            painter.setBrush(QColor("#FF4F91"));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(marker);

            painter.setPen(QPen(QColor(255, 114, 170, 180), 1));
            painter.drawLine(cutX, 8, cutX, 25);
        }
    }

    if (inPoint >= 0.0 && outPoint > inPoint) {
        int x1 = timeToX(inPoint);
        int x2 = timeToX(outPoint);
        painter.fillRect(QRect(x1, 25, std::max(2, x2 - x1), height() - 25), QColor(255, 79, 145, 30));
    }

    if (inPoint >= 0.0) {
        int x = timeToX(inPoint);
        painter.setPen(QPen(QColor("#FF4F91"), 2));
        painter.drawLine(x, 0, x, height());
        QPolygon tri;
        tri << QPoint(x - 5, 0) << QPoint(x + 5, 0) << QPoint(x, 7);
        painter.setBrush(QColor("#FF4F91"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(tri);
    }

    if (outPoint >= 0.0) {
        int x = timeToX(outPoint);
        painter.setPen(QPen(QColor("#FF72AA"), 2));
        painter.drawLine(x, 0, x, height());
        QPolygon tri;
        tri << QPoint(x - 5, 0) << QPoint(x + 5, 0) << QPoint(x, 7);
        painter.setBrush(QColor("#FF72AA"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(tri);
    }

    bool hasAnyClips = false;
    for (const auto& track : tracks) {
        if (!track.clips.empty()) { hasAnyClips = true; break; }
    }
    if (!hasAnyClips) {
        painter.setPen(QColor("#918B92"));
        painter.setFont(QFont("Segoe UI", 11, QFont::DemiBold));
        painter.drawText(QRect(80, 25, width() - 80, height() - 25), Qt::AlignCenter, "Drag video files here or press Ctrl+I to import media");
    }

    for (size_t tIdx = 0; tIdx < tracks.size() && tIdx < (size_t)numTracks; ++tIdx) {
        int y = 25 + tIdx * trackHeight;
        for (int cIdx = 0; cIdx < (int)tracks[tIdx].clips.size(); ++cIdx) {
            const auto& clip = tracks[tIdx].clips[cIdx];
            int startX = timeToX(clip.timelineStart);
            int endX = timeToX(clip.timelineStart + clip.sourceDuration);
            QRect clipRect(startX, y + 3, endX - startX, trackHeight - 14);
            const bool isAudioTrack = tracks[tIdx].type == TimelineTrackType::Audio;
            painter.fillRect(clipRect, isAudioTrack ? QColor(16, 51, 58, 200) : QColor(61, 18, 38, 220));
            
            if (static_cast<int>(tIdx) == selectedTrackIndex && cIdx == selectedClipIndex) {
                painter.setPen(QPen(QColor("#FFB8D2"), 2.0));
            } else {
                painter.setPen(QPen(isAudioTrack ? QColor(45, 190, 191) : QColor("#FF4F91"), 1.0));
            }
            painter.drawRect(clipRect);
            
            painter.setPen(Qt::white);
            QString clipName = QString::fromStdString(clip.name);
            painter.drawText(clipRect.adjusted(5, 0, -30, 0), Qt::AlignLeft | Qt::AlignVCenter, clipName);

            if (!isAudioTrack && (!clip.effects.empty() || clip.useClipEffects)) {
                painter.setPen(QColor("#FFB8D2"));
                painter.drawText(clipRect.adjusted(0, 0, -5, 0), Qt::AlignRight | Qt::AlignVCenter, "[FX]");
            }

            const double clipEnd = clip.timelineStart + clip.sourceDuration;
            const ProjectClip* nextClip = nullptr;
            double bestNextStart = std::numeric_limits<double>::max();
            for (const auto& other : tracks[tIdx].clips) {
                if (other.id == clip.id) continue;
                if (other.timelineStart >= clipEnd - 0.6 && other.timelineStart < bestNextStart) {
                    bestNextStart = other.timelineStart;
                    nextClip = &other;
                }
            }

            if (!isAudioTrack && nextClip) {
                const int bx = timeToX(nextClip->timelineStart);
                const int by = y + 10;
                painter.setPen(QPen(QColor("#FF4F91"), 1.5));
                painter.setBrush(QColor("#32101F"));
                painter.drawEllipse(QPoint(bx, by), 7, 7);
                painter.setPen(QColor("#FFB8D2"));
                painter.setFont(QFont("JetBrains Mono", 8, QFont::Bold));
                painter.drawText(QRect(bx - 5, by - 6, 10, 12), Qt::AlignCenter, "+");
            }
        }
        if (tracks[tIdx].type == TimelineTrackType::Audio) continue;
        const int laneY = y + trackHeight - 9;
        const int laneH = 6;
        const int kHandleW = 5;
        for (int tIdx2 = 0; tIdx2 < (int)tracks[tIdx].transitions.size(); ++tIdx2) {
            const auto& trans = tracks[tIdx].transitions[tIdx2];
            double halfDur = trans.duration / 2.0;
            int startX = timeToX(trans.cutTime - halfDur);
            int endX   = timeToX(trans.cutTime + halfDur);
            int w = std::max(endX - startX, 8);
            QRect transRect(startX, laneY, w, laneH);

            bool isSel = (tIdx == (size_t)selectedTransTrackIndex && tIdx2 == selectedTransIndex);

            painter.fillRect(transRect, QColor(92, 30, 56, 210));

            painter.setPen(QPen(isSel ? Qt::white : QColor("#FF4F91"), isSel ? 2.0 : 1.0));
            painter.drawRect(transRect);

            if (w > 60) {
                QString plugName = QString::fromStdString(trans.pluginId).section('/', -1);
                if (auto* pluginMeta = PluginManager::instance().findPlugin(trans.pluginId)) {
                    plugName = QString::fromStdString(pluginMeta->name);
                }
                painter.setPen(QColor("#FFB8D2"));
                QFont labelFont("JetBrains Mono", 7);
                painter.setFont(labelFont);
                QFontMetrics fm(labelFont);
                QString elided = fm.elidedText(plugName, Qt::ElideRight, w - 6);
                QRect labelRect(startX + 3, laneY - 11, w - 6, 10);
                painter.drawText(labelRect, Qt::AlignCenter | Qt::TextSingleLine, elided);
            }

            QRect lHandle(startX, laneY, kHandleW, laneH);
            painter.fillRect(lHandle, QColor(255, 79, 145, 220));
            painter.setPen(QColor("#FF72AA"));
            painter.drawRect(lHandle);

            QRect rHandle(endX - kHandleW, laneY, kHandleW, laneH);
            painter.fillRect(rHandle, QColor(255, 79, 145, 220));
            painter.setPen(QColor("#FF72AA"));
            painter.drawRect(rHandle);

            int cutX = timeToX(trans.cutTime);
            painter.setPen(QPen(QColor(255, 184, 210, 90), 1, Qt::DashLine));
            painter.drawLine(cutX, y + 2, cutX, y + trackHeight - 2);
        }
    }

    if (!activeEffectId.isEmpty() && !activeParamName.isEmpty()) {
        int kfLaneY = 25 + numTracks * trackHeight + 10;
        int kfLaneH = height() - kfLaneY - 10;
        if (kfLaneH > 15) {
            painter.fillRect(80, kfLaneY, width() - 80, kfLaneH, QColor("#111116"));
            painter.setPen(QColor("#303036"));
            painter.drawRect(80, kfLaneY, width() - 80, kfLaneH);

            painter.setPen(QColor("#C3BEC3"));
            QString laneLabel = QString("Keyframes: %1 - %2").arg(activeEffectId, activeParamName);
            painter.drawText(90, kfLaneY + 15, laneLabel);

            QList<QPointF> kfPoints;
            const auto& tracks = Project::instance().getTracks();
            auto appendKeyframes = [&](const std::vector<AppliedEffect>& effects) {
                for (const auto& eff : effects) {
                    if (QString::fromStdString(eff.pluginId) != activeEffectId) continue;
                    for (const auto& param : eff.parameters) {
                        if (QString::fromStdString(param.name) != activeParamName) continue;
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
            };

            for (const auto& track : tracks) {
                appendKeyframes(track.effects);
                for (const auto& clip : track.clips) {
                    if (clip.useClipEffects) {
                        appendKeyframes(clip.effects);
                    }
                }
            }

            std::sort(kfPoints.begin(), kfPoints.end(), [](const QPointF& a, const QPointF& b) {
                return a.x() < b.x();
            });

            if (kfPoints.size() > 1) {
                painter.setPen(QPen(QColor("#FF4F91"), 1.0, Qt::DashLine));
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
                painter.setBrush(QColor("#FF72AA"));
                painter.drawPolygon(diamond);
                painter.setBrush(Qt::NoBrush);
            }
        }
    }

    int phX = timeToX(playheadTime);
    if (phX >= 80) {
        painter.setPen(QPen(QColor("#FFB8D2"), 1.5));
        painter.drawLine(phX, 0, phX, height());
        QPolygon playheadHead;
        playheadHead << QPoint(phX - 6, 0)
                     << QPoint(phX + 6, 0)
                     << QPoint(phX + 6, 12)
                     << QPoint(phX, 18)
                     << QPoint(phX - 6, 12);
        painter.setBrush(QColor("#FF4F91"));
        painter.drawPolygon(playheadHead);
    }
}

bool Timeline::hitTestTransition(const QPoint& pos, int& trackIndex, int& transIndex, bool& isLeftEdge) const {
    const auto& tracks = Project::instance().getTracks();
    const int trackHeight = 35;
    const int yOffset = 25;
    const int kHandleW = 8;

    if (pos.x() < 80 || pos.y() < yOffset) return false;

    int tIdx = (pos.y() - yOffset) / trackHeight;
    if (tIdx < 0 || tIdx >= (int)tracks.size()) return false;
    if (tracks[tIdx].type == TimelineTrackType::Audio) return false;

    for (int i = 0; i < (int)tracks[tIdx].transitions.size(); ++i) {
        const auto& trans = tracks[tIdx].transitions[i];
        double halfDur = trans.duration / 2.0;
        int startX = timeToX(trans.cutTime - halfDur);
        int endX   = timeToX(trans.cutTime + halfDur);
        int bodyY  = yOffset + tIdx * trackHeight + trackHeight - 9;
        int bodyH  = 6;

        QRect bodyRect(startX, bodyY - 3, endX - startX, bodyH + 6);
        if (!bodyRect.contains(pos)) continue;

        trackIndex = tIdx;
        transIndex = i;
        // Determine if we're in the left handle zone or right handle zone
        if (pos.x() <= startX + kHandleW) {
            isLeftEdge = true;
        } else if (pos.x() >= endX - kHandleW) {
            isLeftEdge = false;
        } else {
            isLeftEdge = false; // body hit — used for selection, caller checks
        }
        return true;
    }
    return false;
}

bool Timeline::hitTestCutEdge(const QPoint& pos, int& trackIndex, double& cutTime) const {
    const auto& tracks = Project::instance().getTracks();
    const int trackHeight = 35;
    const int yOffset = 25;

    if (pos.x() < 80 || pos.y() < yOffset) return false;

    int tIdx = (pos.y() - yOffset) / trackHeight;
    if (tIdx < 0 || tIdx >= static_cast<int>(tracks.size())) return false;

    const auto& track = tracks[tIdx];
    if (track.type == TimelineTrackType::Audio) return false;
    if (track.clips.size() < 2) return false;

    const int snapPx = 14;
    bool found = false;
    int bestDx = std::numeric_limits<int>::max();
    double bestCut = 0.0;

    for (const auto& a : track.clips) {
        const double endA = a.timelineStart + a.sourceDuration;
        for (const auto& b : track.clips) {
            if (a.id == b.id) continue;
            if (std::abs(endA - b.timelineStart) > 0.35) continue;

            const int cutX = timeToX(b.timelineStart);
            const int dx = std::abs(pos.x() - cutX);
            if (dx <= snapPx && dx < bestDx) {
                bestDx = dx;
                bestCut = b.timelineStart;
                found = true;
            }
        }
    }

    if (!found) return false;
    trackIndex = tIdx;
    cutTime = bestCut;
    return true;
}

bool Timeline::hitTestTransitionButton(const QPoint& pos, int& trackIndex, double& cutTime) const {
    const auto& tracks = Project::instance().getTracks();
    const int trackHeight = 35;
    const int yOffset = 25;

    if (pos.x() < 80 || pos.y() < yOffset) return false;

    int tIdx = (pos.y() - yOffset) / trackHeight;
    if (tIdx < 0 || tIdx >= static_cast<int>(tracks.size())) return false;

    const auto& track = tracks[tIdx];
    if (track.type == TimelineTrackType::Audio) return false;
    const int buttonR = 7;

    for (const auto& clip : track.clips) {
        const double clipEnd = clip.timelineStart + clip.sourceDuration;
        const ProjectClip* nextClip = nullptr;
        double bestNextStart = std::numeric_limits<double>::max();
        for (const auto& other : track.clips) {
            if (other.id == clip.id) continue;
            if (other.timelineStart >= clipEnd - 0.6 && other.timelineStart < bestNextStart) {
                bestNextStart = other.timelineStart;
                nextClip = &other;
            }
        }
        if (!nextClip) continue;

        int x = timeToX(nextClip->timelineStart);
        int y = yOffset + tIdx * trackHeight + 10;
        int dx = pos.x() - x;
        int dy = pos.y() - y;
        if (dx * dx + dy * dy <= (buttonR + 2) * (buttonR + 2)) {
            trackIndex = tIdx;
            cutTime = nextClip->timelineStart;
            return true;
        }
    }

    return false;
}

void Timeline::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int btnTrack = -1;
        double btnCutTime = 0.0;
        if (hitTestTransitionButton(event->pos(), btnTrack, btnCutTime)) {
            QMenu quickMenu(this);
            QMap<QAction*, QString> actionToId = addTransitionActions(&quickMenu);
            QAction* chosen = quickMenu.exec(event->globalPosition().toPoint());
            if (actionToId.contains(chosen)) {
                emit transitionApplyRequested(btnTrack, btnCutTime, actionToId.value(chosen));
            }
            return;
        }

        int transTrack = -1, transIdx = -1;
        bool isLeftEdge = false;
        if (hitTestTransition(event->pos(), transTrack, transIdx, isLeftEdge)) {
            // Clear clip selection
            selectedTrackIndex = -1;
            selectedClipIndex = -1;
            selectedTransTrackIndex = transTrack;
            selectedTransIndex = transIdx;
            emit transitionSelected(transTrack, transIdx);

            // Check if we're actually in an edge handle zone
            auto& tracks = Project::instance().getTracks();
            if (transTrack < (int)tracks.size() && transIdx < (int)tracks[transTrack].transitions.size()) {
                const auto& trans = tracks[transTrack].transitions[transIdx];
                double halfDur = trans.duration / 2.0;
                int startX = timeToX(trans.cutTime - halfDur);
                int endX   = timeToX(trans.cutTime + halfDur);
                const int kHandleW = 8;
                bool inLeftHandle  = event->pos().x() <= startX + kHandleW;
                bool inRightHandle = event->pos().x() >= endX - kHandleW;

                if (inLeftHandle || inRightHandle) {
                    draggingTransitionEdge = true;
                    dragTransTrackIndex = transTrack;
                    dragTransIndex = transIdx;
                    dragTransLeftEdge = inLeftHandle;
                    dragTransOriginalDuration = trans.duration;
                    dragTransOriginalCutTime = trans.cutTime;
                    dragTransAnchorX = event->pos().x();
                }
            }
            update();
            return;
        }

        int trackIndex = -1;
        int clipIndex = -1;
        double clipStart = 0.0;
        double clipDuration = 0.0;
        if (hitTestClip(event->pos(), trackIndex, clipIndex, clipStart, clipDuration)) {
            selectedTrackIndex = trackIndex;
            selectedClipIndex = clipIndex;
            selectedTransTrackIndex = -1;
            selectedTransIndex = -1;
            draggingClip = true;
            dragTrackIndex = trackIndex;
            dragClipIndex = clipIndex;
            dragClipOffsetTime = xToTime(event->pos().x()) - clipStart;
            emit clipMoveStarted();
            emit clipSelected(trackIndex, clipIndex);
            update();
            return;
        }

        selectedTrackIndex = -1;
        selectedClipIndex = -1;
        selectedTransTrackIndex = -1;
        selectedTransIndex = -1;
        update();

        double clickedTime = xToTime(event->pos().x());
        emit scrubbed(clickedTime);

    } else if (event->button() == Qt::RightButton) {
        int transTrack = -1, transIdx = -1;
        bool isLeftEdge = false;
        if (hitTestTransition(event->pos(), transTrack, transIdx, isLeftEdge)) {
            selectedTransTrackIndex = transTrack;
            selectedTransIndex = transIdx;
            update();
            QMenu contextMenu(this);
            QMenu* changeTypeMenu = contextMenu.addMenu("Change Transition Type");
            QMap<QAction*, QString> changeTypeActions = addTransitionActions(changeTypeMenu);
            QAction* deleteAction = contextMenu.addAction("Delete Transition");
            QAction* chosen = contextMenu.exec(event->globalPosition().toPoint());
            if (changeTypeActions.contains(chosen)) {
                const auto& tracks = Project::instance().getTracks();
                if (transTrack >= 0 && transTrack < static_cast<int>(tracks.size()) &&
                    transIdx >= 0 && transIdx < static_cast<int>(tracks[transTrack].transitions.size())) {
                    emit transitionApplyRequested(transTrack, tracks[transTrack].transitions[transIdx].cutTime, changeTypeActions.value(chosen));
                }
            } else if (chosen == deleteAction) {
                emit deleteTransitionRequested(transTrack, transIdx);
            }
            return;
        }

        int cutTrack = -1;
        double cutTime = 0.0;
        if (hitTestCutEdge(event->pos(), cutTrack, cutTime)) {
            QMenu contextMenu(this);
            QMenu* applyMenu = contextMenu.addMenu("Add Transition");
            QMap<QAction*, QString> applyActions = addTransitionActions(applyMenu);
            QAction* chosen = contextMenu.exec(event->globalPosition().toPoint());
            if (applyActions.contains(chosen)) {
                emit transitionApplyRequested(cutTrack, cutTime, applyActions.value(chosen));
            }
            return;
        }

        int trackIndex = -1;
        int clipIndex = -1;
        double clipStart = 0.0;
        double clipDuration = 0.0;
        if (hitTestClip(event->pos(), trackIndex, clipIndex, clipStart, clipDuration)) {
            selectedTrackIndex = trackIndex;
            selectedClipIndex = clipIndex;
            emit clipSelected(trackIndex, clipIndex);
            update();

            QMenu contextMenu(this);
            QMap<QAction*, QString> addActions;
            if (Project::instance().getTracks()[trackIndex].type == TimelineTrackType::Video) {
                QMenu* addTransitionMenu = contextMenu.addMenu("Add Transition");
                addActions = addTransitionActions(addTransitionMenu);
                contextMenu.addSeparator();
            }
            QAction* renameAction = contextMenu.addAction("Rename Clip");
            QAction* deleteAction = contextMenu.addAction("Delete Clip");
            
            QAction* chosenAction = contextMenu.exec(event->globalPosition().toPoint());
            if (addActions.contains(chosenAction)) {
                const auto& tracks = Project::instance().getTracks();
                if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size()) &&
                    clipIndex >= 0 && clipIndex < static_cast<int>(tracks[trackIndex].clips.size())) {
                    const auto& clip = tracks[trackIndex].clips[clipIndex];
                    const double leftEdgeTime = clip.timelineStart;
                    const double rightEdgeTime = clip.timelineStart + clip.sourceDuration;
                    const double clickTime = xToTime(event->pos().x());
                    const double cutTime = (std::abs(clickTime - leftEdgeTime) < std::abs(clickTime - rightEdgeTime))
                        ? leftEdgeTime
                        : rightEdgeTime;
                    emit transitionApplyRequested(trackIndex, cutTime, addActions.value(chosenAction));
                }
            } else if (chosenAction == renameAction) {
                emit renameClipRequested(trackIndex, clipIndex);
            } else if (chosenAction == deleteAction) {
                emit deleteClipRequested(trackIndex, clipIndex);
            }
        }
    }
}

void Timeline::mouseMoveEvent(QMouseEvent* event) {
    if (draggingTransitionEdge && (event->buttons() & Qt::LeftButton)) {
        auto& tracks = Project::instance().getTracks();
        if (dragTransTrackIndex >= 0 && dragTransTrackIndex < (int)tracks.size()) {
            auto& trans = tracks[dragTransTrackIndex].transitions[dragTransIndex];
            double deltaX = event->pos().x() - dragTransAnchorX;
            double deltaT = deltaX / pixelsPerSecond;
            const double kMinDur = 0.1;

            if (dragTransLeftEdge) {
                // Left edge drag: right edge (cutTime + dur/2) stays fixed
                // New left edge = originalCutTime - originalDur/2 + deltaT
                double origLeftTime = dragTransOriginalCutTime - dragTransOriginalDuration / 2.0;
                double newLeftTime = origLeftTime + deltaT;
                double fixedRight = dragTransOriginalCutTime + dragTransOriginalDuration / 2.0;
                double newDur = std::max(kMinDur, fixedRight - newLeftTime);
                trans.duration = newDur;
                trans.cutTime = fixedRight - newDur / 2.0;
            } else {
                // Right edge drag: left edge (cutTime - dur/2) stays fixed
                double origRightTime = dragTransOriginalCutTime + dragTransOriginalDuration / 2.0;
                double newRightTime = origRightTime + deltaT;
                double fixedLeft = dragTransOriginalCutTime - dragTransOriginalDuration / 2.0;
                double newDur = std::max(kMinDur, newRightTime - fixedLeft);
                trans.duration = newDur;
                trans.cutTime = fixedLeft + newDur / 2.0;
            }
        }
        update();
        return;
    }

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
            if (idx >= 0 && idx <= static_cast<int>(tracks.size())) {
                targetTrackIndex = idx;
            }
        }

        if (targetTrackIndex != dragTrackIndex) {
            const auto& tracks = Project::instance().getTracks();
            const bool createsSameTypeTrack = targetTrackIndex == static_cast<int>(tracks.size());
            if (createsSameTypeTrack || tracks[targetTrackIndex].type == tracks[dragTrackIndex].type) {
                emit clipTrackChangeRequested(dragTrackIndex, dragClipIndex, targetTrackIndex, targetStart);
            }
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
    if (draggingClip) {
        emit clipMoveFinished();
    }
    draggingClip = false;
    dragTrackIndex = -1;
    dragClipIndex = -1;
    dragClipOffsetTime = 0.0;
    draggingTransitionEdge = false;
    dragTransTrackIndex = -1;
    dragTransIndex = -1;
}

void Timeline::updateDragIndices(int newTrack, int newClip) {
    dragTrackIndex = newTrack;
    dragClipIndex = newClip;
}

void Timeline::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (selectedTransTrackIndex >= 0 && selectedTransIndex >= 0) {
            int tTrack = selectedTransTrackIndex;
            int tIdx = selectedTransIndex;
            selectedTransTrackIndex = -1;
            selectedTransIndex = -1;
            emit deleteTransitionRequested(tTrack, tIdx);
            event->accept();
            return;
        }
        if (selectedTrackIndex >= 0 && selectedClipIndex >= 0) {
            int cTrack = selectedTrackIndex;
            int cIdx = selectedClipIndex;
            selectedTrackIndex = -1;
            selectedClipIndex = -1;
            emit deleteClipRequested(cTrack, cIdx);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void Timeline::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText() || event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void Timeline::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasText() || event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void Timeline::dropEvent(QDropEvent* event) {
    int y = event->position().toPoint().y();
    if (y < 25) return;
    int trackIndex = (y - 25) / 35; 
    double dropTime = (event->position().toPoint().x() - 80) / pixelsPerSecond; 
    if (dropTime < 0) dropTime = 0;

    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                QString ext = QFileInfo(path).suffix().toLower();
                if (ext == "mp4" || ext == "m4v" || ext == "mov" || ext == "avi" || ext == "mkv" || ext == "webm" || ext == "flv" || ext == "ts") {
                    emit fileDropped(trackIndex, dropTime, path);
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }

    QString effectName;
    if (event->mimeData()->hasText()) {
        effectName = event->mimeData()->text();
    } else if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        QByteArray encoded = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
        QDataStream stream(&encoded, QIODevice::ReadOnly);
        while (!stream.atEnd()) {
            int row, col;
            QMap<int, QVariant> roleDataMap;
            stream >> row >> col >> roleDataMap;
            if (roleDataMap.contains(Qt::UserRole)) {
                effectName = roleDataMap.value(Qt::UserRole).toString();
                break;
            }
            if (roleDataMap.contains(Qt::DisplayRole)) {
                effectName = roleDataMap.value(Qt::DisplayRole).toString();
                break;
            }
        }
    }
    if (!effectName.isEmpty()) {
        emit effectDropped(trackIndex, dropTime, effectName);
    }
    event->acceptProposedAction();
}
