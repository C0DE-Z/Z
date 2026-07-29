#version 330 core
// @name Fisheye
// @desc Extreme barrel fisheye lens distortion
// @param strength Fisheye Strength 0.0 1.0 0.5
// @param zoom Fisheye Zoom 0.5 2.0 1.0
// @param vignette Edge Vignette 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float strength;
uniform float zoom;
uniform float vignette;

void main() {
    vec2 uv = TexCoord - 0.5;
    float r = length(uv);
    float theta = atan(uv.y, uv.x);
    float r2 = pow(r, 1.0 + strength) * zoom;
    vec2 fish = vec2(cos(theta), sin(theta)) * r2 + 0.5;

    if (fish.x < 0.0 || fish.x > 1.0 || fish.y < 0.0 || fish.y > 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    vec4 color = texture(videoTexture, fish);
    color.rgb *= 1.0 - vignette * r;
    FragColor = color;
}
