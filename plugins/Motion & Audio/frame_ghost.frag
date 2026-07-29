#version 330 core
// @name Frame Ghost
// @desc Multi-offset frame ghosting with color fringing
// @param ghosts Ghost Count 1.0 8.0 4.0
// @param spacing Ghost Spacing 0.0 0.05 0.01
// @param opacity Ghost Opacity 0.0 1.0 0.4
// @param direction Ghost Direction 0.0 6.28 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float ghosts;
uniform float spacing;
uniform float opacity;
uniform float direction;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    vec2 dir = vec2(cos(direction), sin(direction));

    int count = int(ghosts);
    for (int i = 1; i < 8; i++) {
        if (i >= count) break;
        vec2 offset = dir * spacing * float(i);
        vec4 ghost = texture(videoTexture, TexCoord - offset);
        float fade = opacity / float(i);
        color.rgb += ghost.rgb * fade;
        if (i == 1) color.r = max(color.r, ghost.r * fade);
        if (i == 2) color.b = max(color.b, ghost.b * fade);
    }

    FragColor = clamp(color, 0.0, 1.0);
}
