cmake_minimum_required(VERSION 3.16)
project(OpenAGC VERSION 0.1.0 LANGUAGES C)

option(OPENAGC_PS5_POLICY_ONLY "Build only the fail-closed, freestanding PS5 target" OFF)

if(NOT OPENAGC_PS5_POLICY_ONLY)
    add_library(openagc STATIC
        src/openagc.c
        src/openagc_gpu.c
        src/openagc_graphics.c
        src/openagc_frontend.c
        src/openagc_raster.c
        src/openagc_vulkan.c
        src/openagc_opengl.c
        src/openagc_sha256.c
        src/openagc_shader.c)
    add_library(OpenAGC::openagc ALIAS openagc)
    target_include_directories(openagc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set_target_properties(openagc PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
endif()

# Link this target instead of openagc in a PS5 image; it cannot touch hardware.
add_library(openagc_ps5_policy STATIC src/openagc_ps5_policy.c)
add_library(OpenAGC::ps5_policy ALIAS openagc_ps5_policy)
target_include_directories(openagc_ps5_policy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(openagc_ps5_policy PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)

if(MSVC)
    if(TARGET openagc)
        target_compile_options(openagc PRIVATE /W4)
    endif()
    target_compile_options(openagc_ps5_policy PRIVATE /W4)
else()
    if(TARGET openagc)
        target_compile_options(openagc PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_options(openagc_ps5_policy PRIVATE
        -Wall -Wextra -Wpedantic -ffreestanding -fno-builtin)
endif()

if(NOT OPENAGC_PS5_POLICY_ONLY)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(openagc_ps5_videoout_tests tests/test_ps5_videoout.c
            src/openagc_ps5_videoout.c)
        target_include_directories(openagc_ps5_videoout_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include")
        set_target_properties(openagc_ps5_videoout_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_ps5_videoout COMMAND openagc_ps5_videoout_tests)

        add_executable(openagc_tests tests/test_openagc.c)
        target_link_libraries(openagc_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_host COMMAND openagc_tests)

        add_executable(openagc_gpu_tests tests/test_openagc_gpu.c)
        target_link_libraries(openagc_gpu_tests PRIVATE OpenAGC::openagc)
        target_include_directories(openagc_gpu_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        set_target_properties(openagc_gpu_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_gpu COMMAND openagc_gpu_tests)

        add_executable(openagc_graphics_tests tests/test_openagc_graphics.c)
        target_link_libraries(openagc_graphics_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_graphics_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_graphics COMMAND openagc_graphics_tests)

        add_executable(openagc_shader_tests tests/test_openagc_shader.c)
        target_link_libraries(openagc_shader_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_shader_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_shader COMMAND openagc_shader_tests)

        add_executable(openagc_psbc_adapter_tests tests/test_openagc_psbc_adapter.c)
        target_link_libraries(openagc_psbc_adapter_tests PRIVATE OpenAGC::openagc)
        target_include_directories(openagc_psbc_adapter_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        set_target_properties(openagc_psbc_adapter_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_psbc_adapter COMMAND openagc_psbc_adapter_tests
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        add_executable(openagc_frontend_tests tests/test_openagc_frontend.c)
        target_link_libraries(openagc_frontend_tests PRIVATE OpenAGC::openagc)
        target_include_directories(openagc_frontend_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        set_target_properties(openagc_frontend_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_frontend COMMAND openagc_frontend_tests)

        add_executable(openagc_raster_tests tests/test_openagc_raster.c)
        target_link_libraries(openagc_raster_tests PRIVATE OpenAGC::openagc)
        target_include_directories(openagc_raster_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/payload")
        set_target_properties(openagc_raster_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_raster COMMAND openagc_raster_tests)

        add_executable(openagc_vulkan_tests tests/test_openagc_vulkan.c)
        target_link_libraries(openagc_vulkan_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_vulkan_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_vulkan COMMAND openagc_vulkan_tests)

        add_executable(openagc_opengl_tests tests/test_openagc_opengl.c)
        target_link_libraries(openagc_opengl_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_opengl_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_opengl COMMAND openagc_opengl_tests)

        add_executable(openagc_equivalence_tests tests/test_openagc_equivalence.c)
        target_link_libraries(openagc_equivalence_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_equivalence_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_equivalence COMMAND openagc_equivalence_tests)

        add_executable(openagc_policy_tests tests/test_ps5_policy.c)
        target_link_libraries(openagc_policy_tests PRIVATE OpenAGC::ps5_policy)
        set_target_properties(openagc_policy_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_ps5_policy COMMAND openagc_policy_tests)
    endif()
endif()
