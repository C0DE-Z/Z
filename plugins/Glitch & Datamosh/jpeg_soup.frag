#version 330 core
// @name JPEG Soup
// @desc DCT block artifacts and mosquito noise around edges
// @param quality JPEG Quality 0.0 1.0 0.2
// @param blockiness Block Size 4.0 32.0 8.0
// @param mosquito Mosquito Noise 0.0 1.0 0.4
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float quality;
uniform float blockiness;
uniform float mosquito;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    vec2 blockUV = floor(uv * blockiness) / blockiness;
    vec2 localUV = fract(uv * blockiness);

    vec4 center = texture(videoTexture, blockUV + vec2(0.5 / blockiness));
    vec4 color = texture(videoTexture, uv);

    float blend = (1.0 - quality) * 0.8;
    color = mix(color, center, blend);

    float edge = length(vec2(dFdx(color.r), dFdy(color.r)));
    if (edge > 0.05 && mosquito > 0.0) {
        float noise = hash(floor(uv * blockiness * 4.0) + time) * mosquito * 0.3;
        color.rgb += vec3(noise) - mosquito * 0.15;
    }

    if (quality < 0.3) {
        color.rgb = floor(color.rgb * (4.0 + quality * 8.0)) / (4.0 + quality * 8.0);
    }

    FragColor = clamp(color, 0.0, 1.0);
}
