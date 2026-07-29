#version 330 core
// @name Legacy CPU XNOR
// @desc CPU-level bitwise XNOR manipulation
// @param xnorValue XNOR Bitmask 0.0 1.0 0.5
// @param intensity XNOR Blend 0.0 1.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
