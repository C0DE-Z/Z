#version 330 core
// @name Barrel Roll
// @desc Full-frame rotation with edge smear
// @param rotation Rotation Speed 0.0 3.0 0.5
// @param wobble Wobble Amount 0.0 0.2 0.02
// @param zoom Roll Zoom 0.8 1.5 1.1
// @param audioSpin Audio Spin Boost 0.0 2.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float rotation;
uniform float wobble;
uniform float zoom;
uniform float audioSpin;
uniform float audioBass;

void main() {
    vec2 uv = TexCoord - 0.5;
    float angle = time * rotation + audioBass * audioSpin;
    angle += sin(time * 3.0) * wobble;

    float s = sin(angle);
    float c = cos(angle);
    uv = mat2(c, -s, s, c) * uv * zoom;

    uv += vec2(sin(uv.y * 20.0) * wobble, cos(uv.x * 20.0) * wobble);
    uv += 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    FragColor = texture(videoTexture, uv);
}
