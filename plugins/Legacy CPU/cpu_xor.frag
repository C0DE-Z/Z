#version 330 core
// @name Legacy CPU XOR
// @desc CPU-level bitwise XOR manipulation
// @param xorValue XOR Bitmask 0.0 1.0 0.5
// @param intensity XOR Blend 0.0 1.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
