#include "keyframe.h"
#include <cmath>

void AnimationCurve::insertKeyframe(double time, double value, InterpolationMode mode) {
    auto it = std::find_if(keyframes.begin(), keyframes.end(), [time](const Keyframe& k) {
        return std::abs(k.time - time) < 0.001;
    });

    if (it != keyframes.end()) {
        it->value = value;
        it->mode = mode;
    } else {
        Keyframe k;
        k.time = time;
        k.value = value;
        k.mode = mode;
        keyframes.push_back(k);
        std::sort(keyframes.begin(), keyframes.end());
    }
}

void AnimationCurve::removeKeyframeAt(double time, double tolerance) {
    keyframes.erase(
        std::remove_if(keyframes.begin(), keyframes.end(), [time, tolerance](const Keyframe& k) {
            return std::abs(k.time - time) < tolerance;
        }),
        keyframes.end()
    );
}

void AnimationCurve::clear() {
    keyframes.clear();
}

double AnimationCurve::evaluate(double time) const {
    if (keyframes.empty()) {
        return defaultValue;
    }

    if (time <= keyframes.front().time) {
        return keyframes.front().value;
    }

    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    auto it = std::upper_bound(keyframes.begin(), keyframes.end(), Keyframe{time, 0.0});
    if (it == keyframes.begin()) {
        return keyframes.front().value;
    }

    const Keyframe& k2 = *it;
    const Keyframe& k1 = *(it - 1);

    double duration = k2.time - k1.time;
    if (duration <= 0.0) {
        return k1.value;
    }

    double t = (time - k1.time) / duration;

    switch (k1.mode) {
        case InterpolationMode::Step:
            return k1.value;
        case InterpolationMode::Bezier:
            return interpolateBezier(k1, k2, t);
        case InterpolationMode::Linear:
        default:
            return interpolateLinear(k1, k2, t);
    }
}

double AnimationCurve::interpolateLinear(const Keyframe& k1, const Keyframe& k2, double t) const {
    return k1.value + t * (k2.value - k1.value);
}

double AnimationCurve::interpolateBezier(const Keyframe& k1, const Keyframe& k2, double t) const {
    double t2 = t * t;
    double t3 = t2 * t;
    double h00 = 2 * t3 - 3 * t2 + 1;
    double h10 = t3 - 2 * t2 + t;
    double h01 = -2 * t3 + 3 * t2;
    double h11 = t3 - t2;
    double m1 = k1.handleOutY / (k1.handleOutX > 0 ? k1.handleOutX : 0.1);
    double m2 = k2.handleInY / (k2.handleInX < 0 ? -k2.handleInX : 0.1);
    return h00 * k1.value + h10 * m1 + h01 * k2.value + h11 * m2;
}
