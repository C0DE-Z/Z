#include "engine/maskgenerator.h"
#include <algorithm>
#include <cassert>
#include <iostream>

void testOddWidthMaskUsesPackedRows() {
    ClipMask rectangle = MaskGenerator::createRectangleMask("rectangle");
    rectangle.feather = 0.0;
    rectangle.mode = MaskMode::Intersect;

    const auto pixels = MaskGenerator::renderMask(7, 5, {rectangle}, 0.0);
    assert(pixels.size() == 35);
    assert(pixels[2 * 7 + 3] > 0);
    assert(pixels[0] == 0);
}

void testFirstIntersectMaskUsesWhiteIdentity() {
    ClipMask ellipse = MaskGenerator::createEllipseMask("ellipse");
    ellipse.feather = 0.0;
    ellipse.mode = MaskMode::Intersect;

    const auto pixels = MaskGenerator::renderMask(9, 7, {ellipse}, 0.0);
    assert(pixels.size() == 63);
    assert(pixels[3 * 9 + 4] > 0);
    assert(pixels.front() == 0);
}

void testMasksCanTargetIndividualEffects() {
    ClipMask rectangle = MaskGenerator::createRectangleMask("targeted");
    rectangle.feather = 0.0;
    rectangle.targetEffectIds = {"gaussian_blur"};
    const auto targeted = MaskGenerator::renderMask(9, 7, {rectangle}, 0.0, "gaussian_blur");
    const auto unrelated = MaskGenerator::renderMask(9, 7, {rectangle}, 0.0, "color_grade");
    assert(targeted.size() == 63);
    assert(targeted[3 * 9 + 4] > 0);
    assert(unrelated.empty());
}

void testOpacityAndExpansion() {
    ClipMask base = MaskGenerator::createRectangleMask("expansion");
    base.feather = 0.0;
    const auto countCoverage = [](const std::vector<uint8_t>& pixels) {
        return std::count_if(pixels.begin(), pixels.end(), [](uint8_t value) { return value > 0; });
    };
    const auto original = MaskGenerator::renderMask(64, 64, {base}, 0.0);
    base.expansion = 5.0;
    const auto expanded = MaskGenerator::renderMask(64, 64, {base}, 0.0);
    base.expansion = -5.0;
    const auto contracted = MaskGenerator::renderMask(64, 64, {base}, 0.0);
    base.expansion = 0.0;
    base.opacity = 0.0;
    const auto invisible = MaskGenerator::renderMask(64, 64, {base}, 0.0);
    assert(countCoverage(expanded) > countCoverage(original));
    assert(countCoverage(contracted) < countCoverage(original));
    assert(countCoverage(invisible) == 0);
}

void testInvalidSizeReturnsNoMask() {
    const auto pixels = MaskGenerator::renderMask(0, 10, {}, 0.0);
    assert(pixels.empty());
}

void testPolygonPositionMovesTheRenderedMask() {
    const std::vector<MaskPoint> points = {
        {0.25f, 0.25f, 0, 0, 0, 0}, {0.75f, 0.25f, 0, 0, 0, 0},
        {0.75f, 0.75f, 0, 0, 0, 0}, {0.25f, 0.75f, 0, 0, 0, 0}
    };
    ClipMask polygon = MaskGenerator::createPolygonMask("polygon", points);
    polygon.posX = 0.25;
    polygon.feather = 0.0;
    const auto pixels = MaskGenerator::renderMask(100, 100, {polygon}, 0.0);
    assert(pixels[50 * 100 + 25] > 0);
    assert(pixels[50 * 100 + 75] == 0);
}

void testOutlineFallbackReturnsEditablePolygon() {
    DetectionBox box;
    box.x = 0.2f;
    box.y = 0.3f;
    box.w = 0.4f;
    box.h = 0.2f;
    bool usedComputerVision = true;
    const auto points = MaskGenerator::traceObjectOutline({}, box, &usedComputerVision);
    assert(points.size() >= 3);
    assert(!usedComputerVision);
    assert(points.front().x >= 0.19f && points.front().x <= 0.21f);
}

int main() {
    testOddWidthMaskUsesPackedRows();
    testFirstIntersectMaskUsesWhiteIdentity();
    testMasksCanTargetIndividualEffects();
    testOpacityAndExpansion();
    testInvalidSizeReturnsNoMask();
    testPolygonPositionMovesTheRenderedMask();
    testOutlineFallbackReturnsEditablePolygon();
    std::cout << "[PASS] MaskGenerator tests\n";
    return 0;
}
