#pragma once

#include <vector>
#include <cstdint>

struct DecodedVideoFrame {
    double timestamp = 0.0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgbData;
    std::vector<uint8_t> alphaData;
    bool hasAlpha = false;
};
