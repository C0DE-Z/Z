#version 330 core
// @name Modern Hardware Bitwise XNOR
// @desc Bitwise NOT-XOR channel equivalence glitch on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 128.0
// @param maskG Green Mask (0-255) 0.0 255.0 64.0
// @param maskB Blue Mask (0-255) 0.0 255.0 32.0
// @param mixRatio Effect Blend 0.0 1.0 0.8
// @param active Enable XNOR Gate 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float maskR;
uniform float maskG;
uniform float maskB;
uniform float mixRatio;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    uvec3 intColor = uvec3(clamp(baseColor.rgb * 255.0, 0.0, 255.0));
    uvec3 xorMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 xnorInt = (~(intColor ^ xorMask)) & 255u;
    vec3 resultRGB = vec3(xnorInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
