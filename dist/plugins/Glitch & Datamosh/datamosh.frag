#version 330 core
// @name Datamosh (CPU)
// @desc Temporal P-frame and I-frame corruption
// @param iDrop I-Frame Drop Toggle 0.0 1.0 0.0 bool
// @param pDup P-Frame Duplicate Chance 0.0 1.0 0.35
// @param pDupCount Duplication Count 1.0 60.0 4.0
// @param pDrop P-Frame Drop Toggle 0.0 1.0 0.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
