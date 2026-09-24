cmake_minimum_required(VERSION 3.16)
project(OpenAGC VERSION 0.1.0 LANGUAGES C)

option(OPENAGC_PS5_POLICY_ONLY "Build only the fail-closed, freestanding PS5 target" OFF)

if(NOT OPENAGC_PS5_POLICY_ONLY)
    add_library(openagc STATIC
        src/openagc.c
        src/openagc_gpu.c
        src/openagc_graphics.c
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
        add_executable(openagc_tests tests/test_openagc.c)
        target_link_libraries(openagc_tests PRIVATE OpenAGC::openagc)
        set_target_properties(openagc_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_host COMMAND openagc_tests)

        add_executable(openagc_gpu_tests tests/test_openagc_gpu.c)
        target_link_libraries(openagc_gpu_tests PRIVATE OpenAGC::openagc)
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

        add_executable(openagc_policy_tests tests/test_ps5_policy.c)
        target_link_libraries(openagc_policy_tests PRIVATE OpenAGC::ps5_policy)
        set_target_properties(openagc_policy_tests PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME openagc_ps5_policy COMMAND openagc_policy_tests)
    endif()
endif()
