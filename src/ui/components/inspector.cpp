#include "inspector.h"
#include "ui/vector_icons.h"
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>
#include <QPainter>
#include <QMouseEvent>
#include <QColor>
#include <QInputDialog>
#include <QComboBox>
#include <QDebug>
#include <functional>
#include <algorithm>

namespace {
class CurvePreviewWidget : public QWidget {
public:
    CurvePreviewWidget(ShaderParameter param,
                       double playheadTime,
                       QWidget* parent = nullptr)
        : QWidget(parent), m_param(std::move(param)), m_playheadTime(playheadTime) {
        setMinimumHeight(44);
        setCursor(Qt::PointingHandCursor);
        setToolTip("Click to add keyframe at playhead | Right-click to remove keyframe");
    }

    void setParam(const ShaderParameter& param) {
        m_param = param;
        update();
    }

    void setPlayheadTime(double time) {
        m_playheadTime = time;
        update();
    }

    std::function<void()> onAddKeyframe;
    std::function<void()> onRemoveKeyframe;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onAddKeyframe) {
            onAddKeyframe();
            return;
        }
        if (event->button() == Qt::RightButton && onRemoveKeyframe) {
            onRemoveKeyframe();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(14, 12, 18));
        p.setPen(QColor(36, 28, 46));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        const auto& keyframes = m_param.curve.getKeyframes();
        if (keyframes.empty()) {
            p.setPen(QColor(110, 100, 125));
            p.drawText(rect().adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, "Static value (Click to add keyframe curve)");
        } else {
            double minTime = keyframes.front().time;
            double maxTime = keyframes.back().time;
            double timeSpan = std::max(1.0, maxTime - minTime);
            double valueSpan = std::max(0.0001, m_param.maxVal - m_param.minVal);

            auto toPoint = [&](double time, double value) {
                double tn = (time - minTime) / timeSpan;
                double vn = (value - m_param.minVal) / valueSpan;
                int x = 8 + static_cast<int>(tn * (width() - 16));
                int y = height() - 8 - static_cast<int>(vn * (height() - 16));
                return QPoint(x, y);
            };

            p.setPen(QPen(QColor(217, 70, 239), 1.5));
            QPoint prev;
            bool havePrev = false;
            for (const auto& kf : keyframes) {
                QPoint pt = toPoint(kf.time, kf.value);
                if (havePrev) p.drawLine(prev, pt);
                prev = pt;
                havePrev = true;
            }

            p.setBrush(QColor(232, 85, 244));
            p.setPen(Qt::NoPen);
            for (const auto& kf : keyframes) {
                QPoint pt = toPoint(kf.time, kf.value);
                p.drawEllipse(pt, 4, 4);
            }

            int x = 8 + static_cast<int>(((m_playheadTime - minTime) / timeSpan) * (width() - 16));
            p.setPen(QPen(QColor(245, 158, 248), 1.5, Qt::DashLine));
            p.drawLine(x, 2, x, height() - 2);
        }
    }

private:
    ShaderParameter m_param;
    double m_playheadTime = 0.0;
};

class ScrubLabel : public QLabel {
public:
    ScrubLabel(double minVal, double maxVal, double defaultVal, double initialVal, QWidget* parent = nullptr)
        : QLabel(parent), minVal(minVal), maxVal(maxVal), defaultVal(defaultVal), currentVal(initialVal) {
        setCursor(Qt::SizeHorCursor);
        setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        setStyleSheet("color: #f59ef8; font-family: 'JetBrains Mono', monospace; font-weight: bold; background: #16161c; border: 1px solid #2e2e3a; padding: 2px 6px; border-radius: 3px;");
        setMinimumWidth(65);
        setToolTip("Click & Drag to scrub value\nDouble-click to type value\nRight-click to reset to default");
        updateText();
    }

    void setValue(double val) {
        currentVal = std::clamp(val, minVal, maxVal);
        updateText();
    }
    double getValue() const { return currentVal; }

    std::function<void(double)> onValueChanged;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = true;
            lastX = event->position().x();
        } else if (event->button() == Qt::RightButton) {
            currentVal = defaultVal;
            updateText();
            if (onValueChanged) onValueChanged(currentVal);
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (isDragging) {
            int dx = event->position().x() - lastX;
            lastX = event->position().x();
            double range = maxVal - minVal;
            double step = (range > 0.0 ? range : 1.0) * 0.005; 
            currentVal += dx * step;
            currentVal = std::clamp(currentVal, minVal, maxVal);
            updateText();
            if (onValueChanged) onValueChanged(currentVal);
        }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = false;
        }
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
        bool ok = false;
        double val = QInputDialog::getDouble(
            this, "Enter Parameter Value", "Value:", 
            currentVal, minVal, maxVal, 4, &ok
        );
        if (ok) {
            currentVal = std::clamp(val, minVal, maxVal);
            updateText();
            if (onValueChanged) onValueChanged(currentVal);
        }
    }
    void wheelEvent(QWheelEvent* event) override {
        double range = maxVal - minVal;
        double step = (range > 0.0 ? range : 1.0) * 0.05;
        if (event->angleDelta().y() > 0) {
            currentVal += step;
        } else {
            currentVal -= step;
        }
        currentVal = std::clamp(currentVal, minVal, maxVal);
        updateText();
        if (onValueChanged) onValueChanged(currentVal);
        event->accept();
    }

private:
    double minVal, maxVal, defaultVal, currentVal;
    bool isDragging = false;
    int lastX = 0;

    void updateText() {
        setText(QString::number(currentVal, 'f', 3));
    }
};
}

Inspector::Inspector(QWidget* parent) : QWidget(parent) {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollWidget = new QWidget(scrollArea);
    scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setAlignment(Qt::AlignTop);
    scrollLayout->setSpacing(8);

    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    clearInspector();
}

void Inspector::clearInspector() {
    activeEffectId.clear();
    currentParameters.clear();
    timeUpdateCallbacks.clear();
    QLayoutItem* item;
    while ((item = scrollLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QWidget* emptyWidget = new QWidget(this);
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setContentsMargins(20, 60, 20, 20);
    emptyLayout->setSpacing(8);
    emptyLayout->setAlignment(Qt::AlignCenter);

    QLabel* noSelLabel = new QLabel("NO EFFECT SELECTED", emptyWidget);
    noSelLabel->setAlignment(Qt::AlignCenter);
    noSelLabel->setStyleSheet("font-weight: bold; font-size: 11px; letter-spacing: 1px; color: #a0a0b5;");
    emptyLayout->addWidget(noSelLabel);

    QLabel* hintLabel = new QLabel("Select an active effect from the stack or double-click an effect from the Library to inspect and edit parameters.", emptyWidget);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #68687a; font-size: 10px; line-height: 14px;");
    emptyLayout->addWidget(hintLabel);

    scrollLayout->addWidget(emptyWidget);
}

void Inspector::setCurrentTime(double currentTime) {
    currentPlayheadTime = currentTime;
    for (auto& cb : timeUpdateCallbacks) {
        cb(currentTime);
    }
    if (scrollWidget) {
        scrollWidget->update();
    }
}

void Inspector::updateParameterValue(const QString& paramName, double value) {
    for (auto& p : currentParameters) {
        if (QString::fromStdString(p.name) == paramName) {
            p.currentVal = value;
            p.curve.setDefaultValue(value);
            break;
        }
    }
    setCurrentTime(currentPlayheadTime);
}

void Inspector::syncParameters(const std::vector<ShaderParameter>& parameters) {
    if (parameters.size() != currentParameters.size()) {
        // Structure changed — caller should reload via loadEffect.
        currentParameters = parameters;
    } else {
        for (size_t i = 0; i < parameters.size(); ++i) {
            currentParameters[i].currentVal = parameters[i].currentVal;
            currentParameters[i].curve = parameters[i].curve;
        }
    }
    setCurrentTime(currentPlayheadTime);
}

void Inspector::loadEffect(const QString& effectId, const std::vector<ShaderParameter>& parameters, double currentTime) {
    activeEffectId.clear();
    currentParameters.clear();
    timeUpdateCallbacks.clear();
    QLayoutItem* item;
    while ((item = scrollLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    activeEffectId = effectId;
    currentParameters = parameters;
    currentPlayheadTime = currentTime;

    QWidget* headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: #1e1e26; border: 1px solid #2e2e3c; border-radius: 4px; padding: 4px;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(6, 4, 6, 4);

    QLabel* titleLabel = new QLabel(QString("EFFECT: %1").arg(effectId.toUpper()), this);
    titleLabel->setStyleSheet("font-weight: bold; color: #f59ef8; font-size: 11px; letter-spacing: 0.5px; border: none; background: transparent;");
    headerLayout->addWidget(titleLabel, 1);

    QPushButton* removeButton = new QPushButton("Remove", this);
    removeButton->setStyleSheet(
        "QPushButton { background: #341b24; border: 1px solid #6d2a4a; color: #ffb1cb; padding: 3px 8px; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background: #8f2f54; color: white; }"
    );
    connect(removeButton, &QPushButton::clicked, this, [this, effectId]() {
        emit removeEffectRequested(effectId);
    });
    headerLayout->addWidget(removeButton);

    scrollLayout->addWidget(headerWidget);

    for (size_t pIdx = 0; pIdx < currentParameters.size(); ++pIdx) {
        const auto& param = currentParameters[pIdx];
        QWidget* rowWidget = new QWidget(this);
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(6);

        QPushButton* labelButton = new QPushButton(QString::fromStdString(param.label), this);
        labelButton->setFlat(true);
        labelButton->setStyleSheet("text-align: left; color: #dcdcdc; padding: 2px; font-weight: 600; font-size: 11px;");
        labelButton->setToolTip("Click to select parameter for Timeline keyframe editing");
        QString paramName = QString::fromStdString(param.name);
        connect(labelButton, &QPushButton::clicked, this, [this, effectId, paramName]() {
            emit parameterSelected(effectId, paramName);
        });
        rowLayout->addWidget(labelButton, 2);

        double currentVal = param.curve.evaluate(currentTime);

        QWidget* inputWidget = nullptr;
        QLabel* valLabel = nullptr;

        if (param.isBool) {
            QCheckBox* checkBox = new QCheckBox(this);
            checkBox->setChecked(currentVal > 0.5);
            checkBox->setStyleSheet(
                "QCheckBox { spacing: 6px; }"
                "QCheckBox::indicator {"
                "  width: 16px;"
                "  height: 16px;"
                "  border: 1px solid #b06ac8;"
                "  border-radius: 3px;"
                "  background: #151515;"
                "}"
                "QCheckBox::indicator:hover { border-color: #df42f5; background: #202020; }"
                "QCheckBox::indicator:checked {"
                "  border: 1px solid #e855f4;"
                "  background: #5a1a63;"
                "}"
            );
            inputWidget = checkBox;
            rowLayout->addWidget(checkBox, 3);
            valLabel = new QLabel(checkBox->isChecked() ? "ON" : "OFF", this);
            valLabel->setMinimumWidth(35);
            valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            valLabel->setStyleSheet("color: #888888; font-family: monospace;");
            rowLayout->addWidget(valLabel);
            connect(checkBox, &QCheckBox::toggled, this, [this, effectId, paramName, valLabel, pIdx](bool checked) {
                valLabel->setText(checked ? "ON" : "OFF");
                if (pIdx < currentParameters.size()) {
                    currentParameters[pIdx].currentVal = checked ? 1.0 : 0.0;
                    currentParameters[pIdx].curve.setDefaultValue(checked ? 1.0 : 0.0);
                }
                emit parameterChanged(effectId, paramName, checked ? 1.0 : 0.0);
            });
        } else {
            ScrubLabel* scrub = new ScrubLabel(param.minVal, param.maxVal, param.defaultVal, currentVal, this);
            inputWidget = scrub;
            rowLayout->addWidget(scrub, 3);
            scrub->onValueChanged = [this, effectId, paramName, pIdx](double val) {
                if (pIdx < currentParameters.size()) {
                    currentParameters[pIdx].currentVal = val;
                    currentParameters[pIdx].curve.setDefaultValue(val);
                }
                emit parameterChanged(effectId, paramName, val);
            };
        }

        bool hasKfAtPlayhead = false;
        for (const auto& kf : param.curve.getKeyframes()) {
            if (std::abs(kf.time - currentPlayheadTime) < 0.001) {
                hasKfAtPlayhead = true;
                break;
            }
        }

        QPushButton* prevKfButton = new QPushButton(this);
        prevKfButton->setIcon(VectorIcon::create(VectorIcon::Type::KeyframePrev, QColor(160, 150, 180), QSize(14, 14)));
        prevKfButton->setIconSize(QSize(14, 14));
        prevKfButton->setFixedWidth(22);
        prevKfButton->setToolTip("Jump to previous keyframe");
        prevKfButton->setStyleSheet("QPushButton { background: #16141e; border: 1px solid #282034; border-radius: 2px; } QPushButton:hover { background: #261c30; border-color: #d946ef; }");
        
        QPushButton* kfButton = new QPushButton(this);
        QColor kfColor = hasKfAtPlayhead ? QColor(245, 158, 248) : QColor(110, 100, 125);
        kfButton->setIcon(VectorIcon::create(VectorIcon::Type::Keyframe, kfColor, QSize(14, 14)));
        kfButton->setIconSize(QSize(14, 14));
        kfButton->setFixedWidth(24);
        kfButton->setToolTip(hasKfAtPlayhead ? "Remove keyframe at playhead" : "Add keyframe at playhead");
        QString kfStyle = hasKfAtPlayhead ? 
            "QPushButton { background: #3b1548; border: 1px solid #d946ef; border-radius: 2px; }" : 
            "QPushButton { background: #16141e; border: 1px solid #282034; border-radius: 2px; } QPushButton:hover { border-color: #d946ef; }";
        kfButton->setStyleSheet(kfStyle);

        QPushButton* nextKfButton = new QPushButton(this);
        nextKfButton->setIcon(VectorIcon::create(VectorIcon::Type::KeyframeNext, QColor(160, 150, 180), QSize(14, 14)));
        nextKfButton->setIconSize(QSize(14, 14));
        nextKfButton->setFixedWidth(22);
        nextKfButton->setToolTip("Jump to next keyframe");
        nextKfButton->setStyleSheet("QPushButton { background: #16141e; border: 1px solid #282034; border-radius: 2px; } QPushButton:hover { background: #261c30; border-color: #d946ef; }");

        connect(prevKfButton, &QPushButton::clicked, this, [this, pIdx]() {
            if (pIdx >= currentParameters.size()) return;
            double bestTime = -1.0;
            for (const auto& kf : currentParameters[pIdx].curve.getKeyframes()) {
                if (kf.time < currentPlayheadTime - 0.001) {
                    bestTime = kf.time;
                }
            }
            if (bestTime >= 0) emit scrubRequested(bestTime);
        });
        connect(nextKfButton, &QPushButton::clicked, this, [this, pIdx]() {
            if (pIdx >= currentParameters.size()) return;
            for (const auto& kf : currentParameters[pIdx].curve.getKeyframes()) {
                if (kf.time > currentPlayheadTime + 0.001) {
                    emit scrubRequested(kf.time);
                    break;
                }
            }
        });

        connect(kfButton, &QPushButton::clicked, this, [this, effectId, paramName, inputWidget, pIdx]() {
            if (pIdx >= currentParameters.size()) return;
            bool currentHasKf = false;
            for (const auto& kf : currentParameters[pIdx].curve.getKeyframes()) {
                if (std::abs(kf.time - currentPlayheadTime) < 0.001) { currentHasKf = true; break; }
            }
            if (currentHasKf) {
                emit keyframeRemoveRequested(effectId, paramName, currentPlayheadTime);
            } else {
                double realVal = 0.0;
                if (currentParameters[pIdx].isBool) {
                    realVal = static_cast<QCheckBox*>(inputWidget)->isChecked() ? 1.0 : 0.0;
                } else {
                    realVal = static_cast<ScrubLabel*>(inputWidget)->getValue();
                }
                emit keyframeRequested(effectId, paramName, currentPlayheadTime, realVal);
            }
        });

        CurvePreviewWidget* curvePreview = new CurvePreviewWidget(param, currentTime, this);
        curvePreview->onAddKeyframe = [this, effectId, paramName, inputWidget, pIdx]() {
            if (pIdx >= currentParameters.size()) return;
            double realVal = 0.0;
            if (currentParameters[pIdx].isBool) {
                realVal = static_cast<QCheckBox*>(inputWidget)->isChecked() ? 1.0 : 0.0;
            } else {
                realVal = static_cast<ScrubLabel*>(inputWidget)->getValue();
            }
            emit keyframeRequested(effectId, paramName, currentPlayheadTime, realVal);
        };
        curvePreview->onRemoveKeyframe = [this, effectId, paramName]() {
            emit keyframeRemoveRequested(effectId, paramName, currentPlayheadTime);
        };

        QComboBox* interpolation = new QComboBox(this);
        interpolation->addItems({"Linear", "Hold", "Bezier"});
        interpolation->setToolTip("Interpolation for the keyframe at the playhead");
        interpolation->setEnabled(hasKfAtPlayhead);
        for (const auto& kf : param.curve.getKeyframes()) {
            if (std::abs(kf.time - currentPlayheadTime) < 0.001) {
                interpolation->setCurrentIndex(static_cast<int>(kf.mode));
                break;
            }
        }
        connect(interpolation, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, effectId, paramName](int mode) {
            emit keyframeInterpolationRequested(effectId, paramName, currentPlayheadTime, mode);
        });

        timeUpdateCallbacks.push_back([this, inputWidget, valLabel, kfButton, interpolation, curvePreview, pIdx](double t) {
            if (pIdx >= currentParameters.size()) return;
            const auto& curParam = currentParameters[pIdx];
            bool hasKf = false;
            if (!curParam.curve.getKeyframes().empty()) {
                double val = curParam.curve.evaluate(t);
                if (curParam.isBool) {
                    auto* cb = static_cast<QCheckBox*>(inputWidget);
                    bool oldState = cb->signalsBlocked();
                    cb->blockSignals(true);
                    cb->setChecked(val > 0.5);
                    cb->blockSignals(oldState);
                    if (valLabel) valLabel->setText(val > 0.5 ? "ON" : "OFF");
                } else {
                    auto* scrub = static_cast<ScrubLabel*>(inputWidget);
                    bool oldState = scrub->signalsBlocked();
                    scrub->blockSignals(true);
                    scrub->setValue(val);
                    scrub->blockSignals(oldState);
                }
            for (const auto& kf : curParam.curve.getKeyframes()) {
                    if (std::abs(kf.time - t) < 0.001) {
                        hasKf = true;
                        const bool wasBlocked = interpolation->signalsBlocked();
                        interpolation->blockSignals(true);
                        interpolation->setCurrentIndex(static_cast<int>(kf.mode));
                        interpolation->blockSignals(wasBlocked);
                        break;
                    }
                }
            }
            interpolation->setEnabled(hasKf);
            curvePreview->setParam(curParam);
            curvePreview->setPlayheadTime(t);

            QString kfStyle = hasKf ? 
                "QPushButton { background: #3b1548; border: 1px solid #d946ef; border-radius: 2px; }" : 
                "QPushButton { background: #16141e; border: 1px solid #282034; border-radius: 2px; } QPushButton:hover { border-color: #d946ef; }";
            kfButton->setStyleSheet(kfStyle);
            kfButton->setIcon(VectorIcon::create(VectorIcon::Type::Keyframe, hasKf ? QColor(245, 158, 248) : QColor(110, 100, 125), QSize(14, 14)));
            kfButton->setToolTip(hasKf ? "Remove keyframe at playhead" : "Add keyframe at playhead");
        });

        rowLayout->addWidget(prevKfButton);
        rowLayout->addWidget(kfButton);
        rowLayout->addWidget(nextKfButton);
        scrollLayout->addWidget(rowWidget);

        scrollLayout->addWidget(interpolation);
        scrollLayout->addWidget(curvePreview);
    }
}
