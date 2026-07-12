#version 330 core
// @name Noise Displace
// @desc Organic noise-based UV warping
// @param scale Noise Scale 1.0 30.0 8.0
// @param strength Displace Strength 0.0 0.1 0.02
// @param speed Animation Speed 0.0 5.0 1.0
// @param octaves Noise Detail 1.0 4.0 2.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float scale;
uniform float strength;
uniform float speed;
uniform float octaves;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec2 uv = TexCoord;
    float n = 0.0;
    float amp = 1.0;
    float freq = 1.0;
    int oct = int(octaves);

    for (int i = 0; i < 4; i++) {
        if (i >= oct) break;
        n += noise(uv * scale * freq + time * speed) * amp;
        amp *= 0.5;
        freq *= 2.0;
    }

    uv += vec2(n, noise(uv * scale + time * speed * 0.7)) * strength;
    uv = clamp(uv, 0.0, 1.0);
    FragColor = texture(videoTexture, uv);
}
