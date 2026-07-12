#version 330 core
// @name Spin Tunnel
// @desc Rotating vortex warp with audio drive
// @param spin Spin Speed 0.0 5.0 1.0
// @param vortex Vortex Strength 0.0 1.0 0.3
// @param radius Vortex Radius 0.0 1.0 0.5
// @param audioDrive Audio Drive 0.0 2.0 1.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float spin;
uniform float vortex;
uniform float radius;
uniform float audioDrive;
uniform float audioMid;

void main() {
    vec2 uv = TexCoord - 0.5;
    float r = length(uv);
    float a = atan(uv.y, uv.x);

    float drive = 1.0 + audioMid * audioDrive;
    float twist = vortex * drive * smoothstep(radius, 0.0, r);
    a += twist * 3.0 + time * spin;

    vec2 warped = vec2(cos(a), sin(a)) * r + 0.5;
    warped = clamp(warped, 0.0, 1.0);
    FragColor = texture(videoTexture, warped);
}
