#version 330 core
// @name FPV Analog Video Feed
// @desc 5.8GHz analog drone video feed simulation with RF multipath and motion smear
// @param signalStrength Signal Strength 0.0 1.0 0.9
// @param channelNoise Adjacent Channel Noise 0.0 1.0 0.1
// @param multipath Multipath Interference 0.0 1.0 0.0
// @param analogLatency Motion Smear (Latency Sim) 0.0 0.5 0.05
// @param lensCurve Camera Lens Distortion 0.0 0.3 0.05
// @param exposurePump Auto-Exposure Pump 0.0 1.0 0.1
// @param active Enable FPV Feed 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform vec2 resolution;
uniform float time;
uniform float signalStrength;
uniform float channelNoise;
uniform float multipath;
uniform float analogLatency;
uniform float lensCurve;
uniform float exposurePump;
uniform float active;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    // 1. Camera lens barrel distortion (FPV wide-angle lens)
    vec2 centered = TexCoord - 0.5;
    float r2 = dot(centered, centered);
    vec2 lensUV = TexCoord + centered * (r2 * lensCurve * 2.0);

    // 2. Multipath interference (RF signal cancellation behind obstacles)
    float mpFlutter = sin(time * 37.0) * cos(time * 19.0 + TexCoord.y * 15.0);
    float mpBurst = step(0.85, hash11(floor(time * 14.0))) * (0.5 + 0.5 * sin(time * 50.0));
    float mpPenalty = multipath * clamp(mpFlutter * 0.35 + mpBurst * 0.65, 0.0, 1.0);
    float effSignal = clamp(signalStrength - mpPenalty, 0.0, 1.0);
    float loss = 1.0 - effSignal;

    // 3. Horizontal tearing and sync loss
    float scanLine = floor(lensUV.y * 280.0);
    float lineRand = hash12(vec2(scanLine, floor(time * 60.0)));
    float lineTear = step(1.0 - smoothstep(0.2, 0.7, loss) * 0.45, lineRand) * (lineRand - 0.5) * smoothstep(0.2, 0.8, loss) * 0.08;

    // Block breakup in low signal
    vec2 blockCoord = floor(lensUV * vec2(16.0, 9.0));
    float blockRand = hash12(blockCoord + floor(time * 15.0));
    float blockShift = step(1.0 - smoothstep(0.5, 0.9, loss) * 0.5, blockRand) * (blockRand - 0.5) * smoothstep(0.5, 0.9, loss) * 0.15;

    vec2 sampleUV = clamp(lensUV + vec2(lineTear + blockShift, 0.0), 0.0, 1.0);

    // 4. Sample current frame and feedback texture for latency smear
    vec4 curFrame = texture(videoTexture, sampleUV);
    vec4 feedFrame = texture(feedbackTexture, sampleUV);
    vec3 videoCol = mix(curFrame.rgb, feedFrame.rgb, clamp(analogLatency, 0.0, 0.5));

    // 5. Camera exposure pumping and highlight clipping (FPV CMOS sensor AGC)
    float pumpOsc = 1.0 + exposurePump * (sin(time * 2.5) * 0.2 + sin(time * 5.3) * 0.08);
    videoCol *= pumpOsc;
    videoCol = mix(videoCol, clamp(videoCol * 1.3 - 0.05, 0.0, 1.0), exposurePump * 0.35);

    // 6. Rolling horizontal static bars (adjacent channel bleed)
    float barPhase = fract(lensUV.y * 2.5 - time * 0.9);
    float barMask = smoothstep(0.0, 0.12, barPhase) * smoothstep(0.4, 0.18, barPhase);
    float barNoise = hash12(lensUV * 260.0 + fract(time * 43.0));
    float barStatic = barMask * channelNoise;

    // 7. Progressive RF snow and static noise
    float snowFine = hash12(lensUV * 500.0 + fract(time * 73.0));
    float hNoiseLines = (sin(lensUV.y * 600.0 + time * 45.0) * 0.5 + 0.5) * hash11(lensUV.y * 400.0 + floor(time * 60.0));

    // Stage a: horizontal noise lines
    videoCol = mix(videoCol, videoCol + vec3(hNoiseLines * 0.25 - 0.12), smoothstep(0.05, 0.3, loss));

    // Stage b: static snow mixed in
    float snowBlend = smoothstep(0.18, 0.75, loss) * 0.75;
    videoCol = mix(videoCol, vec3(snowFine), snowBlend);

    // Stage c: block snow replacement
    float blockSnow = step(0.35, blockRand) * smoothstep(0.55, 0.95, loss);
    videoCol = mix(videoCol, vec3(snowFine), blockSnow);

    // Adjacent channel rolling bars overlay
    videoCol = mix(videoCol, vec3(barNoise), barStatic * 0.85);

    // Stage d: failsafe territory (mostly snow, brief glimpses of video)
    float failsafeGlimpse = step(0.92, hash11(floor(time * 9.0))) * smoothstep(0.98, 0.8, loss);
    float nearTotalLoss = smoothstep(0.8, 1.0, loss);
    videoCol = mix(videoCol, vec3(snowFine), nearTotalLoss * (1.0 - failsafeGlimpse * 0.35));

    FragColor = vec4(clamp(videoCol, 0.0, 1.0), baseColor.a);
}
