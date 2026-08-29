#version 330 core
// @name Halftone
// @desc Comic book halftone dot pattern
// @param dotSize Dot Size 2.0 20.0 6.0
// @param angle Screen Angle 0.0 90.0 45.0
// @param contrast Dot Contrast 0.0 2.0 1.0
// @param colorMode Color Halftone 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float dotSize;
uniform float angle;
uniform float contrast;
uniform float colorMode;

void main() {
    float rad = angle * 0.0174533;
    mat2 rot = mat2(cos(rad), -sin(rad), sin(rad), cos(rad));
    vec2 uv = rot * (TexCoord - 0.5) * dotSize * 50.0;
    vec2 cell = fract(uv) - 0.5;

    vec4 color = texture(videoTexture, TexCoord);
    float dist = length(cell);

    if (colorMode > 0.5) {
        float dr = smoothstep(0.5 - color.r * contrast * 0.5, 0.5, dist);
        float dg = smoothstep(0.5 - color.g * contrast * 0.5, 0.5, dist);
        float db = smoothstep(0.5 - color.b * contrast * 0.5, 0.5, dist);
        FragColor = vec4(dr, dg, db, 1.0);
    } else {
        float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        float d = smoothstep(0.5 - luma * contrast * 0.5, 0.5, dist);
        FragColor = vec4(vec3(d), 1.0);
    }
}
