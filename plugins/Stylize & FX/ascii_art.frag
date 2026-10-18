#version 330 core
// @name ASCII Art
// @desc Renders the video as text symbols, Watch Dogs style. Pick a symbol set, or set Symbol Set to 5 and choose one glyph: 1 . 2 : 3 - 4 = 5 + 6 * 7 # 8 % 9 @ 10 zero 11 one 12 X 13 slash 14 backslash 15 pipe 16 O 17 light 18 mid 19 full block
// @param cellSize Character Size (px) 4.0 40.0 10.0
// @param symbolSet Symbol Set (0=Classic 1=Binary 2=Blocks 3=Lines 4=Matrix 5=Single) 0.0 5.0 0.0
// @param glyphChoice Single Glyph Index 1.0 19.0 7.0
// @param colorMode Color (0=Green 1=White 2=Source 3=Amber 4=Cyan) 0.0 4.0 2.0
// @param brightness Brightness 0.2 3.0 1.0
// @param contrast Contrast 0.5 3.0 1.3
// @param invert Invert Density 0.0 1.0 0.0 bool
// @param background Show Video Behind 0.0 1.0 0.0
// @param active Enable ASCII 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float cellSize;
uniform float symbolSet;
uniform float glyphChoice;
uniform float colorMode;
uniform float brightness;
uniform float contrast;
uniform float invert;
uniform float background;
uniform float active;

int G(int a, int b, int c, int d, int e) {
    return a | (b << 5) | (c << 10) | (d << 15) | (e << 20);
}

int glyphBits(int idx) {
    switch (idx) {
        case 0:  return 0;
        case 1:  return G(0, 0, 0, 0, 4);
        case 2:  return G(0, 4, 0, 4, 0);
        case 3:  return G(0, 0, 14, 0, 0);
        case 4:  return G(0, 14, 0, 14, 0);
        case 5:  return G(4, 4, 31, 4, 4);
        case 6:  return G(21, 14, 31, 14, 21);
        case 7:  return G(10, 31, 10, 31, 10);
        case 8:  return G(25, 2, 4, 8, 19);
        case 9:  return G(14, 21, 23, 16, 14);
        case 10: return G(14, 19, 21, 25, 14);
        case 11: return G(4, 12, 4, 4, 14);
        case 12: return G(17, 10, 4, 10, 17);
        case 13: return G(1, 2, 4, 8, 16);
        case 14: return G(16, 8, 4, 2, 1);
        case 15: return G(4, 4, 4, 4, 4);
        case 16: return G(14, 17, 17, 17, 14);
        case 17: return G(17, 0, 4, 0, 17);
        case 18: return G(21, 10, 21, 10, 21);
        case 19: return G(31, 31, 31, 31, 31);
        case 20: return G(2, 4, 8, 4, 2);
        case 21: return G(8, 4, 2, 4, 8);
    }
    return 0;
}

// Ten density levels per symbol set, indexes into glyphBits above.
const int SETS[50] = int[50](
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,        // Classic
    0, 1, 11, 11, 10, 10, 10, 11, 7, 19, // Binary
    0, 17, 17, 18, 18, 18, 19, 19, 19, 19, // Blocks
    0, 1, 3, 13, 14, 15, 12, 5, 7, 19,   // Lines
    0, 1, 2, 20, 21, 11, 10, 12, 7, 9    // Matrix
);

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }

    float size = max(cellSize, 4.0);
    vec2 cellCoord = TexCoord * resolution / size;
    vec2 cell = floor(cellCoord);
    vec2 local = fract(cellCoord);
    vec2 center = (cell + 0.5) * size / resolution;
    vec2 d = 0.25 * size / resolution;
    vec3 avg = (texture(videoTexture, clamp(center, 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(d.x, d.y), 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(-d.x, d.y), 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(d.x, -d.y), 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(-d.x, -d.y), 0.0, 1.0)).rgb) / 5.0;

    float l = clamp((luma(avg) - 0.5) * contrast + 0.5, 0.0, 1.0) * brightness;
    l = clamp(l, 0.0, 1.0);
    if (invert > 0.5) l = 1.0 - l;
    int level = int(min(l * 10.0, 9.0));

    int setIdx = clamp(int(symbolSet + 0.5), 0, 5);
    int glyph;
    if (setIdx == 5) {
        glyph = level == 0 ? 0 : int(glyphChoice + 0.5);
    } else {
        glyph = SETS[setIdx * 10 + level];
    }

    // 5x5 bitmap inside the cell with a one-subpixel gutter around it.
    vec2 g = (local - 0.1) / 0.8;
    float on = 0.0;
    if (g.x >= 0.0 && g.x < 1.0 && g.y >= 0.0 && g.y < 1.0) {
        int gx = int(g.x * 5.0);
        int gy = 4 - int(g.y * 5.0);
        int row = (glyphBits(glyph) >> (gy * 5)) & 31;
        on = float((row >> (4 - gx)) & 1);
    }

    int cm = int(colorMode + 0.5);
    vec3 ink;
    float glow = 0.35 + 0.65 * l;
    if (cm == 0) ink = vec3(0.15, 1.0, 0.3) * glow;
    else if (cm == 1) ink = vec3(1.0) * glow;
    else if (cm == 2) ink = clamp(avg * 1.6, 0.0, 1.0);
    else if (cm == 3) ink = vec3(1.0, 0.69, 0.1) * glow;
    else ink = vec3(0.2, 0.9, 1.0) * glow;

    vec3 paper = mix(vec3(0.0), orig.rgb * 0.5, background);
    vec3 outCol = mix(paper, ink, on);
    FragColor = vec4(outCol, orig.a);
}
