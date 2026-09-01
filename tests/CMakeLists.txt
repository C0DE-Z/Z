# tests/CMakeLists.txt - Automated Test Suite

enable_testing()

add_executable(test_keyframe
    unit/test_keyframe.cpp
    ../src/core/keyframe.cpp
)
target_include_directories(test_keyframe PRIVATE ../src)
target_link_libraries(test_keyframe PRIVATE Qt6::Core)
add_test(NAME KeyframeTests COMMAND test_keyframe)

add_executable(test_mediaimporter
    unit/test_mediaimporter.cpp
    ../src/media/mediaimporter.cpp
)
target_include_directories(test_mediaimporter PRIVATE ../src)
target_link_libraries(test_mediaimporter PRIVATE Qt6::Core Qt6::Widgets PkgConfig::AVFORMAT)
add_test(NAME MediaImporterTests COMMAND test_mediaimporter)

add_executable(test_appstate
    unit/test_appstate.cpp
    ../src/core/appstate.cpp
    ../src/core/project.cpp
    ../src/core/keyframe.cpp
    ../src/engine/pluginmanager.cpp
)
target_include_directories(test_appstate PRIVATE ../src)
target_link_libraries(test_appstate PRIVATE Qt6::Core Qt6::Gui OpenGL::GL)
add_test(NAME AppStateTests COMMAND test_appstate)

add_executable(test_project
    unit/test_project.cpp
    ../src/core/project.cpp
    ../src/core/keyframe.cpp
    ../src/engine/pluginmanager.cpp
)
target_include_directories(test_project PRIVATE ../src)
target_link_libraries(test_project PRIVATE Qt6::Core Qt6::Gui OpenGL::GL)
add_test(NAME ProjectTests COMMAND test_project)

add_executable(test_detector
    unit/test_detector.cpp
    ../src/engine/detector.cpp
)
target_include_directories(test_detector PRIVATE ../src)
target_link_libraries(test_detector PRIVATE Qt6::Gui)
if(OpenCV_FOUND)
    target_include_directories(test_detector PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(test_detector PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(test_detector PRIVATE Z_HAS_OPENCV_DNN=1)
endif()
add_test(NAME DetectorTests COMMAND test_detector)
