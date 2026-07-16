#pragma once

#include <vector>
#include <cstdint>

namespace CpuEffects {
    void blendWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double strength, double smearStrength, double bleedStrength, double lumaStrength);
    void cpuXorWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double xorValue, double intensity);
    void cpuOrWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double orValue, double intensity);
    void cpuAndWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double andValue, double intensity);
    void cpuXnorWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double xnorValue, double intensity);
    void cpuNandWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double nandValue, double intensity);
}
