#version 330 core
// @name Glitch Slide Transition
// @desc Horizontal slice distortion transition
// @param progress Progress 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv1 = TexCoord;
    vec2 uv2 = TexCoord;
    float offset = hash(vec2(floor(TexCoord.y * 30.0), progress)) * 0.2 * sin(progress * 3.14159);
    uv1.x -= progress + offset;
    uv2.x += (1.0 - progress) + offset;
    vec4 c1 = (uv1.x >= 0.0 && uv1.x <= 1.0) ? texture(videoTexture, uv1) : vec4(0.0);
    vec4 c2 = (uv2.x >= 0.0 && uv2.x <= 1.0) ? texture(videoTexture2, uv2) : vec4(0.0);
    FragColor = mix(c1, c2, smoothstep(0.4, 0.6, progress));
}
