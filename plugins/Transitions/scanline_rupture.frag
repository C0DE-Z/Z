#version 330 core
// @name Scanline Rupture
// @desc CRT-style scanline tear and channel offset transition
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;
uniform vec2 resolution;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float p = clamp(progress, 0.0, 1.0);

    float tearBand = smoothstep(0.18, 0.22, abs(uv.y - (0.15 + 0.7 * p)));
    float scan = sin((uv.y * resolution.y) * 0.45) * 0.003;
    float jitter = (hash(vec2(floor(uv.y * 160.0), floor(p * 120.0))) - 0.5) * 0.03 * sin(p * 3.14159);

    vec2 uv1 = uv;
    vec2 uv2 = uv;
    uv1.x -= p + jitter + scan * (1.0 - tearBand);
    uv2.x += (1.0 - p) - jitter - scan * tearBand;

    vec4 c1r = texture(videoTexture, uv1 + vec2(0.004, 0.0));
    vec4 c1g = texture(videoTexture, uv1);
    vec4 c1b = texture(videoTexture, uv1 - vec2(0.004, 0.0));
    vec4 c1 = vec4(c1r.r, c1g.g, c1b.b, 1.0);

    vec4 c2 = texture(videoTexture2, uv2);
    float blend = smoothstep(0.2, 0.85, p);

    vec3 col = mix(c1.rgb, c2.rgb, blend);
    col *= 0.97 + 0.03 * sin((uv.y * resolution.y) * 0.35);
    FragColor = vec4(col, 1.0);
}
