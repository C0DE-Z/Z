---
id: cross_dissolve
name: Cross Dissolve
type: transition
---

vec4 processFrame(vec2 uv) {
    vec4 c1 = texture(videoTexture, uv);
    vec4 c2 = texture(videoTexture2, uv);
    return mix(c1, c2, progress);
}
