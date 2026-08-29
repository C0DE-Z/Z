#version 330 core
// @name Glitch Blocks
// @desc Random colored block insertions
// @param density Block Density 0.0 1.0 0.2
// @param blockSize Block Size 4.0 32.0 8.0
// @param colorNoise Color Noise 0.0 1.0 0.5
// @param shift Block Shift 0.0 0.2 0.05
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float density;
uniform float blockSize;
uniform float colorNoise;
uniform float shift;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    vec2 blockId = floor(uv * blockSize);
    float h = hash(blockId + floor(time * 8.0));

    if (h < density * 0.3) {
        vec3 glitchColor = vec3(hash(blockId), hash(blockId + 1.0), hash(blockId + 2.0));
        FragColor = vec4(mix(texture(videoTexture, uv).rgb, glitchColor, colorNoise), 1.0);
        return;
    }

    if (h < density * 0.6) {
        uv.x += (hash(blockId + 3.0) - 0.5) * shift;
    }

    FragColor = texture(videoTexture, clamp(uv, 0.0, 1.0));
}
