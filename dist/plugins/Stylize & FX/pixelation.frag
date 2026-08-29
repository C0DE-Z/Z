#version 330 core
// @name Pixelation
// @desc Pixelates the video screen to simulate low-res retro rendering
// @param pixelSize Pixel Size 1.0 100.0 16.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float pixelSize;

void main() {
    float size = max(1.0, pixelSize);
    vec2 sizeUV = size / resolution;
    vec2 uv = floor(TexCoord / sizeUV) * sizeUV + sizeUV * 0.5;
    FragColor = texture(videoTexture, uv);
}
