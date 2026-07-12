#version 330 core
// @name Temporal Echo
// @desc Blends previous frames to create a ghosting trail
// @param feedback Blend Amount 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float feedback;

void main() {
    vec4 current = texture(videoTexture, TexCoord);
    vec4 prev = texture(feedbackTexture, TexCoord);
    
    FragColor = mix(current, prev, feedback);
}
