#version 330 core
// @name Bit Rotate
// @desc Bitwise rotation corruption on color channels
// @param rotateR Red Bit Rotate 0.0 7.0 3.0
// @param rotateG Green Bit Rotate 0.0 7.0 2.0
// @param rotateB Blue Bit Rotate 0.0 7.0 1.0
// @param mixAmount Effect Mix 0.0 1.0 0.8
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float rotateR;
uniform float rotateG;
uniform float rotateB;
uniform float mixAmount;

uint rotl8(uint x, int n) {
    n = n % 8;
    return (x << n) | (x >> (8 - n));
}

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    uvec3 bits = uvec3(color.rgb * 255.0);

    uvec3 rotated = uvec3(
        rotl8(bits.r, int(rotateR)),
        rotl8(bits.g, int(rotateG)),
        rotl8(bits.b, int(rotateB))
    );

    vec3 result = mix(color.rgb, vec3(rotated) / 255.0, mixAmount);
    FragColor = vec4(result, color.a);
}
