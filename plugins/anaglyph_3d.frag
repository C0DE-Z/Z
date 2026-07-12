#version 330 core
// @name Anaglyph 3D
// @desc Red/cyan anaglyph with depth offset
// @param separation Eye Separation 0.0 0.05 0.01
// @param balance Red/Cyan Balance 0.0 1.0 0.5
// @param tint Cyan Tint 0.0 1.0 0.7
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float separation;
uniform float balance;
uniform float tint;

void main() {
    vec2 uv = TexCoord;
    float r = texture(videoTexture, uv - vec2(separation, 0.0)).r;
    float g = texture(videoTexture, uv).g * (1.0 - tint);
    float b = texture(videoTexture, uv + vec2(separation, 0.0)).b;

    vec3 anaglyph = vec3(r * balance, g * 0.3, b * (1.0 - balance + 0.5));
    FragColor = vec4(anaglyph, 1.0);
}
