#version 330 core
// @name Xbox 360 Red Ring of Death
// @desc 3-Quadrant Red Ring glow overlay with hardware failure overheat distortion
// @param rrodGlow Ring Glow Brightness 0.0 2.0 1.0
// @param overheatFlicker Overheat Signal Flicker 0.0 1.0 0.5
// @param thermalRed Heat Color Shift 0.0 1.0 0.7
// @param active RROD Glitch Active 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float rrodGlow;
uniform float overheatFlicker;
uniform float thermalRed;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    if (overheatFlicker > 0.0 && hash(vec2(floor(uv.y * 40.0), floor(time * 15.0))) > 0.92 - (overheatFlicker * 0.1)) {
        uv.x += (hash(vec2(time)) - 0.5) * 0.08 * overheatFlicker;
    }
    vec4 color = texture(videoTexture, uv);
    if (thermalRed > 0.0) {
        color.r = mix(color.r, color.r * 1.8 + 0.2, thermalRed);
        color.g *= (1.0 - thermalRed * 0.5);
        color.b *= (1.0 - thermalRed * 0.7);
    }
    vec2 pos = (uv - vec2(0.5, 0.5)) * vec2(1.77, 1.0);
    float r = length(pos);
    float angle = atan(pos.y, pos.x);
    if (r > 0.22 && r < 0.32) {
        bool quad1 = (angle >= -0.7 && angle < 0.7);
        bool quad2 = (angle >= 0.8 && angle < 2.3);
        bool quad3 = (angle >= 2.4 || angle < -2.4);
        if (quad1 || quad2 || quad3) {
            float ringIntensity = smoothstep(0.05, 0.0, abs(r - 0.27)) * rrodGlow;
            float flicker = 0.8 + 0.2 * sin(time * 30.0);
            vec3 redColor = vec3(1.0, 0.05, 0.02) * 2.0 * flicker;
            color.rgb = mix(color.rgb, redColor, ringIntensity * 0.8);
        }
    }
    FragColor = color;
}
