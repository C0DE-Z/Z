#version 330 core
// @name Legacy CPU AND
// @desc CPU-level bitwise AND manipulation
// @param andValue AND Bitmask 0.0 1.0 1.0
// @param intensity AND Blend 0.0 1.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
