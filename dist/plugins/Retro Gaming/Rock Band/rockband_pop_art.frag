#version 330 core
// @name Rock Band Pop-Art & Ink Outlines
// @desc Posterized color palette reduction with heavy black ink outlines
// @param levels Color Levels 2.0 16.0 4.0
// @param inkLines Ink Outlines 0.0 2.0 1.0
// @param active Enable Pop Art 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float levels;
uniform float inkLines;
uniform float active;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec3 col = floor(orig.rgb * levels) / levels;
    if (inkLines > 0.0) {
        vec2 step = 1.0 / max(vec2(1.0), resolution);
        float l = luma(texture(videoTexture, TexCoord - vec2(step.x, 0.0)).rgb);
        float r = luma(texture(videoTexture, TexCoord + vec2(step.x, 0.0)).rgb);
        float u = luma(texture(videoTexture, TexCoord - vec2(0.0, step.y)).rgb);
        float d = luma(texture(videoTexture, TexCoord + vec2(0.0, step.y)).rgb);
        float edge = length(vec2(r - l, d - u)) * inkLines;
        if (edge > 0.2) {
            col = vec3(0.0);
        }
    }
    FragColor = vec4(col, orig.a);
}
