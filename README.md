# Z Video Editor

Z is a native, experimental non-linear video editor built for real-time GLSL effects, compositing, and datamosh workflows. It is written in C++20 with Qt 6, OpenGL, FFmpeg, PortAudio, and SQLite.

> Z is under active development. Keep copies of source media and project files while you work.

## What it includes

- Multi-track editing with clips, transitions, keyframes, and undo/redo state.
- GPU rendering on desktop OpenGL 3.3 Core, including feedback-based effects.
- GLSL fragment-shader plugins with editable parameters and manual reload support.
- FFmpeg-powered import, decoding, and export; PortAudio-backed playback.
- Motion-region masking and optional OpenCV DNN / YOLO object detection.

## Build from source

### Requirements

- A C++20 compiler: GCC 13+, Clang 16+, or Visual Studio 2022.
- CMake 3.20+ and Ninja.
- Qt 6 modules: Core, Widgets, Gui, Network, OpenGL, and OpenGLWidgets.
- FFmpeg development libraries: `libavformat`, `libavcodec`, `libavutil`, `libswscale`, and `libswresample`.
- PortAudio, SQLite 3, and desktop OpenGL 3.3+.
- Optional: OpenCV with `core`, `dnn`, and `imgproc` for YOLO detection.

### Windows — MSYS2 MinGW64

Run the following from an MSYS2 MinGW64 shell:

```powershell
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt6-base mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-portaudio mingw-w64-x86_64-sqlite3
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is written to `build/bin/z.exe`.

### macOS — Homebrew

```bash
brew install qt@6 ffmpeg portaudio sqlite cmake ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build
```

### Linux — Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build qt6-base-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libportaudio2 portaudio19-dev libsqlite3-dev libgl1-mesa-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Create a shader plugin

Place an annotated `.frag` file anywhere under `plugins/`. Folder names become effect categories unless an `@category` annotation overrides them. From the editor, choose **Effects → Reload Plugins** after changing a plugin; Z does not watch shader files automatically.

```glsl
// @name My Effect
// @desc A short description shown in the effect browser
// @param amount Amount 0.0 1.0 0.5

uniform sampler2D videoTexture;
uniform float amount;
in vec2 TexCoord;
out vec4 FragColor;

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    FragColor = vec4(color.rgb * amount, color.a);
}
```

Read the [Shader Plugin Developer Guide](docs/shader_plugin_guide.html) for the complete annotation and uniform reference.

## Documentation

- [Build guide](docs/building/BUILDING.md)
- [System architecture](docs/architecture/ARCHITECTURE.md)
- [Shader Plugin Developer Guide](docs/shader_plugin_guide.html)
- [Brand guide](docs/BRAND.md)

## Website and releases

- Website: [z.codezey.dev](https://z.codezey.dev)
- Releases: [GitHub Releases](https://github.com/C0DE-Z/Z/releases)
- Issues: [GitHub Issues](https://github.com/C0DE-Z/Z/issues)
