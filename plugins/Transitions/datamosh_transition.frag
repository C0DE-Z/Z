#version 330 core
// @name Datamosh Transition
// @desc Pixel sorting and offset datamosh effect
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform vec2 resolution;
uniform float progress;

float luma(vec3 color) { return dot(color, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 c2 = texture(videoTexture2, TexCoord);
    float stepX = 1.0 / resolution.x;
    float stepY = 1.0 / resolution.y;
    float l = luma(texture(videoTexture2, TexCoord - vec2(stepX, 0.0)).rgb);
    float r = luma(texture(videoTexture2, TexCoord + vec2(stepX, 0.0)).rgb);
    float u = luma(texture(videoTexture2, TexCoord - vec2(0.0, stepY)).rgb);
    float d = luma(texture(videoTexture2, TexCoord + vec2(0.0, stepY)).rgb);
    vec2 gradient = vec2(r - l, d - u);
    vec2 displacedUV = TexCoord + gradient * progress * 0.5;
    float blocks = 64.0 - progress * 48.0; 
    vec2 blockUV = floor(displacedUV * blocks) / blocks;
    vec4 moshedC1 = texture(videoTexture, blockUV);
    float blend = smoothstep(0.8, 1.0, progress);
    FragColor = mix(moshedC1, c2, blend);
}
