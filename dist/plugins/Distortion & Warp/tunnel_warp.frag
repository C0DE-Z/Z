#version 330 core
// @name Tunnel Warp
// @desc Zooming tunnel distortion (classic music visualizer)
// @param speed Tunnel Speed 0.0 5.0 1.0
// @param twist Twist Amount 0.0 3.0 0.5
// @param depth Tunnel Depth 0.0 1.0 0.4
// @param audio React To Audio 0.0 2.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float speed;
uniform float twist;
uniform float depth;
uniform float audio;
uniform float audioBass;

void main() {
    vec2 uv = TexCoord - 0.5;
    float r = length(uv);
    float a = atan(uv.y, uv.x);

    float bassBoost = 1.0 + audioBass * audio * 0.5;
    a += twist * r * 10.0 + time * speed * 0.5;
    r = pow(r, 1.0 + depth * bassBoost);

    float tunnel = sin(r * 20.0 - time * speed * 3.0) * depth * 0.02;
    vec2 tUV = vec2(cos(a), sin(a)) * (r + tunnel) + 0.5;
    tUV = clamp(tUV, 0.0, 1.0);

    FragColor = texture(videoTexture, tUV);
}
