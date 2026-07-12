#version 330 core
// @name Strobe Flash
// @desc Hard strobe cuts and beat-synced flashes
// @param rate Strobe Rate 0.0 30.0 8.0
// @param intensity Flash Intensity 0.0 1.0 0.7
// @param invert Invert On Flash 0.0 1.0 0.0 bool
// @param audioSync Audio Sync 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float rate;
uniform float intensity;
uniform float invert;
uniform float audioSync;
uniform float audioBass;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    float beat = audioBass * audioSync;
    float strobe = step(0.5, fract(time * rate * 0.1 + beat));

    if (strobe > 0.5) {
        if (invert > 0.5) {
            color.rgb = 1.0 - color.rgb;
        } else {
            color.rgb = mix(color.rgb, vec3(1.0), intensity);
        }
    } else {
        color.rgb *= 1.0 - intensity * 0.3 * (1.0 - strobe);
    }

    FragColor = color;
}
