#version 330 core
// @name Feedback XOR
// @desc XOR blend with feedback buffer for cascading corruption
// @param xorAmount XOR Mix 0.0 1.0 0.5
// @param mask XOR Mask 0.0 255.0 85.0
// @param drift Feedback Drift 0.0 0.02 0.005
// @param decay Corruption Decay 0.0 1.0 0.1
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float xorAmount;
uniform float mask;
uniform float drift;
uniform float decay;

void main() {
    vec4 current = texture(videoTexture, TexCoord);
    vec4 prev = texture(feedbackTexture, TexCoord - vec2(drift, 0.0));

    prev.rgb *= 1.0 - decay * 0.05;

    uvec3 a = uvec3(current.rgb * 255.0);
    uvec3 b = uvec3(prev.rgb * 255.0);
    uvec3 xored = a ^ b ^ uvec3(uint(mask));

    vec3 result = mix(current.rgb, vec3(xored) / 255.0, xorAmount);
    FragColor = vec4(result, current.a);
}
