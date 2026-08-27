#version 330 core
// @name Quantum Block Shift
// @desc Macroblock teleport and probabilistic reveal transition
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    float p = clamp(progress, 0.0, 1.0);
    vec2 uv = TexCoord;

    float blocks = mix(22.0, 92.0, p);
    vec2 gid = floor(uv * blocks);
    vec2 guv = (gid + 0.5) / blocks;

    float rnd = hash(gid * 1.37 + floor(p * 90.0));
    float reveal = smoothstep(0.0, 1.0, p * 1.2 - rnd * 0.7);

    vec2 jump = vec2(hash(gid + 13.0) - 0.5, hash(gid + 29.0) - 0.5) * (1.0 - p) * 0.18;
    vec4 c1 = texture(videoTexture, guv + jump);
    vec4 c2 = texture(videoTexture2, guv - jump * 0.5);

    vec3 col = mix(c1.rgb, c2.rgb, reveal);
    FragColor = vec4(col, 1.0);
}
