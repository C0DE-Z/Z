#version 330 core
// @name Beat Flash
// @desc Color burst on audio transients
// @param sensitivity Audio Sensitivity 0.0 3.0 1.0
// @param hueShift Hue Shift 0.0 1.0 0.3
// @param flashWhite White Flash 0.0 1.0 0.5
// @param decay Flash Decay 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float sensitivity;
uniform float hueShift;
uniform float flashWhite;
uniform float decay;
uniform float audioBass;
uniform float audioTreble;

vec3 hueRotate(vec3 color, float angle) {
    float c = cos(angle * 6.28318);
    float s = sin(angle * 6.28318);
    mat3 m = mat3(
        0.299 + 0.701*c + 0.168*s, 0.587 - 0.587*c + 0.330*s, 0.114 - 0.114*c - 0.497*s,
        0.299 - 0.299*c - 0.328*s, 0.587 + 0.413*c + 0.035*s, 0.114 - 0.114*c + 0.292*s,
        0.299 - 0.300*c + 1.250*s, 0.587 - 0.588*c - 1.050*s, 0.114 + 0.886*c - 0.203*s
    );
    return clamp(m * color, 0.0, 1.0);
}

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    vec4 prev = texture(feedbackTexture, TexCoord);

    float energy = (audioBass + audioTreble) * sensitivity;
    energy = clamp(energy, 0.0, 1.0);

    color.rgb = hueRotate(color.rgb, hueShift * energy);
    color.rgb = mix(color.rgb, vec3(1.0), energy * flashWhite);
    color = mix(prev, color, 1.0 - decay * (1.0 - energy));

    FragColor = color;
}
