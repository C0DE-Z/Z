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
// @desc Real H.264 packet Datamosh (configure a non-zero packet action to activate)
// @param iDrop I-Frame Drop Toggle 0.0 1.0 0.0 bool
// @param pDup P-Frame Duplicate Chance 0.0 1.0 0.0
// @param pDupCount Duplication Count 1.0 60.0 1.0
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

    writeFrag("Stylize & FX/watch_dogs_2_still", R"(#version 330 core
// @name Watch Dogs 2 (Still)
// @desc ctOS-style hacked-feed still: block displacement, RGB split, scanlines and data grid
// @param blockSize Glitch Block Size 8.0 128.0 48.0
// @param glitchAmount Block Displacement 0.0 1.0 0.5
// @param rgbSplit RGB Split 0.0 0.05 0.012
// @param scanline Scanlines 0.0 1.0 0.4
// @param grid Data Grid 0.0 1.0 0.3
// @param tint ctOS Tint 0.0 1.0 0.5
// @param seed Glitch Seed 0.0 100.0 7.0
// @param active Enable Watch Dogs 2 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float blockSize;
uniform float glitchAmount;
uniform float rgbSplit;
uniform float scanline;
uniform float grid;
uniform float tint;
uniform float seed;
uniform float active;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.13) * 43758.5453);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }

    vec2 px = TexCoord * resolution;
    vec2 cell = floor(px / max(blockSize, 1.0));
    float rowPick = hash(vec2(0.0, cell.y));
    float cellPick = hash(cell);
    float glitchRow = step(1.0 - glitchAmount * 0.35, rowPick);
    float glitchCell = step(1.0 - glitchAmount * 0.15, cellPick);

    vec2 uv = TexCoord;
    uv.x += glitchRow * (rowPick - 0.5) * 0.12 * glitchAmount;
    uv += glitchCell * (vec2(cellPick, hash(cell + 3.7)) - 0.5) * 0.04 * glitchAmount;
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    float split = rgbSplit * (1.0 + glitchRow * 3.0);
    float r = texture(videoTexture, clamp(uv + vec2(split, 0.0), 0.0, 1.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, clamp(uv - vec2(split, 0.0), 0.0, 1.0)).b;
    vec3 col = vec3(r, g, b);

    float l = dot(col, vec3(0.299, 0.587, 0.114));
    vec3 ctos = mix(vec3(0.02, 0.10, 0.16), vec3(0.35, 0.95, 1.0), l);
    col = mix(col, ctos * 1.1 + col * 0.15, tint);
    col = mix(col, vec3(col.r * 1.4, col.g * 0.4, col.b * 1.2), glitchCell * 0.5 * glitchAmount);

    float lines = 0.5 + 0.5 * sin(px.y * 3.14159);
    col *= 1.0 - scanline * 0.35 * lines;

    vec2 g2 = abs(fract(px / 32.0) - 0.5);
    float gridLine = 1.0 - smoothstep(0.0, 0.03, min(g2.x, g2.y));
    col += vec3(0.1, 0.8, 1.0) * gridLine * grid * 0.35;

    FragColor = vec4(clamp(col, 0.0, 1.0), orig.a);
}
)");

    writeFrag("Stylize & FX/dither_boy_still", R"(#version 330 core
// @name Dither Boy (Still)
// @desc Ordered Bayer dither with a limited retro palette, like a handheld-console still
// @param ditherScale Pixel Size 1.0 16.0 3.0
// @param levels Color Levels 2.0 8.0 2.0
// @param strength Dither Strength 0.0 1.0 1.0
// @param palette Palette (0=Mono 1=Green LCD 2=Amber 3=Color) 0.0 3.0 1.0
// @param contrast Contrast 0.5 2.5 1.1
// @param active Enable Dither Boy 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float ditherScale;
uniform float levels;
uniform float strength;
uniform float palette;
uniform float contrast;
uniform float active;

float bayer4(ivec2 p) {
    const int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[(p.y & 3) * 4 + (p.x & 3)]) + 0.5) / 16.0;
}

vec3 shade(float v, int pal) {
    if (pal == 0) return vec3(v);
    if (pal == 1) return mix(vec3(0.06, 0.22, 0.06), vec3(0.61, 0.74, 0.06), v);
    return mix(vec3(0.15, 0.05, 0.0), vec3(1.0, 0.69, 0.1), v);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }

    float size = max(ditherScale, 1.0);
    vec2 snapped = (floor(TexCoord * resolution / size) + 0.5) * size / resolution;
    vec3 src = texture(videoTexture, clamp(snapped, 0.0, 1.0)).rgb;
    src = clamp((src - 0.5) * contrast + 0.5, 0.0, 1.0);

    float steps = max(levels, 2.0) - 1.0;
    float threshold = (bayer4(ivec2(floor(TexCoord * resolution / size))) - 0.5) * strength;
    int pal = int(palette + 0.5);

    vec3 outCol;
    if (pal >= 3) {
        outCol = floor(clamp(src + threshold / steps, 0.0, 1.0) * steps + 0.5) / steps;
    } else {
        float l = dot(src, vec3(0.299, 0.587, 0.114));
        l = floor(clamp(l + threshold / steps, 0.0, 1.0) * steps + 0.5) / steps;
        outCol = shade(l, pal);
    }
    FragColor = vec4(outCol, orig.a);
}
)");

    writeFrag("Stylize & FX/ascii_art", R"(#version 330 core
// @name ASCII Art
// @desc Renders the video as text symbols, Watch Dogs style. Pick a symbol set, or set Symbol Set to 5 and choose one glyph: 1 . 2 : 3 - 4 = 5 + 6 * 7 # 8 % 9 @ 10 zero 11 one 12 X 13 slash 14 backslash 15 pipe 16 O 17 light 18 mid 19 full block
// @param cellSize Character Size (px) 4.0 40.0 10.0
// @param symbolSet Symbol Set (0=Classic 1=Binary 2=Blocks 3=Lines 4=Matrix 5=Single) 0.0 5.0 0.0
// @param glyphChoice Single Glyph Index 1.0 19.0 7.0
// @param colorMode Color (0=Green 1=White 2=Source 3=Amber 4=Cyan) 0.0 4.0 2.0
// @param brightness Brightness 0.2 3.0 1.0
// @param contrast Contrast 0.5 3.0 1.3
// @param invert Invert Density 0.0 1.0 0.0 bool
// @param background Show Video Behind 0.0 1.0 0.0
// @param active Enable ASCII 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float cellSize;
uniform float symbolSet;
uniform float glyphChoice;
uniform float colorMode;
uniform float brightness;
uniform float contrast;
uniform float invert;
uniform float background;
uniform float active;

int G(int a, int b, int c, int d, int e) {
    return a | (b << 5) | (c << 10) | (d << 15) | (e << 20);
}

int glyphBits(int idx) {
    switch (idx) {
        case 0:  return 0;
        case 1:  return G(0, 0, 0, 0, 4);
        case 2:  return G(0, 4, 0, 4, 0);
        case 3:  return G(0, 0, 14, 0, 0);
        case 4:  return G(0, 14, 0, 14, 0);
        case 5:  return G(4, 4, 31, 4, 4);
        case 6:  return G(21, 14, 31, 14, 21);
        case 7:  return G(10, 31, 10, 31, 10);
        case 8:  return G(25, 2, 4, 8, 19);
        case 9:  return G(14, 21, 23, 16, 14);
        case 10: return G(14, 19, 21, 25, 14);
        case 11: return G(4, 12, 4, 4, 14);
        case 12: return G(17, 10, 4, 10, 17);
        case 13: return G(1, 2, 4, 8, 16);
        case 14: return G(16, 8, 4, 2, 1);
        case 15: return G(4, 4, 4, 4, 4);
        case 16: return G(14, 17, 17, 17, 14);
        case 17: return G(17, 0, 4, 0, 17);
        case 18: return G(21, 10, 21, 10, 21);
        case 19: return G(31, 31, 31, 31, 31);
        case 20: return G(2, 4, 8, 4, 2);
        case 21: return G(8, 4, 2, 4, 8);
    }
    return 0;
}

// Ten density levels per symbol set, indexes into glyphBits above.
const int SETS[50] = int[50](
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,        // Classic
    0, 1, 11, 11, 10, 10, 10, 11, 7, 19, // Binary
    0, 17, 17, 18, 18, 18, 19, 19, 19, 19, // Blocks
    0, 1, 3, 13, 14, 15, 12, 5, 7, 19,   // Lines
    0, 1, 2, 20, 21, 11, 10, 12, 7, 9    // Matrix
);

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) { FragColor = orig; return; }

    float size = max(cellSize, 4.0);
    vec2 cellCoord = TexCoord * resolution / size;
    vec2 cell = floor(cellCoord);
    vec2 local = fract(cellCoord);
    vec2 center = (cell + 0.5) * size / resolution;
    vec2 d = 0.25 * size / resolution;
    vec3 avg = (texture(videoTexture, clamp(center, 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(d.x, d.y), 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(-d.x, d.y), 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(d.x, -d.y), 0.0, 1.0)).rgb
        + texture(videoTexture, clamp(center + vec2(-d.x, -d.y), 0.0, 1.0)).rgb) / 5.0;

    float l = clamp((luma(avg) - 0.5) * contrast + 0.5, 0.0, 1.0) * brightness;
    l = clamp(l, 0.0, 1.0);
    if (invert > 0.5) l = 1.0 - l;
    int level = int(min(l * 10.0, 9.0));

    int setIdx = clamp(int(symbolSet + 0.5), 0, 5);
    int glyph;
    if (setIdx == 5) {
        glyph = level == 0 ? 0 : int(glyphChoice + 0.5);
    } else {
        glyph = SETS[setIdx * 10 + level];
    }

    // 5x5 bitmap inside the cell with a one-subpixel gutter around it.
    vec2 g = (local - 0.1) / 0.8;
    float on = 0.0;
    if (g.x >= 0.0 && g.x < 1.0 && g.y >= 0.0 && g.y < 1.0) {
        int gx = int(g.x * 5.0);
        int gy = 4 - int(g.y * 5.0);
        int row = (glyphBits(glyph) >> (gy * 5)) & 31;
        on = float((row >> (4 - gx)) & 1);
    }

    int cm = int(colorMode + 0.5);
    vec3 ink;
    float glow = 0.35 + 0.65 * l;
    if (cm == 0) ink = vec3(0.15, 1.0, 0.3) * glow;
    else if (cm == 1) ink = vec3(1.0) * glow;
    else if (cm == 2) ink = clamp(avg * 1.6, 0.0, 1.0);
    else if (cm == 3) ink = vec3(1.0, 0.69, 0.1) * glow;
    else ink = vec3(0.2, 0.9, 1.0) * glow;

    vec3 paper = mix(vec3(0.0), orig.rgb * 0.5, background);
    vec3 outCol = mix(paper, ink, on);
    FragColor = vec4(outCol, orig.a);
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

    // --- Analog & FPV ---
    writeFrag("Analog & FPV/fpv_analog_feed", R"(#version 330 core
// @name FPV Analog Video Feed
// @desc 5.8GHz analog drone video feed simulation with RF multipath and motion smear
// @param signalStrength Signal Strength 0.0 1.0 0.9
// @param channelNoise Adjacent Channel Noise 0.0 1.0 0.1
// @param multipath Multipath Interference 0.0 1.0 0.0
// @param analogLatency Motion Smear (Latency Sim) 0.0 0.5 0.05
// @param lensCurve Camera Lens Distortion 0.0 0.3 0.05
// @param exposurePump Auto-Exposure Pump 0.0 1.0 0.1
// @param active Enable FPV Feed 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D feedbackTexture;
uniform vec2 resolution;
uniform float time;
uniform float signalStrength;
uniform float channelNoise;
uniform float multipath;
uniform float analogLatency;
uniform float lensCurve;
uniform float exposurePump;
uniform float active;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    // 1. Camera lens barrel distortion (FPV wide-angle lens)
    vec2 centered = TexCoord - 0.5;
    float r2 = dot(centered, centered);
    vec2 lensUV = TexCoord + centered * (r2 * lensCurve * 2.0);

    // 2. Multipath interference (RF signal cancellation behind obstacles)
    float mpFlutter = sin(time * 37.0) * cos(time * 19.0 + TexCoord.y * 15.0);
    float mpBurst = step(0.85, hash11(floor(time * 14.0))) * (0.5 + 0.5 * sin(time * 50.0));
    float mpPenalty = multipath * clamp(mpFlutter * 0.35 + mpBurst * 0.65, 0.0, 1.0);
    float effSignal = clamp(signalStrength - mpPenalty, 0.0, 1.0);
    float loss = 1.0 - effSignal;

    // 3. Horizontal tearing and sync loss
    float scanLine = floor(lensUV.y * 280.0);
    float lineRand = hash12(vec2(scanLine, floor(time * 60.0)));
    float lineTear = step(1.0 - smoothstep(0.2, 0.7, loss) * 0.45, lineRand) * (lineRand - 0.5) * smoothstep(0.2, 0.8, loss) * 0.08;

    // Block breakup in low signal
    vec2 blockCoord = floor(lensUV * vec2(16.0, 9.0));
    float blockRand = hash12(blockCoord + floor(time * 15.0));
    float blockShift = step(1.0 - smoothstep(0.5, 0.9, loss) * 0.5, blockRand) * (blockRand - 0.5) * smoothstep(0.5, 0.9, loss) * 0.15;

    vec2 sampleUV = clamp(lensUV + vec2(lineTear + blockShift, 0.0), 0.0, 1.0);

    // 4. Sample current frame and feedback texture for latency smear
    vec4 curFrame = texture(videoTexture, sampleUV);
    vec4 feedFrame = texture(feedbackTexture, sampleUV);
    vec3 videoCol = mix(curFrame.rgb, feedFrame.rgb, clamp(analogLatency, 0.0, 0.5));

    // 5. Camera exposure pumping and highlight clipping (FPV CMOS sensor AGC)
    float pumpOsc = 1.0 + exposurePump * (sin(time * 2.5) * 0.2 + sin(time * 5.3) * 0.08);
    videoCol *= pumpOsc;
    videoCol = mix(videoCol, clamp(videoCol * 1.3 - 0.05, 0.0, 1.0), exposurePump * 0.35);

    // 6. Rolling horizontal static bars (adjacent channel bleed)
    float barPhase = fract(lensUV.y * 2.5 - time * 0.9);
    float barMask = smoothstep(0.0, 0.12, barPhase) * smoothstep(0.4, 0.18, barPhase);
    float barNoise = hash12(lensUV * 260.0 + fract(time * 43.0));
    float barStatic = barMask * channelNoise;

    // 7. Progressive RF snow and static noise
    float snowFine = hash12(lensUV * 500.0 + fract(time * 73.0));
    float hNoiseLines = (sin(lensUV.y * 600.0 + time * 45.0) * 0.5 + 0.5) * hash11(lensUV.y * 400.0 + floor(time * 60.0));

    // Stage a: horizontal noise lines
    videoCol = mix(videoCol, videoCol + vec3(hNoiseLines * 0.25 - 0.12), smoothstep(0.05, 0.3, loss));

    // Stage b: static snow mixed in
    float snowBlend = smoothstep(0.18, 0.75, loss) * 0.75;
    videoCol = mix(videoCol, vec3(snowFine), snowBlend);

    // Stage c: block snow replacement
    float blockSnow = step(0.35, blockRand) * smoothstep(0.55, 0.95, loss);
    videoCol = mix(videoCol, vec3(snowFine), blockSnow);

    // Adjacent channel rolling bars overlay
    videoCol = mix(videoCol, vec3(barNoise), barStatic * 0.85);

    // Stage d: failsafe territory (mostly snow, brief glimpses of video)
    float failsafeGlimpse = step(0.92, hash11(floor(time * 9.0))) * smoothstep(0.98, 0.8, loss);
    float nearTotalLoss = smoothstep(0.8, 1.0, loss);
    videoCol = mix(videoCol, vec3(snowFine), nearTotalLoss * (1.0 - failsafeGlimpse * 0.35));

    FragColor = vec4(clamp(videoCol, 0.0, 1.0), baseColor.a);
}
)");

    writeFrag("Analog & FPV/tape_dropout", R"(#version 330 core
// @name Analog Tape Dropout
// @desc Magnetic tape oxide loss, helical scan dropouts, edge damage, and head clog
// @param dropoutRate Dropout Frequency 0.0 1.0 0.15
// @param dropoutLength Max Dropout Length 0.0 1.0 0.3
// @param edgeDamage Edge Damage 0.0 1.0 0.1
// @param stickyBurst Sticky Shed Bursts 0.0 1.0 0.0
// @param headClog Head Clog Level 0.0 1.0 0.0
// @param speedWobble Tape Speed Wobble 0.0 1.0 0.0
// @param active Enable Tape Dropout 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float dropoutRate;
uniform float dropoutLength;
uniform float edgeDamage;
uniform float stickyBurst;
uniform float headClog;
uniform float speedWobble;
uniform float active;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    // 1. Sticky shed burst envelope
    float burstCycle = fract(time * 0.35);
    float burstEnv = smoothstep(0.65, 0.8, burstCycle) * (1.0 - smoothstep(0.85, 0.95, burstCycle));
    float activeSticky = stickyBurst * burstEnv;

    // 2. Tape speed wobble (capstan flutter / wow and binder drag)
    float totalWobble = speedWobble + activeSticky * 0.7;
    float flutterY = (sin(time * 30.0) * 0.0015 + sin(time * 6.0) * 0.004) * totalWobble;
    float stretchY = sin(TexCoord.y * 8.0 + time * 3.5) * 0.003 * totalWobble;
    float jitterX = sin(time * 48.0 + TexCoord.y * 110.0) * 0.0012 * totalWobble;
    vec2 uv = TexCoord + vec2(jitterX, flutterY + stretchY);

    // 3. Head clog buildup & sudden dislodgement
    float clogCycle = fract(time * 0.1);
    float clogRamp = smoothstep(0.0, 0.82, clogCycle) * (1.0 - step(0.88, clogCycle));
    float currentClog = headClog * clogRamp;

    // Head clog high-frequency loss (horizontal blurring)
    vec4 sampledColor = texture(videoTexture, uv);
    vec3 procCol = sampledColor.rgb;
    if (currentClog > 0.01) {
        float bSpread = currentClog * 0.012;
        vec3 b1 = texture(videoTexture, uv + vec2(-bSpread, 0.0)).rgb;
        vec3 b2 = texture(videoTexture, uv + vec2(bSpread, 0.0)).rgb;
        vec3 b3 = texture(videoTexture, uv + vec2(-bSpread * 2.0, 0.0)).rgb;
        vec3 b4 = texture(videoTexture, uv + vec2(bSpread * 2.0, 0.0)).rgb;
        procCol = (procCol * 2.0 + b1 + b2 + (b3 + b4) * 0.5) / 5.0;
    }

    // 4. Edge damage weighting (tape edges crinkling near top and bottom)
    float distFromEdge = min(uv.y, 1.0 - uv.y);
    float edgeFactor = 1.0 + edgeDamage * 5.0 * exp(-distFromEdge * 14.0);

    // 5. Helical scanline dropout generation
    float scanline = floor(uv.y * 480.0);
    float frameTick = floor(time * 29.97);
    float effDropRate = clamp(dropoutRate * edgeFactor + activeSticky * 0.45 + currentClog * 0.35, 0.0, 1.0);

    float lineSeed = hash12(vec2(scanline, frameTick));
    float hasDropout = step(1.0 - effDropRate * 0.4, lineSeed);

    float inDropout = 0.0;
    float chromaBleed = 0.0;

    if (hasDropout > 0.5) {
        float startX = hash12(vec2(scanline * 2.71, frameTick * 1.41));
        float dLen = (0.01 + hash12(vec2(scanline * 5.19, frameTick * 3.17)) * dropoutLength) * (1.0 + activeSticky * 1.5);

        if (uv.x >= startX && uv.x <= startX + dLen) {
            inDropout = 1.0;
        }

        // Chroma bleed: horizontal color smear trailing the dropout streak
        float trail = uv.x - (startX + dLen);
        if (trail > 0.0 && trail < 0.08) {
            chromaBleed = 1.0 - trail / 0.08;
        }
    }

    // Dropout clustering on nearby scanlines
    float clusterSeed = hash12(vec2(floor(scanline * 0.5), frameTick * 0.7));
    if (clusterSeed < effDropRate * 0.3) {
        float cStartX = hash12(vec2(scanline * 8.33, frameTick * 4.91));
        float cLen = hash12(vec2(scanline * 11.2, frameTick * 7.13)) * dropoutLength * 0.4;
        if (uv.x >= cStartX && uv.x <= cStartX + cLen) {
            inDropout = 1.0;
        }
    }

    // Head clog dropout bands
    if (currentClog > 0.45) {
        float clogBandY = fract(time * 0.04 + 0.35);
        if (abs(uv.y - clogBandY) < 0.035 * currentClog) {
            float clogNoise = hash12(vec2(floor(uv.x * 50.0), scanline + frameTick));
            if (clogNoise < 0.65) {
                inDropout = 1.0;
            }
        }
    }

    // 6. Render dropout: replacement signal from DOC (previous line) or white/gray RF spike
    if (inDropout > 0.5) {
        vec2 docUV = vec2(uv.x, uv.y - 1.0 / 480.0);
        vec3 docColor = texture(videoTexture, docUV).rgb;
        float whiteSpike = step(0.4, hash12(vec2(floor(uv.x * 240.0), scanline)));
        vec3 streakCol = mix(docColor, vec3(0.92, 0.94, 0.97), whiteSpike * 0.85);
        procCol = streakCol;
    } else if (chromaBleed > 0.0) {
        vec3 smearedCol = texture(videoTexture, uv - vec2(0.025 * chromaBleed, 0.0)).rgb;
        procCol = mix(procCol, smearedCol, chromaBleed * 0.65);
    }

    FragColor = vec4(clamp(procCol, 0.0, 1.0), baseColor.a);
}
)");

    writeFrag("Analog & FPV/signal_loss", R"(#version 330 core
// @name Progressive Signal Loss
// @desc Progressive analog signal degradation from subtle grain to total static loss
// @param distance Signal Loss / Distance 0.0 1.0 0.0
// @param instability Signal Instability 0.0 1.0 0.3
// @param colorFade Color Loss Speed 0.5 3.0 1.5
// @param active Enable Signal Loss 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float distance;
uniform float instability;
uniform float colorFade;
uniform float active;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    // 1. Distance fluctuation from signal instability
    float tFlux = sin(time * 2.8) * 0.45 + sin(time * 7.1) * 0.35 + sin(time * 19.4) * 0.2;
    float effDist = clamp(distance + instability * 0.12 * tFlux, 0.0, 1.0);

    // 2. Horizontal jitter and tearing stages
    float scanline = floor(TexCoord.y * 360.0);
    float frameTick = floor(time * 50.0);

    // Subtle jitter (0.1 - 0.25)
    float minorJitter = (hash12(vec2(scanline, frameTick)) - 0.5) * smoothstep(0.1, 0.25, effDist) * 0.005;

    // Horizontal tearing & displacement (0.25 - 0.6)
    float tearLineSeed = hash12(vec2(scanline * 0.3, floor(time * 25.0)));
    float tearActive = step(1.0 - smoothstep(0.25, 0.6, effDist) * 0.35, tearLineSeed);
    float tearDisplace = (tearLineSeed - 0.5) * tearActive * smoothstep(0.25, 0.6, effDist) * 0.06;

    // Heavy displacement (0.6 - 0.95)
    float heavyDisplace = (hash12(vec2(floor(TexCoord.y * 25.0), floor(time * 18.0))) - 0.5) * smoothstep(0.6, 0.9, effDist) * 0.22;

    float totalDisplaceX = minorJitter + tearDisplace + heavyDisplace;

    // 3. Vertical hold drift and vertical roll (0.4 - 0.8)
    float vDrift = sin(time * 2.0) * smoothstep(0.4, 0.6, effDist) * 0.06;
    float rollSpeed = smoothstep(0.6, 0.8, effDist) * (1.8 + sin(time * 0.6) * 0.4);
    float vRoll = fract(TexCoord.y + time * rollSpeed + vDrift);
    float uvY = mix(TexCoord.y + vDrift, vRoll, smoothstep(0.55, 0.75, effDist));

    vec2 sampleUV = fract(vec2(TexCoord.x + totalDisplaceX, uvY));

    // 4. Sample video with ghost image RF multipath echoes (0.25 - 0.55)
    vec4 sampledColor = texture(videoTexture, sampleUV);
    float ghostEnv = smoothstep(0.25, 0.38, effDist) * (1.0 - smoothstep(0.55, 0.75, effDist));
    if (ghostEnv > 0.01) {
        vec2 ghostUV = fract(sampleUV + vec2(0.045, 0.002));
        vec4 ghostColor = texture(videoTexture, ghostUV);
        sampledColor = mix(sampledColor, ghostColor, ghostEnv * 0.35);
    }

    // 5. Color loss / desaturation (NTSC/PAL color burst threshold loss)
    float sat = clamp(1.0 - pow(effDist, 1.0 / max(colorFade, 0.2)) * colorFade, 0.0, 1.0);
    float luma = dot(sampledColor.rgb, vec3(0.299, 0.587, 0.114));
    vec3 col = mix(vec3(luma), sampledColor.rgb, sat);

    // 6. AGC brightness fluctuation (0.4 - 0.6)
    float agcGain = 1.0 + (sin(time * 11.3) * 0.25 + cos(time * 17.1) * 0.15) * smoothstep(0.38, 0.65, effDist) * (1.0 - smoothstep(0.75, 0.9, effDist));
    col *= clamp(agcGain, 0.5, 1.5);

    // 7. Static snow generation across stages
    float snowFine = hash12(TexCoord * 600.0 + fract(time * 59.3));
    float snowCoarse = hash12(floor(TexCoord * 200.0) + fract(time * 31.7));
    float snowPattern = mix(snowFine, snowCoarse, 0.35);

    float snowMix = smoothstep(0.08, 0.95, effDist);

    // Brief flashes / glimpses of recognizable image at 0.6 - 0.95
    float glimpseFlicker = step(0.9, hash12(vec2(floor(time * 9.0), 3.14))) * (1.0 - smoothstep(0.85, 0.96, effDist));
    snowMix = clamp(snowMix - glimpseFlicker * 0.35, 0.0, 1.0);

    vec3 result = mix(col, vec3(snowPattern), snowMix);

    // 8. Complete signal loss (0.95 - 1.0)
    float completeLoss = smoothstep(0.95, 1.0, effDist);
    float blackOrStatic = hash12(vec2(floor(time * 2.0), 7.0));
    vec3 finalLoss = mix(vec3(snowPattern * 0.7), vec3(0.02), step(0.5, blackOrStatic));
    result = mix(result, finalLoss, completeLoss);

    FragColor = vec4(clamp(result, 0.0, 1.0), baseColor.a);
}
)");

    writeFrag("Analog & FPV/analog_reception", R"(#version 330 core
// @name Analog TV Reception
// @desc Authentic over-the-air analog television reception simulation with multipath ghosting, Gaussian static, and sync instability
// @param ghostStrength Ghost Image Strength 0.0 0.5 0.1
// @param ghostOffset Ghost Horizontal Offset 0.001 0.05 0.02
// @param snow Snow/Static Level 0.0 1.0 0.15
// @param vHoldDrift Vertical Hold Drift 0.0 1.0 0.0
// @param hSyncJitter H-Sync Instability 0.0 1.0 0.05
// @param agcPump AGC Pumping 0.0 1.0 0.0
// @param impulseNoise Impulse Noise 0.0 1.0 0.02
// @param active Enable Analog Reception 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform vec2 resolution;
uniform float ghostStrength;
uniform float ghostOffset;
uniform float snow;
uniform float vHoldDrift;
uniform float hSyncJitter;
uniform float agcPump;
uniform float impulseNoise;
uniform float active;

vec2 hash2(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float hash1(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float gaussianNoise(vec2 p) {
    vec2 u = hash2(p);
    float r = sqrt(max(1e-7, -2.0 * log(clamp(u.x, 1e-7, 0.99999))));
    float phi = 6.28318530718 * u.y;
    return r * cos(phi);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = orig;
        return;
    }

    vec2 res = (resolution.x > 1.0 && resolution.y > 1.0) ? resolution : vec2(textureSize(videoTexture, 0));
    if (res.x < 1.0) res = vec2(1920.0, 1080.0);

    vec2 uv = TexCoord;

    // 1. Vertical Hold Drift: rolls vertically and catches near frame boundary
    float vbiMask = 0.0;
    if (vHoldDrift > 0.001) {
        float rollSpeed = vHoldDrift * 0.75;
        float rawRoll = time * rollSpeed;
        float rollCycle = fract(rawRoll);
        float stutter = sin(rollCycle * 6.2831853) * 0.06 * min(vHoldDrift * 2.0, 1.0);
        float vShift = fract(rawRoll + stutter);
        uv.y = fract(uv.y + vShift);
        float bar = smoothstep(0.0, 0.015, uv.y) * (1.0 - smoothstep(0.92, 0.94, uv.y));
        vbiMask = 1.0 - bar;
    }

    // 2. Horizontal Sync Instability (jitter & flag waving)
    float scanline = floor(uv.y * res.y);
    float frameTick = floor(time * 30.0);
    if (hSyncJitter > 0.001) {
        float hNoise = (hash1(vec2(scanline, frameTick)) - 0.5) * 2.0;
        float wave = sin(uv.y * 14.0 + time * 6.0) * cos(uv.y * 4.0 - time * 3.0);
        float tearProb = hash1(vec2(floor(time * 15.0), scanline * 0.02));
        float tear = (tearProb > 0.988) ? (hash1(vec2(scanline, time * 2.0)) - 0.5) * 0.05 : 0.0;
        float totalJitter = (hNoise * 0.003 + wave * 0.003 + tear) * hSyncJitter;
        uv.x = clamp(uv.x + totalJitter, 0.0, 1.0);
    }

    vec3 signal = texture(videoTexture, uv).rgb;
    if (vbiMask > 0.0) {
        signal = mix(signal, vec3(0.04), vbiMask);
        if (vbiMask > 0.5 && uv.x > 0.3 && uv.x < 0.7) signal = vec3(0.0);
    }

    // 3. Multipath Ghosting
    if (ghostStrength > 0.001) {
        vec2 gUV1 = vec2(clamp(uv.x + ghostOffset, 0.0, 1.0), uv.y);
        vec2 gUV2 = vec2(clamp(uv.x + ghostOffset * 2.15, 0.0, 1.0), uv.y);
        vec3 ghost1 = texture(videoTexture, gUV1).rgb;
        vec3 ghost2 = texture(videoTexture, gUV2).rgb;
        signal = signal + (ghost1 * 0.85 - ghost2 * 0.25) * ghostStrength;
    }

    // 4. AGC Pumping
    if (agcPump > 0.001) {
        float agcOsc = sin(time * 1.8) * 0.4 + sin(time * 0.6 + 1.7) * 0.35 + sin(time * 4.3) * 0.25;
        float agcGain = 1.0 + agcOsc * (agcPump * 0.4);
        signal = signal * agcGain + vec3(max(0.0, agcGain - 1.0) * 0.1 * agcPump);
    }

    // 5. Impulse Noise
    if (impulseNoise > 0.001) {
        float impRand = hash1(vec2(scanline, floor(time * 60.0)));
        if (impRand > (1.0 - impulseNoise * 0.04)) {
            float streakStart = hash1(vec2(scanline * 2.3, floor(time * 60.0)));
            float streakLen = 0.03 + hash1(vec2(scanline * 5.1, floor(time * 60.0))) * 0.18;
            if (uv.x >= streakStart && uv.x <= streakStart + streakLen) {
                float trail = 1.0 - (uv.x - streakStart) / streakLen;
                signal += vec3(trail * 1.6);
            }
        }
    }

    // 6. Snow / Static Noise
    if (snow > 0.001) {
        vec2 pixelCoord = uv * res;
        vec2 seedFine = pixelCoord + vec2(time * 137.15, time * 269.83);
        vec2 seedLine = vec2(pixelCoord.x * 0.35, scanline) + vec2(time * 91.31, 0.0);
        float gFine = gaussianNoise(seedFine);
        float gLine = gaussianNoise(seedLine);
        float gaussianSum = gFine * 0.7 + gLine * 0.55;

        float luma = dot(signal, vec3(0.299, 0.587, 0.114));
        signal = mix(signal, vec3(luma), clamp(snow * 1.25, 0.0, 0.85));
        signal += vec3(gaussianSum * snow * 0.45);
    }

    FragColor = vec4(clamp(signal, 0.0, 1.0), orig.a);
}
)");

    writeFrag("Analog & FPV/ntsc_composite", R"(#version 330 core
// @name NTSC Composite Artifacts
// @desc Authentic NTSC composite video artifacts including dot crawl, cross-color rainbows, and chroma bandwidth limiting
// @param dotCrawl Dot Crawl 0.0 1.0 0.3
// @param crossColor Cross-Color Rainbows 0.0 1.0 0.2
// @param chromaBlur Chroma Bandwidth Limit 0.0 1.0 0.4
// @param edgeRinging Edge Ringing 0.0 1.0 0.2
// @param signalNoise Signal Noise Floor 0.0 0.5 0.05
// @param active Enable NTSC Artifacts 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform vec2 resolution;
uniform float dotCrawl;
uniform float crossColor;
uniform float chromaBlur;
uniform float edgeRinging;
uniform float signalNoise;
uniform float active;

vec3 rgb2yiq(vec3 c) {
    return vec3(
        0.299 * c.r + 0.587 * c.g + 0.114 * c.b,
        0.596 * c.r - 0.274 * c.g - 0.322 * c.b,
        0.211 * c.r - 0.523 * c.g + 0.312 * c.b
    );
}

vec3 yiq2rgb(vec3 yiq) {
    return vec3(
        yiq.x + 0.956 * yiq.y + 0.621 * yiq.z,
        yiq.x - 0.272 * yiq.y - 0.647 * yiq.z,
        yiq.x - 1.106 * yiq.y + 1.703 * yiq.z
    );
}

float hash1(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = orig;
        return;
    }

    vec2 res = (resolution.x > 1.0 && resolution.y > 1.0) ? resolution : vec2(textureSize(videoTexture, 0));
    if (res.x < 1.0) res = vec2(640.0, 480.0);

    float lineIdx = floor(TexCoord.y * res.y);
    float frameIdx = floor(time * 29.97);

    float scFreq = 227.5 * 6.28318530718;
    float scPhase = TexCoord.x * scFreq + lineIdx * 3.14159265 + frameIdx * 3.14159265;

    vec3 centerCol = orig.rgb;
    vec3 centerYiq = rgb2yiq(centerCol);
    float yCenter = centerYiq.x;

    // 1. Chroma Bandwidth Limiting
    float dx = (1.0 / res.x) * (1.0 + chromaBlur * 6.0);
    float chromaDelay = (1.0 / res.x) * 1.5 * chromaBlur;
    vec2 chromaAccum = vec2(0.0);
    float weights[7] = float[7](0.061, 0.121, 0.242, 0.382, 0.242, 0.121, 0.061);
    float totalW = 0.0;
    for (int i = -3; i <= 3; ++i) {
        float offset = float(i) * dx - chromaDelay;
        vec3 sYiq = rgb2yiq(texture(videoTexture, TexCoord + vec2(offset, 0.0)).rgb);
        float w = weights[i + 3];
        chromaAccum += sYiq.yz * w;
        totalW += w;
    }
    vec2 filteredChroma = chromaAccum / totalW;

    // 2. Edge Ringing
    float ringDx = 1.0 / res.x;
    float yL2 = rgb2yiq(texture(videoTexture, TexCoord - vec2(ringDx * 2.0, 0.0)).rgb).x;
    float yL1 = rgb2yiq(texture(videoTexture, TexCoord - vec2(ringDx, 0.0)).rgb).x;
    float yR1 = rgb2yiq(texture(videoTexture, TexCoord + vec2(ringDx, 0.0)).rgb).x;
    float yR2 = rgb2yiq(texture(videoTexture, TexCoord + vec2(ringDx * 2.0, 0.0)).rgb).x;
    float yRinging = yCenter + ((yCenter - yL1) * 0.35 - (yL1 - yL2) * 0.18 + (yCenter - yR1) * 0.35 - (yR1 - yR2) * 0.18) * edgeRinging;

    // 3. Dot Crawl & Cross-Luminance
    vec2 chromaDiff = abs(filteredChroma - centerYiq.yz);
    float chromaEdge = length(chromaDiff) * 3.5 + length(filteredChroma) * 0.35;
    float crawlMod = sin(scPhase) * chromaEdge * dotCrawl * 0.55;
    yRinging += crawlMod;

    // 4. Cross-Color Rainbow Banding
    float highFreqLuma = (2.0 * yCenter - yL1 - yR1);
    float falseI = highFreqLuma * cos(scPhase) * crossColor * 1.8;
    float falseQ = highFreqLuma * sin(scPhase) * crossColor * 1.8;
    filteredChroma.x += falseI;
    filteredChroma.y += falseQ;

    // 5. Signal Noise Floor
    if (signalNoise > 0.001) {
        vec2 nSeed = TexCoord * res + vec2(time * 73.1, time * 37.9);
        float nLuma = (hash1(nSeed) - 0.5) * signalNoise * 0.5;
        float nI = (hash1(nSeed + vec2(13.7, 42.1)) - 0.5) * signalNoise * 1.2;
        float nQ = (hash1(nSeed + vec2(57.3, 89.4)) - 0.5) * signalNoise * 1.2;
        yRinging += nLuma;
        filteredChroma.x += nI;
        filteredChroma.y += nQ;
    }

    vec3 finalRgb = yiq2rgb(vec3(yRinging, filteredChroma.x, filteredChroma.y));
    FragColor = vec4(clamp(finalRgb, 0.0, 1.0), orig.a);
}
)");

    writeFrag("Analog & FPV/rf_interference", R"(#version 330 core
// @name RF Interference
// @desc Realistic radio frequency interference including herringbone patterns, power line hum bars, motor whine, and ignition noise
// @param herringbone Herringbone Pattern 0.0 1.0 0.0
// @param humbars Power Line Hum Bars 0.0 1.0 0.0
// @param motorWhine Motor Whine Lines 0.0 1.0 0.0
// @param ignitionNoise Ignition Noise 0.0 1.0 0.0
// @param digitalBleed Digital Clock Bleed 0.0 1.0 0.0
// @param signalBeat Signal Beat Frequency 0.1 20.0 2.0
// @param active Enable RF Interference 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform vec2 resolution;
uniform float herringbone;
uniform float humbars;
uniform float motorWhine;
uniform float ignitionNoise;
uniform float digitalBleed;
uniform float signalBeat;
uniform float active;

float hash1(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec4 orig = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = orig;
        return;
    }

    vec2 res = (resolution.x > 1.0 && resolution.y > 1.0) ? resolution : vec2(textureSize(videoTexture, 0));
    if (res.x < 1.0) res = vec2(1920.0, 1080.0);

    // 1. Signal Beating
    float beatEnv = 0.82 + 0.18 * sin(time * signalBeat * 6.2831853);

    vec2 uv = TexCoord;

    // 2. Power Line Hum Bars
    float humLuma = 0.0;
    if (humbars > 0.001) {
        float humPhase = uv.y * 1.5 - time * 0.22;
        float humCycle = humPhase * 6.2831853;
        float hWarp = cos(humCycle) * 0.006 * humbars;
        uv.x = clamp(uv.x + hWarp, 0.0, 1.0);
        humLuma = sin(humCycle) * 0.18 * humbars * beatEnv;
    }

    vec3 signal = texture(videoTexture, uv).rgb;
    signal *= (1.0 + (beatEnv - 1.0) * 0.15);
    signal += vec3(humLuma);

    // 3. Herringbone Pattern
    if (herringbone > 0.001) {
        float a1 = (uv.x * 125.0 + uv.y * 85.0) * 3.14159265 + time * 16.0;
        float a2 = (uv.x * 125.0 - uv.y * 85.0) * 3.14159265 - time * 13.0;
        float chevron = sin(a1) * cos(a2);
        chevron += sin(a1 * 2.0 + 1.2) * 0.25;
        signal += vec3(chevron * herringbone * 0.28 * beatEnv);
    }

    // 4. Motor Whine Lines
    if (motorWhine > 0.001) {
        float rpmFlutter = sin(time * 7.5) * 6.0;
        float whinePhase = (uv.x * 65.0 + uv.y * 290.0 + time * (130.0 + rpmFlutter));
        float whine1 = pow(0.5 + 0.5 * sin(whinePhase), 14.0);
        float whine2 = pow(0.5 + 0.5 * sin(whinePhase * 1.5 - time * 45.0), 18.0) * 0.45;
        signal += vec3((whine1 + whine2) * motorWhine * 0.45 * beatEnv);
    }

    // 5. Ignition Noise
    if (ignitionNoise > 0.001) {
        float scanline = floor(uv.y * res.y);
        float frameTick = floor(time * 60.0);
        float sparkRand = hash1(vec2(scanline, frameTick));
        if (sparkRand > (1.0 - ignitionNoise * 0.055)) {
            float sparkX = hash1(vec2(scanline * 3.7, frameTick));
            float sparkLen = 0.025 + hash1(vec2(scanline * 7.1, frameTick)) * 0.14;
            if (uv.x >= sparkX && uv.x <= sparkX + sparkLen) {
                float d = (uv.x - sparkX) / sparkLen;
                float pulse = exp(-d * 3.2) * (1.0 + sin(d * 24.0) * 0.5);
                signal += vec3(pulse * 1.35 * beatEnv);
            }
        }
    }

    // 6. Digital Clock Bleed
    if (digitalBleed > 0.001) {
        float colPos = fract(uv.x * 45.0);
        float clockLine = smoothstep(0.0, 0.08, colPos) * smoothstep(0.16, 0.08, colPos);
        float busNoise = hash1(vec2(floor(uv.x * 45.0), floor(time * 35.0) + floor(uv.y * 28.0)));
        float clockStripe = clockLine * (0.6 + 0.4 * busNoise);
        signal += vec3(clockStripe * digitalBleed * 0.35 * beatEnv);
    }

    FragColor = vec4(clamp(signal, 0.0, 1.0), orig.a);
}
)");

    writeFrag("Analog & FPV/fpv_osd", R"(#version 330 core
// @name FPV Drone OSD
// @desc Realistic Betaflight/INAV analog drone OSD overlay with MAX7456 bitmap font
// @param batteryVoltage Battery Voltage 10.0 17.0 14.8
// @param rssi RSSI Signal 0.0 100.0 85.0
// @param throttle Throttle % 0.0 100.0 50.0
// @param flightMode Flight Mode 0.0 2.0 0.0
// @param showCrosshair Show Crosshair 0.0 1.0 1.0 bool
// @param osdOpacity OSD Opacity 0.0 1.0 0.85
// @param fontScale Font Scale 0.5 3.0 1.0
// @param horizonTilt Horizon Tilt Angle -45.0 45.0 0.0
// @param active Enable OSD 0.0 1.0 1.0 bool
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform vec2 resolution;
uniform float time;
uniform float batteryVoltage;
uniform float rssi;
uniform float throttle;
uniform float flightMode;
uniform float showCrosshair;
uniform float osdOpacity;
uniform float fontScale;
uniform float horizonTilt;
uniform float active;

// Pack 5x7 glyph: 25 bits in .x (rows 0-4), 10 bits in .y (rows 5-6)
uvec2 fontGlyph(int ch) {
    switch (ch) {
        case 48: return uvec2(20637230u, 465u); // 0
        case 49: return uvec2(4329668u, 452u);  // 1
        case 50: return uvec2(4473390u, 994u);  // 2
        case 51: return uvec2(17252879u, 496u); // 3
        case 52: return uvec2(32811400u, 264u); // 4
        case 53: return uvec2(17316927u, 465u); // 5
        case 54: return uvec2(18318382u, 465u); // 6
        case 55: return uvec2(2236959u, 66u);   // 7
        case 56: return uvec2(18302510u, 465u); // 8
        case 57: return uvec2(17778222u, 464u); // 9
        case 58: return uvec2(4198528u, 4u);    // :
        case 46: return uvec2(0u, 198u);        // .
        case 37: return uvec2(4264307u, 826u);  // %
        case 47: return uvec2(2236688u, 34u);   // /
        case 45: return uvec2(1015808u, 0u);    // -
        case 43: return uvec2(5214336u, 4u);    // +
        case 32: return uvec2(0u, 0u);          // space
        case 65: return uvec2(18859566u, 561u); // A
        case 66: return uvec2(18335279u, 497u); // B
        case 67: return uvec2(1082926u, 465u);  // C
        case 68: return uvec2(18400815u, 497u); // D
        case 69: return uvec2(1541183u, 993u);  // E
        case 70: return uvec2(1541183u, 33u);   // F
        case 71: return uvec2(18777646u, 465u); // G
        case 72: return uvec2(18859569u, 561u); // H
        case 73: return uvec2(4329614u, 452u);  // I
        case 74: return uvec2(8659228u, 201u);  // J
        case 75: return uvec2(5346609u, 553u);  // K
        case 76: return uvec2(1082401u, 993u);  // L
        case 77: return uvec2(18405233u, 561u); // M
        case 78: return uvec2(18667121u, 561u); // N
        case 79: return uvec2(18400814u, 465u); // O
        case 80: return uvec2(1558063u, 33u);   // P
        case 81: return uvec2(22595118u, 713u); // Q
        case 82: return uvec2(5752367u, 553u);  // R
        case 83: return uvec2(17237550u, 465u); // S
        case 84: return uvec2(4329631u, 132u);  // T
        case 85: return uvec2(18400817u, 465u); // U
        case 86: return uvec2(18400817u, 138u); // V
        case 87: return uvec2(22726193u, 571u); // W
        case 88: return uvec2(4329809u, 554u);  // X
        case 89: return uvec2(4336177u, 132u);  // Y
        case 90: return uvec2(2236959u, 993u);  // Z
        default: return uvec2(0u, 0u);
    }
}

bool getGlyphBit(uvec2 g, int x, int y) {
    if (x < 0 || x > 4 || y < 0 || y > 6) return false;
    if (y < 5) {
        return ((g.x >> uint(y * 5 + x)) & 1u) != 0u;
    } else {
        return ((g.y >> uint((y - 5) * 5 + x)) & 1u) != 0u;
    }
}

uvec2 pack4(int c0, int c1, int c2, int c3) {
    return uvec2(uint(c0) | (uint(c1) << 8u) | (uint(c2) << 16u) | (uint(c3) << 24u), 0u);
}
uvec2 pack5(int c0, int c1, int c2, int c3, int c4) {
    return uvec2(uint(c0) | (uint(c1) << 8u) | (uint(c2) << 16u) | (uint(c3) << 24u), uint(c4));
}
uvec2 pack7(int c0, int c1, int c2, int c3, int c4, int c5, int c6) {
    return uvec2(uint(c0) | (uint(c1) << 8u) | (uint(c2) << 16u) | (uint(c3) << 24u),
                 uint(c4) | (uint(c5) << 8u) | (uint(c6) << 16u));
}
uvec2 pack8(int c0, int c1, int c2, int c3, int c4, int c5, int c6, int c7) {
    return uvec2(uint(c0) | (uint(c1) << 8u) | (uint(c2) << 16u) | (uint(c3) << 24u),
                 uint(c4) | (uint(c5) << 8u) | (uint(c6) << 16u) | (uint(c7) << 24u));
}

void drawText8(inout vec4 osd, vec2 pix, vec2 pos, vec2 cellSize, float pixSize, uvec2 pack, int len, vec3 textColor) {
    float totalW = float(len) * cellSize.x;
    float totalH = cellSize.y;
    if (pix.x < pos.x || pix.x >= pos.x + totalW || pix.y < pos.y || pix.y >= pos.y + totalH) {
        return;
    }
    int charIdx = int((pix.x - pos.x) / cellSize.x);
    if (charIdx < 0 || charIdx >= len) return;

    int ch = int((charIdx < 4 ? (pack.x >> uint(charIdx * 8)) : (pack.y >> uint((charIdx - 4) * 8))) & 0xFFu);
    if (ch <= 32) return;

    vec2 charOrigin = pos + vec2(float(charIdx) * cellSize.x, 0.0);
    vec2 localPix = (pix - charOrigin) / pixSize;
    ivec2 gc = ivec2(floor(localPix));

    int gx = gc.x - 1;
    int gy = gc.y - 1;

    uvec2 g = fontGlyph(ch);
    vec4 cellCol;
    if (getGlyphBit(g, gx, gy)) {
        cellCol = vec4(textColor, 1.0);
    } else {
        bool border = false;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (getGlyphBit(g, gx + dx, gy + dy)) {
                    border = true;
                    break;
                }
            }
            if (border) break;
        }
        if (border) {
            cellCol = vec4(0.0, 0.0, 0.0, 0.95);
        } else {
            cellCol = vec4(0.0, 0.0, 0.0, 0.45);
        }
    }

    osd.rgb = mix(osd.rgb, cellCol.rgb, cellCol.a);
    osd.a = max(osd.a, cellCol.a);
}

void drawRssiBars(inout vec4 osd, vec2 pix, vec2 pos, float pixSize, float rssiVal) {
    float barW = pixSize * 2.0;
    float barGap = pixSize * 1.5;
    float maxH = pixSize * 8.0;
    float totalW = 4.0 * barW + 3.0 * barGap;

    if (pix.x < pos.x - pixSize || pix.x >= pos.x + totalW + pixSize ||
        pix.y < pos.y - pixSize || pix.y >= pos.y + maxH + pixSize) {
        return;
    }

    for (int i = 0; i < 4; ++i) {
        float bx = pos.x + float(i) * (barW + barGap);
        float hRatio = float(i + 1) * 0.25;
        float bh = maxH * hRatio;
        float by = pos.y + (maxH - bh);

        bool inBar = (pix.x >= bx && pix.x < bx + barW && pix.y >= by && pix.y < pos.y + maxH);
        bool inBorder = (pix.x >= bx - pixSize && pix.x < bx + barW + pixSize &&
                         pix.y >= by - pixSize && pix.y < pos.y + maxH + pixSize);

        float thresh = float(i + 1) * 23.0;
        bool lit = (rssiVal >= thresh);

        if (inBar) {
            vec3 bColor = lit ? vec3(0.2, 1.0, 0.4) : vec3(0.25, 0.25, 0.25);
            osd.rgb = mix(osd.rgb, bColor, 1.0);
            osd.a = max(osd.a, 1.0);
        } else if (inBorder) {
            osd.rgb = mix(osd.rgb, vec3(0.0), 0.95);
            osd.a = max(osd.a, 0.95);
        }
    }
}

void drawCrosshair(inout vec4 osd, vec2 pix, vec2 center, float pixSize) {
    vec2 d = abs(pix - center);
    float armLen = pixSize * 9.0;
    float gap = pixSize * 2.5;
    float th = pixSize * 0.75;
    float borderTh = pixSize * 1.75;

    bool hTick = (d.x >= gap && d.x <= armLen && d.y <= th);
    bool hBorder = (d.x >= gap - pixSize && d.x <= armLen + pixSize && d.y <= borderTh);

    bool vTick = (d.y >= gap && d.y <= armLen && d.x <= th);
    bool vBorder = (d.y >= gap - pixSize && d.y <= armLen + pixSize && d.x <= borderTh);

    bool cDot = (d.x <= th && d.y <= th);
    bool cBorder = (d.x <= borderTh && d.y <= borderTh);

    if (hTick || vTick || cDot) {
        osd.rgb = mix(osd.rgb, vec3(1.0), 1.0);
        osd.a = max(osd.a, 1.0);
    } else if (hBorder || vBorder || cBorder) {
        osd.rgb = mix(osd.rgb, vec3(0.0), 0.95);
        osd.a = max(osd.a, 0.95);
    }
}

void drawHorizon(inout vec4 osd, vec2 pix, vec2 center, float tiltDeg, float pixSize, float screenW) {
    float rad = radians(tiltDeg);
    float cosA = cos(rad);
    float sinA = sin(rad);
    vec2 d = pix - center;
    vec2 r = vec2(d.x * cosA + d.y * sinA, -d.x * sinA + d.y * cosA);

    float halfSpan = screenW * 0.16;
    float gap = screenW * 0.04;
    float th = pixSize * 0.75;
    float bTh = pixSize * 1.75;

    float ax = abs(r.x);
    bool onLine = (ax >= gap && ax <= halfSpan && abs(r.y) <= th);
    bool onBorder = (ax >= gap - pixSize && ax <= halfSpan + pixSize && abs(r.y) <= bTh);

    float notchH = pixSize * 3.5;
    bool onNotch = (ax >= halfSpan - pixSize * 1.5 && ax <= halfSpan && r.y >= 0.0 && r.y <= notchH);
    bool onNotchBorder = (ax >= halfSpan - pixSize * 2.5 && ax <= halfSpan + pixSize && r.y >= -pixSize && r.y <= notchH + pixSize);

    if (onLine || onNotch) {
        osd.rgb = mix(osd.rgb, vec3(1.0), 1.0);
        osd.a = max(osd.a, 1.0);
    } else if (onBorder || onNotchBorder) {
        osd.rgb = mix(osd.rgb, vec3(0.0), 0.95);
        osd.a = max(osd.a, 0.95);
    }
}

void main() {
    vec4 baseColor = texture(videoTexture, TexCoord);
    if (active < 0.5) {
        FragColor = baseColor;
        return;
    }

    vec2 res = (resolution.x > 1.0 && resolution.y > 1.0) ? resolution : vec2(1920.0, 1080.0);
    vec2 pix = vec2(TexCoord.x, 1.0 - TexCoord.y) * res;

    float baseUnit = res.y / 480.0;
    float pixSize = max(1.0, floor(baseUnit * clamp(fontScale, 0.4, 3.0)));
    vec2 cellSize = vec2(7.0 * pixSize, 9.0 * pixSize);

    vec4 osd = vec4(0.0);

    // 1. Top-left: Battery voltage (with green/yellow/red color)
    int bv = int(clamp(batteryVoltage, 0.0, 99.9) * 10.0 + 0.5);
    int b1 = (bv / 100) % 10;
    int b2 = (bv / 10) % 10;
    int b3 = bv % 10;
    int cellS = (batteryVoltage >= 13.0) ? 52 : 51; // '4' or '3'
    uvec2 battStr = pack8(cellS, 83, 32, 48 + b1, 48 + b2, 46, 48 + b3, 86);
    vec3 voltCol = (batteryVoltage >= 14.0) ? vec3(0.2, 1.0, 0.2) :
                   (batteryVoltage >= 13.0) ? vec3(1.0, 0.9, 0.1) :
                                              vec3(1.0, 0.25, 0.2);

    float marginX = res.x * 0.07;
    float marginY = res.y * 0.07;
    drawText8(osd, pix, vec2(marginX, marginY), cellSize, pixSize, battStr, 8, voltCol);

    // 2. Top-center: Flight Mode
    uvec2 modeStr;
    int modeLen;
    if (flightMode < 0.5) {
        modeStr = pack4(65, 67, 82, 79); // ACRO
        modeLen = 4;
    } else if (flightMode < 1.5) {
        modeStr = pack5(65, 78, 71, 76, 69); // ANGLE
        modeLen = 5;
    } else {
        modeStr = pack7(72, 79, 82, 73, 90, 79, 78); // HORIZON
        modeLen = 7;
    }
    float modeX = res.x * 0.5 - float(modeLen) * cellSize.x * 0.5;
    drawText8(osd, pix, vec2(modeX, marginY), cellSize, pixSize, modeStr, modeLen, vec3(1.0));

    // 3. Top-right: RSSI Signal + Bars
    int rVal = int(clamp(rssi, 0.0, 99.0));
    uvec2 rssiStr = pack7(82, 83, 83, 73, 32, 48 + (rVal / 10), 48 + (rVal % 10)); // RSSI XX
    float rssiX = res.x - marginX - float(7) * cellSize.x - pixSize * 15.0;
    drawText8(osd, pix, vec2(rssiX, marginY), cellSize, pixSize, rssiStr, 7, vec3(1.0));
    drawRssiBars(osd, pix, vec2(rssiX + float(7) * cellSize.x + pixSize * 2.5, marginY + pixSize * 0.5), pixSize, rssi);

    // 4. Artificial Horizon (Upper/top area)
    vec2 hCenter = vec2(res.x * 0.5, res.y * 0.38);
    drawHorizon(osd, pix, hCenter, horizonTilt, pixSize, res.x);

    // 5. Center Reticle
    if (showCrosshair > 0.5) {
        drawCrosshair(osd, pix, res * 0.5, pixSize);
    }

    // 6. Bottom-left: Flight Timer (MM:SS)
    int totalSec = int(max(time, 0.0));
    int tmMin = (totalSec / 60) % 100;
    int tmSec = totalSec % 60;
    uvec2 timerStr = pack5(48 + (tmMin / 10), 48 + (tmMin % 10), 58, 48 + (tmSec / 10), 48 + (tmSec % 10));
    float botY = res.y - marginY - cellSize.y;
    drawText8(osd, pix, vec2(marginX, botY), cellSize, pixSize, timerStr, 5, vec3(1.0));

    // 7. Bottom-center: Throttle %
    int thrVal = int(clamp(throttle, 0.0, 100.0));
    uvec2 thrStr;
    int thrLen;
    if (thrVal >= 100) {
        thrStr = pack8(84, 72, 82, 32, 49, 48, 48, 37); // THR 100%
        thrLen = 8;
    } else {
        thrStr = pack7(84, 72, 82, 32, 48 + (thrVal / 10), 48 + (thrVal % 10), 37); // THR XX%
        thrLen = 7;
    }
    float thrX = res.x * 0.5 - float(thrLen) * cellSize.x * 0.5;
    drawText8(osd, pix, vec2(thrX, botY), cellSize, pixSize, thrStr, thrLen, vec3(1.0));

    // 8. Bottom-right: Altitude & Speed
    int altVal = int(clamp(25.0 + throttle * 0.4 + sin(time * 0.7) * 4.0, 0.0, 999.0));
    uvec2 altStr;
    int altLen;
    if (altVal >= 100) {
        altStr = pack8(65, 76, 84, 32, 48 + (altVal / 100), 48 + ((altVal / 10) % 10), 48 + (altVal % 10), 77);
        altLen = 8;
    } else {
        altStr = pack7(65, 76, 84, 32, 48 + (altVal / 10), 48 + (altVal % 10), 77);
        altLen = 7;
    }

    int spdVal = int(clamp(throttle * 0.75 + cos(time * 0.5) * 5.0, 0.0, 999.0));
    uvec2 spdStr;
    int spdLen;
    if (spdVal >= 100) {
        spdStr = pack8(83, 80, 68, 32, 48 + (spdVal / 100), 48 + ((spdVal / 10) % 10), 48 + (spdVal % 10), 75);
        spdLen = 8;
    } else {
        spdStr = pack7(83, 80, 68, 32, 48 + (spdVal / 10), 48 + (spdVal % 10), 75);
        spdLen = 7;
    }

    float rightX = res.x - marginX - float(8) * cellSize.x;
    drawText8(osd, pix, vec2(rightX, botY - cellSize.y * 1.3), cellSize, pixSize, altStr, altLen, vec3(1.0));
    drawText8(osd, pix, vec2(rightX, botY), cellSize, pixSize, spdStr, spdLen, vec3(1.0));

    // Overlay onto base video with opacity preservation
    vec3 finalRGB = mix(baseColor.rgb, osd.rgb, osd.a * clamp(osdOpacity, 0.0, 1.0));
    FragColor = vec4(finalRGB, baseColor.a);
}
)");
}

