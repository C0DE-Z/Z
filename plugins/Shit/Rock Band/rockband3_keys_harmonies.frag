#version 330 core
// @name Rock Band 3 Keyboard & Vocal Harmonies
// @desc 25-key keyboard track highway and triple vocal harmony lines from Rock Band 3
// @param speed Key Track Speed 0.1 5.0 1.5
// @param harmonyLines Vocal Harmonies Glow 0.0 2.0 1.0
// @param active Enable Keys & Harmonies 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float speed;
uniform float harmonyLines;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    vec3 col = baseColor.rgb;
    if (harmonyLines > 0.0 && uv.y > 0.85) {
        float line1 = smoothstep(0.01, 0.0, abs(uv.y - 0.95 - sin(uv.x * 20.0 + time * 4.0) * 0.01));
        float line2 = smoothstep(0.01, 0.0, abs(uv.y - 0.91 - cos(uv.x * 15.0 - time * 3.0) * 0.01));
        float line3 = smoothstep(0.01, 0.0, abs(uv.y - 0.87 - sin(uv.x * 25.0 + time * 5.0) * 0.01));
        col += vec3(0.1, 0.8, 1.0) * line1 * harmonyLines;
        col += vec3(1.0, 0.8, 0.1) * line2 * harmonyLines;
        col += vec3(0.9, 0.2, 0.8) * line3 * harmonyLines;
    }
    FragColor = vec4(col, baseColor.a);
}
