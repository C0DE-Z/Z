#version 330 core
// @name Xbox 360 Blades Dashboard
// @desc Authentic 2005 Xbox 360 Xenon curved blades dashboard UI
// @param bladeIndex Active Blade (0=System 1=Media 2=Games 3=Live) 0.0 3.0 1.0
// @param bladeGlow Xenon Neon Glow 0.0 2.0 1.0
// @param glassReflect Curve Reflection 0.0 1.0 0.5
// @param active Show Blades UI 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float bladeIndex;
uniform float bladeGlow;
uniform float glassReflect;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    vec3 colors[4] = vec3[4](
        vec3(0.5, 0.1, 0.7),
        vec3(0.2, 0.75, 0.2),
        vec3(0.1, 0.5, 0.9),
        vec3(0.95, 0.45, 0.1)
    );
    int activeIdx = int(clamp(bladeIndex, 0.0, 3.0));
    vec3 bladeColor = colors[activeIdx];
    float curveY = 0.5 + sin(uv.x * 3.14159) * 0.15;
    float bladeDist = abs(uv.y - curveY);
    vec3 overlay = vec3(0.0);
    float alpha = 0.0;
    if (uv.y > curveY - 0.08 && uv.y < curveY + 0.08) {
        float edge = smoothstep(0.08, 0.0, bladeDist);
        overlay = mix(bladeColor * 1.5, vec3(1.0), pow(1.0 - bladeDist / 0.08, 3.0) * glassReflect);
        alpha = edge * 0.7;
    }
    vec2 orbPos = vec2(0.85, 0.85);
    float orbDist = length(uv - orbPos);
    if (orbDist < 0.2) {
        float glow = (1.0 - orbDist / 0.2) * bladeGlow;
        overlay += bladeColor * glow * 1.2;
        alpha = max(alpha, glow * 0.5);
    }
    float scanline = sin(uv.y * 400.0) * 0.05;
    overlay += vec3(scanline);
    FragColor = vec4(mix(baseColor.rgb, overlay, alpha), baseColor.a);
}
