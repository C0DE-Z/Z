#version 330 core
// @name Posterize
// @desc Crush color levels into bands
// @param levels Color Levels 2.0 32.0 6.0
// @param dither Dither Noise 0.0 1.0 0.1
// @param saturation Saturation 0.0 2.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float levels;
uniform float dither;
uniform float saturation;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    float d = (hash(TexCoord * 1000.0) - 0.5) * dither / levels;
    color.rgb = floor(color.rgb * levels + d) / levels;

    float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    color.rgb = mix(vec3(luma), color.rgb, saturation);

    FragColor = color;
}
