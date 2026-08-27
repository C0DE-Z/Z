#version 330 core
// @name Milkdrop Visualizer
// @desc Audio reactive fractal warping
// @param bass Bass Influence 0.0 2.0 1.0
// @param mid Mid Influence 0.0 2.0 1.0
// @param treble Treble Influence 0.0 2.0 1.0
// @param zoom Fractal Zoom 0.5 2.0 1.05
// @param rotation Fractal Rotation -1.0 1.0 0.1
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float bass;
uniform float mid;
uniform float treble;
uniform float zoom;
uniform float rotation;
uniform float audioBass;
uniform float audioMid;
uniform float audioTreble;

void main() {
    vec2 uv = TexCoord - 0.5;
    float rBass = audioBass * bass;
    float rMid = audioMid * mid;
    float rTreble = audioTreble * treble;
    float angle = rotation * rBass * 3.14159;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    uv = rot * uv;
    float z = zoom - (rTreble * 0.1);
    float radius = length(uv);
    uv *= z - (sin(radius * 10.0 - time * 5.0) * rMid * 0.05);
    uv += 0.5;
    vec4 color = texture(videoTexture, uv);
    FragColor = color;
}
