#version 330 core
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
