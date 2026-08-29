#version 330 core
// @name Rock Band 1 Venue Lighting
// @desc Stage spotlights, smoke haze, and pulse strobes from Rock Band 1
// @param lightSpeed Light Sweep Speed 0.1 5.0 1.0
// @param haze Smoke Haze 0.0 1.0 0.4
// @param strobe Strobe Intensity 0.0 2.0 1.0
// @param active Enable Venue Lights 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float lightSpeed;
uniform float haze;
uniform float strobe;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float t = time * lightSpeed;
    vec3 c1 = vec3(0.9, 0.1, 0.4) * max(0.0, sin(uv.x * 6.0 + t * 2.0));
    vec3 c2 = vec3(0.1, 0.4, 0.9) * max(0.0, cos(uv.x * 5.0 - t * 1.5));
    vec3 c3 = vec3(0.6, 0.1, 0.9) * max(0.0, sin((uv.x + uv.y) * 4.0 + t));
    vec3 spotLights = (c1 + c2 + c3) * (1.0 - uv.y * 0.7);
    float strobeFlash = pow(max(0.0, sin(t * 12.0)), 8.0) * strobe;
    vec3 finalCol = baseColor.rgb + spotLights * 0.5 + vec3(strobeFlash * 0.4);
    if (haze > 0.0) {
        float fog = sin(uv.x * 10.0 + t) * cos(uv.y * 8.0 - t * 0.5) * 0.1 * haze;
        finalCol += vec3(fog);
    }
    FragColor = vec4(finalCol, baseColor.a);
}
