#version 330 core
// @name Optical Smear
// @desc Motion blur and color smear trail
// @param frameMerge Frame Merge 0.0 1.0 0.25
// @param frameSmear Frame Smear 0.0 1.0 0.1
// @param colorBleed Color Bleed 0.0 1.0 0.25
// @param lumaBias Luma Bias 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float frameMerge;

void main() {
    vec4 c = texture(videoTexture, TexCoord);
    FragColor = c;
}
