#version 330 core
// @name Rolling Shutter
// @desc CMOS rolling shutter skew and wobble
// @param skew Skew Amount 0.0 0.1 0.02
// @param wobbleRow Row Wobble 0.0 1.0 0.3
// @param speed Scan Speed 0.0 5.0 1.0
// @param bend Vertical Bend 0.0 0.05 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float skew;
uniform float wobbleRow;
uniform float speed;
uniform float bend;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float row = floor(uv.y * 480.0);

    uv.x += uv.y * skew;
    uv.x += sin(row * 0.1 + time * speed) * wobbleRow * 0.01;
    uv.x += hash(vec2(row, floor(time * 10.0))) * wobbleRow * 0.005;
    uv.y += sin(uv.x * 10.0) * bend;

    uv = clamp(uv, 0.0, 1.0);
    FragColor = texture(videoTexture, uv);
}
