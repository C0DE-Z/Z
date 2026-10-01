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
        track.type = trackObj.value("type").toString().compare("audio", Qt::CaseInsensitive) == 0
            ? TimelineTrackType::Audio : TimelineTrackType::Video;

        QJsonArray clipsArray = trackObj.value("clips").toArray();
        for (int j = 0; j < clipsArray.size(); ++j) {
            QJsonObject clipObj = clipsArray[j].toObject();
            ProjectClip clip;
            clip.id = clipObj.value("id").toString().toStdString();
            clip.mediaId = clipObj.value("mediaId").toString(QString::fromStdString(clip.id)).toStdString();
            clip.name = clipObj.value("name").toString(clipObj.value("id").toString()).toStdString();
            clip.filePath = clipObj.value("filePath").toString().toStdString();
            clip.datamoshProxyPath = clipObj.value("datamoshProxyPath").toString().toStdString();
            clip.sourceStart = clipObj.value("sourceStart").toDouble();
            clip.sourceDuration = clipObj.value("sourceDuration").toDouble();
            clip.timelineStart = clipObj.value("timelineStart").toDouble();
            clip.useClipEffects = clipObj.value("useClipEffects").toBool(false);

            QJsonArray clipEffectsArray = clipObj.value("effects").toArray();
            for (int e = 0; e < clipEffectsArray.size(); ++e) {
                QJsonObject effectObj = clipEffectsArray[e].toObject();
                AppliedEffect effect;
                effect.pluginId = effectObj.value("pluginId").toString().toStdString();
                effect.startOffset = std::max(0.0, effectObj.value("startOffset").toDouble(0.0));

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
                    param.isBool = paramObj.value("isBool").toBool(false);
                    param.curve = AnimationCurve(param.defaultVal);

                    QJsonArray curvesArray = paramObj.value("keyframes").toArray();
                    for (int m = 0; m < curvesArray.size(); ++m) {
                        QJsonObject kfObj = curvesArray[m].toObject();
                        double t = kfObj.value("time").toDouble();
                        double v = kfObj.value("value").toDouble();
                        int modeInt = kfObj.value("mode").toInt(0);
                        param.curve.insertKeyframe(t, v, static_cast<InterpolationMode>(modeInt));
                        auto& key = *std::find_if(param.curve.getKeyframes().begin(), param.curve.getKeyframes().end(), [t](const Keyframe& k) { return std::abs(k.time - t) < 0.001; });
                        key.handleInX = kfObj.value("handleInX").toDouble(key.handleInX);
                        key.handleInY = kfObj.value("handleInY").toDouble(key.handleInY);
                        key.handleOutX = kfObj.value("handleOutX").toDouble(key.handleOutX);
                        key.handleOutY = kfObj.value("handleOutY").toDouble(key.handleOutY);
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
                                if (ep.name == p.name) {
                                    ep.isBool = p.isBool;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) effect.parameters.push_back(p);
                        }
                    }
                }
                clip.effects.push_back(effect);
            }

            QJsonArray masksArray = clipObj.value("masks").toArray();
            for (int mIdx = 0; mIdx < masksArray.size(); ++mIdx) {
                QJsonObject mObj = masksArray[mIdx].toObject();
                ClipMask mask;
                mask.id = mObj.value("id").toString().toStdString();
                mask.name = mObj.value("name").toString("Mask").toStdString();
                mask.shapeType = static_cast<MaskShapeType>(mObj.value("shapeType").toInt(0));
                mask.mode = static_cast<MaskMode>(mObj.value("mode").toInt(0));
                mask.enabled = mObj.value("enabled").toBool(true);
                mask.inverted = mObj.value("inverted").toBool(false);
                mask.closed = mObj.value("closed").toBool(true);
                mask.posX = mObj.value("posX").toDouble(0.5);
                mask.posY = mObj.value("posY").toDouble(0.5);
                mask.scaleX = mObj.value("scaleX").toDouble(1.0);
                mask.scaleY = mObj.value("scaleY").toDouble(1.0);
                mask.rotation = mObj.value("rotation").toDouble(0.0);
                mask.feather = mObj.value("feather").toDouble(6.0);
                mask.opacity = mObj.value("opacity").toDouble(1.0);
                mask.expansion = mObj.value("expansion").toDouble(0.0);
                mask.maskSourceClip = mObj.value("maskSourceClip").toBool(false);

                auto deserializeCurve = [](AnimationCurve& curve, const QJsonArray& arr) {
                    for (int k = 0; k < arr.size(); ++k) {
                        QJsonObject kfObj = arr[k].toObject();
                        double t = kfObj.value("time").toDouble();
                        double v = kfObj.value("value").toDouble();
                        int modeInt = kfObj.value("mode").toInt(0);
                        curve.insertKeyframe(t, v, static_cast<InterpolationMode>(modeInt));
                        auto& key = *std::find_if(curve.getKeyframes().begin(), curve.getKeyframes().end(), [t](const Keyframe& k) { return std::abs(k.time - t) < 0.001; });
                        key.handleInX = kfObj.value("handleInX").toDouble(key.handleInX);
                        key.handleInY = kfObj.value("handleInY").toDouble(key.handleInY);
                        key.handleOutX = kfObj.value("handleOutX").toDouble(key.handleOutX);
                        key.handleOutY = kfObj.value("handleOutY").toDouble(key.handleOutY);
                    }
                };

                deserializeCurve(mask.posXCurve, mObj.value("posXCurve").toArray());
                deserializeCurve(mask.posYCurve, mObj.value("posYCurve").toArray());
                deserializeCurve(mask.scaleXCurve, mObj.value("scaleXCurve").toArray());
                deserializeCurve(mask.scaleYCurve, mObj.value("scaleYCurve").toArray());
                deserializeCurve(mask.rotationCurve, mObj.value("rotationCurve").toArray());
                deserializeCurve(mask.featherCurve, mObj.value("featherCurve").toArray());
                deserializeCurve(mask.opacityCurve, mObj.value("opacityCurve").toArray());
                deserializeCurve(mask.expansionCurve, mObj.value("expansionCurve").toArray());

                QJsonArray ptsArr = mObj.value("points").toArray();
                for (int pIdx = 0; pIdx < ptsArr.size(); ++pIdx) {
                    QJsonObject pObj = ptsArr[pIdx].toObject();
                    MaskPoint pt;
                    pt.x = static_cast<float>(pObj.value("x").toDouble(0.5));
                    pt.y = static_cast<float>(pObj.value("y").toDouble(0.5));
                    pt.inHandleX = static_cast<float>(pObj.value("inHandleX").toDouble(0.0));
                    pt.inHandleY = static_cast<float>(pObj.value("inHandleY").toDouble(0.0));
                    pt.outHandleX = static_cast<float>(pObj.value("outHandleX").toDouble(0.0));
                    pt.outHandleY = static_cast<float>(pObj.value("outHandleY").toDouble(0.0));
                    mask.points.push_back(pt);
                }

                QJsonArray effTargetArr = mObj.value("targetEffectIds").toArray();
                for (int eIdx = 0; eIdx < effTargetArr.size(); ++eIdx) {
                    mask.targetEffectIds.push_back(effTargetArr[eIdx].toString().toStdString());
                }

                clip.masks.push_back(mask);
            }

            track.clips.push_back(clip);
        }

        QJsonArray effectsArray = trackObj.value("effects").toArray();
        for (int j = 0; j < effectsArray.size(); ++j) {
            QJsonObject effectObj = effectsArray[j].toObject();
            AppliedEffect effect;
            effect.pluginId = effectObj.value("pluginId").toString().toStdString();
            effect.startOffset = std::max(0.0, effectObj.value("startOffset").toDouble(0.0));

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
                param.isBool = paramObj.value("isBool").toBool(false);
                param.curve = AnimationCurve(param.defaultVal);
                QJsonArray curvesArray = paramObj.value("keyframes").toArray();
                for (int m = 0; m < curvesArray.size(); ++m) {
                    QJsonObject kfObj = curvesArray[m].toObject();
                    double t = kfObj.value("time").toDouble();
                    double v = kfObj.value("value").toDouble();
                    int modeInt = kfObj.value("mode").toInt(0);
                    param.curve.insertKeyframe(t, v, static_cast<InterpolationMode>(modeInt));
                    auto& key = *std::find_if(param.curve.getKeyframes().begin(), param.curve.getKeyframes().end(), [t](const Keyframe& k) { return std::abs(k.time - t) < 0.001; });
                    key.handleInX = kfObj.value("handleInX").toDouble(key.handleInX);
                    key.handleInY = kfObj.value("handleInY").toDouble(key.handleInY);
                    key.handleOutX = kfObj.value("handleOutX").toDouble(key.handleOutX);
                    key.handleOutY = kfObj.value("handleOutY").toDouble(key.handleOutY);
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
                                if (ep.name == p.name) {
                                    ep.isBool = p.isBool;
                                    found = true;
                                    break;
                                }
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
            transition.duration = transObj.value("duration").toDouble(1.0);
            transition.cutTime = transObj.value("cutTime").toDouble(0.0);
            transition.alignment = transObj.value("alignment").toString("center").toStdString();

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
                    auto& key = *std::find_if(param.curve.getKeyframes().begin(), param.curve.getKeyframes().end(), [t](const Keyframe& k) { return std::abs(k.time - t) < 0.001; });
                    key.handleInX = kfObj.value("handleInX").toDouble(key.handleInX);
                    key.handleInY = kfObj.value("handleInY").toDouble(key.handleInY);
                    key.handleOutX = kfObj.value("handleOutX").toDouble(key.handleOutX);
                    key.handleOutY = kfObj.value("handleOutY").toDouble(key.handleOutY);
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
                            if (ep.name == p.name) {
                                ep.isBool = p.isBool;
                                found = true;
                                break;
                            }
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
        trackObj["type"] = track.type == TimelineTrackType::Audio ? "audio" : "video";

        QJsonArray clipsArray;
        for (const auto& clip : track.clips) {
            QJsonObject clipObj;
            clipObj["id"] = QString::fromStdString(clip.id);
            clipObj["mediaId"] = QString::fromStdString(clip.mediaId.empty() ? clip.id : clip.mediaId);
            clipObj["name"] = QString::fromStdString(clip.name);
            clipObj["filePath"] = QString::fromStdString(clip.filePath);
            clipObj["datamoshProxyPath"] = QString::fromStdString(clip.datamoshProxyPath);
            clipObj["sourceStart"] = clip.sourceStart;
            clipObj["sourceDuration"] = clip.sourceDuration;
            clipObj["timelineStart"] = clip.timelineStart;
            clipObj["useClipEffects"] = clip.useClipEffects;

            QJsonArray clipEffectsArray;
            for (const auto& effect : clip.effects) {
                QJsonObject effectObj;
                effectObj["pluginId"] = QString::fromStdString(effect.pluginId);
                effectObj["startOffset"] = effect.startOffset;

                QJsonArray paramsArray;
                for (const auto& param : effect.parameters) {
                    QJsonObject paramObj;
                    paramObj["name"] = QString::fromStdString(param.name);
                    paramObj["label"] = QString::fromStdString(param.label);
                    paramObj["min"] = param.minVal;
                    paramObj["max"] = param.maxVal;
                    paramObj["default"] = param.defaultVal;
                    paramObj["value"] = param.currentVal;
                    paramObj["isBool"] = param.isBool;

                    QJsonArray keyframesArray;
                    for (const auto& kf : param.curve.getKeyframes()) {
                        QJsonObject kfObj;
                        kfObj["time"] = kf.time;
                        kfObj["value"] = kf.value;
                        kfObj["mode"] = static_cast<int>(kf.mode);
                        kfObj["handleInX"] = kf.handleInX;
                        kfObj["handleInY"] = kf.handleInY;
                        kfObj["handleOutX"] = kf.handleOutX;
                        kfObj["handleOutY"] = kf.handleOutY;
                        keyframesArray.append(kfObj);
                    }
                    paramObj["keyframes"] = keyframesArray;
                    paramsArray.append(paramObj);
                }
                effectObj["parameters"] = paramsArray;
                clipEffectsArray.append(effectObj);
            }
            clipObj["effects"] = clipEffectsArray;

            QJsonArray masksArray;
            for (const auto& mask : clip.masks) {
                QJsonObject mObj;
                mObj["id"] = QString::fromStdString(mask.id);
                mObj["name"] = QString::fromStdString(mask.name);
                mObj["shapeType"] = static_cast<int>(mask.shapeType);
                mObj["mode"] = static_cast<int>(mask.mode);
                mObj["enabled"] = mask.enabled;
                mObj["inverted"] = mask.inverted;
                mObj["closed"] = mask.closed;
                mObj["posX"] = mask.posX;
                mObj["posY"] = mask.posY;
                mObj["scaleX"] = mask.scaleX;
                mObj["scaleY"] = mask.scaleY;
                mObj["rotation"] = mask.rotation;
                mObj["feather"] = mask.feather;
                mObj["opacity"] = mask.opacity;
                mObj["expansion"] = mask.expansion;
                mObj["maskSourceClip"] = mask.maskSourceClip;

                auto serializeCurve = [](const AnimationCurve& curve) {
                    QJsonArray arr;
                    for (const auto& kf : curve.getKeyframes()) {
                        QJsonObject kfObj;
                        kfObj["time"] = kf.time;
                        kfObj["value"] = kf.value;
                        kfObj["mode"] = static_cast<int>(kf.mode);
                        kfObj["handleInX"] = kf.handleInX;
                        kfObj["handleInY"] = kf.handleInY;
                        kfObj["handleOutX"] = kf.handleOutX;
                        kfObj["handleOutY"] = kf.handleOutY;
                        arr.append(kfObj);
                    }
                    return arr;
                };

                mObj["posXCurve"] = serializeCurve(mask.posXCurve);
                mObj["posYCurve"] = serializeCurve(mask.posYCurve);
                mObj["scaleXCurve"] = serializeCurve(mask.scaleXCurve);
                mObj["scaleYCurve"] = serializeCurve(mask.scaleYCurve);
                mObj["rotationCurve"] = serializeCurve(mask.rotationCurve);
                mObj["featherCurve"] = serializeCurve(mask.featherCurve);
                mObj["opacityCurve"] = serializeCurve(mask.opacityCurve);
                mObj["expansionCurve"] = serializeCurve(mask.expansionCurve);

                QJsonArray ptsArr;
                for (const auto& pt : mask.points) {
                    QJsonObject pObj;
                    pObj["x"] = pt.x;
                    pObj["y"] = pt.y;
                    pObj["inHandleX"] = pt.inHandleX;
                    pObj["inHandleY"] = pt.inHandleY;
                    pObj["outHandleX"] = pt.outHandleX;
                    pObj["outHandleY"] = pt.outHandleY;
                    ptsArr.append(pObj);
                }
                mObj["points"] = ptsArr;

                QJsonArray effTargetArr;
                for (const auto& eId : mask.targetEffectIds) {
                    effTargetArr.append(QString::fromStdString(eId));
                }
                mObj["targetEffectIds"] = effTargetArr;

                masksArray.append(mObj);
            }
            clipObj["masks"] = masksArray;

            clipsArray.append(clipObj);
        }
        trackObj["clips"] = clipsArray;

        QJsonArray effectsArray;
        for (const auto& effect : track.effects) {
            QJsonObject effectObj;
            effectObj["pluginId"] = QString::fromStdString(effect.pluginId);
            effectObj["startOffset"] = effect.startOffset;

            QJsonArray paramsArray;
            for (const auto& param : effect.parameters) {
                QJsonObject paramObj;
                paramObj["name"] = QString::fromStdString(param.name);
                paramObj["label"] = QString::fromStdString(param.label);
                paramObj["min"] = param.minVal;
                paramObj["max"] = param.maxVal;
                paramObj["default"] = param.defaultVal;
                paramObj["value"] = param.currentVal;
                paramObj["isBool"] = param.isBool;

                QJsonArray keyframesArray;
                for (const auto& kf : param.curve.getKeyframes()) {
                    QJsonObject kfObj;
                    kfObj["time"] = kf.time;
                    kfObj["value"] = kf.value;
                    kfObj["mode"] = static_cast<int>(kf.mode);
                    kfObj["handleInX"] = kf.handleInX;
                    kfObj["handleInY"] = kf.handleInY;
                    kfObj["handleOutX"] = kf.handleOutX;
                    kfObj["handleOutY"] = kf.handleOutY;
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
            transObj["cutTime"] = transition.cutTime;
            transObj["alignment"] = QString::fromStdString(transition.alignment);

            QJsonArray paramsArray;
            for (const auto& param : transition.parameters) {
                QJsonObject paramObj;
                paramObj["name"] = QString::fromStdString(param.name);
                paramObj["label"] = QString::fromStdString(param.label);
                paramObj["min"] = param.minVal;
                paramObj["max"] = param.maxVal;
                paramObj["default"] = param.defaultVal;
                paramObj["value"] = param.currentVal;
                paramObj["isBool"] = param.isBool;

                QJsonArray keyframesArray;
                for (const auto& kf : param.curve.getKeyframes()) {
                    QJsonObject kfObj;
                    kfObj["time"] = kf.time;
                    kfObj["value"] = kf.value;
                    kfObj["mode"] = static_cast<int>(kf.mode);
                    kfObj["handleInX"] = kf.handleInX;
                    kfObj["handleInY"] = kf.handleInY;
                    kfObj["handleOutX"] = kf.handleOutX;
                    kfObj["handleOutY"] = kf.handleOutY;
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
    double maxDur = 0.0; 
    for (const auto& track : tracks) {
        for (const auto& clip : track.clips) {
            maxDur = std::max(maxDur, clip.timelineStart + clip.sourceDuration);
        }
    }
    return maxDur;
}
