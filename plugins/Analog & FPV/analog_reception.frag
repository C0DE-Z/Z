#version 330 core
// @name Analog TV Reception
// @desc Authentic over-the-air analog television reception simulation with multipath ghosting, Gaussian static, and sync instability
// @param ghostStrength Ghost Image Strength 0.0 0.5 0.1
// @param ghostOffset Ghost Horizontal Offset 0.001 0.05 0.02
// @param snow Snow/Static Level 0.0 1.0 0.15
// @param vHoldDrift Vertical Hold Drift 0.0 1.0 0.0
// @param hSyncJitter H-Sync Instability 0.0 1.0 0.05
// @param agcPump AGC Pumping 0.0 1.0 0.0
// @param impulseNoise Impulse Noise 0.0 1.0 0.02
// @param active Enable Analog Reception 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform vec2 resolution;
uniform float ghostStrength;
uniform float ghostOffset;
uniform float snow;
uniform float vHoldDrift;
uniform float hSyncJitter;
uniform float agcPump;
uniform float impulseNoise;
uniform float active;

vec2 hash2(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float hash1(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float gaussianNoise(vec2 p) {
    vec2 u = hash2(p);
    float r = sqrt(max(1e-7, -2.0 * log(clamp(u.x, 1e-7, 0.99999))));
    float phi = 6.28318530718 * u.y;
    return r * cos(phi);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = orig;
        return;
    }

    vec2 res = (resolution.x > 1.0 && resolution.y > 1.0) ? resolution : vec2(textureSize(videoTexture, 0));
    if (res.x < 1.0) res = vec2(1920.0, 1080.0);

    vec2 uv = TexCoord;

    // 1. Vertical Hold Drift: rolls vertically and catches near frame boundary
    float vbiMask = 0.0;
    if (vHoldDrift > 0.001) {
        float rollSpeed = vHoldDrift * 0.75;
        float rawRoll = time * rollSpeed;
        float rollCycle = fract(rawRoll);
        float stutter = sin(rollCycle * 6.2831853) * 0.06 * min(vHoldDrift * 2.0, 1.0);
        float vShift = fract(rawRoll + stutter);
        uv.y = fract(uv.y + vShift);
        float bar = smoothstep(0.0, 0.015, uv.y) * (1.0 - smoothstep(0.92, 0.94, uv.y));
        vbiMask = 1.0 - bar;
    }

    // 2. Horizontal Sync Instability (jitter & flag waving)
    float scanline = floor(uv.y * res.y);
    float frameTick = floor(time * 30.0);
    if (hSyncJitter > 0.001) {
        float hNoise = (hash1(vec2(scanline, frameTick)) - 0.5) * 2.0;
        float wave = sin(uv.y * 14.0 + time * 6.0) * cos(uv.y * 4.0 - time * 3.0);
        float tearProb = hash1(vec2(floor(time * 15.0), scanline * 0.02));
        float tear = (tearProb > 0.988) ? (hash1(vec2(scanline, time * 2.0)) - 0.5) * 0.05 : 0.0;
        float totalJitter = (hNoise * 0.003 + wave * 0.003 + tear) * hSyncJitter;
        uv.x = clamp(uv.x + totalJitter, 0.0, 1.0);
    }

    vec3 signal = texture(videoTexture, uv).rgb;
    if (vbiMask > 0.0) {
        signal = mix(signal, vec3(0.04), vbiMask);
        if (vbiMask > 0.5 && uv.x > 0.3 && uv.x < 0.7) signal = vec3(0.0);
    }

    // 3. Multipath Ghosting
    if (ghostStrength > 0.001) {
        vec2 gUV1 = vec2(clamp(uv.x + ghostOffset, 0.0, 1.0), uv.y);
        vec2 gUV2 = vec2(clamp(uv.x + ghostOffset * 2.15, 0.0, 1.0), uv.y);
        vec3 ghost1 = texture(videoTexture, gUV1).rgb;
        vec3 ghost2 = texture(videoTexture, gUV2).rgb;
        signal = signal + (ghost1 * 0.85 - ghost2 * 0.25) * ghostStrength;
    }

    // 4. AGC Pumping
    if (agcPump > 0.001) {
        float agcOsc = sin(time * 1.8) * 0.4 + sin(time * 0.6 + 1.7) * 0.35 + sin(time * 4.3) * 0.25;
        float agcGain = 1.0 + agcOsc * (agcPump * 0.4);
        signal = signal * agcGain + vec3(max(0.0, agcGain - 1.0) * 0.1 * agcPump);
    }

    // 5. Impulse Noise
    if (impulseNoise > 0.001) {
        float impRand = hash1(vec2(scanline, floor(time * 60.0)));
        if (impRand > (1.0 - impulseNoise * 0.04)) {
            float streakStart = hash1(vec2(scanline * 2.3, floor(time * 60.0)));
            float streakLen = 0.03 + hash1(vec2(scanline * 5.1, floor(time * 60.0))) * 0.18;
            if (uv.x >= streakStart && uv.x <= streakStart + streakLen) {
                float trail = 1.0 - (uv.x - streakStart) / streakLen;
                signal += vec3(trail * 1.6);
            }
        }
    }

    // 6. Snow / Static Noise
    if (snow > 0.001) {
        vec2 pixelCoord = uv * res;
        vec2 seedFine = pixelCoord + vec2(time * 137.15, time * 269.83);
        vec2 seedLine = vec2(pixelCoord.x * 0.35, scanline) + vec2(time * 91.31, 0.0);
        float gFine = gaussianNoise(seedFine);
        float gLine = gaussianNoise(seedLine);
        float gaussianSum = gFine * 0.7 + gLine * 0.55;

        float luma = dot(signal, vec3(0.299, 0.587, 0.114));
        signal = mix(signal, vec3(luma), clamp(snow * 1.25, 0.0, 0.85));
        signal += vec3(gaussianSum * snow * 0.45);
    }

    FragColor = vec4(clamp(signal, 0.0, 1.0), orig.a);
}
