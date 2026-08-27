#version 330 core
// @name Security Cam
// @desc Low-res surveillance camera degradation
// @param resolution Cam Resolution 0.01 0.5 0.1
// @param noise Cam Noise 0.0 1.0 0.3
// @param timestamp Show Timestamp Burn 0.0 1.0 0.0 bool
// @param greenTint Night Vision Tint 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float resolution;
uniform float noise;
uniform float timestamp;
uniform float greenTint;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = floor(TexCoord / resolution) * resolution;
    vec4 color = texture(videoTexture, uv + resolution * 0.5);

    color.rgb += vec3(hash(uv + time) * noise * 0.3 - noise * 0.15);
    color.rgb = mix(color.rgb, color.ggg, greenTint);

    if (timestamp > 0.5 && TexCoord.y > 0.92 && TexCoord.x < 0.4) {
        float blink = step(0.5, fract(time * 0.5));
        color.rgb = mix(color.rgb, vec3(1.0, 0.2, 0.2), blink * 0.8);
    }

    FragColor = clamp(color, 0.0, 1.0);
}
