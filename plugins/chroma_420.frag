#version 330 core
// @name Chroma 4:2:0 Glitch
// @desc YUV subsampling bleed — color smears across luma blocks
// @param bleed Chroma Bleed 0.0 1.0 0.5
// @param blockW Chroma Block Width 2.0 32.0 8.0
// @param blockH Chroma Block Height 2.0 32.0 8.0
// @param shift Color Shift 0.0 0.1 0.02
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float bleed;
uniform float blockW;
uniform float blockH;
uniform float shift;

void main() {
    vec2 uv = TexCoord;
    vec2 chromaUV = floor(uv * vec2(blockW, blockH)) / vec2(blockW, blockH);
    chromaUV += vec2(0.5 / blockW, 0.5 / blockH);

    float luma = dot(texture(videoTexture, uv).rgb, vec3(0.299, 0.587, 0.114));
    vec3 chroma = texture(videoTexture, chromaUV).rgb;

    vec3 yuv = vec3(luma, chroma.r - luma, chroma.b - luma);
    vec3 result = vec3(
        yuv.x + yuv.y * bleed,
        yuv.x,
        yuv.x + yuv.z * bleed
    );

    float r = texture(videoTexture, uv - vec2(shift * bleed, 0.0)).r;
    float b = texture(videoTexture, uv + vec2(shift * bleed, 0.0)).b;
    result.r = mix(result.r, r, bleed);
    result.b = mix(result.b, b, bleed);

    FragColor = vec4(clamp(result, 0.0, 1.0), 1.0);
}
