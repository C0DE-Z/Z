#version 330 core
// @name Rock Band Sepia 16mm Film
// @desc Vintage 16mm warm sepia film filter with vertical scratches and gate flicker
// @param sepiaWarmth Sepia Tone 0.0 1.0 0.8
// @param scratches Film Scratches 0.0 1.0 0.4
// @param flicker Gate Flicker 0.0 1.0 0.25
// @param active Enable Sepia Film 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float sepiaWarmth;
uniform float scratches;
uniform float flicker;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec2 uv = TexCoord;
    float gray = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    vec3 sepia = vec3(gray * 1.2, gray * 0.9, gray * 0.6);
    vec3 color = mix(orig.rgb, sepia, sepiaWarmth);
    if (scratches > 0.0) {
        float scratchX = hash(vec2(floor(time * 20.0), 1.0));
        if (abs(uv.x - scratchX) < 0.002 * scratches) {
            color += vec3(0.4 * scratches);
        }
    }
    if (flicker > 0.0) {
        float f = (hash(vec2(time * 30.0, 0.0)) - 0.5) * 0.2 * flicker;
        color += vec3(f);
    }
    FragColor = vec4(color, orig.a);
}
