#version 330 core
// @name Feedback Trail
// @desc Long ghosting trails with color decay
// @param trailLength Trail Length 0.0 0.98 0.85
// @param colorDecay Color Fade 0.0 1.0 0.3
// @param drift Trail Drift X 0.0 0.02 0.005
// @param brighten Trail Brighten 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float trailLength;
uniform float colorDecay;
uniform float drift;
uniform float brighten;

void main() {
    vec4 current = texture(videoTexture, TexCoord);
    vec4 prev = texture(feedbackTexture, TexCoord - vec2(drift, 0.0));

    prev.rgb *= 1.0 - colorDecay * 0.1;
    prev.rgb += brighten * 0.05;

    FragColor = mix(current, prev, trailLength);
}
