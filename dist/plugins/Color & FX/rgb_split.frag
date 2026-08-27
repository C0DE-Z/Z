#version 330 core
// @name RGB Split
// @desc Aggressive chromatic channel separation
// @param offsetR Red Offset X 0.0 0.1 0.02
// @param offsetG Green Offset X -0.1 0.1 0.0
// @param offsetB Blue Offset X 0.0 0.1 0.02
// @param vertical Vertical Split 0.0 0.05 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float offsetR;
uniform float offsetG;
uniform float offsetB;
uniform float vertical;

void main() {
    vec2 uv = TexCoord;
    float r = texture(videoTexture, uv + vec2(offsetR, vertical)).r;
    float g = texture(videoTexture, uv + vec2(offsetG, -vertical)).g;
    float b = texture(videoTexture, uv + vec2(offsetB, vertical * 0.5)).b;
    FragColor = vec4(r, g, b, 1.0);
}
