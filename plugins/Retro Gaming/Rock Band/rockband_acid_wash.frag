#version 330 core
// @name Rock Band Acid Wash & Solarization
// @desc Psychedelic solarized color inversion and neon halo edge bleed
// @param solarize Solarization 0.0 1.0 0.7
// @param hueShift Hue Shift 0.0 3.14 1.57
// @param active Enable Acid Wash 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float solarize;
uniform float hueShift;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec3 col = orig.rgb;
    if (solarize > 0.0) {
        col = abs(col - vec3(solarize));
    }
    float angle = hueShift;
    mat3 hueMat = mat3(
        0.213 + 0.787 * cos(angle) - 0.213 * sin(angle),
        0.213 - 0.213 * cos(angle) + 0.143 * sin(angle),
        0.213 - 0.213 * cos(angle) - 0.787 * sin(angle),
        0.715 - 0.715 * cos(angle) - 0.715 * sin(angle),
        0.715 + 0.285 * cos(angle) + 0.140 * sin(angle),
        0.715 - 0.715 * cos(angle) + 0.715 * sin(angle),
        0.072 - 0.072 * cos(angle) + 0.928 * sin(angle),
        0.072 - 0.072 * cos(angle) - 0.283 * sin(angle),
        0.072 + 0.928 * cos(angle) + 0.072 * sin(angle)
    );
    col = clamp(hueMat * col, 0.0, 1.0);
    FragColor = vec4(col, orig.a);
}
