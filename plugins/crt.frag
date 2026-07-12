#version 330 core
// @name CRT Simulation
// @desc Phosphor grid, bloom, and barrel curvature
// @param curvature Tube Curvature 0.0 0.5 0.15
// @param bloom Bloom Intensity 0.0 2.0 0.5
// @param persistence Phosphor Persistence 0.0 1.0 0.3
// @param rgbOffset RGB Shift Offset 0.0 0.02 0.003
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float curvature;
uniform float bloom;
uniform float rgbOffset;
// Persistence is not cleanly supported in this generic pass without feedback buff
void main() {
    vec2 uv = TexCoord;
    if (curvature > 0.0) {
        uv = uv - 0.5;
        float dist = dot(uv, uv);
        uv = uv * (1.0 + curvature * dist + curvature * 0.5 * dist * dist);
        uv = uv + 0.5;
    }
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0); return;
    }
    float r = texture(videoTexture, uv - vec2(rgbOffset, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(rgbOffset, 0.0)).b;
    vec3 color = vec3(r, g, b);
    float scanline = sin(uv.y * 800.0) * 0.15;
    color -= vec3(scanline);
    float mask = sin(uv.x * 1200.0) * 0.08;
    color += vec3(mask);
    if (bloom > 0.0) {
        vec3 blurColor = texture(videoTexture, uv + vec2(0.005, 0.005)).rgb;
        blurColor += texture(videoTexture, uv - vec2(0.005, 0.005)).rgb;
        color += blurColor * 0.12 * bloom;
    }
    FragColor = vec4(color, 1.0);
}
