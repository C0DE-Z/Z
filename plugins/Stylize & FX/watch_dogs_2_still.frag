#version 330 core
// @name Watch Dogs 2 (Still)
// @desc ctOS-style hacked-feed still: block displacement, RGB split, scanlines and data grid
// @param blockSize Glitch Block Size 8.0 128.0 48.0
// @param glitchAmount Block Displacement 0.0 1.0 0.5
// @param rgbSplit RGB Split 0.0 0.05 0.012
// @param scanline Scanlines 0.0 1.0 0.4
// @param grid Data Grid 0.0 1.0 0.3
// @param tint ctOS Tint 0.0 1.0 0.5
// @param seed Glitch Seed 0.0 100.0 7.0
// @param active Enable Watch Dogs 2 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float blockSize;
uniform float glitchAmount;
uniform float rgbSplit;
uniform float scanline;
uniform float grid;
uniform float tint;
uniform float seed;
uniform float active;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.13) * 43758.5453);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }

    vec2 px = TexCoord * resolution;
    vec2 cell = floor(px / max(blockSize, 1.0));
    float rowPick = hash(vec2(0.0, cell.y));
    float cellPick = hash(cell);
    float glitchRow = step(1.0 - glitchAmount * 0.35, rowPick);
    float glitchCell = step(1.0 - glitchAmount * 0.15, cellPick);

    vec2 uv = TexCoord;
    uv.x += glitchRow * (rowPick - 0.5) * 0.12 * glitchAmount;
    uv += glitchCell * (vec2(cellPick, hash(cell + 3.7)) - 0.5) * 0.04 * glitchAmount;
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    float split = rgbSplit * (1.0 + glitchRow * 3.0);
    float r = texture(videoTexture, clamp(uv + vec2(split, 0.0), 0.0, 1.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, clamp(uv - vec2(split, 0.0), 0.0, 1.0)).b;
    vec3 col = vec3(r, g, b);

    float l = dot(col, vec3(0.299, 0.587, 0.114));
    vec3 ctos = mix(vec3(0.02, 0.10, 0.16), vec3(0.35, 0.95, 1.0), l);
    col = mix(col, ctos * 1.1 + col * 0.15, tint);
    col = mix(col, vec3(col.r * 1.4, col.g * 0.4, col.b * 1.2), glitchCell * 0.5 * glitchAmount);

    float lines = 0.5 + 0.5 * sin(px.y * 3.14159);
    col *= 1.0 - scanline * 0.35 * lines;

    vec2 g2 = abs(fract(px / 32.0) - 0.5);
    float gridLine = 1.0 - smoothstep(0.0, 0.03, min(g2.x, g2.y));
    col += vec3(0.1, 0.8, 1.0) * gridLine * grid * 0.35;

    FragColor = vec4(clamp(col, 0.0, 1.0), orig.a);
}
