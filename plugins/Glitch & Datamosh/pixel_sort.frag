#version 330 core
// @name Pixel Sort
// @desc Threshold-based horizontal pixel sorting melt
// @param threshold Sort Threshold 0.0 1.0 0.5
// @param sortLength Sort Length 0.0 0.3 0.05
// @param direction Sort Direction 0.0 1.0 0.0 bool
// @param intensity Effect Mix 0.0 1.0 0.7
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float threshold;
uniform float sortLength;
uniform float direction;
uniform float intensity;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));

    vec2 uv = TexCoord;
    if (luma > threshold) {
        float offset = sortLength * (luma - threshold);
        if (direction > 0.5) {
            uv.x = fract(uv.x + offset);
        } else {
            uv.x = fract(uv.x - offset);
        }
    }

    vec4 sorted = texture(videoTexture, uv);
    FragColor = mix(color, sorted, intensity);
}
