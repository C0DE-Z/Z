#version 330 core
// @name Buffer Underrun
// @desc Frozen frame stutter with partial buffer fills
// @param stutter Stutter Rate 0.0 1.0 0.3
// @param freeze Freeze Amount 0.0 1.0 0.5
// @param partialFill Partial Buffer 0.0 1.0 0.4
// @param tear Horizontal Tear 0.0 0.2 0.05
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float time;
uniform float stutter;
uniform float freeze;
uniform float partialFill;
uniform float tear;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 current = texture(videoTexture, TexCoord);
    vec4 frozen = texture(feedbackTexture, TexCoord);

    float frame = floor(time * 24.0);
    float h = hash(vec2(frame, 0.0));

    vec4 color;
    if (h < stutter) {
        color = frozen;
    } else {
        color = mix(current, frozen, freeze * 0.5);
    }

    float fillLine = hash(vec2(frame, 1.0));
    if (TexCoord.y > fillLine * partialFill + (1.0 - partialFill)) {
        color = frozen;
    }

    vec2 uv = TexCoord;
    if (hash(vec2(floor(uv.y * 100.0), frame)) < 0.1) {
        uv.x += tear;
        color = mix(color, texture(feedbackTexture, fract(uv)), 0.5);
    }

    FragColor = color;
}
