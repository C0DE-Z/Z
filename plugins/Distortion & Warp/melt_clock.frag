#version 330 core
// @name Melt Clock
// @desc Dripping melt distortion (Salvador Dali clock vibes)
// @param melt Melt Strength 0.0 0.3 0.08
// @param drip Drip Frequency 1.0 20.0 5.0
// @param gravity Gravity Pull 0.0 1.0 0.5
// @param wobble Drip Wobble 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float melt;
uniform float drip;
uniform float gravity;
uniform float wobble;

void main() {
    vec2 uv = TexCoord;
    float dripWave = sin(uv.x * drip * 3.14159 + time) * 0.5 + 0.5;
    dripWave *= sin(uv.x * drip * 7.0 - time * 0.5) * 0.5 + 0.5;

    uv.y += dripWave * melt * gravity;
    uv.x += sin(uv.y * 15.0 + time) * melt * wobble * 0.3;

    uv = clamp(uv, 0.0, 1.0);
    FragColor = texture(videoTexture, uv);
}
