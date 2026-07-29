#version 330 core
// @name Color Cycle
// @desc Cycling hue shift with audio reactivity
// @param speed Hue Speed 0.0 5.0 1.0
// @param saturation Color Saturation 0.0 2.0 1.0
// @param audioReact Audio Reactivity 0.0 2.0 1.0
// @param preserveLuma Preserve Luminance 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float speed;
uniform float saturation;
uniform float audioReact;
uniform float preserveLuma;
uniform float audioMid;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 0.001)), d / (q.x + 0.001), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    float origLuma = dot(color.rgb, vec3(0.299, 0.587, 0.114));

    vec3 hsv = rgb2hsv(color.rgb);
    hsv.x = fract(hsv.x + time * speed * 0.1 + audioMid * audioReact * 0.2);
    hsv.y *= saturation;

    vec3 result = hsv2rgb(hsv);
    float newLuma = dot(result, vec3(0.299, 0.587, 0.114));
    result = mix(result, result * (origLuma / max(newLuma, 0.001)), preserveLuma);

    FragColor = vec4(result, color.a);
}
