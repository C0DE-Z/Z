# Building Z

## Prerequisites
- **C++ Compiler**: GCC 13+ / Clang 16+ / MSVC 2022 (with C++20 support)
- **CMake**: 3.20 or newer
- **Ninja** or GNU Make

## Dependencies
- **Qt 6**: `Qt6Core`, `Qt6Widgets`, `Qt6Gui`, `Qt6OpenGL`, `Qt6OpenGLWidgets`
- **FFmpeg**: `libavformat`, `libavcodec`, `libavutil`, `libswscale`, `libswresample`
- **PortAudio**: `portaudio-2.0`
- **SQLite 3**: `sqlite3`
- **OpenGL**: Desktop OpenGL 3.3+

---

## Windows (MSYS2 MinGW64)
```powershell
# 1. Install packages in MSYS2
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt6-base mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-portaudio mingw-w64-x86_64-sqlite3

# 2. Configure with CMake
$env:PATH="C:\msys64\mingw64\bin;C:\msys64\usr\bin;$env:PATH"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Build executable
cmake --build build

# 4. Run automated tests
ctest --test-dir build --output-on-failure
```

---

## macOS (Homebrew)
```bash
brew install qt@6 ffmpeg portaudio sqlite cmake ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build
```

---

## Linux (Ubuntu / Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build qt6-base-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libportaudio2 portaudio19-dev libsqlite3-dev libgl1-mesa-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
