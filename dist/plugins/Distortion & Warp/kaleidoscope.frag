#version 330 core
// @name Kaleidoscope
// @desc Mirror-fold kaleidoscope (Rock Band visualizer style)
// @param segments Mirror Segments 2.0 16.0 6.0
// @param rotation Spin Speed 0.0 2.0 0.1
// @param zoom Inner Zoom 0.5 3.0 1.0
// @param blend Blend With Original 0.0 1.0 0.3
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float segments;
uniform float rotation;
uniform float zoom;
uniform float blend;

void main() {
    vec2 uv = TexCoord - 0.5;
    float angle = atan(uv.y, uv.x) + time * rotation;
    float radius = length(uv) * zoom;

    float segAngle = 6.28318 / segments;
    angle = mod(angle, segAngle);
    if (mod(floor(atan(TexCoord.y - 0.5, TexCoord.x - 0.5) / segAngle), 2.0) < 1.0) {
        angle = segAngle - angle;
    }

    vec2 kUV = vec2(cos(angle), sin(angle)) * radius + 0.5;
    kUV = clamp(kUV, 0.0, 1.0);

    vec4 kaleid = texture(videoTexture, kUV);
    vec4 original = texture(videoTexture, TexCoord);
    FragColor = mix(kaleid, original, blend);
}
