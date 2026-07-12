#version 330 core
// @name Analog Bleed
// @desc Horizontal phosphor smear and brightness bleed
// @param bleed Bleed Distance 0.0 0.05 0.01
// @param intensity Bleed Intensity 0.0 1.0 0.5
// @param warmth Warm Bleed Tint 0.0 1.0 0.3
// @param scanGlow Scanline Glow 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float bleed;
uniform float intensity;
uniform float warmth;
uniform float scanGlow;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    vec4 left = texture(videoTexture, TexCoord - vec2(bleed, 0.0));
    vec4 right = texture(videoTexture, TexCoord + vec2(bleed, 0.0));

    vec3 bled = (left.rgb + color.rgb + right.rgb) / 3.0;
    bled.r += warmth * 0.1;
    color.rgb = mix(color.rgb, bled, intensity);

    float scan = sin(TexCoord.y * 400.0) * 0.5 + 0.5;
    color.rgb += vec3(scan * scanGlow * 0.1);

    FragColor = color;
}
