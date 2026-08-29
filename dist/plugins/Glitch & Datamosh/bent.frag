#version 330 core
// @name Circuit Bent Camera
// @desc Hardware failure simulation
// @param syncDrift Sync Drift 0.0 1.0 0.0
// @param clockCorruption Pixel Clock Corruption 0.0 1.0 0.0
// @param railInstability Power Rail Instability 0.0 1.0 0.0
// @param addressScramble Address Scrambling 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float syncDrift;
uniform float clockCorruption;
uniform float railInstability;
uniform float addressScramble;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    if (syncDrift > 0.0) {
        float drift = sin(uv.y * 10.0 + time * 5.0) * 0.02 * syncDrift;
        uv.x = fract(uv.x + drift);
    }
    vec4 color = texture(videoTexture, uv);
    FragColor = color;
}
