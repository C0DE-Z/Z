#version 330 core
// @name Object Mask
// @category Masking
// @desc Cut out / isolate detected regions using the ML mask texture
// @param strength Mask Strength 0.0 1.0 1.0
// @param invert Invert Mask 0.0 1.0 0.0
// @param soft Soft Background 0.0 1.0 0.15
// @param feather Edge Softness 0.0 1.0 0.25
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D maskTexture;
uniform float strength;
uniform float invert;
uniform float soft;
uniform float feather;
uniform float time;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    float m = texture(maskTexture, TexCoord).r;

    // Slight analytical feather if mask is hard-edged
    float maskVal = smoothstep(0.0, max(0.001, feather * 0.5 + 0.001), m);
    if (invert > 0.5) {
        maskVal = 1.0 - maskVal;
    }

    float keep = mix(1.0, maskVal, clamp(strength, 0.0, 1.0));
    vec3 bg = color.rgb * soft;
    FragColor = vec4(mix(bg, color.rgb, keep), color.a);
}
