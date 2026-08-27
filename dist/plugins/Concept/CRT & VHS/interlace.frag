#version 330 core
// @name Interlace Comb
// @desc CRT interlacing with field combing artifacts
// @param strength Comb Strength 0.0 1.0 0.5
// @param lineOffset Field Offset 0.0 0.01 0.002
// @param flicker Field Flicker 0.0 1.0 0.2
// @param darken Odd Field Darken 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float strength;
uniform float lineOffset;
uniform float flicker;
uniform float darken;

void main() {
    float field = mod(floor(TexCoord.y * 480.0), 2.0);
    float flick = sin(time * 60.0) * flicker;

    vec2 uv = TexCoord;
    if (field < 1.0) {
        uv.y += lineOffset;
    }

    vec4 color = texture(videoTexture, uv);
    vec4 altField = texture(videoTexture, TexCoord + vec2(0.0, lineOffset * 2.0));
    color = mix(color, altField, strength * 0.5);

    if (field < 1.0) {
        color.rgb *= 1.0 - darken;
    }
    color.rgb *= 1.0 + flick * 0.1;

    FragColor = color;
}
