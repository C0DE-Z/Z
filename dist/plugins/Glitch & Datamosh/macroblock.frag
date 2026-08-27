#version 330 core
// @name Macroblock Corruption
// @desc H.264-style macroblock displacement and smear (Supermosh vibes)
// @param blockSize Block Size 4.0 64.0 16.0
// @param corruption Corruption Amount 0.0 1.0 0.3
// @param smear Smear Length 0.0 0.3 0.05
// @param seed Random Seed 0.0 100.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float blockSize;
uniform float corruption;
uniform float smear;
uniform float seed;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    vec2 block = floor(uv * blockSize) / blockSize;
    vec2 blockId = floor(uv * blockSize);
    float h = hash(blockId + seed + floor(time * 3.0));

    if (h < corruption * 0.4) {
        vec2 offset = vec2(hash(blockId + 1.0), hash(blockId + 2.0)) - 0.5;
        offset *= smear * 4.0;
        uv = block + fract(uv * blockSize) / blockSize + offset;
    } else if (h < corruption * 0.7) {
        uv.x = block.x + hash(blockId + 3.0) * (1.0 / blockSize);
    }

    uv = clamp(uv, 0.0, 1.0);
    vec4 color = texture(videoTexture, uv);

    if (h < corruption * 0.15) {
        color = texture(videoTexture, block + vec2(0.5 / blockSize));
    }

    FragColor = color;
}
