#version 330 core
// @name Signal Scramble
// @desc Analog signal frequency interference
// @param freq Interference Freq 1.0 100.0 20.0
// @param amp Interference Amp 0.0 0.1 0.02
// @param chaos Chaos Amount 0.0 1.0 0.3
// @param colorShift Color Interference 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float freq;
uniform float amp;
uniform float chaos;
uniform float colorShift;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float interference = sin(uv.y * freq + time * 10.0) * amp;
    interference += sin(uv.x * freq * 0.7 - time * 7.0) * amp * 0.5;

    if (hash(vec2(floor(uv.y * 50.0), time)) < chaos * 0.1) {
        interference += (hash(uv + time) - 0.5) * amp * 5.0;
    }

    uv.x += interference;
    uv = fract(uv);

    vec4 color;
    color.r = texture(videoTexture, uv + vec2(colorShift * 0.01, 0.0)).r;
    color.g = texture(videoTexture, uv).g;
    color.b = texture(videoTexture, uv - vec2(colorShift * 0.01, 0.0)).b;
    color.a = 1.0;

    FragColor = color;
}
