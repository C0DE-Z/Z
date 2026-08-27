#version 330 core
// @name Bad Signal
// @desc TV signal loss, static bars, and rolling noise
// @param staticAmount Static Level 0.0 1.0 0.3
// @param rollSpeed Roll Speed 0.0 2.0 0.3
// @param barCount Noise Bars 0.0 20.0 5.0
// @param dropout Signal Dropout 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float staticAmount;
uniform float rollSpeed;
uniform float barCount;
uniform float dropout;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    uv.y = fract(uv.y + time * rollSpeed * 0.05);

    vec4 color = texture(videoTexture, uv);
    float noise = hash(uv * 500.0 + time);
    color.rgb = mix(color.rgb, vec3(noise), staticAmount);

    float bar = floor(uv.y * barCount);
    if (hash(vec2(bar, floor(time * 3.0))) < dropout * 0.3) {
        color.rgb = vec3(hash(vec2(bar, time)));
    }

    if (hash(vec2(floor(uv.y * 3.0), time)) > 0.98) {
        color.rgb = vec3(0.0);
    }

    FragColor = color;
}
