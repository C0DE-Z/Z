#version 330 core

// @name Tutorial 1: Color & Time Wobble
// @category Color & FX
// @desc Line-by-line educational shader for color manipulation
// @param wobbleSpeed Wobble Speed 0.1 5.0 1.0
// @param saturation Saturation Boost 0.0 2.0 1.2
// @param active Enable Filter 0.0 1.0 1.0 bool

in vec2 TexCoord;    // Normalized UV coordinates [0.0, 0.0] top-left to [1.0, 1.0] bottom-right
out vec4 FragColor;  // Final RGBA pixel color written to framebuffer

uniform sampler2D videoTexture; // Input video texture sampler
uniform float time;             // Timeline playback time in seconds
uniform vec2 resolution;         // Viewport width and height in pixels

uniform float wobbleSpeed;
uniform float saturation;
uniform float active;

void main() {
    // Read original source video pixel color at current TexCoord
    vec4 baseColor = texture(videoTexture, TexCoord);

    // If active toggle is OFF (0.0), bypass shader and output raw video
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    // Calculate horizontal UV offset using trigonometric sine wave
    vec2 uv = TexCoord;
    float wave = sin(uv.y * 20.0 + time * wobbleSpeed * 4.0) * 0.01;
    uv.x += wave;

    // Sample video frame at distorted UV position
    vec4 color = texture(videoTexture, uv);

    // Calculate perceptual luma (grayscale brightness) using ITU-R BT.601 weights
    float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));

    // Perform linear interpolation (mix) between grayscale luma and full color to adjust saturation
    vec3 saturatedColor = mix(vec3(luma), color.rgb, saturation);

    // Write final RGBA color output
    FragColor = vec4(saturatedColor, color.a);
}
