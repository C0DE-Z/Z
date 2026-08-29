#version 330 core
// @name Pulse Zoom
// @desc Audio-reactive zoom pulse (Rock Band energy bursts)
// @param amount Zoom Amount 0.0 0.5 0.15
// @param speed Pulse Speed 0.0 10.0 3.0
// @param bassWeight Bass Influence 0.0 3.0 1.5
// @param trebleWeight Treble Influence 0.0 3.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float amount;
uniform float speed;
uniform float bassWeight;
uniform float trebleWeight;
uniform float audioBass;
uniform float audioTreble;

void main() {
    float pulse = sin(time * speed) * 0.5 + 0.5;
    pulse += audioBass * bassWeight;
    pulse += audioTreble * trebleWeight * 0.5;
    pulse = clamp(pulse, 0.0, 1.0);

    vec2 uv = TexCoord - 0.5;
    uv *= 1.0 - amount * pulse;
    uv += 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    FragColor = texture(videoTexture, uv);
}
