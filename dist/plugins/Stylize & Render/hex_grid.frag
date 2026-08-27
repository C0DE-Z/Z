#version 330 core
// @name Hex Grid
// @desc Hexagonal pixel grid displacement
// @param hexSize Hex Size 4.0 40.0 12.0
// @param displacement Displacement 0.0 0.1 0.02
// @param colorShift Per-Hex Tint 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float hexSize;
uniform float displacement;
uniform float colorShift;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

vec2 hexRound(vec2 p) {
    vec2 r = vec2(1.0, 1.732);
    vec2 h = r * 0.5;
    vec2 a = mod(p, r) - h;
    vec2 b = mod(p - h, r) - h;
    return dot(a, a) < dot(b, b) ? a : b;
}

void main() {
    vec2 p = TexCoord * hexSize;
    vec2 cell = p - hexRound(p);
    float h = hash(floor(p));

    vec2 uv = TexCoord + cell * displacement * (h - 0.5);
    uv = clamp(uv, 0.0, 1.0);

    vec4 color = texture(videoTexture, uv);
    color.rgb += vec3(h * colorShift * 0.2, h * colorShift * 0.1, 0.0);

    FragColor = color;
}
