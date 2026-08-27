#version 330 core
// @name Rock Band 2 4x Multiplier Streak
// @desc Signature 4x score streak gold/purple aura border flare from Rock Band 2
// @param streakGlow Multiplier Glow 0.0 3.0 1.5
// @param pulseBass Bass Groove Pulse 0.0 2.0 1.0
// @param active Enable 4x Streak 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float streakGlow;
uniform float pulseBass;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float edgeDist = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float border = smoothstep(0.15, 0.0, edgeDist);
    float pulse = sin(time * 6.0) * 0.2 + 0.8;
    vec3 purpleGold = mix(vec3(0.7, 0.1, 0.9), vec3(1.0, 0.85, 0.2), sin(time * 3.0) * 0.5 + 0.5);
    vec3 glow = purpleGold * border * streakGlow * pulse;
    FragColor = vec4(baseColor.rgb + glow, baseColor.a);
}
