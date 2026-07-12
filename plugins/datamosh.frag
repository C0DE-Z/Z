#version 330 core
// @name Datamosh (CPU)
// @desc FFmpeg-level P-frame and I-frame corruption
// @param iDrop I-Frame Drop 0.0 1.0 0.0
// @param pDup P-Frame Duplicate 0.0 1.0 0.0
// @param pDupCount Duplication Count 1.0 60.0 4.0
// @param pDrop P-Frame Drop 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    // This is a CPU effect handled by VideoEngine via FFmpeg packets.
    // The shader just passes the video frame through.
    FragColor = texture(videoTexture, TexCoord);
}
