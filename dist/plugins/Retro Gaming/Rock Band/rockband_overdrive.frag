#version 330 core
// @name Rock Band Overdrive
// @desc Electric lightning bolts and golden energy aura pulse
// @param intensity Overdrive Gold Brightness 0.0 2.0 1.0
// @param lightning Electric Lightning Bolts 0.0 1.0 0.7
// @param speed Energy Pulse Speed 0.0 5.0 2.0
// @param active Overdrive Active 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float intensity;
uniform float lightning;
uniform float speed;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float pulse = sin(time * speed * 4.0) * 0.15 + 0.85;
    vec3 goldColor = vec3(1.0, 0.85, 0.2) * intensity * pulse;
    float bolt = 0.0;
    if (lightning > 0.0) {
        float n = noise(vec2(uv.x * 10.0, time * 8.0 * speed));
        float boltLine = abs(uv.y - (n * 0.4 + 0.3));
        bolt = smoothstep(0.03, 0.0, boltLine) * lightning;
    }
    vec3 finalColor = mix(baseColor.rgb, baseColor.rgb * goldColor + vec3(0.2, 0.15, 0.0), 0.4 * intensity);
    finalColor += vec3(0.9, 0.95, 1.0) * bolt;
    FragColor = vec4(finalColor, baseColor.a);
}
