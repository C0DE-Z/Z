#version 330 core
// @name Rock Band Solo & Big Rock Ending
// @desc Solo section spotlight aura and Big Rock Ending (BRE) pyro explosions
// @param soloGlow Solo Spotlight 0.0 3.0 1.5
// @param pyroSparks Pyro Sparks 0.0 2.0 1.0
// @param active Enable Solo FX 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float soloGlow;
uniform float pyroSparks;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float dist = length(uv - vec2(0.5, 0.5));
    float spot = (1.0 - smoothstep(0.2, 0.5, dist)) * soloGlow;
    vec3 spotCol = vec3(0.2, 0.7, 1.0) * spot;
    vec3 sparkCol = vec3(0.0);
    if (pyroSparks > 0.0 && hash(uv + time) > 0.94) {
        sparkCol = vec3(1.0, 0.9, 0.4) * pyroSparks * 2.0;
    }
    FragColor = vec4(baseColor.rgb + spotCol + sparkCol, baseColor.a);
}
