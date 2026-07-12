#version 330 core
// @name XOR Color Glitch
// @desc Bitwise XOR corruption of color channels
// @param xorR XOR Red Mask 0.0 255.0 128.0
// @param xorG XOR Green Mask 0.0 255.0 64.0
// @param xorB XOR Blue Mask 0.0 255.0 32.0
// @param temporal Temporal Blend 0.0 1.0 0.5
// @param spatial Spatial Offset 0.0 0.05 0.0
// @param chromaShift Chroma Split 0.0 0.05 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float xorR;
uniform float xorG;
uniform float xorB;
uniform float temporal;
uniform float spatial;
uniform float chromaShift;

void main() {
    vec2 uv = TexCoord;
    
    vec4 color;
    if (chromaShift > 0.0) {
        color.r = texture(videoTexture, uv - vec2(chromaShift, 0.0)).r;
        color.g = texture(videoTexture, uv).g;
        color.b = texture(videoTexture, uv + vec2(chromaShift, 0.0)).b;
        color.a = texture(videoTexture, uv).a;
    } else {
        color = texture(videoTexture, uv);
    }
    
    uvec3 iColor = uvec3(color.rgb * 255.0);
    
    if (spatial > 0.0) {
        vec4 offsetColor = texture(videoTexture, uv + vec2(spatial, 0.0));
        uvec3 iOffset = uvec3(offsetColor.rgb * 255.0);
        iColor = iColor ^ iOffset;
    }
    
    if (temporal > 0.0) {
        vec4 prevColor = texture(feedbackTexture, uv);
        uvec3 iPrev = uvec3(prevColor.rgb * 255.0);
        uvec3 xorTemp = iColor ^ iPrev;
        iColor = uvec3(mix(vec3(iColor), vec3(xorTemp), temporal));
    }
    
    uvec3 mask = uvec3(uint(xorR), uint(xorG), uint(xorB));
    iColor = iColor ^ mask;
    
    FragColor = vec4(vec3(iColor) / 255.0, color.a);
}
