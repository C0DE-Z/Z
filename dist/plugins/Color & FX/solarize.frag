#version 330 core
// @name Solarize
// @desc Sabattier solarization with threshold inversion
// @param threshold Solarize Threshold 0.0 1.0 0.5
// @param intensity Effect Strength 0.0 1.0 0.8
// @param channels Per-Channel 0.0 1.0 0.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float threshold;
uniform float intensity;
uniform float channels;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    vec3 solar;

    if (channels > 0.5) {
        solar.r = color.r > threshold ? 1.0 - color.r : color.r;
        solar.g = color.g > threshold ? 1.0 - color.g : color.g;
        solar.b = color.b > threshold ? 1.0 - color.b : color.b;
    } else {
        float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        float s = luma > threshold ? 1.0 - luma : luma;
        solar = color.rgb * (s / max(luma, 0.001));
    }

    FragColor = vec4(mix(color.rgb, solar, intensity), color.a);
}
