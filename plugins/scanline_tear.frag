#version 330 core
// @name Scanline Tear
// @desc Horizontal scanline displacement and tearing
// @param tearAmount Tear Strength 0.0 0.3 0.05
// @param lineCount Scanline Count 50.0 500.0 200.0
// @param jitter Line Jitter 0.0 1.0 0.3
// @param roll Vertical Roll 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float tearAmount;
uniform float lineCount;
uniform float jitter;
uniform float roll;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float line = floor(uv.y * lineCount);
    float h = hash(vec2(line, floor(time * 10.0)));

    if (h < jitter * 0.3) {
        uv.x += (h - 0.15) * tearAmount * 10.0;
    } else {
        uv.x += sin(line * 0.1 + time * 5.0) * tearAmount * 0.3;
    }

    uv.y = fract(uv.y + roll * time * 0.1);
    uv.x = fract(uv.x);
    FragColor = texture(videoTexture, uv);
}
