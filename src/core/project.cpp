#include "engine/pluginmanager.h"
#include "project.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

bool Project::load(const std::string& filePath) {
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }

    fromJson(doc.object());
    return true;
}

void Project::fromJson(const QJsonObject& root) {
    clear();

    QJsonArray tracksArray = root.value("tracks").toArray();
    for (int i = 0; i < tracksArray.size(); ++i) {
        QJsonObject trackObj = tracksArray[i].toObject();
        TimelineTrack track;
        track.id = trackObj.value("id").toInt();
        track.name = trackObj.value("name").toString().toStdString();

        QJsonArray clipsArray = trackObj.value("clips").toArray();
        for (int j = 0; j < clipsArray.size(); ++j) {
            QJsonObject clipObj = clipsArray[j].toObject();
            ProjectClip clip;
            clip.id = clipObj.value("id").toString().toStdString();
            clip.filePath = clipObj.value("filePath").toString().toStdString();
            clip.sourceStart = clipObj.value("sourceStart").toDouble();
            clip.sourceDuration = clipObj.value("sourceDuration").toDouble();
            clip.timelineStart = clipObj.value("timelineStart").toDouble();
            clip.useClipEffects = clipObj.value("useClipEffects").toBool(false);

            QJsonArray clipEffectsArray = clipObj.value("effects").toArray();
            for (int e = 0; e < clipEffectsArray.size(); ++e) {
                QJsonObject effectObj = clipEffectsArray[e].toObject();
                AppliedEffect effect;
                effect.pluginId = effectObj.value("pluginId").toString().toStdString();

                QJsonArray paramsArray = effectObj.value("parameters").toArray();
                for (int k = 0; k < paramsArray.size(); ++k) {
                    QJsonObject paramObj = paramsArray[k].toObject();
                    ShaderParameter param;
                    param.name = paramObj.value("name").toString().toStdString();
                    param.label = paramObj.value("label").toString().toStdString();
                    param.minVal = paramObj.value("min").toDouble();
                    param.maxVal = paramObj.value("max").toDouble();
                    param.defaultVal = paramObj.value("default").toDouble();
                    param.currentVal = paramObj.value("value").toDouble();
                    param.curve = AnimationCurve(param.defaultVal);

                    QJsonArray curvesArray = paramObj.value("keyframes").toArray();
                    for (int m = 0; m < curvesArray.size(); ++m) {
                        QJsonObject kfObj = curvesArray[m].toObject();
                        double t = kfObj.value("time").toDouble();
                        double v = kfObj.value("value").toDouble();
                        int modeInt = kfObj.value("mode").toInt(0);
                        param.curve.insertKeyframe(t, v, static_cast<InterpolationMode>(modeInt));
                    }

                    effect.parameters.push_back(param);
                }

                if (effect.pluginId == "xor") effect.pluginId = "xor_gate";
                if (effect.pluginId == "feedback") effect.pluginId = "temporal_echo";
                auto* plugin = PluginManager::instance().findPlugin(effect.pluginId);

                if (plugin) {
                    if (effect.parameters.empty()) {
                        effect.parameters = plugin->parameters;
                    } else {
                        for (const auto& p : plugin->parameters) {
                            bool found = false;
                            for (auto& ep : effect.parameters) {
                                if (ep.name == p.name) { found = true; break; }
                            }
                            if (!found) effect.parameters.push_back(p);
                        }
                    }
                }
                clip.effects.push_back(effect);

            }
            track.clips.push_back(clip);
        }

        QJsonArray effectsArray = trackObj.value("effects").toArray();
        for (int j = 0; j < effectsArray.size(); ++j) {
            QJsonObject effectObj = effectsArray[j].toObject();
            AppliedEffect effect;
            effect.pluginId = effectObj.value("pluginId").toString().toStdString();

            QJsonArray paramsArray = effectObj.value("parameters").toArray();
            for (int k = 0; k < paramsArray.size(); ++k) {
                QJsonObject paramObj = paramsArray[k].toObject();
                ShaderParameter param;
                param.name = paramObj.value("name").toString().toStdString();
                param.label = paramObj.value("label").toString().toStdString();
                param.minVal = paramObj.value("min").toDouble();
                param.maxVal = paramObj.value("max").toDouble();
                param.defaultVal = paramObj.value("default").toDouble();
                param.currentVal = paramObj.value("value").toDouble();
                param.curve = AnimationCurve(param.defaultVal);
                QJsonArray curvesArray = paramObj.value("keyframes").toArray();
                for (int m = 0; m < curvesArray.size(); ++m) {
                    QJsonObject kfObj = curvesArray[m].toObject();
                    double t = kfObj.value("time").toDouble();
                    double v = kfObj.value("value").toDouble();
                    int modeInt = kfObj.value("mode").toInt(0);
                    param.curve.insertKeyframe(t, v, static_cast<InterpolationMode>(modeInt));
                }
                effect.parameters.push_back(param);
            }
                if (effect.pluginId == "xor") effect.pluginId = "xor_gate";
                if (effect.pluginId == "feedback") effect.pluginId = "temporal_echo";
                auto* plugin = PluginManager::instance().findPlugin(effect.pluginId);

                if (plugin) {
                    if (effect.parameters.empty()) {
                        effect.parameters = plugin->parameters;
                    } else {
                        for (const auto& p : plugin->parameters) {
                            bool found = false;
                            for (auto& ep : effect.parameters) {
                                if (ep.name == p.name) { found = true; break; }
                            }
                            if (!found) effect.parameters.push_back(p);
                        }
                    }
                }
                track.effects.push_back(effect);
        }

        QJsonArray transitionsArray = trackObj.value("transitions").toArray();
        for (int j = 0; j < transitionsArray.size(); ++j) {
            QJsonObject transObj = transitionsArray[j].toObject();
            ProjectTransition transition;
            transition.id = transObj.value("id").toString().toStdString();
            transition.pluginId = transObj.value("pluginId").toString().toStdString();
            transition.leftClipId = transObj.value("leftClipId").toString().toStdString();
            transition.rightClipId = transObj.value("rightClipId").toString().toStdString();
            transition.duration = transObj.value("duration").toDouble();

            QJsonArray paramsArray = transObj.value("parameters").toArray();
            for (int k = 0; k < paramsArray.size(); ++k) {
                QJsonObject paramObj = paramsArray[k].toObject();
                ShaderParameter param;
                param.name = paramObj.value("name").toString().toStdString();
                param.label = paramObj.value("label").toString().toStdString();
                param.minVal = paramObj.value("min").toDouble();
                param.maxVal = paramObj.value("max").toDouble();
                param.defaultVal = paramObj.value("default").toDouble();
                param.currentVal = paramObj.value("value").toDouble();
                param.curve = AnimationCurve(param.defaultVal);
                QJsonArray curvesArray = paramObj.value("keyframes").toArray();
                for (int m = 0; m < curvesArray.size(); ++m) {
                    QJsonObject kfObj = curvesArray[m].toObject();
                    double t = kfObj.value("time").toDouble();
                    double v = kfObj.value("value").toDouble();
                    int modeInt = kfObj.value("mode").toInt(0);
                    param.curve.insertKeyframe(t, v, static_cast<InterpolationMode>(modeInt));
                }
                transition.parameters.push_back(param);
            }
            auto* plugin = PluginManager::instance().findPlugin(transition.pluginId);
            if (plugin) {
                if (transition.parameters.empty()) {
                    transition.parameters = plugin->parameters;
                } else {
                    for (const auto& p : plugin->parameters) {
                        bool found = false;
                        for (auto& ep : transition.parameters) {
                            if (ep.name == p.name) { found = true; break; }
                        }
                        if (!found) transition.parameters.push_back(p);
                    }
                }
            }
            track.transitions.push_back(transition);
        }

        tracks.push_back(track);
    }

    QJsonArray nodesArray = root.value("nodes").toArray();
    for (int i = 0; i < nodesArray.size(); ++i) {
        QJsonObject nodeObj = nodesArray[i].toObject();
        PatchNode node;
        node.id = nodeObj.value("id").toString().toStdString();
        node.type = nodeObj.value("type").toString().toStdString();
        node.posX = nodeObj.value("posX").toDouble();
        node.posY = nodeObj.value("posY").toDouble();
        nodes.push_back(node);
    }

    QJsonArray connArray = root.value("connections").toArray();
    for (int i = 0; i < connArray.size(); ++i) {
        QJsonObject connObj = connArray[i].toObject();
        PatchConnection conn;
        conn.fromNodeId = connObj.value("fromNodeId").toString().toStdString();
        conn.fromPinIdx = connObj.value("fromPinIdx").toInt();
        conn.toNodeId = connObj.value("toNodeId").toString().toStdString();
        conn.toPinIdx = connObj.value("toPinIdx").toInt();
        connections.push_back(conn);
    }

    QJsonArray modArray = root.value("modulations").toArray();
    for (int i = 0; i < modArray.size(); ++i) {
        QJsonObject modObj = modArray[i].toObject();
        ModulationBinding binding;
        binding.targetEffectIdx = modObj.value("targetEffectIdx").toString().toStdString();
        binding.targetParamName = modObj.value("targetParamName").toString().toStdString();
        binding.sourceName = modObj.value("sourceName").toString().toStdString();
        binding.scale = modObj.value("scale").toDouble(1.0);
        modulations.push_back(binding);
    }
}

bool Project::save(const std::string& filePath) {
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson());
    return true;
}

QJsonObject Project::toJson() const {
    QJsonObject root;

    QJsonArray tracksArray;
    for (const auto& track : tracks) {
        QJsonObject trackObj;
        trackObj["id"] = track.id;
        trackObj["name"] = QString::fromStdString(track.name);

        QJsonArray clipsArray;
        for (const auto& clip : track.clips) {
            QJsonObject clipObj;
            clipObj["id"] = QString::fromStdString(clip.id);
            clipObj["filePath"] = QString::fromStdString(clip.filePath);
            clipObj["sourceStart"] = clip.sourceStart;
            clipObj["sourceDuration"] = clip.sourceDuration;
            clipObj["timelineStart"] = clip.timelineStart;
            clipObj["useClipEffects"] = clip.useClipEffects;

            QJsonArray clipEffectsArray;
            for (const auto& effect : clip.effects) {
                QJsonObject effectObj;
                effectObj["pluginId"] = QString::fromStdString(effect.pluginId);

                QJsonArray paramsArray;
                for (const auto& param : effect.parameters) {
                    QJsonObject paramObj;
                    paramObj["name"] = QString::fromStdString(param.name);
                    paramObj["label"] = QString::fromStdString(param.label);
                    paramObj["min"] = param.minVal;
                    paramObj["max"] = param.maxVal;
                    paramObj["default"] = param.defaultVal;
                    paramObj["value"] = param.currentVal;

                    QJsonArray keyframesArray;
                    for (const auto& kf : param.curve.getKeyframes()) {
                        QJsonObject kfObj;
                        kfObj["time"] = kf.time;
                        kfObj["value"] = kf.value;
                        kfObj["mode"] = static_cast<int>(kf.mode);
                        keyframesArray.append(kfObj);
                    }
                    paramObj["keyframes"] = keyframesArray;
                    paramsArray.append(paramObj);
                }
                effectObj["parameters"] = paramsArray;
                clipEffectsArray.append(effectObj);
            }
            clipObj["effects"] = clipEffectsArray;
            clipsArray.append(clipObj);
        }
        trackObj["clips"] = clipsArray;

        QJsonArray effectsArray;
        for (const auto& effect : track.effects) {
            QJsonObject effectObj;
            effectObj["pluginId"] = QString::fromStdString(effect.pluginId);

            QJsonArray paramsArray;
            for (const auto& param : effect.parameters) {
                QJsonObject paramObj;
                paramObj["name"] = QString::fromStdString(param.name);
                paramObj["label"] = QString::fromStdString(param.label);
                paramObj["min"] = param.minVal;
                paramObj["max"] = param.maxVal;
                paramObj["default"] = param.defaultVal;
                paramObj["value"] = param.currentVal;

                QJsonArray keyframesArray;
                for (const auto& kf : param.curve.getKeyframes()) {
                    QJsonObject kfObj;
                    kfObj["time"] = kf.time;
                    kfObj["value"] = kf.value;
                    kfObj["mode"] = static_cast<int>(kf.mode);
                    keyframesArray.append(kfObj);
                }
                paramObj["keyframes"] = keyframesArray;
                paramsArray.append(paramObj);
            }
            effectObj["parameters"] = paramsArray;
            effectsArray.append(effectObj);
        }
        trackObj["effects"] = effectsArray;

        QJsonArray transitionsArray;
        for (const auto& transition : track.transitions) {
            QJsonObject transObj;
            transObj["id"] = QString::fromStdString(transition.id);
            transObj["pluginId"] = QString::fromStdString(transition.pluginId);
            transObj["leftClipId"] = QString::fromStdString(transition.leftClipId);
            transObj["rightClipId"] = QString::fromStdString(transition.rightClipId);
            transObj["duration"] = transition.duration;

            QJsonArray paramsArray;
            for (const auto& param : transition.parameters) {
                QJsonObject paramObj;
                paramObj["name"] = QString::fromStdString(param.name);
                paramObj["label"] = QString::fromStdString(param.label);
                paramObj["min"] = param.minVal;
                paramObj["max"] = param.maxVal;
                paramObj["default"] = param.defaultVal;
                paramObj["value"] = param.currentVal;

                QJsonArray keyframesArray;
                for (const auto& kf : param.curve.getKeyframes()) {
                    QJsonObject kfObj;
                    kfObj["time"] = kf.time;
                    kfObj["value"] = kf.value;
                    kfObj["mode"] = static_cast<int>(kf.mode);
                    keyframesArray.append(kfObj);
                }
                paramObj["keyframes"] = keyframesArray;
                paramsArray.append(paramObj);
            }
            transObj["parameters"] = paramsArray;
            transitionsArray.append(transObj);
        }
        trackObj["transitions"] = transitionsArray;

        tracksArray.append(trackObj);
    }
    root["tracks"] = tracksArray;

    QJsonArray nodesArray;
    for (const auto& node : nodes) {
        QJsonObject nodeObj;
        nodeObj["id"] = QString::fromStdString(node.id);
        nodeObj["type"] = QString::fromStdString(node.type);
        nodeObj["posX"] = node.posX;
        nodeObj["posY"] = node.posY;
        nodesArray.append(nodeObj);
    }
    root["nodes"] = nodesArray;

    QJsonArray connArray;
    for (const auto& conn : connections) {
        QJsonObject connObj;
        connObj["fromNodeId"] = QString::fromStdString(conn.fromNodeId);
        connObj["fromPinIdx"] = conn.fromPinIdx;
        connObj["toNodeId"] = QString::fromStdString(conn.toNodeId);
        connObj["toPinIdx"] = conn.toPinIdx;
        connArray.append(connObj);
    }
    root["connections"] = connArray;

    QJsonArray modulationsArray;
    for (const auto& mod : modulations) {
        QJsonObject modObj;
        modObj["targetEffectIdx"] = QString::fromStdString(mod.targetEffectIdx);
        modObj["targetParamName"] = QString::fromStdString(mod.targetParamName);
        modObj["sourceName"] = QString::fromStdString(mod.sourceName);
        modObj["scale"] = mod.scale;
        modulationsArray.append(modObj);
    }
    root["modulations"] = modulationsArray;

    return root;
}

void Project::clear() {
    tracks.clear();
    nodes.clear();
    connections.clear();
    modulations.clear();
}

double Project::getDuration() const {
    double maxDur = 10.0; 
    for (const auto& track : tracks) {
        for (const auto& clip : track.clips) {
            maxDur = std::max(maxDur, clip.timelineStart + clip.sourceDuration);
        }
    }
    return maxDur;
}
