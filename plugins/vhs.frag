#version 330 core
// @name VHS Degradation
// @desc Authentic tape wear and chroma bleed
// @param tracking Tracking Offset 0.0 1.0 0.1
// @param noise Signal Noise 0.0 1.0 0.2
// @param wear Tape Wear 0.0 1.0 0.1
// @param chromaDelay Chroma Bleed Delay 0.0 0.05 0.01
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float tracking;
uniform float noise;
uniform float wear;
uniform float chromaDelay;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    if (uv.y < 0.02) {
        float switchOffset = hash(vec2(uv.y, time)) * 0.05;
        uv.x = fract(uv.x + switchOffset);
    }
    float trackLine = fract(time * 0.2);
    float distToTrack = abs(uv.y - trackLine);
    if (distToTrack < 0.05 * tracking) {
        uv.x += sin(uv.y * 50.0 + time * 10.0) * 0.015 * tracking;
    }
    float r = texture(videoTexture, uv - vec2(chromaDelay, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(chromaDelay, 0.0)).b;
    vec4 color = vec4(r, g, b, 1.0);
    if (noise > 0.0 || wear > 0.0) {
        float n = hash(uv + time);
        color.rgb += vec3(n * 0.15 * noise);
        if (hash(vec2(uv.y * 200.0, time)) > 0.99 - (wear * 0.03)) {
            color.rgb = vec3(0.8);
        }
    }
    FragColor = color;
}
