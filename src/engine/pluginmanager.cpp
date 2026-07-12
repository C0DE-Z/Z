#include "pluginmanager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

void PluginManager::scanPluginsDir(const std::string& path) {
    plugins.clear();
    QDir dir(QString::fromStdString(path));
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }

    QStringList filters;
    filters << "*.frag";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo : fileList) {
        QString fragPath = fileInfo.absoluteFilePath();
        QString baseName = fileInfo.baseName();

        QFile fragFile(fragPath);
        if (fragFile.open(QIODevice::ReadOnly)) {
            QTextStream in(&fragFile);
            ShaderPlugin plugin;
            plugin.id = baseName.toStdString();
            plugin.name = baseName.toStdString();
            plugin.description = "No description";
            plugin.fragmentShaderPath = fragPath.toStdString();

            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("// @name ")) {
                    plugin.name = line.mid(9).trimmed().toStdString();
                } else if (line.startsWith("// @desc ")) {
                    plugin.description = line.mid(9).trimmed().toStdString();
                } else if (line.startsWith("// @param ")) {
                    QStringList parts = line.mid(10).trimmed().split(" ", Qt::SkipEmptyParts);
                    bool isBool = (!parts.isEmpty() && parts.last() == "bool");
                    int numOffset = isBool ? 1 : 0;
                    int n = parts.size();
                    if (n >= 4 + numOffset) {
                        ShaderParameter param;
                        param.name = parts[0].toStdString();
                        param.defaultVal = parts[n - 1 - numOffset].toDouble();
                        param.maxVal = parts[n - 2 - numOffset].toDouble();
                        param.minVal = parts[n - 3 - numOffset].toDouble();
                        param.currentVal = param.defaultVal;
                        param.curve = AnimationCurve(param.defaultVal);
                        param.isBool = isBool;
                        QString label;
                        for (int i = 1; i < n - 3 - numOffset; ++i) {
                            if (i > 1) label += " ";
                            label += parts[i];
                        }
                        if (label.isEmpty()) label = parts[0];
                        param.label = label.toStdString();
                        plugin.parameters.push_back(param);
                    }
                }
            }
            if (std::none_of(plugins.begin(), plugins.end(), [&](const ShaderPlugin& p){ return p.id == plugin.id; })) {
                plugins.push_back(plugin);
            }
        }
    }
}

ShaderPlugin* PluginManager::findPlugin(const std::string& id) {
    for (auto& plugin : plugins) {
        if (plugin.id == id) {
            return &plugin;
        }
    }
    return nullptr;
}

void PluginManager::createDefaultPlugins(const std::string& path) {
    QDir dir(QString::fromStdString(path));
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto writeFrag = [&](const QString& name, const QString& content) {
        QString fragPath = dir.absoluteFilePath(name + ".frag");
        if (!QFile::exists(fragPath)) {
            QFile fragFile(fragPath);
            if (fragFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&fragFile);
                out << content;
                fragFile.close();
            }
        }
    };

    writeFrag("cross_dissolve", R"(#version 330 core
// @name Cross Dissolve
// @desc Classic cross dissolve transition
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;

void main() {
    vec4 c1 = texture(videoTexture, TexCoord);
    vec4 c2 = texture(videoTexture2, TexCoord);
    FragColor = mix(c1, c2, progress);
}
)");

    writeFrag("datamosh_transition", R"(#version 330 core
// @name Datamosh Transition
// @desc Pixel sorting and offset datamosh effect
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform vec2 resolution;
uniform float progress;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec4 c2 = texture(videoTexture2, TexCoord);
    vec2 offset = vec2(0.0);
    float stepX = 1.0 / resolution.x;
    float stepY = 1.0 / resolution.y;
    float l = luma(texture(videoTexture2, TexCoord - vec2(stepX, 0.0)).rgb);
    float r = luma(texture(videoTexture2, TexCoord + vec2(stepX, 0.0)).rgb);
    float u = luma(texture(videoTexture2, TexCoord - vec2(0.0, stepY)).rgb);
    float d = luma(texture(videoTexture2, TexCoord + vec2(0.0, stepY)).rgb);
    vec2 gradient = vec2(r - l, d - u);
    vec2 displacedUV = TexCoord + gradient * progress * 0.5;
    float blocks = 64.0 - progress * 48.0; 
    vec2 blockUV = floor(displacedUV * blocks) / blocks;
    vec4 moshedC1 = texture(videoTexture, blockUV);
    float blend = smoothstep(0.8, 1.0, progress);
    FragColor = mix(moshedC1, c2, blend);
}
)");

    writeFrag("datamosh", R"(#version 330 core
// @name Datamosh (CPU)
// @desc FFmpeg-level P-frame and I-frame corruption
// @param iDrop I-Frame Drop 0.0 1.0 0.0
// @param pDup P-Frame Duplicate 0.0 1.0 0.0
// @param pDupCount Duplication Count 1.0 60.0 4.0
// @param pDrop P-Frame Drop 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
)");

    writeFrag("xor_gate", R"(#version 330 core
// @name XOR Color Glitch
// @desc Bitwise XOR corruption of color channels
// @param xorR XOR Red Mask 0.0 255.0 128.0
// @param xorG XOR Green Mask 0.0 255.0 64.0
// @param xorB XOR Blue Mask 0.0 255.0 32.0
// @param temporal Temporal Blend 0.0 1.0 0.5
// @param spatial Spatial Offset 0.0 0.05 0.0
// @param chromaShift Chroma Split 0.0 0.05 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float xorR;
uniform float xorG;
uniform float xorB;
uniform float temporal;
uniform float spatial;
uniform float chromaShift;

void main() {
    vec2 uv = TexCoord;
    // Chromatic split prior to XOR
    vec4 color;
    if (chromaShift > 0.0) {
        color.r = texture(videoTexture, uv - vec2(chromaShift, 0.0)).r;
        color.g = texture(videoTexture, uv).g;
        color.b = texture(videoTexture, uv + vec2(chromaShift, 0.0)).b;
        color.a = texture(videoTexture, uv).a;
    } else {
        color = texture(videoTexture, uv);
    }
    uvec3 iColor = uvec3(color.rgb * 255.0);
    // Spatial offset XOR
    if (spatial > 0.0) {
        vec4 offsetColor = texture(videoTexture, uv + vec2(spatial, 0.0));
        uvec3 iOffset = uvec3(offsetColor.rgb * 255.0);
        iColor = iColor ^ iOffset;
    }
    // Temporal feedback XOR
    if (temporal > 0.0) {
        vec4 prevColor = texture(feedbackTexture, uv);
        uvec3 iPrev = uvec3(prevColor.rgb * 255.0);
        uvec3 xorTemp = iColor ^ iPrev;
        iColor = uvec3(mix(vec3(iColor), vec3(xorTemp), temporal));
    }
    // Custom channel masks
    uvec3 mask = uvec3(uint(xorR), uint(xorG), uint(xorB));
    iColor = iColor ^ mask;
    FragColor = vec4(vec3(iColor) / 255.0, color.a);
}
)");

    writeFrag("temporal_echo", R"(#version 330 core
// @name Temporal Echo
// @desc Blends previous frames to create a ghosting trail
// @param feedback Blend Amount 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform float feedback;

void main() {
    vec4 current = texture(videoTexture, TexCoord);
    vec4 prev = texture(feedbackTexture, TexCoord);
    FragColor = mix(current, prev, feedback);
}
)");

    writeFrag("vhs", R"(#version 330 core
// @name VHS Degradation
// @desc Authentic tape wear and chroma bleed
// @param tracking Tracking Offset 0.0 1.0 0.1
// @param noise Signal Noise 0.0 1.0 0.2
// @param wear Tape Wear 0.0 1.0 0.1
// @param chromaDelay Chroma Bleed Delay 0.0 0.05 0.01
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float tracking;
uniform float noise;
uniform float wear;
uniform float chromaDelay;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    if (uv.y < 0.02) {
        float switchOffset = hash(vec2(uv.y, time)) * 0.05;
        uv.x = fract(uv.x + switchOffset);
    }
    float trackLine = fract(time * 0.2);
    float distToTrack = abs(uv.y - trackLine);
    if (distToTrack < 0.05 * tracking) {
        uv.x += sin(uv.y * 50.0 + time * 10.0) * 0.015 * tracking;
    }
    float r = texture(videoTexture, uv - vec2(chromaDelay, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(chromaDelay, 0.0)).b;
    vec4 color = vec4(r, g, b, 1.0);
    if (noise > 0.0 || wear > 0.0) {
        float n = hash(uv + time);
        color.rgb += vec3(n * 0.15 * noise);
        if (hash(vec2(uv.y * 200.0, time)) > 0.99 - (wear * 0.03)) {
            color.rgb = vec3(0.8);
        }
    }
    FragColor = color;
}
)");

    writeFrag("crt", R"(#version 330 core
// @name CRT Simulation
// @desc Phosphor grid, bloom, and barrel curvature
// @param curvature Tube Curvature 0.0 0.5 0.15
// @param bloom Bloom Intensity 0.0 2.0 0.5
// @param persistence Phosphor Persistence 0.0 1.0 0.3
// @param rgbOffset RGB Shift Offset 0.0 0.02 0.003
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float curvature;
uniform float bloom;
uniform float rgbOffset;
// Persistence is not cleanly supported in this generic pass without feedback buff
void main() {
    vec2 uv = TexCoord;
    if (curvature > 0.0) {
        uv = uv - 0.5;
        float dist = dot(uv, uv);
        uv = uv * (1.0 + curvature * dist + curvature * 0.5 * dist * dist);
        uv = uv + 0.5;
    }
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0); return;
    }
    float r = texture(videoTexture, uv - vec2(rgbOffset, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(rgbOffset, 0.0)).b;
    vec3 color = vec3(r, g, b);
    float scanline = sin(uv.y * 800.0) * 0.15;
    color -= vec3(scanline);
    float mask = sin(uv.x * 1200.0) * 0.08;
    color += vec3(mask);
    if (bloom > 0.0) {
        vec3 blurColor = texture(videoTexture, uv + vec2(0.005, 0.005)).rgb;
        blurColor += texture(videoTexture, uv - vec2(0.005, 0.005)).rgb;
        color += blurColor * 0.12 * bloom;
    }
    FragColor = vec4(color, 1.0);
}
)");

    writeFrag("bent", R"(#version 330 core
// @name Circuit Bent Camera
// @desc Hardware failure simulation
// @param syncDrift Sync Drift 0.0 1.0 0.0
// @param clockCorruption Pixel Clock Corruption 0.0 1.0 0.0
// @param railInstability Power Rail Instability 0.0 1.0 0.0
// @param addressScramble Address Scrambling 0.0 1.0 0.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float syncDrift;
uniform float clockCorruption;
uniform float railInstability;
uniform float addressScramble;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv = TexCoord;
    if (syncDrift > 0.0) {
        float drift = sin(uv.y * 10.0 + time * 5.0) * 0.02 * syncDrift;
        if (hash(vec2(floor(uv.y * 30.0), time)) > 0.98 - (syncDrift * 0.05)) {
            drift += hash(vec2(time)) * 0.1 * syncDrift;
        }
        uv.x = fract(uv.x + drift);
    }
    if (clockCorruption > 0.0) {
        float colSteps = 128.0 - (clockCorruption * 100.0);
        float stepX = floor(uv.x * colSteps) / colSteps;
        if (hash(vec2(stepX, time)) > 0.95 - (clockCorruption * 0.1)) {
            uv.x = stepX;
        }
    }
    if (addressScramble > 0.0) {
        float grid = 8.0;
        vec2 cell = floor(uv * grid) / grid;
        if (hash(cell + time) < addressScramble * 0.2) {
            uv = fract(uv + hash(cell) * 0.5);
        }
    }
    vec4 color = texture(videoTexture, uv);
    if (railInstability > 0.0) {
        float noiseValue = sin(time * 20.0) * cos(time * 35.0);
        color.rgb *= 1.0 + noiseValue * 0.4 * railInstability;
        if (noiseValue > 0.8) {
            color.rgb += vec3(0.3 * railInstability);
        }
    }
    FragColor = color;
}
)");

    writeFrag("milkdrop", R"(#version 330 core
// @name Milkdrop Visualizer
// @desc Audio reactive fractal warping
// @param bass Bass Influence 0.0 2.0 1.0
// @param mid Mid Influence 0.0 2.0 1.0
// @param treble Treble Influence 0.0 2.0 1.0
// @param zoom Fractal Zoom 0.5 2.0 1.05
// @param rotation Fractal Rotation -1.0 1.0 0.1
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float bass;
uniform float mid;
uniform float treble;
uniform float zoom;
uniform float rotation;
uniform float audioBass;
uniform float audioMid;
uniform float audioTreble;

void main() {
    vec2 uv = TexCoord - 0.5;
    // Realtime audio inputs mixed with custom parameter biases
    float rBass = audioBass * bass;
    float rMid = audioMid * mid;
    float rTreble = audioTreble * treble;

    float angle = rotation * rBass * 3.14159;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    uv = rot * uv;
    float z = zoom - (rTreble * 0.1);
    float radius = length(uv);
    uv *= z - (sin(radius * 10.0 - time * 5.0) * rMid * 0.05);
    uv += 0.5;
    float r = texture(videoTexture, uv + (uv - 0.5) * rBass * 0.05).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv - (uv - 0.5) * rBass * 0.05).b;
    vec4 color = vec4(r, g, b, 1.0);
    color.rgb *= 1.0 + (rMid * 0.2) + (rTreble * 0.2);
    FragColor = color;
}
)");

    writeFrag("digital_noise", R"(#version 330 core
// @name Digital Noise Glitch
// @desc Custom color displacement and horizontal line slicing
// @param slice_density Slice Density 0.0 50.0 10.0
// @param slice_speed Glitch Speed 0.0 10.0 2.0
// @param glitch_active Is Glitch Active 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D videoTexture;
uniform float time;
uniform float slice_density;
uniform float slice_speed;
uniform float glitch_active;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }
void main() {
    vec2 uv = TexCoord;
    if (glitch_active > 0.5) {
        float sliceVal = sin(uv.y * slice_density + time * slice_speed);
        if (sliceVal > 0.8) {
            uv.x += hash(vec2(uv.y, time)) * 0.1 - 0.05;
        }
    }
    vec4 color = texture(videoTexture, uv);
    if (glitch_active > 0.5 && hash(uv + time) > 0.95) {
        color.r = texture(videoTexture, uv + vec2(0.01, 0.0)).r;
        color.b = texture(videoTexture, uv - vec2(0.01, 0.0)).b;
    }
    FragColor = color;
}
)");

    writeFrag("pixelation", R"(#version 330 core
// @name Pixelation
// @desc Pixelates the video screen to simulate low-res retro rendering
// @param pixelSize Pixel Size 1.0 100.0 16.0
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float pixelSize;

void main() {
    float size = max(1.0, pixelSize);
    vec2 sizeUV = size / resolution;
    vec2 uv = floor(TexCoord / sizeUV) * sizeUV + sizeUV * 0.5;
    FragColor = texture(videoTexture, uv);
}
)");
}
