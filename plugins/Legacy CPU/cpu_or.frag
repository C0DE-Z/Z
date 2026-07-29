#version 330 core
// @name Modern Hardware Bitwise OR
// @desc Bitwise OR color channel saturation on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 64.0
// @param maskG Green Mask (0-255) 0.0 255.0 0.0
// @param maskB Blue Mask (0-255) 0.0 255.0 128.0
// @param mixRatio Effect Blend 0.0 1.0 1.0
// @param active Enable OR Gate 0.0 1.0 1.0 bool
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
    uvec3 orMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 resultInt = intColor | orMask;
    vec3 resultRGB = vec3(resultInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
