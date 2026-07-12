#version 330 core
// @name Vertigo
// @desc Radial zoom blur / dolly zoom disorientation
// @param zoomAmount Zoom Pull 0.0 0.3 0.1
// @param samples Blur Samples 2.0 16.0 8.0
// @param centerX Center X 0.0 1.0 0.5
// @param centerY Center Y 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float zoomAmount;
uniform float samples;
uniform float centerX;
uniform float centerY;

void main() {
    vec2 center = vec2(centerX, centerY);
    vec2 dir = TexCoord - center;
    int n = int(samples);
    vec4 color = vec4(0.0);

    for (int i = 0; i < 16; i++) {
        if (i >= n) break;
        float t = float(i) / float(n - 1);
        vec2 uv = center + dir * (1.0 + zoomAmount * (t - 0.5) * 2.0);
        if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {
            color += texture(videoTexture, uv);
        }
    }

    FragColor = color / samples;
}
