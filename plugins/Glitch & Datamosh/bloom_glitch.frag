#version 330 core
// @name Bloom Glitch
// @desc Threshold bloom with glitchy bright bleed
// @param threshold Bloom Threshold 0.0 1.0 0.6
// @param intensity Bloom Intensity 0.0 2.0 1.0
// @param glitch Bloom Glitch 0.0 1.0 0.3
// @param spread Bloom Spread 0.0 0.02 0.005
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float threshold;
uniform float intensity;
uniform float glitch;
uniform float spread;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 px = vec2(spread) / resolution;
    vec4 color = texture(videoTexture, TexCoord);

    vec4 bloom = vec4(0.0);
    bloom += texture(videoTexture, TexCoord + vec2(px.x, 0.0));
    bloom += texture(videoTexture, TexCoord - vec2(px.x, 0.0));
    bloom += texture(videoTexture, TexCoord + vec2(0.0, px.y));
    bloom += texture(videoTexture, TexCoord - vec2(0.0, px.y));
    bloom *= 0.25;

    vec3 bright = max(bloom.rgb - vec3(threshold), vec3(0.0)) * intensity;

    if (hash(TexCoord + time) < glitch * 0.1) {
        bright = bright.grb;
        bright *= 2.0;
    }

    FragColor = vec4(clamp(color.rgb + bright, 0.0, 1.0), color.a);
}
