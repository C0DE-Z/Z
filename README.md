# Z — A Video Editor

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Qt6](https://img.shields.io/badge/Qt-6.0%2B-green.svg)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3%20Core-orange.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

**Z** is a video editor.. 

---

## Codebase Architecture

The repository is organized following clean architectural boundaries and strict dependency hierarchy:

```
Z/
├── cmake/                      # Modular build configurations
│   ├── CompilerOptions.cmake   # Warning levels, optimization & standard flags
│   ├── Dependencies.cmake      # Package discovery (Qt6, FFmpeg, PortAudio, SQLite3)
│   └── Platform.cmake          # Windows (MSYS2/MSVC), macOS, and Linux toolchains
├── docs/                       # Engineering & plugin documentation
│   ├── architecture/           # System design diagrams and execution pipelines
│   ├── building/               # Platform-specific compile guides
│   └── plugins/                # Shader manifest specifications and uniform reference
├── plugins/                    # First-party GLSL effects, datamoshers & transitions
├── src/                        # First-party C++20 source tree
│   ├── app/                    # Application lifecycle, styling & main entrypoint
│   ├── core/                   # Foundation types, keyframes, curves & undo/redo state
│   ├── project/                # Tracks, clips, transitions & JSON project serialization
│   ├── media/                  # Transcoding pipelines and multi-threaded media exporter
│   ├── engine/                 # Real-time audio, video decoding, rendering & effects
│   │   ├── audio/              # PortAudio driver, sample loading & frequency meters
│   │   ├── video/              # FFmpeg demuxer/decoder, frame cache & prefetching
│   │   ├── rendering/          # OpenGL context, FBO feedback loop & shaders
│   │   └── effects/            # CPU bitwise blenders (XOR, AND, OR, XNOR, NAND)
│   ├── plugins/                # Plugin discovery, manifest loader & parameter reflection
│   └── ui/                     # Qt6 modern dark-theme user interface
│       ├── components/         # Timeline, Inspector, Media Pool, Track Manager
│       └── dialogs/            # Preferences dialog and export configurations
└── tests/                      # Automated CTest unit testing suite
    └── unit/                   # Keyframe math, project serialization & app state tests
```

---

## Building from Source

### Prerequisites
- C++20 compliant compiler (GCC 13+, Clang 16+, or MSVC 2022)
- CMake 3.20+ and Ninja
- Qt 6 (Core, Widgets, Gui, OpenGL, OpenGLWidgets)
- FFmpeg (libavformat, libavcodec, libavutil, libswscale, libswresample)
- PortAudio 2.0 & SQLite3

### Windows (MSYS2 MinGW64)
```powershell
$env:PATH="C:\msys64\mingw64\bin;C:\msys64\usr\bin;$env:PATH"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Running Automated Tests
```powershell
ctest --test-dir build --output-on-failure
```

---

## Documentation
- [System Architecture](docs/architecture/ARCHITECTURE.md)
- [Build Instructions](docs/building/BUILDING.md)
- [Shader Plugin Specification](docs/plugins/PLUGIN_SPEC.md)

---

Website: [https://z.codezey.dev](https://z.codezey.dev)