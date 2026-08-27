#version 330 core
// @name Rock Band Duotone Video
// @desc Maps video luminance onto a 2-color rock venue gradient (Midnight Blue/Gold, Crimson/Cyan, Purple/Lime)
// @param preset Palette Preset (0=Blue/Gold 1=Crimson/Cyan 2=Purple/Lime) 0.0 2.0 0.0
// @param contrast Duotone Contrast 0.5 3.0 1.5
// @param active Enable Duotone 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float preset;
uniform float contrast;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    float gray = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    gray = clamp(pow(gray, contrast), 0.0, 1.0);
    vec3 colA = vec3(0.05, 0.1, 0.35);
    vec3 colB = vec3(1.0, 0.8, 0.15);
    int p = int(clamp(preset, 0.0, 2.0));
    if (p == 1) {
        colA = vec3(0.85, 0.05, 0.1);
        colB = vec3(0.1, 0.95, 0.9);
    } else if (p == 2) {
        colA = vec3(0.4, 0.05, 0.6);
        colB = vec3(0.4, 0.95, 0.2);
    }
    vec3 finalCol = mix(colA, colB, gray);
    FragColor = vec4(finalCol, orig.a);
}
