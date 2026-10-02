#ifndef MASKGENERATOR_H
#define MASKGENERATOR_H

#include <vector>
#include <cstdint>
#include <string>
#include "core/project.h"
#include "engine/detector.h"

class MaskGenerator {
public:

    static std::vector<uint8_t> renderMask(
        int width,
        int height,
        const std::vector<ClipMask>& masks,
        double clipLocalTime,
        const std::string& targetEffectId = "",
        bool targetSourceClip = false
    );

    static ClipMask createRectangleMask(const std::string& id, const std::string& name = "Mask 1");
    static ClipMask createEllipseMask(const std::string& id, const std::string& name = "Mask 1");
    static ClipMask createPolygonMask(const std::string& id, const std::vector<MaskPoint>& points, const std::string& name = "Mask 1");
    // Trace an editable polygon from a detected box. Uses GrabCut when OpenCV
    // is available and falls back to the detector's guide or box rectangle.
    static std::vector<MaskPoint> traceObjectOutline(
        const DecodedVideoFrame& frame,
        const DetectionBox& box,
        bool* usedComputerVision = nullptr);

    static void applyFeather(uint8_t* data, int width, int height, float featherPx);
};

#endif // MASKGENERATOR_H
