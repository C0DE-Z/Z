#version 330 core
// @name Datamosh Transition
// @desc Melts the previous clip into the new one using luma displacement
// @param blend Crossfade 0.0 1.0 0.0
// @param melt Melt Strength 0.0 2.0 1.0
// @param blockiness Block Size 1.0 128.0 32.0

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float blend;
uniform float melt;
uniform float blockiness;

void main() {
    vec2 uv = TexCoord;
    
    // Pixelate UV for blocky motion vectors
    float blocks = 256.0 / blockiness;
    vec2 blockUV = floor(uv * blocks) / blocks;
    
    vec4 current = texture(videoTexture, uv);
    
    // Fake motion vector using luma derivative of the current frame
    vec2 e = vec2(1.0 / 1280.0, 1.0 / 720.0) * blockiness;
    float lumaRight = dot(texture(videoTexture, blockUV + vec2(e.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
    float lumaLeft = dot(texture(videoTexture, blockUV - vec2(e.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
    float lumaUp = dot(texture(videoTexture, blockUV + vec2(0.0, e.y)).rgb, vec3(0.299, 0.587, 0.114));
    float lumaDown = dot(texture(videoTexture, blockUV - vec2(0.0, e.y)).rgb, vec3(0.299, 0.587, 0.114));
    
    vec2 motion = vec2(lumaLeft - lumaRight, lumaDown - lumaUp); // Invert for pushing
    
    // Apply motion drift to the previous frame (feedback)
    vec2 smudgedUV = uv + motion * melt * 0.05;
    
    // Clamp UVs to avoid edge wrap artifacts
    smudgedUV = clamp(smudgedUV, 0.0, 1.0);
    
    vec4 prev = texture(feedbackTexture, smudgedUV);
    
    // Mix between the datamoshed previous frame and the clean current frame
    FragColor = mix(prev, current, blend);
}
