#ifndef PROJECT_H
#define PROJECT_H

#include <string>
#include <vector>
#include <map>
#include "core/keyframe.h"
#include "engine/pluginmanager.h"
#include <QJsonObject>
#include <QJsonArray>

struct AppliedEffect {
    std::string pluginId;
    // Seconds from the start of the owning clip. Effects added at the
    // playhead affect the rest of that clip only.
    double startOffset = 0.0;
    std::vector<ShaderParameter> parameters;
};

enum class MaskShapeType {
    Rectangle,
    Ellipse,
    Polygon,
    Bezier
};

enum class MaskMode {
    Add,
    Subtract,
    Intersect
};

struct MaskPoint {
    float x = 0.5f; // normalized 0..1
    float y = 0.5f;
    float inHandleX = 0.0f;  // relative to (x, y)
    float inHandleY = 0.0f;
    float outHandleX = 0.0f;
    float outHandleY = 0.0f;
};

struct ClipMask {
    std::string id;
    std::string name = "Mask 1";
    MaskShapeType shapeType = MaskShapeType::Rectangle;
    MaskMode mode = MaskMode::Add;
    bool enabled = true;
    bool inverted = false;
    bool closed = true;

    // Transform & Feather properties (base values)
    double posX = 0.5; // normalized center (0..1)
    double posY = 0.5;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotation = 0.0; // degrees
    double feather = 6.0;  // pixels
    double opacity = 1.0;  // 0..1
    double expansion = 0.0; // pixels

    // Keyframe curves for animating properties over clip local time
    AnimationCurve posXCurve;
    AnimationCurve posYCurve;
    AnimationCurve scaleXCurve;
    AnimationCurve scaleYCurve;
    AnimationCurve rotationCurve;
    AnimationCurve featherCurve;
    AnimationCurve opacityCurve;
    AnimationCurve expansionCurve;

    // Control vertices
    std::vector<MaskPoint> points;

    // Targeting: empty = all effects on clip
    std::vector<std::string> targetEffectIds;
    bool maskSourceClip = false; // if true, masks the source video clip itself

    double evalProp(const AnimationCurve& curve, double defaultVal, double localTime) const {
        return curve.getKeyframes().empty() ? defaultVal : curve.evaluate(localTime);
    }
};

struct ProjectClip {
    std::string id;
    // The decoder/cache key. Audio and video clips can share one media source
    // while remaining independently movable on their own timeline lanes.
    std::string mediaId;
    std::string name;
    std::string filePath;
    // Cached H.264 P-frame proxy used by the native Datamosh effect. It is
    // intentionally separate from filePath so normal/alpha editing remains lossless.
    std::string datamoshProxyPath;
    double sourceStart; 
    double sourceDuration;
    double timelineStart; 
    bool useClipEffects = false;
    std::vector<AppliedEffect> effects; 
    std::vector<ClipMask> masks;
};

enum class TimelineTrackType {
    Video,
    Audio
};

struct ProjectTransition {
    std::string id;
    std::string pluginId;
    std::string leftClipId;
    std::string rightClipId;
    double duration = 1.0;
    double cutTime = 0.0;    // exact timeline position of the cut point
    std::string alignment = "center"; // "center", "start", "end"
    std::vector<ShaderParameter> parameters;
};

struct TimelineTrack {
    int id;
    std::string name;
    TimelineTrackType type = TimelineTrackType::Video;
    std::vector<ProjectClip> clips;
    std::vector<AppliedEffect> effects;
    std::vector<ProjectTransition> transitions;
};

struct PatchNode {
    std::string id;
    std::string type; 
    double posX;
    double posY;
};

struct PatchConnection {
    std::string fromNodeId;
    int fromPinIdx;
    std::string toNodeId;
    int toPinIdx;
};

struct ModulationBinding {
    std::string targetEffectIdx; 
    std::string targetParamName;
    std::string sourceName; 
    double scale = 1.0;
};

class Project {
public:
    static Project& instance() {
        static Project inst;
        return inst;
    }

    bool load(const std::string& filePath);
    bool save(const std::string& filePath);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& root);
    void clear();

    std::vector<TimelineTrack>& getTracks() { return tracks; }
    const std::vector<TimelineTrack>& getTracks() const { return tracks; }

    std::vector<PatchNode>& getNodes() { return nodes; }
    std::vector<PatchConnection>& getConnections() { return connections; }
    std::vector<ModulationBinding>& getModulations() { return modulations; }

    double getDuration() const;

private:
    Project() = default;
    std::vector<TimelineTrack> tracks;
    std::vector<PatchNode> nodes;
    std::vector<PatchConnection> connections;
    std::vector<ModulationBinding> modulations;
};

#endif 
