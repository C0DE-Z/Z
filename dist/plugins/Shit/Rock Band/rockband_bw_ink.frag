#version 330 core
// @name Rock Band B&W Ink & XOR
// @desc High-contrast B&W ink threshold with bitwise XOR channel inversion from Rock Band music videos
// @param contrast Contrast Threshold 0.5 5.0 2.5
// @param xorInvert XOR Channel Invert 0.0 1.0 0.0 bool
// @param active Enable B&W Ink 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float contrast;
uniform float xorInvert;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    float luma = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    float bw = smoothstep(0.5 - 0.5 / contrast, 0.5 + 0.5 / contrast, luma);
    vec3 col = vec3(bw);
    if (xorInvert > 0.5) {
        uvec3 iCol = uvec3(col * 255.0);
        uvec3 xorMask = uvec3(128u, 64u, 255u);
        iCol = iCol ^ xorMask;
        col = vec3(iCol) / 255.0;
    }
    FragColor = vec4(col, orig.a);
}
