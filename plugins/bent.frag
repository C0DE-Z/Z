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
        if (hash(vec2(floor(uv.y * 30.0), time)) > 0.98 - (syncDrift * 0.05)) {
            drift += hash(vec2(time)) * 0.1 * syncDrift;
        }
        uv.x = fract(uv.x + drift);
    }
    if (clockCorruption > 0.0) {
        float colSteps = 128.0 - (clockCorruption * 100.0);
        float stepX = floor(uv.x * colSteps) / colSteps;
        if (hash(vec2(stepX, time)) > 0.95 - (clockCorruption * 0.1)) {
            uv.x = stepX;
        }
    }
    if (addressScramble > 0.0) {
        float grid = 8.0;
        vec2 cell = floor(uv * grid) / grid;
        if (hash(cell + time) < addressScramble * 0.2) {
            uv = fract(uv + hash(cell) * 0.5);
        }
    }
    vec4 color = texture(videoTexture, uv);
    if (railInstability > 0.0) {
        float noiseValue = sin(time * 20.0) * cos(time * 35.0);
        color.rgb *= 1.0 + noiseValue * 0.4 * railInstability;
        if (noiseValue > 0.8) {
            color.rgb += vec3(0.3 * railInstability);
        }
    }
    FragColor = color;
}
