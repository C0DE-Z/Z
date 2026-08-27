#version 330 core
// @name Edge Glow Glitch
// @desc Sobel edge detection with neon bleed
// @param edgeStrength Edge Intensity 0.0 2.0 1.0
// @param glowColor Glow Mix 0.0 1.0 0.5
// @param threshold Edge Threshold 0.0 0.5 0.1
// @param invert Invert Edges 0.0 1.0 0.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float edgeStrength;
uniform float glowColor;
uniform float threshold;
uniform float invert;

void main() {
    vec2 px = 1.0 / resolution;
    vec4 tl = texture(videoTexture, TexCoord + vec2(-px.x, -px.y));
    vec4 tm = texture(videoTexture, TexCoord + vec2(0.0, -px.y));
    vec4 tr = texture(videoTexture, TexCoord + vec2(px.x, -px.y));
    vec4 ml = texture(videoTexture, TexCoord + vec2(-px.x, 0.0));
    vec4 mr = texture(videoTexture, TexCoord + vec2(px.x, 0.0));
    vec4 bl = texture(videoTexture, TexCoord + vec2(-px.x, px.y));
    vec4 bm = texture(videoTexture, TexCoord + vec2(0.0, px.y));
    vec4 br = texture(videoTexture, TexCoord + vec2(px.x, px.y));

    float gx = -tl.r - 2.0*ml.r - bl.r + tr.r + 2.0*mr.r + br.r;
    float gy = -tl.r - 2.0*tm.r - tr.r + bl.r + 2.0*bm.r + br.r;
    float edge = sqrt(gx*gx + gy*gy);

    if (invert > 0.5) edge = 1.0 - edge;
    edge = smoothstep(threshold, threshold + 0.2, edge) * edgeStrength;

    vec4 color = texture(videoTexture, TexCoord);
    vec3 glow = vec3(edge * 0.5, edge * 0.2, edge);
    color.rgb = mix(color.rgb, glow, glowColor * edge);
    color.rgb += glow * 0.3;

    FragColor = clamp(color, 0.0, 1.0);
}
