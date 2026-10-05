cmake_minimum_required(VERSION 3.16)
project(RinVulkan VERSION 1.0.0 LANGUAGES C)

set(RINGPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RinGPU" CACHE PATH
    "RinGPU source directory")
if(NOT EXISTS "${RINGPU_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "RinGPU is required; set RINGPU_DIR to its checkout")
endif()

if(NOT TARGET RinGPU::RinGPU)
    set(RINGPU_BUILD_TESTS OFF CACHE BOOL "Disable RinGPU tests in RinVulkan" FORCE)
    set(RINGPU_BUILD_DIFFERENTIAL_TESTS OFF CACHE BOOL
        "Disable RinGPU differential tests in RinVulkan" FORCE)
    add_subdirectory("${RINGPU_DIR}" "ringpu-build" EXCLUDE_FROM_ALL)
endif()

set(RINVULKAN_SOURCES
    src/buffer_ownership.c
    src/command_runtime.c
    src/descriptor_runtime.c
    src/graphics.c
    src/graphics_runtime.c
    src/icd.c
    src/profile.c
    src/runtime.c
    src/platform/time.c
    src/software_platform.c
    src/sync2_scope.c
    src/wsi.c)
add_library(RinVulkan STATIC ${RINVULKAN_SOURCES})
add_library(RinVulkan::RinVulkan ALIAS RinVulkan)
target_compile_features(RinVulkan PUBLIC c_std_11)
target_include_directories(RinVulkan
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(RinVulkan PUBLIC RinGPU::RinGPU)
if(MSVC)
    target_compile_options(RinVulkan PRIVATE /W4)
else()
    target_compile_options(RinVulkan PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    if(NOT WIN32)
        find_package(Threads REQUIRED)
    endif()
    add_executable(rinvulkan-abi-test tests/rinvulkan_abi_test.c)
    target_link_libraries(rinvulkan-abi-test PRIVATE RinVulkan::RinVulkan)
    add_test(NAME rinvulkan-abi COMMAND rinvulkan-abi-test)
    add_executable(rinvulkan-graphics-runtime-test
        tests/rinvulkan_graphics_runtime_test.c)
    target_link_libraries(rinvulkan-graphics-runtime-test PRIVATE
        RinVulkan::RinVulkan)
    add_test(NAME rinvulkan-graphics-runtime
        COMMAND rinvulkan-graphics-runtime-test)
    add_executable(rinvulkan-atomic-compat-test
        tests/rinvulkan_atomic_compat_test.c)
    target_compile_features(rinvulkan-atomic-compat-test PRIVATE c_std_11)
    add_test(NAME rinvulkan-atomic-compat
        COMMAND rinvulkan-atomic-compat-test)
    add_executable(rinvulkan-resource-thread-test
        tests/rinvulkan_resource_thread_test.c)
    target_compile_features(rinvulkan-resource-thread-test PRIVATE c_std_11)
    target_link_libraries(rinvulkan-resource-thread-test PRIVATE
        RinVulkan::RinVulkan)
    if(NOT WIN32)
        target_link_libraries(rinvulkan-resource-thread-test PRIVATE
            Threads::Threads)
    endif()
    add_test(NAME rinvulkan-resource-thread
        COMMAND rinvulkan-resource-thread-test)
    add_executable(rinvulkan-wsi-display-test
        tests/rinvulkan_wsi_display_test.c)
    target_compile_features(rinvulkan-wsi-display-test PRIVATE c_std_11)
    target_link_libraries(rinvulkan-wsi-display-test PRIVATE
        RinVulkan::RinVulkan)
    add_test(NAME rinvulkan-wsi-display
        COMMAND rinvulkan-wsi-display-test)
    add_executable(rinvulkan-buffer-ownership-test
        tests/rinvulkan_buffer_ownership_test.c
        src/buffer_ownership.c)
    target_compile_features(rinvulkan-buffer-ownership-test PRIVATE c_std_11)
    target_include_directories(rinvulkan-buffer-ownership-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME rinvulkan-buffer-ownership
        COMMAND rinvulkan-buffer-ownership-test)
    add_executable(rinvulkan-sync2-scope-test
        tests/rinvulkan_sync2_scope_test.c
        src/sync2_scope.c)
    target_compile_features(rinvulkan-sync2-scope-test PRIVATE c_std_11)
    target_include_directories(rinvulkan-sync2-scope-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME rinvulkan-sync2-scope
        COMMAND rinvulkan-sync2-scope-test)
    add_executable(rinvulkan-platform-time-test
        tests/rinvulkan_platform_time_test.c
        src/platform/time.c)
    target_compile_features(rinvulkan-platform-time-test PRIVATE c_std_11)
    target_include_directories(rinvulkan-platform-time-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME rinvulkan-platform-time
        COMMAND rinvulkan-platform-time-test)
endif()
