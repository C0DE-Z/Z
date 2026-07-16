#include "shaders.h"

const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 position;
out vec2 TexCoord;
void main() {
    TexCoord = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

const char* bentShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float syncDrift;
uniform float clockCorruption;
uniform float railInstability;
uniform float addressScramble;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 uv = TexCoord;
    // Sync Drift (horizontal shifting / tearing of rows)
    if (syncDrift > 0.0) {
        float drift = sin(uv.y * 10.0 + time * 5.0) * 0.02 * syncDrift;
        // Add random sync breaks
        if (hash(vec2(floor(uv.y * 30.0), time)) > 0.98 - (syncDrift * 0.05)) {
            drift += hash(vec2(time)) * 0.1 * syncDrift;
        }
        uv.x = fract(uv.x + drift);
    }
    // Clock Corruption (repeated columns, stretched pixels)
    if (clockCorruption > 0.0) {
        float colSteps = 128.0 - (clockCorruption * 100.0);
        float stepX = floor(uv.x * colSteps) / colSteps;
        if (hash(vec2(stepX, time)) > 0.95 - (clockCorruption * 0.1)) {
            uv.x = stepX; // lock pixel clock column
        }
    }

    // Address Scrambling (corrupted RAM addressing)
    if (addressScramble > 0.0) {
        float grid = 8.0;
        vec2 cell = floor(uv * grid) / grid;
        if (hash(cell + time) < addressScramble * 0.2) {
            // swap texture blocks
            uv = fract(uv + hash(cell) * 0.5);
        }
    }

    vec4 color = texture(videoTexture, uv);

    // Power Rail Instability (brightness oscillations and resets)
    if (railInstability > 0.0) {
        float noiseValue = sin(time * 20.0) * cos(time * 35.0);
        color.rgb *= 1.0 + noiseValue * 0.4 * railInstability;
        // color washouts
        if (noiseValue > 0.8) {
            color.rgb += vec3(0.3 * railInstability);
        }
    }

    FragColor = color;
}
)";

const char* vhsShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float tracking;
uniform float noise;
uniform float wear;
uniform float chromaDelay;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 uv = TexCoord;

    // Head Switching Noise (tearing at the bottom 2% of the screen)
    if (uv.y < 0.02) {
        float switchOffset = hash(vec2(uv.y, time)) * 0.05;
        uv.x = fract(uv.x + switchOffset);
    }

    // Tracking Jitter/Lines
    float trackLine = fract(time * 0.2);
    float distToTrack = abs(uv.y - trackLine);
    if (distToTrack < 0.05 * tracking) {
        uv.x += sin(uv.y * 50.0 + time * 10.0) * 0.015 * tracking;
    }

    // Chroma Delay (VHS color bleeding: shift red and blue channels)
    float r = texture(videoTexture, uv - vec2(chromaDelay, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(chromaDelay, 0.0)).b;

    vec4 color = vec4(r, g, b, 1.0);

    // Add Tape Noise / Generation Loss Wear
    if (noise > 0.0 || wear > 0.0) {
        float n = hash(uv + time);
        color.rgb += vec3(n * 0.15 * noise);
        // Horizontal scan dropouts
        if (hash(vec2(uv.y * 200.0, time)) > 0.99 - (wear * 0.03)) {
            color.rgb = vec3(0.8); // White dropout line
        }
    }

    FragColor = color;
}
)";

const char* crtShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float curvature;
uniform float bloom;
uniform float persistence;
uniform float rgbOffset;

void main() {
    vec2 uv = TexCoord;

    // 1. CRT Screen Curvature (barrel distortion)
    if (curvature > 0.0) {
        uv = uv - 0.5;
        float dist = dot(uv, uv);
        uv = uv * (1.0 + curvature * dist + curvature * 0.5 * dist * dist);
        uv = uv + 0.5;
    }

    // Border cut off for curved tube edges
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // 2. Phosphor Mask (Convergence Offset)
    float r = texture(videoTexture, uv - vec2(rgbOffset, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(rgbOffset, 0.0)).b;
    vec3 color = vec3(r, g, b);

    // 3. Scanline grid overlay
    float scanline = sin(uv.y * 800.0) * 0.15;
    color -= vec3(scanline);

    // 4. Subtle RGB slot mask pattern
    float mask = sin(uv.x * 1200.0) * 0.08;
    color += vec3(mask);

    // 5. Bloom/Glow emulation
    if (bloom > 0.0) {
        vec3 blurColor = texture(videoTexture, uv + vec2(0.005, 0.005)).rgb;
        blurColor += texture(videoTexture, uv - vec2(0.005, 0.005)).rgb;
        color += blurColor * 0.12 * bloom;
    }

    FragColor = vec4(color, 1.0);
}
)";

const char* feedbackShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D currentTexture;
uniform sampler2D feedbackTexture;
uniform float gain;
uniform float zoom;
uniform float rotation;
uniform vec2 shift;

void main() {
    vec2 uv = TexCoord;
    // Read clean, current frame
    vec4 current = texture(currentTexture, uv);

    // Apply zoom, rotation, offset mapping to feedback texture coordinates
    vec2 f_uv = uv - 0.5;
    // Zoom
    f_uv /= zoom;
    // Rotate
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    f_uv = vec2(f_uv.x * cosR - f_uv.y * sinR, f_uv.x * sinR + f_uv.y * cosR);
    f_uv = f_uv + 0.5;
    // Shift/Offset
    f_uv -= shift;

    // Read historical feedback buffer
    vec4 feedback = texture(feedbackTexture, f_uv);

    // Blend current with historical feedback
    FragColor = mix(current, feedback, gain);
}
)";

const char* passthroughShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D videoTexture;
void main() {
    FragColor = texture(videoTexture, TexCoord);
}
)";

const char* milkdropShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float bass;
uniform float mid;
uniform float treble;
uniform float zoom;
uniform float rotation;

void main() {
    vec2 uv = TexCoord - 0.5;
    // Apply audio-reactive rotation
    float angle = rotation * bass * 3.14159;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    uv = rot * uv;
    // Apply audio-reactive zoom and non-linear fractal warp
    float z = zoom - (treble * 0.1);
    float radius = length(uv);
    uv *= z - (sin(radius * 10.0 - time * 5.0) * mid * 0.05);
    uv += 0.5;
    // Chromatic aberration driven by bass
    float r = texture(videoTexture, uv + (uv - 0.5) * bass * 0.05).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv - (uv - 0.5) * bass * 0.05).b;
    vec4 color = vec4(r, g, b, 1.0);
    // Brighten based on mid/treble hits
    color.rgb *= 1.0 + (mid * 0.2) + (treble * 0.2);
    FragColor = color;
}
)";

const char* xorShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D blendTexture;
uniform float intensity;

void main() {
    vec4 c1 = texture(videoTexture, TexCoord);
    vec4 c2 = texture(blendTexture, TexCoord);
    // Scale to 255 integers to perform bitwise XOR
    uvec3 u1 = uvec3(c1.rgb * 255.0);
    uvec3 u2 = uvec3(c2.rgb * 255.0);
    uvec3 xorVal = u1 ^ u2;
    vec3 result = vec3(xorVal) / 255.0;
    FragColor = vec4(mix(c1.rgb, result, intensity), 1.0);
}
)";
