#include "inspector.h"
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
#include <QDebug>
#include <functional>
#include <algorithm>

namespace {
class CurvePreviewWidget : public QWidget {
public:
    CurvePreviewWidget(const ShaderParameter& param,
                       const double* playheadTime,
                       QWidget* parent = nullptr)
        : QWidget(parent), param(param), playheadTime(playheadTime) {
        setMinimumHeight(44);
        setCursor(Qt::PointingHandCursor);
        setToolTip("");
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
        p.fillRect(rect(), QColor(20, 20, 20));
        p.setPen(QColor(60, 60, 60));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        const auto& keyframes = param.curve.getKeyframes();
        if (keyframes.empty()) {
            p.setPen(QColor(120, 120, 120));
            p.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, "No keyframes yet");
        } else {
            double minTime = keyframes.front().time;
            double maxTime = keyframes.back().time;
            double timeSpan = std::max(1.0, maxTime - minTime);
            double valueSpan = std::max(0.0001, param.maxVal - param.minVal);

            auto toPoint = [&](double time, double value) {
                double tn = (time - minTime) / timeSpan;
                double vn = (value - param.minVal) / valueSpan;
                int x = 8 + static_cast<int>(tn * (width() - 16));
                int y = height() - 8 - static_cast<int>(vn * (height() - 16));
                return QPoint(x, y);
            };

            p.setPen(QPen(QColor(255, 85, 0), 1.5));
            QPoint prev;
            bool havePrev = false;
            for (const auto& kf : keyframes) {
                QPoint pt = toPoint(kf.time, kf.value);
                if (havePrev) p.drawLine(prev, pt);
                prev = pt;
                havePrev = true;
            }

            p.setBrush(QColor(255, 85, 0));
            p.setPen(Qt::NoPen);
            for (const auto& kf : keyframes) {
                QPoint pt = toPoint(kf.time, kf.value);
                p.drawEllipse(pt, 4, 4);
            }

            if (playheadTime) {
                double ph = *playheadTime;
                int x = 8 + static_cast<int>(((ph - minTime) / timeSpan) * (width() - 16));
                p.setPen(QPen(QColor(255, 190, 90), 1, Qt::DashLine));
                p.drawLine(x, 4, x, height() - 4);
            }
        }
    }

private:
    ShaderParameter param;
    const double* playheadTime = nullptr;
};

class ScrubLabel : public QLabel {
    Q_OBJECT
public:
    ScrubLabel(double minVal, double maxVal, double initialVal, QWidget* parent = nullptr)
        : QLabel(parent), minVal(minVal), maxVal(maxVal), currentVal(initialVal) {
        setCursor(Qt::SizeHorCursor);
        setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        setStyleSheet("color: #e855f4; font-family: monospace; font-weight: bold; background: #1a1a1a; padding: 2px 4px; border-radius: 2px;");
        setMinimumWidth(60);
        updateText();
    }

    void setValue(double val) {
        currentVal = std::clamp(val, minVal, maxVal);
        updateText();
    }
    double getValue() const { return currentVal; }

signals:
    void valueChanged(double val);

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = true;
            lastX = event->position().x();
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
            emit valueChanged(currentVal);
        }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isDragging = false;
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
        emit valueChanged(currentVal);
        event->accept();
    }

private:
    double minVal, maxVal, currentVal;
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
}

void Inspector::clearInspector() {
    activeEffectId.clear();
    timeUpdateCallbacks.clear();
    QLayoutItem* item;
    while ((item = scrollLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
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

void Inspector::loadEffect(const QString& effectId, std::vector<ShaderParameter>& parameters, double currentTime) {
    clearInspector();
    activeEffectId = effectId;
    currentPlayheadTime = currentTime;

    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* titleLabel = new QLabel(QString("EFFECT: %1").arg(effectId.toUpper()), this);
    titleLabel->setStyleSheet("font-weight: bold; color: #ff9090; font-size: 12px;");
    headerLayout->addWidget(titleLabel, 1);

    QPushButton* removeButton = new QPushButton("Remove", this);
    removeButton->setStyleSheet(
        "QPushButton { background: #341b1b; border: 1px solid #6d2a2a; color: #ffb1b1; padding: 4px 8px; border-radius: 3px; }"
        "QPushButton:hover { background: #8f2f2f; color: white; }"
    );
    connect(removeButton, &QPushButton::clicked, this, [this, effectId]() {
        emit removeEffectRequested(effectId);
    });
    headerLayout->addWidget(removeButton);

    scrollLayout->addWidget(headerWidget);

    QLabel* helpLabel = new QLabel("", this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: #8a8a8a; font-size: 10px;");
    scrollLayout->addWidget(helpLabel);

    for (const auto& param : parameters) {
        QWidget* rowWidget = new QWidget(this);
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(6);
        QPushButton* labelButton = new QPushButton(QString::fromStdString(param.label), this);
        labelButton->setFlat(true);
        labelButton->setStyleSheet("text-align: left; color: #dcdcdc; padding: 2px; font-weight: 500;");
        labelButton->setToolTip("Click to edit keyframe curve on Timeline");
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
            inputWidget = checkBox;
            rowLayout->addWidget(checkBox, 3);
            valLabel = new QLabel(checkBox->isChecked() ? "ON" : "OFF", this);
            valLabel->setMinimumWidth(35);
            valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            valLabel->setStyleSheet("color: #888888; font-family: monospace;");
            rowLayout->addWidget(valLabel);
            connect(checkBox, &QCheckBox::checkStateChanged, this, [this, effectId, paramName, valLabel](Qt::CheckState state) {
                valLabel->setText(state == Qt::Checked ? "ON" : "OFF");
                emit parameterChanged(effectId, paramName, state == Qt::Checked ? 1.0 : 0.0);
            });
        } else {
            ScrubLabel* scrub = new ScrubLabel(param.minVal, param.maxVal, currentVal, this);
            inputWidget = scrub;
            rowLayout->addWidget(scrub, 3);
            connect(scrub, &ScrubLabel::valueChanged, this, [this, effectId, paramName, param](double val) {
                emit parameterChanged(effectId, paramName, val);
            });
        }

        bool hasKfAtPlayhead = false;
        for (const auto& kf : param.curve.getKeyframes()) {
            if (std::abs(kf.time - currentPlayheadTime) < 0.001) {
                hasKfAtPlayhead = true;
                break;
            }
        }

        QPushButton* prevKfButton = new QPushButton("<", this);
        prevKfButton->setFixedWidth(20);
        prevKfButton->setToolTip("Previous Keyframe");
        prevKfButton->setStyleSheet("QPushButton { background: transparent; color: #888; border: none; font-weight: bold; } QPushButton:hover { color: white; }");
        QPushButton* kfButton = new QPushButton("◆", this);
        kfButton->setFixedWidth(24);
        kfButton->setToolTip(hasKfAtPlayhead ? "Remove keyframe" : "Add keyframe");
        QString kfStyle = hasKfAtPlayhead ? 
            "QPushButton { background: transparent; color: #00a8ff; border: none; font-size: 16px; }" : 
            "QPushButton { background: transparent; color: #555; border: none; font-size: 16px; } QPushButton:hover { color: #888; }";
        kfButton->setStyleSheet(kfStyle);

        QPushButton* nextKfButton = new QPushButton(">", this);
        nextKfButton->setFixedWidth(20);
        nextKfButton->setToolTip("Next Keyframe");
        nextKfButton->setStyleSheet("QPushButton { background: transparent; color: #888; border: none; font-weight: bold; } QPushButton:hover { color: white; }");

        connect(prevKfButton, &QPushButton::clicked, this, [this, param]() {
            double bestTime = -1.0;
            for (const auto& kf : param.curve.getKeyframes()) {
                if (kf.time < currentPlayheadTime - 0.001) {
                    bestTime = kf.time;
                }
            }
            if (bestTime >= 0) emit scrubRequested(bestTime);
        });
        connect(nextKfButton, &QPushButton::clicked, this, [this, param]() {
            for (const auto& kf : param.curve.getKeyframes()) {
                if (kf.time > currentPlayheadTime + 0.001) {
                    emit scrubRequested(kf.time);
                    break;
                }
            }
        });

        connect(kfButton, &QPushButton::clicked, this, [this, effectId, paramName, inputWidget, param]() {
            bool currentHasKf = false;
            for (const auto& kf : param.curve.getKeyframes()) {
                if (std::abs(kf.time - currentPlayheadTime) < 0.001) { currentHasKf = true; break; }
            }
            if (currentHasKf) {
                emit keyframeRemoveRequested(effectId, paramName, currentPlayheadTime);
            } else {
                double realVal = 0.0;
                if (param.isBool) {
                    realVal = static_cast<QCheckBox*>(inputWidget)->isChecked() ? 1.0 : 0.0;
                } else {
                    realVal = static_cast<ScrubLabel*>(inputWidget)->getValue();
                }
                emit keyframeRequested(effectId, paramName, currentPlayheadTime, realVal);
            }
        });

        timeUpdateCallbacks.push_back([inputWidget, valLabel, kfButton, param](double t) {
            bool hasKf = false;
            if (!param.curve.getKeyframes().empty()) {
                double val = param.curve.evaluate(t);
                if (param.isBool) {
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
                for (const auto& kf : param.curve.getKeyframes()) {
                    if (std::abs(kf.time - t) < 0.001) { hasKf = true; break; }
                }
            }
            QString kfStyle = hasKf ? 
                "QPushButton { background: transparent; color: #00a8ff; border: none; font-size: 16px; }" : 
                "QPushButton { background: transparent; color: #555; border: none; font-size: 16px; } QPushButton:hover { color: #888; }";
            kfButton->setStyleSheet(kfStyle);
            kfButton->setToolTip(hasKf ? "Remove keyframe" : "Add keyframe");
        });

        rowLayout->addWidget(prevKfButton);
        rowLayout->addWidget(kfButton);
        rowLayout->addWidget(nextKfButton);
        scrollLayout->addWidget(rowWidget);

        CurvePreviewWidget* curvePreview = new CurvePreviewWidget(param, &currentPlayheadTime, this);
        curvePreview->onAddKeyframe = [this, effectId, paramName, inputWidget, param]() {
            double realVal = 0.0;
            if (param.isBool) {
                realVal = static_cast<QCheckBox*>(inputWidget)->isChecked() ? 1.0 : 0.0;
            } else {
                realVal = static_cast<ScrubLabel*>(inputWidget)->getValue();
            }
            emit keyframeRequested(effectId, paramName, currentPlayheadTime, realVal);
        };
        curvePreview->onRemoveKeyframe = [this, effectId, paramName]() {
            emit keyframeRemoveRequested(effectId, paramName, currentPlayheadTime);
        };
        scrollLayout->addWidget(curvePreview);
    }
}

#include "inspector.moc"
