#version 330 core
// @name Chromatic Aberration
// @desc Radial lens chromatic aberration
// @param strength CA Strength 0.0 0.05 0.01
// @param radial Radial Falloff 0.0 2.0 1.0
// @param barrel Barrel Distort 0.0 0.5 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float strength;
uniform float radial;
uniform float barrel;

void main() {
    vec2 uv = TexCoord - 0.5;
    float r = length(uv);

    if (barrel > 0.0) {
        uv *= 1.0 + barrel * r * r;
    }

    float ca = strength * (1.0 + r * radial);
    vec2 base = uv + 0.5;

    float r_ch = texture(videoTexture, base + vec2(ca, 0.0)).r;
    float g_ch = texture(videoTexture, base).g;
    float b_ch = texture(videoTexture, base - vec2(ca, 0.0)).b;

    FragColor = vec4(r_ch, g_ch, b_ch, 1.0);
}
