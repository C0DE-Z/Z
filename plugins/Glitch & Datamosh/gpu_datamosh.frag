#version 330 core
// @name GPU Datamosh
// @desc GPU approximation of P-frame smearing via feedback
// @param smear Smear Amount 0.0 1.0 0.7
// @param blockiness Block Artifact 0.0 1.0 0.4
// @param drift Motion Drift 0.0 0.05 0.01
// @param corrupt Corruption Rate 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float time;
uniform float smear;
uniform float blockiness;
uniform float drift;
uniform float corrupt;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    vec2 blockUV = floor(uv * (8.0 + blockiness * 24.0)) / (8.0 + blockiness * 24.0);

    vec4 current = texture(videoTexture, uv);
    vec4 prev = texture(feedbackTexture, blockUV + vec2(drift, 0.0));

    float h = hash(blockUV + floor(time * 2.0));
    if (h < corrupt) {
        prev = texture(feedbackTexture, blockUV + vec2(hash(blockUV) * 0.1 - 0.05, 0.0));
    }

    vec4 color = mix(current, prev, smear);

    if (blockiness > 0.0 && h < blockiness * 0.3) {
        color = texture(feedbackTexture, blockUV);
    }

    FragColor = color;
}
