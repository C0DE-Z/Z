#version 330 core
// @name Cross Dissolve
// @desc Classic cross dissolve transition
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;

void main() {
    vec4 c1 = texture(videoTexture, TexCoord);
    vec4 c2 = texture(videoTexture2, TexCoord);
    FragColor = mix(c1, c2, progress);
}
