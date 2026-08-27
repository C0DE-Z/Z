# CompilerOptions.cmake - Standardized compiler flags and warnings

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX-
        /permissive-
        /Zc:__cplusplus
        /utf-8
        $<$<CONFIG:Release>:/O2 /Oi /Ot /GL>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-unused-function
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-g3 -O0>
    )
endif()
