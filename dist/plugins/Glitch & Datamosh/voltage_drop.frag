#version 330 core
// @name Voltage Drop
// @desc Circuit bent power rail brownout and line collapse
// @param dropout Dropout Rate 0.0 1.0 0.2
// @param dimming Brightness Dim 0.0 1.0 0.4
// @param lineCollapse Line Collapse 0.0 1.0 0.3
// @param colorCast Voltage Color Cast 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float dropout;
uniform float dimming;
uniform float lineCollapse;
uniform float colorCast;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float line = floor(uv.y * 240.0);
    float h = hash(vec2(line, floor(time * 4.0)));

    if (h < lineCollapse * 0.2) {
        uv.y = floor(uv.y * 240.0) / 240.0;
    }

    vec4 color = texture(videoTexture, uv);

    if (hash(vec2(floor(time * 2.0), 0.0)) < dropout) {
        color.rgb *= 0.1;
    } else {
        color.rgb *= 1.0 - dimming * (0.5 + 0.5 * sin(time * 3.0));
    }

    color.r += colorCast * 0.1;
    color.b -= colorCast * 0.05;

    FragColor = clamp(color, 0.0, 1.0);
}
