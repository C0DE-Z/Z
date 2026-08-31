#version 330 core
// @name Datamosh (CPU)
// @desc Real H.264 I-frame removal and P-frame packet manipulation
// @param iDrop Remove I-Frames 0.0 1.0 1.0 bool
// @param pDup P-Frame Repeat Chance 0.0 1.0 0.0
// @param pDupCount P-Frame Repeat Count 1.0 20.0 2.0
// @param pDrop P-Frame Drop Chance 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
