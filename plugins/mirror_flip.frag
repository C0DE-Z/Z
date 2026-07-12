#version 330 core
// @name Mirror Flip
// @desc Random mirror and flip segments
// @param segments Segment Count 1.0 16.0 4.0
// @param flipH Horizontal Flip 0.0 1.0 0.5
// @param flipV Vertical Flip 0.0 1.0 0.0
// @param animate Change Over Time 0.0 5.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float segments;
uniform float flipH;
uniform float flipV;
uniform float animate;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float seg = floor(uv.x * segments);
    float t = animate > 0.0 ? floor(time * animate) : 0.0;
    float h = hash(vec2(seg, t));

    if (h < flipH) uv.x = 1.0 - uv.x;
    if (h < flipV) uv.y = 1.0 - uv.y;

    FragColor = texture(videoTexture, uv);
}
