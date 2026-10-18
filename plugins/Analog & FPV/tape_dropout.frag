#version 330 core
// @name Analog Tape Dropout
// @desc Magnetic tape oxide loss, helical scan dropouts, edge damage, and head clog
// @param dropoutRate Dropout Frequency 0.0 1.0 0.15
// @param dropoutLength Max Dropout Length 0.0 1.0 0.3
// @param edgeDamage Edge Damage 0.0 1.0 0.1
// @param stickyBurst Sticky Shed Bursts 0.0 1.0 0.0
// @param headClog Head Clog Level 0.0 1.0 0.0
// @param speedWobble Tape Speed Wobble 0.0 1.0 0.0
// @param active Enable Tape Dropout 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float dropoutRate;
uniform float dropoutLength;
uniform float edgeDamage;
uniform float stickyBurst;
uniform float headClog;
uniform float speedWobble;
uniform float active;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    // 1. Sticky shed burst envelope
    float burstCycle = fract(time * 0.35);
    float burstEnv = smoothstep(0.65, 0.8, burstCycle) * (1.0 - smoothstep(0.85, 0.95, burstCycle));
    float activeSticky = stickyBurst * burstEnv;

    // 2. Tape speed wobble (capstan flutter / wow and binder drag)
    float totalWobble = speedWobble + activeSticky * 0.7;
    float flutterY = (sin(time * 30.0) * 0.0015 + sin(time * 6.0) * 0.004) * totalWobble;
    float stretchY = sin(TexCoord.y * 8.0 + time * 3.5) * 0.003 * totalWobble;
    float jitterX = sin(time * 48.0 + TexCoord.y * 110.0) * 0.0012 * totalWobble;
    vec2 uv = TexCoord + vec2(jitterX, flutterY + stretchY);

    // 3. Head clog buildup & sudden dislodgement
    float clogCycle = fract(time * 0.1);
    float clogRamp = smoothstep(0.0, 0.82, clogCycle) * (1.0 - step(0.88, clogCycle));
    float currentClog = headClog * clogRamp;

    // Head clog high-frequency loss (horizontal blurring)
    vec4 sampledColor = texture(videoTexture, uv);
    vec3 procCol = sampledColor.rgb;
    if (currentClog > 0.01) {
        float bSpread = currentClog * 0.012;
        vec3 b1 = texture(videoTexture, uv + vec2(-bSpread, 0.0)).rgb;
        vec3 b2 = texture(videoTexture, uv + vec2(bSpread, 0.0)).rgb;
        vec3 b3 = texture(videoTexture, uv + vec2(-bSpread * 2.0, 0.0)).rgb;
        vec3 b4 = texture(videoTexture, uv + vec2(bSpread * 2.0, 0.0)).rgb;
        procCol = (procCol * 2.0 + b1 + b2 + (b3 + b4) * 0.5) / 5.0;
    }

    // 4. Edge damage weighting (tape edges crinkling near top and bottom)
    float distFromEdge = min(uv.y, 1.0 - uv.y);
    float edgeFactor = 1.0 + edgeDamage * 5.0 * exp(-distFromEdge * 14.0);

    // 5. Helical scanline dropout generation
    float scanline = floor(uv.y * 480.0);
    float frameTick = floor(time * 29.97);
    float effDropRate = clamp(dropoutRate * edgeFactor + activeSticky * 0.45 + currentClog * 0.35, 0.0, 1.0);

    float lineSeed = hash12(vec2(scanline, frameTick));
    float hasDropout = step(1.0 - effDropRate * 0.4, lineSeed);

    float inDropout = 0.0;
    float chromaBleed = 0.0;

    if (hasDropout > 0.5) {
        float startX = hash12(vec2(scanline * 2.71, frameTick * 1.41));
        float dLen = (0.01 + hash12(vec2(scanline * 5.19, frameTick * 3.17)) * dropoutLength) * (1.0 + activeSticky * 1.5);

        if (uv.x >= startX && uv.x <= startX + dLen) {
            inDropout = 1.0;
        }

        // Chroma bleed: horizontal color smear trailing the dropout streak
        float trail = uv.x - (startX + dLen);
        if (trail > 0.0 && trail < 0.08) {
            chromaBleed = 1.0 - trail / 0.08;
        }
    }

    // Dropout clustering on nearby scanlines
    float clusterSeed = hash12(vec2(floor(scanline * 0.5), frameTick * 0.7));
    if (clusterSeed < effDropRate * 0.3) {
        float cStartX = hash12(vec2(scanline * 8.33, frameTick * 4.91));
        float cLen = hash12(vec2(scanline * 11.2, frameTick * 7.13)) * dropoutLength * 0.4;
        if (uv.x >= cStartX && uv.x <= cStartX + cLen) {
            inDropout = 1.0;
        }
    }

    // Head clog dropout bands
    if (currentClog > 0.45) {
        float clogBandY = fract(time * 0.04 + 0.35);
        if (abs(uv.y - clogBandY) < 0.035 * currentClog) {
            float clogNoise = hash12(vec2(floor(uv.x * 50.0), scanline + frameTick));
            if (clogNoise < 0.65) {
                inDropout = 1.0;
            }
        }
    }

    // 6. Render dropout: replacement signal from DOC (previous line) or white/gray RF spike
    if (inDropout > 0.5) {
        vec2 docUV = vec2(uv.x, uv.y - 1.0 / 480.0);
        vec3 docColor = texture(videoTexture, docUV).rgb;
        float whiteSpike = step(0.4, hash12(vec2(floor(uv.x * 240.0), scanline)));
        vec3 streakCol = mix(docColor, vec3(0.92, 0.94, 0.97), whiteSpike * 0.85);
        procCol = streakCol;
    } else if (chromaBleed > 0.0) {
        vec3 smearedCol = texture(videoTexture, uv - vec2(0.025 * chromaBleed, 0.0)).rgb;
        procCol = mix(procCol, smearedCol, chromaBleed * 0.65);
    }

    FragColor = vec4(clamp(procCol, 0.0, 1.0), baseColor.a);
}
