#version 330 core
// @name Block Smear
// @desc Motion-vector style block trailing smear
// @param intensity Smear Intensity 0.0 1.0 0.5
// @param direction Smear Direction 0.0 6.28 0.0
// @param blocks Block Count 4.0 32.0 12.0
// @param decay Trail Decay 0.0 1.0 0.6
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float intensity;
uniform float direction;
uniform float blocks;
uniform float decay;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    vec2 blockId = floor(uv * blocks);
    vec2 blockUV = floor(uv * blocks) / blocks;

    float h = hash(blockId);
    vec2 smearDir = vec2(cos(direction + h * 6.28), sin(direction + h * 6.28));
    smearDir *= intensity * 0.15;

    vec4 current = texture(videoTexture, uv);
    vec4 prev = texture(feedbackTexture, uv - smearDir);
    vec4 smeared = texture(videoTexture, uv - smearDir * 2.0);

    vec4 color = mix(current, smeared, intensity * 0.5);
    color = mix(color, prev, decay * intensity * 0.4);

    if (hash(blockId + 0.5) < intensity * 0.2) {
        color = texture(videoTexture, blockUV + smearDir);
    }

    FragColor = color;
}
