#ifndef INSPECTOR_H
#define INSPECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <vector>
#include <functional>
#include "engine/pluginmanager.h"

class Inspector : public QWidget {
    Q_OBJECT
public:
    explicit Inspector(QWidget* parent = nullptr);

    void loadEffect(const QString& effectId, const std::vector<ShaderParameter>& parameters, double currentTime);
    void clearInspector();
    void setCurrentTime(double currentTime);
    void updateParameterValue(const QString& paramName, double value);
    void syncParameters(const std::vector<ShaderParameter>& parameters);

signals:
    void parameterChanged(const QString& effectId, const QString& paramName, double value);
    void keyframeRequested(const QString& effectId, const QString& paramName, double time, double value);
    void keyframeRemoveRequested(const QString& effectId, const QString& paramName, double time);
    void keyframeInterpolationRequested(const QString& effectId, const QString& paramName, double time, int mode);
    void parameterSelected(const QString& effectId, const QString& paramName);
    void removeEffectRequested(const QString& effectId);
    void scrubRequested(double time);

private:
    QString activeEffectId;
    double currentPlayheadTime = 0.0;
    std::vector<ShaderParameter> currentParameters;

    QVBoxLayout* mainLayout = nullptr;
    QScrollArea* scrollArea = nullptr;
    QWidget* scrollWidget = nullptr;
    QVBoxLayout* scrollLayout = nullptr;

    std::vector<std::function<void(double)>> timeUpdateCallbacks;
};

#endif
