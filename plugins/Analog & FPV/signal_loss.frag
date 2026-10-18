#version 330 core
// @name Progressive Signal Loss
// @desc Progressive analog signal degradation from subtle grain to total static loss
// @param distance Signal Loss / Distance 0.0 1.0 0.0
// @param instability Signal Instability 0.0 1.0 0.3
// @param colorFade Color Loss Speed 0.5 3.0 1.5
// @param active Enable Signal Loss 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float distance;
uniform float instability;
uniform float colorFade;
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

    // 1. Distance fluctuation from signal instability
    float tFlux = sin(time * 2.8) * 0.45 + sin(time * 7.1) * 0.35 + sin(time * 19.4) * 0.2;
    float effDist = clamp(distance + instability * 0.12 * tFlux, 0.0, 1.0);

    // 2. Horizontal jitter and tearing stages
    float scanline = floor(TexCoord.y * 360.0);
    float frameTick = floor(time * 50.0);

    // Subtle jitter (0.1 - 0.25)
    float minorJitter = (hash12(vec2(scanline, frameTick)) - 0.5) * smoothstep(0.1, 0.25, effDist) * 0.005;

    // Horizontal tearing & displacement (0.25 - 0.6)
    float tearLineSeed = hash12(vec2(scanline * 0.3, floor(time * 25.0)));
    float tearActive = step(1.0 - smoothstep(0.25, 0.6, effDist) * 0.35, tearLineSeed);
    float tearDisplace = (tearLineSeed - 0.5) * tearActive * smoothstep(0.25, 0.6, effDist) * 0.06;

    // Heavy displacement (0.6 - 0.95)
    float heavyDisplace = (hash12(vec2(floor(TexCoord.y * 25.0), floor(time * 18.0))) - 0.5) * smoothstep(0.6, 0.9, effDist) * 0.22;

    float totalDisplaceX = minorJitter + tearDisplace + heavyDisplace;

    // 3. Vertical hold drift and vertical roll (0.4 - 0.8)
    float vDrift = sin(time * 2.0) * smoothstep(0.4, 0.6, effDist) * 0.06;
    float rollSpeed = smoothstep(0.6, 0.8, effDist) * (1.8 + sin(time * 0.6) * 0.4);
    float vRoll = fract(TexCoord.y + time * rollSpeed + vDrift);
    float uvY = mix(TexCoord.y + vDrift, vRoll, smoothstep(0.55, 0.75, effDist));

    vec2 sampleUV = fract(vec2(TexCoord.x + totalDisplaceX, uvY));

    // 4. Sample video with ghost image RF multipath echoes (0.25 - 0.55)
    vec4 sampledColor = texture(videoTexture, sampleUV);
    float ghostEnv = smoothstep(0.25, 0.38, effDist) * (1.0 - smoothstep(0.55, 0.75, effDist));
    if (ghostEnv > 0.01) {
        vec2 ghostUV = fract(sampleUV + vec2(0.045, 0.002));
        vec4 ghostColor = texture(videoTexture, ghostUV);
        sampledColor = mix(sampledColor, ghostColor, ghostEnv * 0.35);
    }

    // 5. Color loss / desaturation (NTSC/PAL color burst threshold loss)
    float sat = clamp(1.0 - pow(effDist, 1.0 / max(colorFade, 0.2)) * colorFade, 0.0, 1.0);
    float luma = dot(sampledColor.rgb, vec3(0.299, 0.587, 0.114));
    vec3 col = mix(vec3(luma), sampledColor.rgb, sat);

    // 6. AGC brightness fluctuation (0.4 - 0.6)
    float agcGain = 1.0 + (sin(time * 11.3) * 0.25 + cos(time * 17.1) * 0.15) * smoothstep(0.38, 0.65, effDist) * (1.0 - smoothstep(0.75, 0.9, effDist));
    col *= clamp(agcGain, 0.5, 1.5);

    // 7. Static snow generation across stages
    float snowFine = hash12(TexCoord * 600.0 + fract(time * 59.3));
    float snowCoarse = hash12(floor(TexCoord * 200.0) + fract(time * 31.7));
    float snowPattern = mix(snowFine, snowCoarse, 0.35);

    float snowMix = smoothstep(0.08, 0.95, effDist);

    // Brief flashes / glimpses of recognizable image at 0.6 - 0.95
    float glimpseFlicker = step(0.9, hash12(vec2(floor(time * 9.0), 3.14))) * (1.0 - smoothstep(0.85, 0.96, effDist));
    snowMix = clamp(snowMix - glimpseFlicker * 0.35, 0.0, 1.0);

    vec3 result = mix(col, vec3(snowPattern), snowMix);

    // 8. Complete signal loss (0.95 - 1.0)
    float completeLoss = smoothstep(0.95, 1.0, effDist);
    float blackOrStatic = hash12(vec2(floor(time * 2.0), 7.0));
    vec3 finalLoss = mix(vec3(snowPattern * 0.7), vec3(0.02), step(0.5, blackOrStatic));
    result = mix(result, finalLoss, completeLoss);

    FragColor = vec4(clamp(result, 0.0, 1.0), baseColor.a);
}
