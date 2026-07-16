cmake_minimum_required(VERSION 3.20)
project(z VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui OpenGL OpenGLWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)

find_package(SQLite3 REQUIRED)

set(SOURCES
    src/main.cpp
    src/core/keyframe.cpp
    src/core/project.cpp
    src/core/appstate.cpp
    src/media/mediaimporter.cpp
    src/media/mediaexporter.cpp
    src/engine/pluginmanager.cpp
    src/engine/videodecoder.cpp
    src/engine/cpueffects.cpp
    src/engine/videoengine.cpp
    src/engine/audioengine.cpp
    src/engine/glwidget.cpp
    src/engine/shaders.cpp
    src/ui/mainwindow.cpp
    src/ui/components/timeline.cpp
    src/ui/components/inspector.cpp
    src/ui/components/effectsbrowser.cpp
    src/ui/components/mediapool.cpp
    src/ui/components/trackcontrol.cpp
)

set(HEADERS
    src/core/keyframe.h
    src/core/project.h
    src/core/appstate.h
    src/media/mediaimporter.h
    src/media/mediaexporter.h
    src/engine/pluginmanager.h
    src/engine/videoframe.h
    src/engine/videodecoder.h
    src/engine/cpueffects.h
    src/engine/videoengine.h
    src/engine/audioengine.h
    src/engine/glwidget.h
    src/engine/shaders.h
    src/ui/mainwindow.h
    src/ui/components/timeline.h
    src/ui/components/inspector.h
    src/ui/components/effectsbrowser.h
    src/ui/components/mediapool.h
    src/ui/components/trackcontrol.h
)

add_executable(z ${SOURCES} ${HEADERS})

target_include_directories(z PRIVATE src)
target_include_directories(z PRIVATE C:/msys64/mingw64/include)

target_link_libraries(z PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::SWRESAMPLE
    PkgConfig::PORTAUDIO
    SQLite::SQLite3
)

set_target_properties(z PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
