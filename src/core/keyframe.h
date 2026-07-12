#ifndef KEYFRAME_H
#define KEYFRAME_H

#include <vector>
#include <algorithm>

enum class InterpolationMode {
    Linear,
    Step,
    Bezier
};

struct Keyframe {
    double time; 
    double value;
    InterpolationMode mode = InterpolationMode::Linear;
    double handleInX = -0.1;
    double handleInY = 0.0;
    double handleOutX = 0.1;
    double handleOutY = 0.0;

    bool operator<(const Keyframe& other) const {
        return time < other.time;
    }
};

class AnimationCurve {
public:
    AnimationCurve() : defaultValue(0.0) {}
    AnimationCurve(double defaultVal) : defaultValue(defaultVal) {}

    void insertKeyframe(double time, double value, InterpolationMode mode = InterpolationMode::Linear);
    void removeKeyframeAt(double time, double tolerance = 0.01);
    void clear();

    double evaluate(double time) const;
    const std::vector<Keyframe>& getKeyframes() const { return keyframes; }
    std::vector<Keyframe>& getKeyframes() { return keyframes; }

private:
    std::vector<Keyframe> keyframes;
    double defaultValue;

    double interpolateLinear(const Keyframe& k1, const Keyframe& k2, double t) const;
    double interpolateBezier(const Keyframe& k1, const Keyframe& k2, double t) const;
};

#endif 
