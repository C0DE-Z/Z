#version 330 core
// @name Spectrum Bars
// @desc Audio-reactive frequency bars overlay (Rock Band style)
// @param barCount Bar Count 8.0 64.0 32.0
// @param height Bar Height 0.0 0.5 0.2
// @param opacity Bar Opacity 0.0 1.0 0.6
// @param position Bar Position 0.0 1.0 0.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float barCount;
uniform float height;
uniform float opacity;
uniform float position;
uniform float audioBass;
uniform float audioMid;
uniform float audioTreble;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    float bar = floor(TexCoord.x * barCount);
    float t = bar / barCount;

    float level;
    if (t < 0.33) level = audioBass;
    else if (t < 0.66) level = audioMid;
    else level = audioTreble;

    level *= (0.5 + 0.5 * sin(bar * 0.7 + t * 10.0));
    float barTop = position > 0.5 ? (1.0 - height * level) : height * level;
    float inBar = position > 0.5 ? step(barTop, TexCoord.y) : step(TexCoord.y, barTop);

    vec3 barColor = vec3(0.2 + t, 0.8 - t * 0.5, 0.3 + t * 0.7);
    color.rgb = mix(color.rgb, barColor, inBar * opacity * level);

    FragColor = color;
}
