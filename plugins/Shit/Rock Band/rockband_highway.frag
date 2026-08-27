#version 330 core
// @name Rock Band Note Highway
// @desc 3D perspective scrolling note track with 5 colorful lanes
// @param speed Highway Speed 0.1 5.0 1.5
// @param laneGlow Highway Glow 0.0 2.0 1.0
// @param perspective Track Perspective 0.1 1.0 0.5
// @param active Enable Highway 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float speed;
uniform float laneGlow;
uniform float perspective;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    vec2 p = uv - vec2(0.5, 0.0);
    if (p.y < 0.01) { FragColor = baseColor; return; }
    vec2 trackUV;
    trackUV.x = p.x / p.y + 0.5;
    trackUV.y = fract(1.0 / p.y * 0.2 + time * speed);
    if (trackUV.x >= 0.1 && trackUV.x <= 0.9 && uv.y < 0.65) {
        float laneWidth = 0.8 / 5.0;
        int laneIdx = int(clamp((trackUV.x - 0.1) / laneWidth, 0.0, 4.0));
        vec3 laneColors[5] = vec3[5](
            vec3(0.1, 0.9, 0.2),
            vec3(0.9, 0.1, 0.15),
            vec3(0.95, 0.9, 0.1),
            vec3(0.15, 0.4, 0.95),
            vec3(0.95, 0.5, 0.1)
        );
        vec3 laneCol = laneColors[laneIdx];
        float lanePos = fract((trackUV.x - 0.1) / laneWidth);
        float lineBorder = smoothstep(0.05, 0.0, abs(lanePos - 0.0)) + smoothstep(0.05, 0.0, abs(lanePos - 1.0));
        float gem = smoothstep(0.15, 0.05, abs(trackUV.y - 0.5));
        vec3 trackColor = mix(vec3(0.05, 0.05, 0.08), laneCol * 0.4, 0.3);
        trackColor += vec3(lineBorder) * 0.5;
        trackColor += laneCol * gem * 1.5 * laneGlow;
        float alpha = (1.0 - uv.y / 0.65) * 0.85;
        baseColor.rgb = mix(baseColor.rgb, trackColor, alpha);
    }
    FragColor = baseColor;
}
