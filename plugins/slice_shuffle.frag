#version 330 core
// @name Slice Shuffle
// @desc Random horizontal slice reordering
// @param slices Slice Count 4.0 64.0 16.0
// @param shuffle Shuffle Amount 0.0 1.0 0.5
// @param speed Shuffle Speed 0.0 10.0 2.0
// @param offset Max Offset 0.0 0.5 0.1
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float slices;
uniform float shuffle;
uniform float speed;
uniform float offset;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    float slice = floor(uv.y * slices);
    float h = hash(vec2(slice, floor(time * speed)));

    if (h < shuffle) {
        float newSlice = floor(h * slices * 10.0) / slices;
        uv.y = newSlice + fract(uv.y * slices) / slices;
        uv.x += (hash(vec2(slice, time)) - 0.5) * offset;
    }

    uv = fract(uv);
    FragColor = texture(videoTexture, uv);
}
