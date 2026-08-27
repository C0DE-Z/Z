#include "core/keyframe.h"
#include <cassert>
#include <cmath>
#include <iostream>

void testConstantCurve() {
    AnimationCurve curve;
    curve.setDefaultValue(42.0);
    assert(std::abs(curve.evaluate(0.0) - 42.0) < 1e-5);
    assert(std::abs(curve.evaluate(10.0) - 42.0) < 1e-5);
    std::cout << "[PASS] testConstantCurve\n";
}

void testLinearInterpolation() {
    AnimationCurve curve;
    curve.insertKeyframe(0.0, 0.0, InterpolationMode::Linear);
    curve.insertKeyframe(10.0, 100.0, InterpolationMode::Linear);

    assert(std::abs(curve.evaluate(0.0) - 0.0) < 1e-5);
    assert(std::abs(curve.evaluate(5.0) - 50.0) < 1e-5);
    assert(std::abs(curve.evaluate(10.0) - 100.0) < 1e-5);
    assert(std::abs(curve.evaluate(15.0) - 100.0) < 1e-5);
    std::cout << "[PASS] testLinearInterpolation\n";
}

void testStepInterpolation() {
    AnimationCurve curve;
    curve.insertKeyframe(0.0, 10.0, InterpolationMode::Step);
    curve.insertKeyframe(5.0, 20.0, InterpolationMode::Step);

    assert(std::abs(curve.evaluate(0.0) - 10.0) < 1e-5);
    assert(std::abs(curve.evaluate(4.99) - 10.0) < 1e-5);
    assert(std::abs(curve.evaluate(5.0) - 20.0) < 1e-5);
    std::cout << "[PASS] testStepInterpolation\n";
}

void testKeyframeRemoval() {
    AnimationCurve curve;
    curve.insertKeyframe(1.0, 10.0);
    curve.insertKeyframe(2.0, 20.0);
    assert(curve.getKeyframes().size() == 2);

    curve.removeKeyframeAt(1.0, 0.1);
    assert(curve.getKeyframes().size() == 1);
    assert(std::abs(curve.getKeyframes()[0].time - 2.0) < 1e-5);
    std::cout << "[PASS] testKeyframeRemoval\n";
}

int main() {
    std::cout << "=== Running Keyframe Unit Tests ===\n";
    testConstantCurve();
    testLinearInterpolation();
    testStepInterpolation();
    testKeyframeRemoval();
    std::cout << "=== All Keyframe Tests Passed ===\n";
    return 0;
}
