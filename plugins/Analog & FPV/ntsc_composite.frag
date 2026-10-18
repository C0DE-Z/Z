#version 330 core
// @name NTSC Composite Artifacts
// @desc Authentic NTSC composite video artifacts including dot crawl, cross-color rainbows, and chroma bandwidth limiting
// @param dotCrawl Dot Crawl 0.0 1.0 0.3
// @param crossColor Cross-Color Rainbows 0.0 1.0 0.2
// @param chromaBlur Chroma Bandwidth Limit 0.0 1.0 0.4
// @param edgeRinging Edge Ringing 0.0 1.0 0.2
// @param signalNoise Signal Noise Floor 0.0 0.5 0.05
// @param active Enable NTSC Artifacts 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform vec2 resolution;
uniform float dotCrawl;
uniform float crossColor;
uniform float chromaBlur;
uniform float edgeRinging;
uniform float signalNoise;
uniform float active;

vec3 rgb2yiq(vec3 c) {
    return vec3(
        0.299 * c.r + 0.587 * c.g + 0.114 * c.b,
        0.596 * c.r - 0.274 * c.g - 0.322 * c.b,
        0.211 * c.r - 0.523 * c.g + 0.312 * c.b
    );
}

vec3 yiq2rgb(vec3 yiq) {
    return vec3(
        yiq.x + 0.956 * yiq.y + 0.621 * yiq.z,
        yiq.x - 0.272 * yiq.y - 0.647 * yiq.z,
        yiq.x - 1.106 * yiq.y + 1.703 * yiq.z
    );
}

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
    if (res.x < 1.0) res = vec2(640.0, 480.0);

    float lineIdx = floor(TexCoord.y * res.y);
    float frameIdx = floor(time * 29.97);

    float scFreq = 227.5 * 6.28318530718;
    float scPhase = TexCoord.x * scFreq + lineIdx * 3.14159265 + frameIdx * 3.14159265;

    vec3 centerCol = orig.rgb;
    vec3 centerYiq = rgb2yiq(centerCol);
    float yCenter = centerYiq.x;

    // 1. Chroma Bandwidth Limiting
    float dx = (1.0 / res.x) * (1.0 + chromaBlur * 6.0);
    float chromaDelay = (1.0 / res.x) * 1.5 * chromaBlur;
    vec2 chromaAccum = vec2(0.0);
    float weights[7] = float[7](0.061, 0.121, 0.242, 0.382, 0.242, 0.121, 0.061);
    float totalW = 0.0;
    for (int i = -3; i <= 3; ++i) {
        float offset = float(i) * dx - chromaDelay;
        vec3 sYiq = rgb2yiq(texture(videoTexture, TexCoord + vec2(offset, 0.0)).rgb);
        float w = weights[i + 3];
        chromaAccum += sYiq.yz * w;
        totalW += w;
    }
    vec2 filteredChroma = chromaAccum / totalW;

    // 2. Edge Ringing
    float ringDx = 1.0 / res.x;
    float yL2 = rgb2yiq(texture(videoTexture, TexCoord - vec2(ringDx * 2.0, 0.0)).rgb).x;
    float yL1 = rgb2yiq(texture(videoTexture, TexCoord - vec2(ringDx, 0.0)).rgb).x;
    float yR1 = rgb2yiq(texture(videoTexture, TexCoord + vec2(ringDx, 0.0)).rgb).x;
    float yR2 = rgb2yiq(texture(videoTexture, TexCoord + vec2(ringDx * 2.0, 0.0)).rgb).x;
    float yRinging = yCenter + ((yCenter - yL1) * 0.35 - (yL1 - yL2) * 0.18 + (yCenter - yR1) * 0.35 - (yR1 - yR2) * 0.18) * edgeRinging;

    // 3. Dot Crawl & Cross-Luminance
    vec2 chromaDiff = abs(filteredChroma - centerYiq.yz);
    float chromaEdge = length(chromaDiff) * 3.5 + length(filteredChroma) * 0.35;
    float crawlMod = sin(scPhase) * chromaEdge * dotCrawl * 0.55;
    yRinging += crawlMod;

    // 4. Cross-Color Rainbow Banding
    float highFreqLuma = (2.0 * yCenter - yL1 - yR1);
    float falseI = highFreqLuma * cos(scPhase) * crossColor * 1.8;
    float falseQ = highFreqLuma * sin(scPhase) * crossColor * 1.8;
    filteredChroma.x += falseI;
    filteredChroma.y += falseQ;

    // 5. Signal Noise Floor
    if (signalNoise > 0.001) {
        vec2 nSeed = TexCoord * res + vec2(time * 73.1, time * 37.9);
        float nLuma = (hash1(nSeed) - 0.5) * signalNoise * 0.5;
        float nI = (hash1(nSeed + vec2(13.7, 42.1)) - 0.5) * signalNoise * 1.2;
        float nQ = (hash1(nSeed + vec2(57.3, 89.4)) - 0.5) * signalNoise * 1.2;
        yRinging += nLuma;
        filteredChroma.x += nI;
        filteredChroma.y += nQ;
    }

    vec3 finalRgb = yiq2rgb(vec3(yRinging, filteredChroma.x, filteredChroma.y));
    FragColor = vec4(clamp(finalRgb, 0.0, 1.0), orig.a);
}
