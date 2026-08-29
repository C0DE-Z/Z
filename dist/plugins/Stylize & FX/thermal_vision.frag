#version 330 core
// @name Thermal Vision
// @desc Infrared heat map color palette simulation
// @param heatContrast Contrast 0.5 3.0 1.2
// @param active Enable Thermal 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float heatContrast;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    float val = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    val = clamp(pow(val, heatContrast), 0.0, 1.0);
    vec3 c1 = vec3(0.0, 0.0, 0.2);
    vec3 c2 = vec3(0.4, 0.0, 0.6);
    vec3 c3 = vec3(0.9, 0.1, 0.1);
    vec3 c4 = vec3(1.0, 0.8, 0.0);
    vec3 c5 = vec3(1.0, 1.0, 1.0);
    vec3 thermal;
    if (val < 0.25) { thermal = mix(c1, c2, val / 0.25); }
    else if (val < 0.5) { thermal = mix(c2, c3, (val - 0.25) / 0.25); }
    else if (val < 0.75) { thermal = mix(c3, c4, (val - 0.5) / 0.25); }
    else { thermal = mix(c4, c5, (val - 0.75) / 0.25); }
    FragColor = vec4(thermal, orig.a);
}
