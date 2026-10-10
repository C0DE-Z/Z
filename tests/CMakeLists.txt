
enable_testing()

set(Z_WINDOWS_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/dist")

function(copy_runtime_dlls_to_target target_name)
    if(NOT WIN32)
        return()
    endif()

    if(EXISTS "${Z_WINDOWS_RUNTIME_DIR}")
        file(GLOB Z_RUNTIME_DLLS "${Z_WINDOWS_RUNTIME_DIR}/*.dll")
        foreach(runtime_dll IN LISTS Z_RUNTIME_DLLS)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${runtime_dll}"
                    "$<TARGET_FILE_DIR:${target_name}>"
                VERBATIM
            )
        endforeach()
    endif()
endfunction()

add_executable(test_keyframe
    unit/test_keyframe.cpp
    ../src/core/keyframe.cpp
)
target_include_directories(test_keyframe PRIVATE ../src)
target_link_libraries(test_keyframe PRIVATE Qt6::Core)
copy_runtime_dlls_to_target(test_keyframe)
add_test(NAME KeyframeTests COMMAND test_keyframe)

add_executable(test_mediaimporter
    unit/test_mediaimporter.cpp
    ../src/media/mediaimporter.cpp
)
target_include_directories(test_mediaimporter PRIVATE ../src)
target_link_libraries(test_mediaimporter PRIVATE Qt6::Core Qt6::Widgets PkgConfig::AVFORMAT)
copy_runtime_dlls_to_target(test_mediaimporter)
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
copy_runtime_dlls_to_target(test_appstate)
add_test(NAME AppStateTests COMMAND test_appstate)

add_executable(test_project
    unit/test_project.cpp
    ../src/core/project.cpp
    ../src/core/keyframe.cpp
    ../src/engine/pluginmanager.cpp
)
target_include_directories(test_project PRIVATE ../src)
target_link_libraries(test_project PRIVATE Qt6::Core Qt6::Gui OpenGL::GL)
copy_runtime_dlls_to_target(test_project)
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
copy_runtime_dlls_to_target(test_detector)
add_test(NAME DetectorTests COMMAND test_detector)

add_executable(test_maskgenerator
    unit/test_maskgenerator.cpp
    ../src/engine/maskgenerator.cpp
    ../src/core/keyframe.cpp
)
target_include_directories(test_maskgenerator PRIVATE ../src)
target_link_libraries(test_maskgenerator PRIVATE Qt6::Gui)
copy_runtime_dlls_to_target(test_maskgenerator)
add_test(NAME MaskGeneratorTests COMMAND test_maskgenerator)

if(WIN32 AND EXISTS "${Z_WINDOWS_RUNTIME_DIR}")
    set(Z_TEST_PATH_ENV "PATH=${Z_WINDOWS_RUNTIME_DIR};$ENV{PATH}")
    foreach(test_name IN ITEMS KeyframeTests MediaImporterTests AppStateTests ProjectTests DetectorTests MaskGeneratorTests)
        set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${Z_TEST_PATH_ENV}")
    endforeach()
endif()
