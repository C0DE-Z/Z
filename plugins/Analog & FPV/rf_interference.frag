#version 330 core
// @name RF Interference
// @desc Realistic radio frequency interference including herringbone patterns, power line hum bars, motor whine, and ignition noise
// @param herringbone Herringbone Pattern 0.0 1.0 0.0
// @param humbars Power Line Hum Bars 0.0 1.0 0.0
// @param motorWhine Motor Whine Lines 0.0 1.0 0.0
// @param ignitionNoise Ignition Noise 0.0 1.0 0.0
// @param digitalBleed Digital Clock Bleed 0.0 1.0 0.0
// @param signalBeat Signal Beat Frequency 0.1 20.0 2.0
// @param active Enable RF Interference 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform vec2 resolution;
uniform float herringbone;
uniform float humbars;
uniform float motorWhine;
uniform float ignitionNoise;
uniform float digitalBleed;
uniform float signalBeat;
uniform float active;

float hash1(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = orig;
        return;
    }

    vec2 res = (resolution.x > 1.0 && resolution.y > 1.0) ? resolution : vec2(textureSize(videoTexture, 0));
    if (res.x < 1.0) res = vec2(1920.0, 1080.0);

    // 1. Signal Beating
    float beatEnv = 0.82 + 0.18 * sin(time * signalBeat * 6.2831853);

    vec2 uv = TexCoord;

    // 2. Power Line Hum Bars
    float humLuma = 0.0;
    if (humbars > 0.001) {
        float humPhase = uv.y * 1.5 - time * 0.22;
        float humCycle = humPhase * 6.2831853;
        float hWarp = cos(humCycle) * 0.006 * humbars;
        uv.x = clamp(uv.x + hWarp, 0.0, 1.0);
        humLuma = sin(humCycle) * 0.18 * humbars * beatEnv;
    }

    vec3 signal = texture(videoTexture, uv).rgb;
    signal *= (1.0 + (beatEnv - 1.0) * 0.15);
    signal += vec3(humLuma);

    // 3. Herringbone Pattern
    if (herringbone > 0.001) {
        float a1 = (uv.x * 125.0 + uv.y * 85.0) * 3.14159265 + time * 16.0;
        float a2 = (uv.x * 125.0 - uv.y * 85.0) * 3.14159265 - time * 13.0;
        float chevron = sin(a1) * cos(a2);
        chevron += sin(a1 * 2.0 + 1.2) * 0.25;
        signal += vec3(chevron * herringbone * 0.28 * beatEnv);
    }

    // 4. Motor Whine Lines
    if (motorWhine > 0.001) {
        float rpmFlutter = sin(time * 7.5) * 6.0;
        float whinePhase = (uv.x * 65.0 + uv.y * 290.0 + time * (130.0 + rpmFlutter));
        float whine1 = pow(0.5 + 0.5 * sin(whinePhase), 14.0);
        float whine2 = pow(0.5 + 0.5 * sin(whinePhase * 1.5 - time * 45.0), 18.0) * 0.45;
        signal += vec3((whine1 + whine2) * motorWhine * 0.45 * beatEnv);
    }

    // 5. Ignition Noise
    if (ignitionNoise > 0.001) {
        float scanline = floor(uv.y * res.y);
        float frameTick = floor(time * 60.0);
        float sparkRand = hash1(vec2(scanline, frameTick));
        if (sparkRand > (1.0 - ignitionNoise * 0.055)) {
            float sparkX = hash1(vec2(scanline * 3.7, frameTick));
            float sparkLen = 0.025 + hash1(vec2(scanline * 7.1, frameTick)) * 0.14;
            if (uv.x >= sparkX && uv.x <= sparkX + sparkLen) {
                float d = (uv.x - sparkX) / sparkLen;
                float pulse = exp(-d * 3.2) * (1.0 + sin(d * 24.0) * 0.5);
                signal += vec3(pulse * 1.35 * beatEnv);
            }
        }
    }

    // 6. Digital Clock Bleed
    if (digitalBleed > 0.001) {
        float colPos = fract(uv.x * 45.0);
        float clockLine = smoothstep(0.0, 0.08, colPos) * smoothstep(0.16, 0.08, colPos);
        float busNoise = hash1(vec2(floor(uv.x * 45.0), floor(time * 35.0) + floor(uv.y * 28.0)));
        float clockStripe = clockLine * (0.6 + 0.4 * busNoise);
        signal += vec3(clockStripe * digitalBleed * 0.35 * beatEnv);
    }

    FragColor = vec4(clamp(signal, 0.0, 1.0), orig.a);
}
