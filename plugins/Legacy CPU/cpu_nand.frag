#version 330 core
// @name Legacy CPU NAND
// @desc CPU-level bitwise NAND manipulation
// @param nandValue NAND Bitmask 0.0 1.0 0.5
// @param intensity NAND Blend 0.0 1.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
