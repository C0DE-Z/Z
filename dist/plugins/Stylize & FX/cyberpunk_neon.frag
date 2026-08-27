#version 330 core
// @name Cyberpunk Neon Glow
// @desc Edge detection neon glow with synthwave palette grading
// @param neonGlow Neon Edge Brightness 0.0 3.0 1.5
// @param scanline Scanline Grid 0.0 1.0 0.3
// @param active Enable Cyberpunk 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float neonGlow;
uniform float scanline;
uniform float active;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 step = 1.0 / max(vec2(1.0), resolution);
    float l = luma(texture(videoTexture, TexCoord - vec2(step.x, 0.0)).rgb);
    float r = luma(texture(videoTexture, TexCoord + vec2(step.x, 0.0)).rgb);
    float u = luma(texture(videoTexture, TexCoord - vec2(0.0, step.y)).rgb);
    float d = luma(texture(videoTexture, TexCoord + vec2(0.0, step.y)).rgb);
    float edge = length(vec2(r - l, d - u)) * neonGlow;
    vec3 cyan = vec3(0.0, 0.9, 1.0);
    vec3 magenta = vec3(1.0, 0.0, 0.7);
    vec3 neonColor = mix(cyan, magenta, TexCoord.x) * edge;
    vec3 graded = mix(baseColor.rgb * vec3(0.6, 0.5, 0.8), neonColor, clamp(edge, 0.0, 1.0));
    if (scanline > 0.0) {
        float grid = sin(TexCoord.y * resolution.y * 0.5) * 0.1 * scanline;
        graded -= vec3(grid);
    }
    FragColor = vec4(graded, baseColor.a);
}
