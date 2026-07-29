#version 330 core
// @name UV Scramble
// @desc Random UV tile scrambling (circuit bent framebuffer)
// @param grid Grid Size 2.0 16.0 4.0
// @param scramble Scramble Amount 0.0 1.0 0.5
// @param animate Animate Scramble 0.0 5.0 1.0
// @param hold Tile Hold 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float grid;
uniform float scramble;
uniform float animate;
uniform float hold;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 cell = floor(TexCoord * grid);
    vec2 local = fract(TexCoord * grid);

    float t = floor(time * animate * (1.0 - hold * 0.9));
    float h = hash(cell + t);

    if (h < scramble) {
        vec2 newCell = floor(vec2(hash(cell + 1.0 + t), hash(cell + 2.0 + t)) * grid);
        vec2 uv = (newCell + local) / grid;
        FragColor = texture(videoTexture, clamp(uv, 0.0, 1.0));
    } else {
        FragColor = texture(videoTexture, TexCoord);
    }
}
