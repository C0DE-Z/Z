#version 330 core
// @name Digital Noise Glitch
// @desc Custom color displacement and horizontal line slicing
// @param slice_density Slice Density 0.0 50.0 10.0
// @param slice_speed Glitch Speed 0.0 10.0 2.0
// @param glitch_active Is Glitch Active 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D videoTexture;
uniform float time;
uniform float slice_density;
uniform float slice_speed;
uniform float glitch_active;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }
void main() {
    vec2 uv = TexCoord;
    if (glitch_active > 0.5) {
        float sliceVal = sin(uv.y * slice_density + time * slice_speed);
        if (sliceVal > 0.8) {
            uv.x += hash(vec2(uv.y, time)) * 0.1 - 0.05;
        }
    }
    vec4 color = texture(videoTexture, uv);
    if (glitch_active > 0.5 && hash(uv + time) > 0.95) {
        color.r = texture(videoTexture, uv + vec2(0.01, 0.0)).r;
        color.b = texture(videoTexture, uv - vec2(0.01, 0.0)).b;
    }
    FragColor = color;
}
