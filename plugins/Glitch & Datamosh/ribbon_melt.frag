#version 330 core
// @name Ribbon Melt
// @desc Vertical ribbon melting and liquify
// @param melt Melt Strength 0.0 0.2 0.05
// @param ribbons Ribbon Count 2.0 20.0 8.0
// @param speed Melt Speed 0.0 5.0 1.0
// @param wobble Wobble 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float melt;
uniform float ribbons;
uniform float speed;
uniform float wobble;

void main() {
    vec2 uv = TexCoord;
    float ribbon = floor(uv.x * ribbons);
    float wave = sin(ribbon * 1.7 + time * speed) * melt;
    wave += sin(uv.y * 20.0 + time * speed * 2.0) * melt * wobble;
    uv.y += wave;
    uv.x += sin(uv.y * 10.0 + time) * melt * wobble * 0.5;

    uv = clamp(uv, 0.0, 1.0);
    FragColor = texture(videoTexture, uv);
}
