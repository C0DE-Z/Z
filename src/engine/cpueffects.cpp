#include "cpueffects.h"
#include <algorithm>

namespace CpuEffects {

inline uint8_t clampByte(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

void blendWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double strength, double smearStrength, double bleedStrength, double lumaStrength) {
    if (current.size() != previous.size() || current.empty() || strength <= 0.0) {
        return;
    }

    const double blend = std::clamp(strength, 0.0, 1.0);
    const double smear = std::clamp(smearStrength, 0.0, 1.0);
    const double bleed = std::clamp(bleedStrength, 0.0, 1.0);
    const double lumaBias = std::clamp(lumaStrength, 0.0, 1.0);

    const size_t totalPixels = current.size() / 3;
    const double bleedMix = bleed * 0.55;
    const double oneMinusBleedMix = 1.0 - bleedMix;
    const double smearFactor = 1.0 - smear * 0.35;
    const double lumaScale = lumaBias * 0.65;

    for (size_t p = 0; p < totalPixels; ++p) {
        const size_t i = p * 3;
        const double prevR = previous[i + 0];
        const double prevG = previous[i + 1];
        const double prevB = previous[i + 2];
        const double prevLuma = (prevR + prevG + prevB) * (1.0 / (3.0 * 255.0));

        double alpha = blend * smearFactor;
        alpha *= (1.0 + prevLuma * lumaScale);
        if (alpha > 1.0) alpha = 1.0;

        const double targetR = prevR * oneMinusBleedMix + prevG * bleedMix;
        const double targetG = prevG * oneMinusBleedMix + prevB * bleedMix;
        const double targetB = prevB * oneMinusBleedMix + prevR * bleedMix;
        const double oneMinusAlpha = 1.0 - alpha;

        current[i + 0] = clampByte(static_cast<int>(current[i + 0] * oneMinusAlpha + targetR * alpha));
        current[i + 1] = clampByte(static_cast<int>(current[i + 1] * oneMinusAlpha + targetG * alpha));
        current[i + 2] = clampByte(static_cast<int>(current[i + 2] * oneMinusAlpha + targetB * alpha));
    }
}

void cpuXorWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double xorValue, double intensity) {
    if (current.size() != previous.size() || current.empty() || intensity <= 0.0) {
        return;
    }

    const uint8_t mask = static_cast<uint8_t>(std::clamp(xorValue * 255.0, 0.0, 255.0));
    const size_t totalBytes = current.size();
    const double mix = std::clamp(intensity, 0.0, 1.0);
    const double oneMinusMix = 1.0 - mix;
    for (size_t i = 0; i < totalBytes; i += 3) {
        uint8_t xorR = current[i + 0] ^ (previous[i + 0] & mask);
        uint8_t xorG = current[i + 1] ^ (previous[i + 1] & mask);
        uint8_t xorB = current[i + 2] ^ (previous[i + 2] & mask);
        current[i + 0] = clampByte(static_cast<int>(current[i + 0] * oneMinusMix + xorR * mix));
        current[i + 1] = clampByte(static_cast<int>(current[i + 1] * oneMinusMix + xorG * mix));
        current[i + 2] = clampByte(static_cast<int>(current[i + 2] * oneMinusMix + xorB * mix));
    }
}

void cpuOrWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double orValue, double intensity) {
    if (current.size() != previous.size() || current.empty() || intensity <= 0.0) {
        return;
    }

    const uint8_t mask = static_cast<uint8_t>(std::clamp(orValue * 255.0, 0.0, 255.0));
    const size_t totalBytes = current.size();
    const double mix = std::clamp(intensity, 0.0, 1.0);
    const double oneMinusMix = 1.0 - mix;
    for (size_t i = 0; i < totalBytes; i += 3) {
        uint8_t valR = current[i + 0] | (previous[i + 0] & mask);
        uint8_t valG = current[i + 1] | (previous[i + 1] & mask);
        uint8_t valB = current[i + 2] | (previous[i + 2] & mask);
        current[i + 0] = clampByte(static_cast<int>(current[i + 0] * oneMinusMix + valR * mix));
        current[i + 1] = clampByte(static_cast<int>(current[i + 1] * oneMinusMix + valG * mix));
        current[i + 2] = clampByte(static_cast<int>(current[i + 2] * oneMinusMix + valB * mix));
    }
}

void cpuAndWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double andValue, double intensity) {
    if (current.size() != previous.size() || current.empty() || intensity <= 0.0) {
        return;
    }

    const uint8_t mask = static_cast<uint8_t>(std::clamp(andValue * 255.0, 0.0, 255.0));
    const size_t totalBytes = current.size();
    const double mix = std::clamp(intensity, 0.0, 1.0);
    const double oneMinusMix = 1.0 - mix;
    for (size_t i = 0; i < totalBytes; i += 3) {
        uint8_t valR = current[i + 0] & (previous[i + 0] | ~mask);
        uint8_t valG = current[i + 1] & (previous[i + 1] | ~mask);
        uint8_t valB = current[i + 2] & (previous[i + 2] | ~mask);
        current[i + 0] = clampByte(static_cast<int>(current[i + 0] * oneMinusMix + valR * mix));
        current[i + 1] = clampByte(static_cast<int>(current[i + 1] * oneMinusMix + valG * mix));
        current[i + 2] = clampByte(static_cast<int>(current[i + 2] * oneMinusMix + valB * mix));
    }
}

void cpuXnorWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double xnorValue, double intensity) {
    if (current.size() != previous.size() || current.empty() || intensity <= 0.0) {
        return;
    }

    const uint8_t mask = static_cast<uint8_t>(std::clamp(xnorValue * 255.0, 0.0, 255.0));
    const size_t totalBytes = current.size();
    const double mix = std::clamp(intensity, 0.0, 1.0);
    const double oneMinusMix = 1.0 - mix;
    for (size_t i = 0; i < totalBytes; i += 3) {
        uint8_t valR = ~(current[i + 0] ^ (previous[i + 0] & mask));
        uint8_t valG = ~(current[i + 1] ^ (previous[i + 1] & mask));
        uint8_t valB = ~(current[i + 2] ^ (previous[i + 2] & mask));
        current[i + 0] = clampByte(static_cast<int>(current[i + 0] * oneMinusMix + valR * mix));
        current[i + 1] = clampByte(static_cast<int>(current[i + 1] * oneMinusMix + valG * mix));
        current[i + 2] = clampByte(static_cast<int>(current[i + 2] * oneMinusMix + valB * mix));
    }
}

void cpuNandWithPreviousFrame(std::vector<uint8_t>& current, const std::vector<uint8_t>& previous, double nandValue, double intensity) {
    if (current.size() != previous.size() || current.empty() || intensity <= 0.0) {
        return;
    }

    const uint8_t mask = static_cast<uint8_t>(std::clamp(nandValue * 255.0, 0.0, 255.0));
    const size_t totalBytes = current.size();
    const double mix = std::clamp(intensity, 0.0, 1.0);
    const double oneMinusMix = 1.0 - mix;
    for (size_t i = 0; i < totalBytes; i += 3) {
        uint8_t valR = ~(current[i + 0] & (previous[i + 0] | ~mask));
        uint8_t valG = ~(current[i + 1] & (previous[i + 1] | ~mask));
        uint8_t valB = ~(current[i + 2] & (previous[i + 2] | ~mask));
        current[i + 0] = clampByte(static_cast<int>(current[i + 0] * oneMinusMix + valR * mix));
        current[i + 1] = clampByte(static_cast<int>(current[i + 1] * oneMinusMix + valG * mix));
        current[i + 2] = clampByte(static_cast<int>(current[i + 2] * oneMinusMix + valB * mix));
    }
}

} 
