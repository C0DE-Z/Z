# Z — A Video Editor

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Qt6](https://img.shields.io/badge/Qt-6.0%2B-green.svg)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3%20Core-orange.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

**Z** is a video editor.. 

---

## Codebase
```
Z/
├── cmake/                      #  build configurations
│   ├── CompilerOptions.cmake
│   ├── Dependencies.cmake      # Package discovery
│   └── Platform.cmake          # Windows (MSYS2/MSVC), macOS, and Linux toolchains
├── docs/                       # Docs
│   ├── architecture/           # System design diagrams and execution pipelines
│   ├── building/               # Platform-specific compile guides
├── plugins/                    # Plugins (Navtive to the platform)
├── src/                        # Source Files
│   ├── app/                    # Application main entry
│   ├── core/                   # Foundation types, keyframes, curves & undo/redo state
│   ├── project/                # Tracks, clips and transitions.
│   ├── media/                  # Transcoding pipelines and media exporter
│   ├── engine/                 # decoding, encoding, effects etc
│   │   ├── audio/              # PortAudio driver
│   │   ├── video/              # FFmpeg demuxer/decoder.
│   │   ├── rendering/          # OpenGL context, FBO feedback loop & shaders
│   │   └── effects/            # CPU bitwise blenders (XOR, AND, OR, XNOR, NAND)
│   ├── plugins/                # Plugin discovery, manifest loader & parameter reflection
│   └── ui/                     # Qt6
│       ├── components/         # Timeline, Inspector, Media Pool, Track Manager
│       └── dialogs/            # Preferences dialog and export configurations
└── tests/                      # Unit Tests
    └── unit/                   #
```

---

## Building

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
- [Architecture](docs/architecture/ARCHITECTURE.md)
- [Build](docs/building/BUILDING.md)

---

Website: [https://z.codezey.dev](https://z.codezey.dev)