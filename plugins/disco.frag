#version 330 core
// @name Disco Ball
// @desc Sparkling mirror-tile reflections
// @param tileSize Tile Size 4.0 32.0 12.0
// @param sparkle Sparkle Intensity 0.0 1.0 0.6
// @param spin Rotation Speed 0.0 3.0 0.5
// @param audioGlow Audio Glow 0.0 2.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float tileSize;
uniform float sparkle;
uniform float spin;
uniform float audioGlow;
uniform float audioBass;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord - 0.5;
    float a = atan(uv.y, uv.x) + time * spin;
    float r = length(uv);
    vec2 polar = vec2(a / 6.28318, r);

    vec2 tile = floor(polar * tileSize) / tileSize;
    float h = hash(tile + floor(time * 5.0));

    vec2 sampleUV = TexCoord;
    if (h > 0.7) {
        float offset = (h - 0.7) * sparkle * 0.1;
        sampleUV += vec2(cos(a), sin(a)) * offset;
    }

    vec4 color = texture(videoTexture, clamp(sampleUV, 0.0, 1.0));
    float glow = h * sparkle + audioBass * audioGlow * 0.3;
    color.rgb += vec3(glow * 0.5);

    FragColor = color;
}
