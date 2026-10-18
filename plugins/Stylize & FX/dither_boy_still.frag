#version 330 core
// @name Dither Boy (Still)
// @desc Ordered Bayer dither with a limited retro palette, like a handheld-console still
// @param ditherScale Pixel Size 1.0 16.0 3.0
// @param levels Color Levels 2.0 8.0 2.0
// @param strength Dither Strength 0.0 1.0 1.0
// @param palette Palette (0=Mono 1=Green LCD 2=Amber 3=Color) 0.0 3.0 1.0
// @param contrast Contrast 0.5 2.5 1.1
// @param active Enable Dither Boy 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float ditherScale;
uniform float levels;
uniform float strength;
uniform float palette;
uniform float contrast;
uniform float active;

float bayer4(ivec2 p) {
    const int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[(p.y & 3) * 4 + (p.x & 3)]) + 0.5) / 16.0;
}

vec3 shade(float v, int pal) {
    if (pal == 0) return vec3(v);
    if (pal == 1) return mix(vec3(0.06, 0.22, 0.06), vec3(0.61, 0.74, 0.06), v);
    return mix(vec3(0.15, 0.05, 0.0), vec3(1.0, 0.69, 0.1), v);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }

    float size = max(ditherScale, 1.0);
    vec2 snapped = (floor(TexCoord * resolution / size) + 0.5) * size / resolution;
    vec3 src = texture(videoTexture, clamp(snapped, 0.0, 1.0)).rgb;
    src = clamp((src - 0.5) * contrast + 0.5, 0.0, 1.0);

    float steps = max(levels, 2.0) - 1.0;
    float threshold = (bayer4(ivec2(floor(TexCoord * resolution / size))) - 0.5) * strength;
    int pal = int(palette + 0.5);

    vec3 outCol;
    if (pal >= 3) {
        outCol = floor(clamp(src + threshold / steps, 0.0, 1.0) * steps + 0.5) / steps;
    } else {
        float l = dot(src, vec3(0.299, 0.587, 0.114));
        l = floor(clamp(l + threshold / steps, 0.0, 1.0) * steps + 0.5) / steps;
        outCol = shade(l, pal);
    }
    FragColor = vec4(outCol, orig.a);
}
