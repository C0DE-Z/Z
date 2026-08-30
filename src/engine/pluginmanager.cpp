#include "pluginmanager.h"
#include <QDir>
#include <QDirIterator>
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

    QDirIterator it(QString::fromStdString(path), QStringList() << "*.frag", QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo fileInfo = it.fileInfo();
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

            QString relDir = dir.relativeFilePath(fileInfo.absolutePath());
            if (relDir != "." && !relDir.isEmpty()) {
                plugin.category = relDir.toStdString();
            }

            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("// @name ")) {
                    plugin.name = line.mid(9).trimmed().toStdString();
                } else if (line.startsWith("// @category ")) {
                    plugin.category = line.mid(13).trimmed().toStdString();
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

    auto writeFrag = [&](const QString& relSubPath, const QString& content) {
        QString fullPath = dir.absoluteFilePath(relSubPath + ".frag");
        QFileInfo info(fullPath);
        if (!info.dir().exists()) {
            info.dir().mkpath(".");
        }
        QFile fragFile(fullPath);
        if (fragFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fragFile);
            out << content;
            fragFile.close();
        }
    };

    // --- Transitions ---
    writeFrag("Transitions/cross_dissolve", R"(#version 330 core
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

    writeFrag("Transitions/datamosh_transition", R"(#version 330 core
// @name Datamosh Transition
// @desc Pixel sorting and offset datamosh effect
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform vec2 resolution;
uniform float progress;

float luma(vec3 color) { return dot(color, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 c2 = texture(videoTexture2, TexCoord);
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

    writeFrag("Transitions/glitch_slide", R"(#version 330 core
// @name Glitch Slide Transition
// @desc Horizontal slice distortion transition
// @param progress Progress 0.0 1.0 0.5
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D videoTexture2;
uniform float progress;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec2 uv1 = TexCoord;
    vec2 uv2 = TexCoord;
    float offset = hash(vec2(floor(TexCoord.y * 30.0), progress)) * 0.2 * sin(progress * 3.14159);
    uv1.x -= progress + offset;
    uv2.x += (1.0 - progress) + offset;
    vec4 c1 = (uv1.x >= 0.0 && uv1.x <= 1.0) ? texture(videoTexture, uv1) : vec4(0.0);
    vec4 c2 = (uv2.x >= 0.0 && uv2.x <= 1.0) ? texture(videoTexture2, uv2) : vec4(0.0);
    FragColor = mix(c1, c2, smoothstep(0.4, 0.6, progress));
}
)");

    // --- Retro Gaming / Xbox 360 ---
    writeFrag("Retro Gaming/Xbox 360/xbox360_blades", R"(#version 330 core
// @name Xbox 360 Blades Dashboard
// @desc Authentic 2005 Xbox 360 Xenon curved blades dashboard UI
// @param bladeIndex Active Blade (0=System 1=Media 2=Games 3=Live) 0.0 3.0 1.0
// @param bladeGlow Xenon Neon Glow 0.0 2.0 1.0
// @param glassReflect Curve Reflection 0.0 1.0 0.5
// @param active Show Blades UI 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float bladeIndex;
uniform float bladeGlow;
uniform float glassReflect;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    vec3 colors[4] = vec3[4](
        vec3(0.5, 0.1, 0.7),
        vec3(0.2, 0.75, 0.2),
        vec3(0.1, 0.5, 0.9),
        vec3(0.95, 0.45, 0.1)
    );
    int activeIdx = int(clamp(bladeIndex, 0.0, 3.0));
    vec3 bladeColor = colors[activeIdx];
    float curveY = 0.5 + sin(uv.x * 3.14159) * 0.15;
    float bladeDist = abs(uv.y - curveY);
    vec3 overlay = vec3(0.0);
    float alpha = 0.0;
    if (uv.y > curveY - 0.08 && uv.y < curveY + 0.08) {
        float edge = smoothstep(0.08, 0.0, bladeDist);
        overlay = mix(bladeColor * 1.5, vec3(1.0), pow(1.0 - bladeDist / 0.08, 3.0) * glassReflect);
        alpha = edge * 0.7;
    }
    vec2 orbPos = vec2(0.85, 0.85);
    float orbDist = length(uv - orbPos);
    if (orbDist < 0.2) {
        float glow = (1.0 - orbDist / 0.2) * bladeGlow;
        overlay += bladeColor * glow * 1.2;
        alpha = max(alpha, glow * 0.5);
    }
    float scanline = sin(uv.y * 400.0) * 0.05;
    overlay += vec3(scanline);
    FragColor = vec4(mix(baseColor.rgb, overlay, alpha), baseColor.a);
}
)");

    writeFrag("Retro Gaming/Xbox 360/xbox360_ring_of_death", R"(#version 330 core
// @name Xbox 360 Red Ring of Death
// @desc 3-Quadrant Red Ring glow overlay with hardware failure overheat distortion
// @param rrodGlow Ring Glow Brightness 0.0 2.0 1.0
// @param overheatFlicker Overheat Signal Flicker 0.0 1.0 0.5
// @param thermalRed Heat Color Shift 0.0 1.0 0.7
// @param active RROD Glitch Active 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float rrodGlow;
uniform float overheatFlicker;
uniform float thermalRed;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    if (overheatFlicker > 0.0 && hash(vec2(floor(uv.y * 40.0), floor(time * 15.0))) > 0.92 - (overheatFlicker * 0.1)) {
        uv.x += (hash(vec2(time)) - 0.5) * 0.08 * overheatFlicker;
    }
    vec4 color = texture(videoTexture, uv);
    if (thermalRed > 0.0) {
        color.r = mix(color.r, color.r * 1.8 + 0.2, thermalRed);
        color.g *= (1.0 - thermalRed * 0.5);
        color.b *= (1.0 - thermalRed * 0.7);
    }
    vec2 pos = (uv - vec2(0.5, 0.5)) * vec2(1.77, 1.0);
    float r = length(pos);
    float angle = atan(pos.y, pos.x);
    if (r > 0.22 && r < 0.32) {
        bool quad1 = (angle >= -0.7 && angle < 0.7);
        bool quad2 = (angle >= 0.8 && angle < 2.3);
        bool quad3 = (angle >= 2.4 || angle < -2.4);
        if (quad1 || quad2 || quad3) {
            float ringIntensity = smoothstep(0.05, 0.0, abs(r - 0.27)) * rrodGlow;
            float flicker = 0.8 + 0.2 * sin(time * 30.0);
            vec3 redColor = vec3(1.0, 0.05, 0.02) * 2.0 * flicker;
            color.rgb = mix(color.rgb, redColor, ringIntensity * 0.8);
        }
    }
    FragColor = color;
}
)");

    // --- Retro Gaming / Rock Band ---
    writeFrag("Retro Gaming/Rock Band/rockband_highway", R"(#version 330 core
// @name Rock Band Note Highway
// @desc 3D perspective scrolling note track with 5 colorful lanes
// @param speed Highway Speed 0.1 5.0 1.5
// @param laneGlow Highway Glow 0.0 2.0 1.0
// @param perspective Track Perspective 0.1 1.0 0.5
// @param active Enable Highway 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float speed;
uniform float laneGlow;
uniform float perspective;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    vec2 p = uv - vec2(0.5, 0.0);
    if (p.y < 0.01) { FragColor = baseColor; return; }
    vec2 trackUV;
    trackUV.x = p.x / p.y + 0.5;
    trackUV.y = fract(1.0 / p.y * 0.2 + time * speed);
    if (trackUV.x >= 0.1 && trackUV.x <= 0.9 && uv.y < 0.65) {
        float laneWidth = 0.8 / 5.0;
        int laneIdx = int(clamp((trackUV.x - 0.1) / laneWidth, 0.0, 4.0));
        vec3 laneColors[5] = vec3[5](
            vec3(0.1, 0.9, 0.2),
            vec3(0.9, 0.1, 0.15),
            vec3(0.95, 0.9, 0.1),
            vec3(0.15, 0.4, 0.95),
            vec3(0.95, 0.5, 0.1)
        );
        vec3 laneCol = laneColors[laneIdx];
        float lanePos = fract((trackUV.x - 0.1) / laneWidth);
        float lineBorder = smoothstep(0.05, 0.0, abs(lanePos - 0.0)) + smoothstep(0.05, 0.0, abs(lanePos - 1.0));
        float gem = smoothstep(0.15, 0.05, abs(trackUV.y - 0.5));
        vec3 trackColor = mix(vec3(0.05, 0.05, 0.08), laneCol * 0.4, 0.3);
        trackColor += vec3(lineBorder) * 0.5;
        trackColor += laneCol * gem * 1.5 * laneGlow;
        float alpha = (1.0 - uv.y / 0.65) * 0.85;
        baseColor.rgb = mix(baseColor.rgb, trackColor, alpha);
    }
    FragColor = baseColor;
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_overdrive", R"(#version 330 core
// @name Rock Band Overdrive
// @desc Electric lightning bolts and golden energy aura pulse
// @param intensity Overdrive Gold Brightness 0.0 2.0 1.0
// @param lightning Electric Lightning Bolts 0.0 1.0 0.7
// @param speed Energy Pulse Speed 0.0 5.0 2.0
// @param active Overdrive Active 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float intensity;
uniform float lightning;
uniform float speed;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float pulse = sin(time * speed * 4.0) * 0.15 + 0.85;
    vec3 goldColor = vec3(1.0, 0.85, 0.2) * intensity * pulse;
    float bolt = 0.0;
    if (lightning > 0.0) {
        float n = noise(vec2(uv.x * 10.0, time * 8.0 * speed));
        float boltLine = abs(uv.y - (n * 0.4 + 0.3));
        bolt = smoothstep(0.03, 0.0, boltLine) * lightning;
    }
    vec3 finalColor = mix(baseColor.rgb, baseColor.rgb * goldColor + vec3(0.2, 0.15, 0.0), 0.4 * intensity);
    finalColor += vec3(0.9, 0.95, 1.0) * bolt;
    FragColor = vec4(finalColor, baseColor.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband1_venue_lights", R"(#version 330 core
// @name Rock Band 1 Venue Lighting
// @desc Stage spotlights, smoke haze, and pulse strobes from Rock Band 1
// @param lightSpeed Light Sweep Speed 0.1 5.0 1.0
// @param haze Smoke Haze 0.0 1.0 0.4
// @param strobe Strobe Intensity 0.0 2.0 1.0
// @param active Enable Venue Lights 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float lightSpeed;
uniform float haze;
uniform float strobe;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float t = time * lightSpeed;
    vec3 c1 = vec3(0.9, 0.1, 0.4) * max(0.0, sin(uv.x * 6.0 + t * 2.0));
    vec3 c2 = vec3(0.1, 0.4, 0.9) * max(0.0, cos(uv.x * 5.0 - t * 1.5));
    vec3 c3 = vec3(0.6, 0.1, 0.9) * max(0.0, sin((uv.x + uv.y) * 4.0 + t));
    vec3 spotLights = (c1 + c2 + c3) * (1.0 - uv.y * 0.7);
    float strobeFlash = pow(max(0.0, sin(t * 12.0)), 8.0) * strobe;
    vec3 finalCol = baseColor.rgb + spotLights * 0.5 + vec3(strobeFlash * 0.4);
    if (haze > 0.0) {
        float fog = sin(uv.x * 10.0 + t) * cos(uv.y * 8.0 - t * 0.5) * 0.1 * haze;
        finalCol += vec3(fog);
    }
    FragColor = vec4(finalCol, baseColor.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband2_multiplier_streak", R"(#version 330 core
// @name Rock Band 2 4x Multiplier Streak
// @desc Signature 4x score streak gold/purple aura border flare from Rock Band 2
// @param streakGlow Multiplier Glow 0.0 3.0 1.5
// @param pulseBass Bass Groove Pulse 0.0 2.0 1.0
// @param active Enable 4x Streak 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float streakGlow;
uniform float pulseBass;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float edgeDist = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float border = smoothstep(0.15, 0.0, edgeDist);
    float pulse = sin(time * 6.0) * 0.2 + 0.8;
    vec3 purpleGold = mix(vec3(0.7, 0.1, 0.9), vec3(1.0, 0.85, 0.2), sin(time * 3.0) * 0.5 + 0.5);
    vec3 glow = purpleGold * border * streakGlow * pulse;
    FragColor = vec4(baseColor.rgb + glow, baseColor.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband3_keys_harmonies", R"(#version 330 core
// @name Rock Band 3 Keyboard & Vocal Harmonies
// @desc 25-key keyboard track highway and triple vocal harmony lines from Rock Band 3
// @param speed Key Track Speed 0.1 5.0 1.5
// @param harmonyLines Vocal Harmonies Glow 0.0 2.0 1.0
// @param active Enable Keys & Harmonies 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float speed;
uniform float harmonyLines;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    vec3 col = baseColor.rgb;
    if (harmonyLines > 0.0 && uv.y > 0.85) {
        float line1 = smoothstep(0.01, 0.0, abs(uv.y - 0.95 - sin(uv.x * 20.0 + time * 4.0) * 0.01));
        float line2 = smoothstep(0.01, 0.0, abs(uv.y - 0.91 - cos(uv.x * 15.0 - time * 3.0) * 0.01));
        float line3 = smoothstep(0.01, 0.0, abs(uv.y - 0.87 - sin(uv.x * 25.0 + time * 5.0) * 0.01));
        col += vec3(0.1, 0.8, 1.0) * line1 * harmonyLines;
        col += vec3(1.0, 0.8, 0.1) * line2 * harmonyLines;
        col += vec3(0.9, 0.2, 0.8) * line3 * harmonyLines;
    }
    FragColor = vec4(col, baseColor.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_solo_button_masher", R"(#version 330 core
// @name Rock Band Solo & Big Rock Ending
// @desc Solo section spotlight aura and Big Rock Ending (BRE) pyro explosions
// @param soloGlow Solo Spotlight 0.0 3.0 1.5
// @param pyroSparks Pyro Sparks 0.0 2.0 1.0
// @param active Enable Solo FX 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float soloGlow;
uniform float pyroSparks;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float dist = length(uv - vec2(0.5, 0.5));
    float spot = (1.0 - smoothstep(0.2, 0.5, dist)) * soloGlow;
    vec3 spotCol = vec3(0.2, 0.7, 1.0) * spot;
    vec3 sparkCol = vec3(0.0);
    if (pyroSparks > 0.0 && hash(uv + time) > 0.94) {
        sparkCol = vec3(1.0, 0.9, 0.4) * pyroSparks * 2.0;
    }
    FragColor = vec4(baseColor.rgb + spotCol + sparkCol, baseColor.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_fail_screen", R"(#version 330 core
// @name Rock Band Track Fail
// @desc Red fail warning vignette and amp feedback static distortion
// @param failRed Fail Warning Red 0.0 1.0 0.8
// @param ampStatic Amp Distortion 0.0 1.0 0.4
// @param active Enable Fail FX 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float failRed;
uniform float ampStatic;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 uv = TexCoord;
    float edge = length(uv - vec2(0.5, 0.5));
    float vignette = smoothstep(0.3, 0.7, edge) * failRed;
    vec3 col = mix(baseColor.rgb, vec3(0.9, 0.05, 0.05), vignette * 0.7);
    if (ampStatic > 0.0) {
        float noise = hash(uv + time) * ampStatic * 0.25;
        col += vec3(noise, noise * 0.2, noise * 0.2);
    }
    FragColor = vec4(col, baseColor.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_bw_ink", R"(#version 330 core
// @name Rock Band B&W Ink & XOR
// @desc High-contrast B&W ink threshold with bitwise XOR channel inversion from Rock Band music videos
// @param contrast Contrast Threshold 0.5 5.0 2.5
// @param xorInvert XOR Channel Invert 0.0 1.0 0.0 bool
// @param active Enable B&W Ink 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float contrast;
uniform float xorInvert;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    float luma = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    float bw = smoothstep(0.5 - 0.5 / contrast, 0.5 + 0.5 / contrast, luma);
    vec3 col = vec3(bw);
    if (xorInvert > 0.5) {
        uvec3 iCol = uvec3(col * 255.0);
        uvec3 xorMask = uvec3(128u, 64u, 255u);
        iCol = iCol ^ xorMask;
        col = vec3(iCol) / 255.0;
    }
    FragColor = vec4(col, orig.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_sepia_film", R"(#version 330 core
// @name Rock Band Sepia 16mm Film
// @desc Vintage 16mm warm sepia film filter with vertical scratches and gate flicker
// @param sepiaWarmth Sepia Tone 0.0 1.0 0.8
// @param scratches Film Scratches 0.0 1.0 0.4
// @param flicker Gate Flicker 0.0 1.0 0.25
// @param active Enable Sepia Film 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float sepiaWarmth;
uniform float scratches;
uniform float flicker;
uniform float active;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec2 uv = TexCoord;
    float gray = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    vec3 sepia = vec3(gray * 1.2, gray * 0.9, gray * 0.6);
    vec3 color = mix(orig.rgb, sepia, sepiaWarmth);
    if (scratches > 0.0) {
        float scratchX = hash(vec2(floor(time * 20.0), 1.0));
        if (abs(uv.x - scratchX) < 0.002 * scratches) {
            color += vec3(0.4 * scratches);
        }
    }
    if (flicker > 0.0) {
        float f = (hash(vec2(time * 30.0, 0.0)) - 0.5) * 0.2 * flicker;
        color += vec3(f);
    }
    FragColor = vec4(color, orig.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_pop_art", R"(#version 330 core
// @name Rock Band Pop-Art & Ink Outlines
// @desc Posterized color palette reduction with heavy black ink outlines
// @param levels Color Levels 2.0 16.0 4.0
// @param inkLines Ink Outlines 0.0 2.0 1.0
// @param active Enable Pop Art 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float levels;
uniform float inkLines;
uniform float active;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec3 col = floor(orig.rgb * levels) / levels;
    if (inkLines > 0.0) {
        vec2 step = 1.0 / max(vec2(1.0), resolution);
        float l = luma(texture(videoTexture, TexCoord - vec2(step.x, 0.0)).rgb);
        float r = luma(texture(videoTexture, TexCoord + vec2(step.x, 0.0)).rgb);
        float u = luma(texture(videoTexture, TexCoord - vec2(0.0, step.y)).rgb);
        float d = luma(texture(videoTexture, TexCoord + vec2(0.0, step.y)).rgb);
        float edge = length(vec2(r - l, d - u)) * inkLines;
        if (edge > 0.2) {
            col = vec3(0.0);
        }
    }
    FragColor = vec4(col, orig.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_acid_wash", R"(#version 330 core
// @name Rock Band Acid Wash & Solarization
// @desc Psychedelic solarized color inversion and neon halo edge bleed
// @param solarize Solarization 0.0 1.0 0.7
// @param hueShift Hue Shift 0.0 3.14 1.57
// @param active Enable Acid Wash 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float solarize;
uniform float hueShift;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec3 col = orig.rgb;
    if (solarize > 0.0) {
        col = abs(col - vec3(solarize));
    }
    float angle = hueShift;
    mat3 hueMat = mat3(
        0.213 + 0.787 * cos(angle) - 0.213 * sin(angle),
        0.213 - 0.213 * cos(angle) + 0.143 * sin(angle),
        0.213 - 0.213 * cos(angle) - 0.787 * sin(angle),
        0.715 - 0.715 * cos(angle) - 0.715 * sin(angle),
        0.715 + 0.285 * cos(angle) + 0.140 * sin(angle),
        0.715 - 0.715 * cos(angle) + 0.715 * sin(angle),
        0.072 - 0.072 * cos(angle) + 0.928 * sin(angle),
        0.072 - 0.072 * cos(angle) - 0.283 * sin(angle),
        0.072 + 0.928 * cos(angle) + 0.072 * sin(angle)
    );
    col = clamp(hueMat * col, 0.0, 1.0);
    FragColor = vec4(col, orig.a);
}
)");

    writeFrag("Retro Gaming/Rock Band/rockband_duotone", R"(#version 330 core
// @name Rock Band Duotone Video
// @desc Maps video luminance onto a 2-color rock venue gradient (Midnight Blue/Gold, Crimson/Cyan, Purple/Lime)
// @param preset Palette Preset (0=Blue/Gold 1=Crimson/Cyan 2=Purple/Lime) 0.0 2.0 0.0
// @param contrast Duotone Contrast 0.5 3.0 1.5
// @param active Enable Duotone 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float preset;
uniform float contrast;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    float gray = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    gray = clamp(pow(gray, contrast), 0.0, 1.0);
    vec3 colA = vec3(0.05, 0.1, 0.35);
    vec3 colB = vec3(1.0, 0.8, 0.15);
    int p = int(clamp(preset, 0.0, 2.0));
    if (p == 1) {
        colA = vec3(0.85, 0.05, 0.1);
        colB = vec3(0.1, 0.95, 0.9);
    } else if (p == 2) {
        colA = vec3(0.4, 0.05, 0.6);
        colB = vec3(0.4, 0.95, 0.2);
    }
    vec3 finalCol = mix(colA, colB, gray);
    FragColor = vec4(finalCol, orig.a);
}
)");

    // --- Retro Gaming / CRT & VHS ---
    writeFrag("Retro Gaming/CRT & VHS/crt", R"(#version 330 core
// @name CRT Simulation
// @desc Phosphor grid, bloom, and barrel curvature
// @param curvature Tube Curvature 0.0 0.5 0.15
// @param bloom Bloom Intensity 0.0 2.0 0.5
// @param rgbOffset RGB Shift Offset 0.0 0.02 0.003
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float curvature;
uniform float bloom;
uniform float rgbOffset;

void main() {
    vec2 uv = TexCoord;
    if (curvature > 0.0) {
        uv = uv - 0.5;
        float dist = dot(uv, uv);
        uv = uv * (1.0 + curvature * dist + curvature * 0.5 * dist * dist);
        uv = uv + 0.5;
    }
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0); return;
    }
    float r = texture(videoTexture, uv - vec2(rgbOffset, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(rgbOffset, 0.0)).b;
    vec3 color = vec3(r, g, b);
    float scanline = sin(uv.y * 800.0) * 0.15;
    color -= vec3(scanline);
    if (bloom > 0.0) {
        vec3 blurColor = texture(videoTexture, uv + vec2(0.005, 0.005)).rgb;
        blurColor += texture(videoTexture, uv - vec2(0.005, 0.005)).rgb;
        color += blurColor * 0.12 * bloom;
    }
    FragColor = vec4(color, texture(videoTexture, uv).a);
}
)");

    writeFrag("Retro Gaming/CRT & VHS/vhs", R"(#version 330 core
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
    vec4 color = vec4(r, g, b, texture(videoTexture, uv).a);
    if (noise > 0.0 || wear > 0.0) {
        float n = hash(uv + time);
        color.rgb += vec3(n * 0.15 * noise);
    }
    FragColor = color;
}
)");

    // --- Glitch & Datamosh ---
    writeFrag("Glitch & Datamosh/datamosh", R"(#version 330 core
// @name Datamosh (CPU)
// @desc FFmpeg-level P-frame and I-frame corruption
// @param iDrop I-Frame Drop Toggle 0.0 1.0 0.0 bool
// @param pDup P-Frame Duplicate Toggle 0.0 1.0 0.0 bool
// @param pDupCount Duplication Count 1.0 60.0 4.0
// @param pDrop P-Frame Drop Toggle 0.0 1.0 0.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
)");

    writeFrag("Glitch & Datamosh/pixel_sorter", R"(#version 330 core
// @name Pixel Sorter
// @desc Luma-based directional pixel sorting glitch
// @param threshold Luma Threshold 0.0 1.0 0.45
// @param sortLength Streak Length 0.0 1.0 0.5
// @param active Enable Pixel Sorter 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float threshold;
uniform float sortLength;
uniform float active;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    vec2 uv = TexCoord;
    float currentLuma = luma(orig.rgb);
    if (currentLuma > threshold) {
        float stepY = 1.0 / max(1.0, resolution.y);
        float offset = floor(currentLuma * sortLength * 50.0) * stepY;
        uv.y = clamp(uv.y - offset, 0.0, 1.0);
    }
    FragColor = texture(videoTexture, uv);
}
)");

    writeFrag("Glitch & Datamosh/bent", R"(#version 330 core
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
        uv.x = fract(uv.x + drift);
    }
    vec4 color = texture(videoTexture, uv);
    FragColor = color;
}
)");

    writeFrag("Glitch & Datamosh/xor_gate", R"(#version 330 core
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
    vec4 color = texture(videoTexture, uv);
    uvec3 iColor = uvec3(color.rgb * 255.0);
    uvec3 mask = uvec3(uint(xorR), uint(xorG), uint(xorB));
    iColor = iColor ^ mask;
    FragColor = vec4(vec3(iColor) / 255.0, color.a);
}
)");

    writeFrag("Glitch & Datamosh/digital_noise", R"(#version 330 core
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
    FragColor = color;
}
)");

    // --- Stylize & FX ---
    writeFrag("Stylize & FX/optical_smear", R"(#version 330 core
// @name Optical Smear
// @desc Motion blur and color smear trail
// @param frameMerge Frame Merge 0.0 1.0 0.25
// @param frameSmear Frame Smear 0.0 1.0 0.1
// @param colorBleed Color Bleed 0.0 1.0 0.25
// @param lumaBias Luma Bias 0.0 1.0 0.2
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float frameMerge;

void main() {
    vec4 c = texture(videoTexture, TexCoord);
    FragColor = c;
}
)");

    writeFrag("Stylize & FX/temporal_echo", R"(#version 330 core
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

    writeFrag("Stylize & FX/milkdrop", R"(#version 330 core
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
    vec4 color = texture(videoTexture, uv);
    FragColor = color;
}
)");

    writeFrag("Stylize & FX/pixelation", R"(#version 330 core
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

    writeFrag("Stylize & FX/cyberpunk_neon", R"(#version 330 core
// @name Cyberpunk Neon Glow
// @desc Edge detection neon glow with synthwave palette grading
// @param neonGlow Neon Edge Brightness 0.0 3.0 1.5
// @param scanline Scanline Grid 0.0 1.0 0.3
// @param active Enable Cyberpunk 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float neonGlow;
uniform float scanline;
uniform float active;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    vec2 step = 1.0 / max(vec2(1.0), resolution);
    float l = luma(texture(videoTexture, TexCoord - vec2(step.x, 0.0)).rgb);
    float r = luma(texture(videoTexture, TexCoord + vec2(step.x, 0.0)).rgb);
    float u = luma(texture(videoTexture, TexCoord - vec2(0.0, step.y)).rgb);
    float d = luma(texture(videoTexture, TexCoord + vec2(0.0, step.y)).rgb);
    float edge = length(vec2(r - l, d - u)) * neonGlow;
    vec3 cyan = vec3(0.0, 0.9, 1.0);
    vec3 magenta = vec3(1.0, 0.0, 0.7);
    vec3 neonColor = mix(cyan, magenta, TexCoord.x) * edge;
    vec3 graded = mix(baseColor.rgb * vec3(0.6, 0.5, 0.8), neonColor, clamp(edge, 0.0, 1.0));
    if (scanline > 0.0) {
        float grid = sin(TexCoord.y * resolution.y * 0.5) * 0.1 * scanline;
        graded -= vec3(grid);
    }
    FragColor = vec4(graded, baseColor.a);
}
)");

    writeFrag("Stylize & FX/thermal_vision", R"(#version 330 core
// @name Thermal Vision
// @desc Infrared heat map color palette simulation
// @param heatContrast Contrast 0.5 3.0 1.2
// @param active Enable Thermal 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float heatContrast;
uniform float active;

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }
    float val = dot(orig.rgb, vec3(0.299, 0.587, 0.114));
    val = clamp(pow(val, heatContrast), 0.0, 1.0);
    vec3 c1 = vec3(0.0, 0.0, 0.2);
    vec3 c2 = vec3(0.4, 0.0, 0.6);
    vec3 c3 = vec3(0.9, 0.1, 0.1);
    vec3 c4 = vec3(1.0, 0.8, 0.0);
    vec3 c5 = vec3(1.0, 1.0, 1.0);
    vec3 thermal;
    if (val < 0.25) { thermal = mix(c1, c2, val / 0.25); }
    else if (val < 0.5) { thermal = mix(c2, c3, (val - 0.25) / 0.25); }
    else if (val < 0.75) { thermal = mix(c3, c4, (val - 0.5) / 0.25); }
    else { thermal = mix(c4, c5, (val - 0.75) / 0.25); }
    FragColor = vec4(thermal, orig.a);
}
)");

    // --- Legacy CPU ---
    writeFrag("Legacy CPU/cpu_and", R"(#version 330 core
// @name Modern Hardware Bitwise AND
// @desc Bitwise AND color channel masking on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 240.0
// @param maskG Green Mask (0-255) 0.0 255.0 255.0
// @param maskB Blue Mask (0-255) 0.0 255.0 128.0
// @param mixRatio Effect Blend 0.0 1.0 1.0
// @param active Enable AND Gate 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float maskR;
uniform float maskG;
uniform float maskB;
uniform float mixRatio;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    uvec3 intColor = uvec3(clamp(baseColor.rgb * 255.0, 0.0, 255.0));
    uvec3 andMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 resultInt = intColor & andMask;
    vec3 resultRGB = vec3(resultInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
)");

    writeFrag("Legacy CPU/cpu_or", R"(#version 330 core
// @name Modern Hardware Bitwise OR
// @desc Bitwise OR color channel saturation on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 64.0
// @param maskG Green Mask (0-255) 0.0 255.0 0.0
// @param maskB Blue Mask (0-255) 0.0 255.0 128.0
// @param mixRatio Effect Blend 0.0 1.0 1.0
// @param active Enable OR Gate 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float maskR;
uniform float maskG;
uniform float maskB;
uniform float mixRatio;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    uvec3 intColor = uvec3(clamp(baseColor.rgb * 255.0, 0.0, 255.0));
    uvec3 orMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 resultInt = intColor | orMask;
    vec3 resultRGB = vec3(resultInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
)");

    writeFrag("Legacy CPU/cpu_nand", R"(#version 330 core
// @name Modern Hardware Bitwise NAND
// @desc Bitwise NOT-AND inverted channel corruption on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 192.0
// @param maskG Green Mask (0-255) 0.0 255.0 128.0
// @param maskB Blue Mask (0-255) 0.0 255.0 255.0
// @param mixRatio Effect Blend 0.0 1.0 0.8
// @param active Enable NAND Gate 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float maskR;
uniform float maskG;
uniform float maskB;
uniform float mixRatio;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    uvec3 intColor = uvec3(clamp(baseColor.rgb * 255.0, 0.0, 255.0));
    uvec3 andMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 nandInt = (~(intColor & andMask)) & 255u;
    vec3 resultRGB = vec3(nandInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
)");

    writeFrag("Legacy CPU/cpu_xnor", R"(#version 330 core
// @name Modern Hardware Bitwise XNOR
// @desc Bitwise NOT-XOR channel equivalence glitch on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 128.0
// @param maskG Green Mask (0-255) 0.0 255.0 64.0
// @param maskB Blue Mask (0-255) 0.0 255.0 32.0
// @param mixRatio Effect Blend 0.0 1.0 0.8
// @param active Enable XNOR Gate 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float maskR;
uniform float maskG;
uniform float maskB;
uniform float mixRatio;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    uvec3 intColor = uvec3(clamp(baseColor.rgb * 255.0, 0.0, 255.0));
    uvec3 xorMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 xnorInt = (~(intColor ^ xorMask)) & 255u;
    vec3 resultRGB = vec3(xnorInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
)");

    writeFrag("Legacy CPU/cpu_xor", R"(#version 330 core
// @name Modern Hardware Bitwise XOR
// @desc Bitwise XOR channel inversion glitch on full RGB video (no graying)
// @param maskR Red Mask (0-255) 0.0 255.0 128.0
// @param maskG Green Mask (0-255) 0.0 255.0 64.0
// @param maskB Blue Mask (0-255) 0.0 255.0 255.0
// @param mixRatio Effect Blend 0.0 1.0 1.0
// @param active Enable XOR Gate 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float maskR;
uniform float maskG;
uniform float maskB;
uniform float mixRatio;
uniform float active;

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = baseColor; return; }
    uvec3 intColor = uvec3(clamp(baseColor.rgb * 255.0, 0.0, 255.0));
    uvec3 xorMask = uvec3(uint(maskR), uint(maskG), uint(maskB));
    uvec3 xorInt = intColor ^ xorMask;
    vec3 resultRGB = vec3(xorInt) / 255.0;
    vec3 finalRGB = mix(baseColor.rgb, resultRGB, mixRatio);
    FragColor = vec4(finalRGB, baseColor.a);
}
)");
}
