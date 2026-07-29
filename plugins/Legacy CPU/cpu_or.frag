#version 330 core
// @name Legacy CPU OR
// @desc CPU-level bitwise OR manipulation
// @param orValue OR Bitmask 0.0 1.0 0.5
// @param intensity OR Blend 0.0 1.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
