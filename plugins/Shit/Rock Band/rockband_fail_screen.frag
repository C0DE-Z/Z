#version 330 core
// @name Rock Band Track Fail
// @desc Red fail warning vignette and amp feedback static distortion
// @param failRed Fail Warning Red 0.0 1.0 0.8
// @param ampStatic Amp Distortion 0.0 1.0 0.4
// @param active Enable Fail FX 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float failRed;
uniform float ampStatic;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float edge = length(uv - vec2(0.5, 0.5));
    float vignette = smoothstep(0.3, 0.7, edge) * failRed;
    vec3 col = mix(baseColor.rgb, vec3(0.9, 0.05, 0.05), vignette * 0.7);
    if (ampStatic > 0.0) {
        float noise = hash(uv + time) * ampStatic * 0.25;
        col += vec3(noise, noise * 0.2, noise * 0.2);
    }
    FragColor = vec4(col, baseColor.a);
}
