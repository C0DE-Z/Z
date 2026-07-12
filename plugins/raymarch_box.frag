#version 330 core
// @name Raymarched Box Field
// @desc A generative raymarching 3D box scene blended with video
// @param blend Blend Amount 0.0 1.0 0.5
// @param speed Time Speed 0.0 3.0 1.0
// @param colorShift Color Shift 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float blend;
uniform float speed;
uniform float colorShift;

float gTime = 0.;
const float REPEAT = 5.0;

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, s, -s, c);
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float box(vec3 pos, float scale) {
    pos *= scale;
    float base = sdBox(pos, vec3(.4, .4, .1)) / 1.5;
    pos.xy *= 5.;
    pos.y -= 3.5;
    pos.xy *= rot(.75);
    return -base;
}

float box_set(vec3 pos) {
    vec3 pos_origin = pos;
    pos = pos_origin;
    pos.y += sin(gTime * 0.4) * 2.5;
    pos.xy *= rot(.8);
    float box1 = box(pos, 2. - abs(sin(gTime * 0.4)) * 1.5);
    
    pos = pos_origin;
    pos.y -= sin(gTime * 0.4) * 2.5;
    pos.xy *= rot(.8);
    float box2 = box(pos, 2. - abs(sin(gTime * 0.4)) * 1.5);
    
    pos = pos_origin;
    pos.x += sin(gTime * 0.4) * 2.5;
    pos.xy *= rot(.8);
    float box3 = box(pos, 2. - abs(sin(gTime * 0.4)) * 1.5);	
    
    pos = pos_origin;
    pos.x -= sin(gTime * 0.4) * 2.5;
    pos.xy *= rot(.8);
    float box4 = box(pos, 2. - abs(sin(gTime * 0.4)) * 1.5);	
    
    pos = pos_origin;
    pos.xy *= rot(.8);
    float box5 = box(pos, .5) * 6.;	
    
    pos = pos_origin;
    float box6 = box(pos, .5) * 6.;	
    
    return max(max(max(max(max(box1, box2), box3), box4), box5), box6);
}

float map(vec3 pos) {
    return box_set(pos);
}

void main() {
    vec2 p = (TexCoord * 2.0 - 1.0) * vec2(resolution.x / resolution.y, 1.0);
    float tTime = time * speed;
    vec3 ro = vec3(0., -0.2, tTime * 4.);
    vec3 ray = normalize(vec3(p, 1.5));
    ray.xy = ray.xy * rot(sin(tTime * .03) * 5.);
    ray.yz = ray.yz * rot(sin(tTime * .05) * .2);
    
    float t = 0.1;
    vec3 col = vec3(0.);
    float ac = 0.0;

    for (int i = 0; i < 99; i++) {
        vec3 pos = ro + ray * t;
        pos = mod(pos - 2., 4.) - 2.;
        gTime = tTime - float(i) * 0.01;
        
        float d = map(pos);

        d = max(abs(d), 0.01);
        ac += exp(-d * 23.);

        t += d * 0.55;
    }

    // Capture original video pixel
    vec4 video = texture(videoTexture, TexCoord);
    vec3 videoColor = video.rgb;

    // Apply optional color shift tinting to base color
    vec3 baseColor = videoColor;
    if (colorShift > 0.0) {
        baseColor.r = mix(videoColor.r, videoColor.g, colorShift);
        baseColor.g = mix(videoColor.g, videoColor.b, colorShift);
        baseColor.b = mix(videoColor.b, videoColor.r, colorShift);
    }

    // Multiply raymarched glow intensity by base video color
    col = (ac * 0.02) * baseColor * 4.0;
    
    // Add subtle ambient tint mapping matching the original shadertoy look, but gated by video color
    vec3 glowTint = vec3(0.1, 0.3 * abs(sin(tTime)), 0.6 + sin(tTime) * 0.2);
    col += glowTint * baseColor * 0.8;

    // Calculate alpha transparency based on ray distance (boxes are opaque, background is transparent)
    float alpha = clamp(1.0 - t * (0.02 + 0.02 * sin(tTime)), 0.0, 1.0);

    // Composite: blend only the boxes over the clean video background using the alpha mask
    vec3 compositeColor = mix(videoColor, col, blend * alpha);
    
    FragColor = vec4(compositeColor, video.a);
}
