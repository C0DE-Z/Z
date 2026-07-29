#version 330 core
// @name Wave Rider
// @desc Sine wave horizontal displacement
// @param amplitude Wave Amplitude 0.0 0.1 0.02
// @param frequency Wave Frequency 1.0 50.0 10.0
// @param speed Wave Speed 0.0 10.0 2.0
// @param layers Wave Layers 1.0 5.0 2.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float amplitude;
uniform float frequency;
uniform float speed;
uniform float layers;

void main() {
    vec2 uv = TexCoord;
    float wave = 0.0;
    for (float i = 0.0; i < 5.0; i++) {
        if (i >= layers) break;
        wave += sin(uv.y * frequency * (1.0 + i * 0.5) + time * speed * (1.0 + i * 0.3)) * amplitude / (1.0 + i);
    }
    uv.x = fract(uv.x + wave);
    FragColor = texture(videoTexture, uv);
}
