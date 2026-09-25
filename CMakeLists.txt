cmake_minimum_required(VERSION 3.16)
project(RinVulkan VERSION 1.0.0 LANGUAGES C)

set(RINGPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RinGPU" CACHE PATH
    "RinGPU source directory")
if(NOT EXISTS "${RINGPU_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "RinGPU is required; set RINGPU_DIR to its checkout")
endif()

set(RINGPU_BUILD_TESTS OFF CACHE BOOL "Disable RinGPU tests in RinVulkan" FORCE)
set(RINGPU_BUILD_DIFFERENTIAL_TESTS OFF CACHE BOOL
    "Disable RinGPU differential tests in RinVulkan" FORCE)
add_subdirectory("${RINGPU_DIR}" "ringpu-build" EXCLUDE_FROM_ALL)

set(RINVULKAN_SOURCES
    src/command_runtime.c
    src/graphics.c
    src/icd.c
    src/profile.c
    src/runtime.c
    src/software_platform.c
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
    add_executable(rinvulkan-abi-test tests/rinvulkan_abi_test.c)
    target_link_libraries(rinvulkan-abi-test PRIVATE RinVulkan::RinVulkan)
    add_test(NAME rinvulkan-abi COMMAND rinvulkan-abi-test)
endif()
