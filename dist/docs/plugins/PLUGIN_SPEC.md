# Z Shader Plugin Specification

## Overview
Z loads GLSL fragment shader plugins at runtime from the `plugins/` directory. Each plugin consists of a pair of files:
1. `<plugin_id>.json`: Metadata, parameter definitions, default values, ranges, and UI labels.
2. `<plugin_id>.glsl`: Fragment shader code.

---

## 1. Manifest Format (`.json`)
```json
{
  "id": "chromatic_aberration",
  "name": "Chromatic Aberration",
  "category": "Distortion & Warp",
  "description": "RGB split displacement effect",
  "parameters": [
    {
      "name": "u_amount",
      "label": "Offset Amount",
      "type": "float",
      "min": 0.0,
      "max": 0.1,
      "default": 0.02
    },
    {
      "name": "u_enable_feedback",
      "label": "Feedback Loop",
      "type": "bool",
      "default": false
    }
  ]
}
```

---

## 2. Standard GLSL Uniforms
Every shader plugin automatically receives the following uniforms from the rendering engine:

| Uniform Name | Type | Description |
| :--- | :--- | :--- |
| `tex0` | `sampler2D` | Current video frame texture |
| `u_feedback` | `sampler2D` | Previous processed frame texture (feedback loop) |
| `tex1` | `sampler2D` | Outgoing transition frame texture (for transitions) |
| `u_time` | `float` | Current timeline playback time in seconds |
| `u_resolution` | `vec2` | Preview viewport width and height in pixels |
| `u_progress` | `float` | Transition progress normalized from `0.0` to `1.0` |
| `u_audio_low` | `float` | Live low-frequency audio energy (0.0 to 1.0) |
| `u_audio_mid` | `float` | Live mid-frequency audio energy (0.0 to 1.0) |
| `u_audio_high` | `float` | Live high-frequency audio energy (0.0 to 1.0) |

---

## 3. Shader Template (`.glsl`)
```glsl
#version 330 core

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D tex0;
uniform sampler2D u_feedback;
uniform vec2 u_resolution;
uniform float u_time;
uniform float u_amount;

void main() {
    vec2 uv = v_uv;
    float r = texture(tex0, uv + vec2(u_amount, 0.0)).r;
    float g = texture(tex0, uv).g;
    float b = texture(tex0, uv - vec2(u_amount, 0.0)).b;
    fragColor = vec4(r, g, b, 1.0);
}
```
