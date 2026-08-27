#version 330 core
// @name Pixel Sorter
// @desc Luma-based directional pixel sorting glitch
// @param threshold Luma Threshold 0.0 1.0 0.45
// @param sortLength Streak Length 0.0 1.0 0.5
// @param active Enable Pixel Sorter 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float threshold;
uniform float sortLength;
uniform float active;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec2 uv = TexCoord;
    float currentLuma = luma(orig.rgb);
    if (currentLuma > threshold) {
        float stepY = 1.0 / max(1.0, resolution.y);
        float offset = floor(currentLuma * sortLength * 50.0) * stepY;
        uv.y = clamp(uv.y - offset, 0.0, 1.0);
    }
    FragColor = texture(videoTexture, uv);
}
