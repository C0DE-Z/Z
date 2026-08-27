cmake_minimum_required(VERSION 3.20)
project(z VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 1. Platform configuration
include(Platform)

# 2. Third-party dependencies
include(Dependencies)

# Object detection is optional: the editor keeps its lightweight region detector
# when an OpenCV DNN build (and a YOLO ONNX model) is not available.
find_package(OpenCV QUIET COMPONENTS core dnn imgproc)

# 3. Source File Definitions
set(Z_SOURCES
    src/main.cpp
    src/core/keyframe.cpp
    src/core/project.cpp
    src/core/appstate.cpp
    src/core/shortcutmanager.cpp
    src/media/mediaimporter.cpp
    src/media/mediaexporter.cpp
    src/engine/pluginmanager.cpp
    src/engine/videodecoder.cpp
    src/engine/cpueffects.cpp
    src/engine/detector.cpp
    src/engine/videoengine.cpp
    src/engine/audioengine.cpp
    src/engine/glwidget.cpp
    src/engine/shaders.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow_docks.cpp
    src/ui/vector_icons.cpp
    src/ui/components/timeline.cpp
    src/ui/components/inspector.cpp
    src/ui/components/effectsbrowser.cpp
    src/ui/components/mediapool.cpp
    src/ui/components/trackcontrol.cpp
    src/ui/preferencesdialog.cpp
)

set(Z_HEADERS
    src/core/logging.h
    src/core/keyframe.h
    src/core/project.h
    src/core/appstate.h
    src/core/shortcutmanager.h
    src/media/mediaimporter.h
    src/media/mediaexporter.h
    src/engine/pluginmanager.h
    src/engine/renderbackend.h
    src/engine/videoframe.h
    src/engine/videodecoder.h
    src/engine/cpueffects.h
    src/engine/detector.h
    src/engine/videoengine.h
    src/engine/audioengine.h
    src/engine/glwidget.h
    src/engine/shaders.h
    src/ui/mainwindow.h
    src/ui/vector_icons.h
    src/ui/preferencesdialog.h
    src/ui/components/timeline.h
    src/ui/components/inspector.h
    src/ui/components/effectsbrowser.h
    src/ui/components/mediapool.h
    src/ui/components/trackcontrol.h
)

if(APPLE)
    add_executable(z MACOSX_BUNDLE ${Z_SOURCES} ${Z_HEADERS})
else()
    add_executable(z ${Z_SOURCES} ${Z_HEADERS})
endif()

target_include_directories(z PRIVATE src)

# 4. Compiler warnings & options
include(CompilerOptions)

target_link_libraries(z PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    OpenGL::GL
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::SWRESAMPLE
    PkgConfig::PORTAUDIO
    SQLite::SQLite3
)

if(OpenCV_FOUND)
    target_include_directories(z PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(z PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(z PRIVATE Z_HAS_OPENCV_DNN=1)
endif()

if(APPLE)
    target_link_libraries(z PRIVATE "-framework OpenGL" "-framework Cocoa")
elseif(UNIX AND NOT APPLE)
    target_link_libraries(z PRIVATE GL)
endif()

set_target_properties(z PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 5. Automated Tests
enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
