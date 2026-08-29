#version 330 core
// @name Neon Bloom Wipe
// @desc Electric neon edge wipe with glow trail
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;
uniform vec2 resolution;

void main() {
    vec2 uv = TexCoord;
    float p = clamp(progress, 0.0, 1.0);

    float wave = sin(uv.y * 18.0 + p * 9.0) * 0.03;
    float edge = p + wave;
    float mask = smoothstep(edge - 0.02, edge + 0.02, uv.x);

    vec4 c1 = texture(videoTexture, uv);
    vec4 c2 = texture(videoTexture2, uv);

    float glow = exp(-abs(uv.x - edge) * 75.0);
    vec3 neon = vec3(0.25, 0.95, 1.0) * glow + vec3(1.0, 0.2, 0.8) * glow * 0.6;

    vec3 col = mix(c1.rgb, c2.rgb, mask);
    col += neon * (0.6 + 0.4 * sin(p * 12.0));

    float vignette = smoothstep(0.95, 0.2, length((uv - 0.5) * vec2(resolution.x / max(1.0, resolution.y), 1.0)));
    col *= 0.9 + 0.1 * vignette;

    FragColor = vec4(col, 1.0);
}
