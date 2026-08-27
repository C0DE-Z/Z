#version 330 core
// @name Vortex Bleed
// @desc Spiral pull transition with chroma bleed
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;
uniform vec2 resolution;

void main() {
    vec2 uv = TexCoord;
    float p = clamp(progress, 0.0, 1.0);

    vec2 center = vec2(0.5);
    vec2 d = uv - center;
    float r = length(d);
    float ang = atan(d.y, d.x);

    float spin = (1.0 - p) * 2.8;
    float warp = smoothstep(0.0, 0.8, 1.0 - r) * spin;

    vec2 uv1 = center + vec2(cos(ang + warp), sin(ang + warp)) * r;
    vec2 uv2 = center + vec2(cos(ang - (2.8 - spin)), sin(ang - (2.8 - spin))) * r;

    vec4 c1 = texture(videoTexture, uv1);
    vec4 c2 = texture(videoTexture2, uv2);

    float radial = smoothstep(0.1, 0.85, p + (0.5 - r) * 0.45);

    float bleed = (1.0 - radial) * 0.01 + p * 0.01;
    vec3 b1 = vec3(
        texture(videoTexture, uv1 + vec2(bleed, 0.0)).r,
        texture(videoTexture, uv1).g,
        texture(videoTexture, uv1 - vec2(bleed, 0.0)).b
    );

    vec3 col = mix(b1, c2.rgb, radial);
    FragColor = vec4(col, 1.0);
}
