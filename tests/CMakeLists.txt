# tests/CMakeLists.txt - Automated Test Suite

enable_testing()

add_executable(test_keyframe
    unit/test_keyframe.cpp
    ../src/core/keyframe.cpp
)
target_include_directories(test_keyframe PRIVATE ../src)
target_link_libraries(test_keyframe PRIVATE Qt6::Core)
add_test(NAME KeyframeTests COMMAND test_keyframe)

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
