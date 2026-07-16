#pragma once

#include <vector>
#include <cstdint>

struct DecodedVideoFrame {
    double timestamp; 
    int width;
    int height;
    std::vector<uint8_t> rgbData;
};
