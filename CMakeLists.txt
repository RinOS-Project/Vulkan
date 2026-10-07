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

option(RINVULKAN_BUILD_VALIDATION_SMOKE
    "Build a shared ICD and exercise it through the Khronos Vulkan loader and validation layer"
    OFF)
if(RINVULKAN_BUILD_VALIDATION_SMOKE)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR
            "RINVULKAN_BUILD_VALIDATION_SMOKE requires the POSIX Vulkan loader")
    endif()
    find_package(Vulkan REQUIRED)
    set_target_properties(RinGPU PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_library(RinVulkanICD SHARED ${RINVULKAN_SOURCES})
    target_compile_features(RinVulkanICD PRIVATE c_std_11)
    target_include_directories(RinVulkanICD
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(RinVulkanICD PRIVATE RinGPU::RinGPU)
    set_target_properties(RinVulkanICD PROPERTIES
        OUTPUT_NAME "rin-vulkan-icd"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # An ICD exports Vulkan entry points but must keep its own references
        # bound locally when it shares a process with the system loader.
        target_link_options(RinVulkanICD PRIVATE "-Wl,-Bsymbolic")
    endif()
    if(MSVC)
        target_compile_options(RinVulkanICD PRIVATE /W4)
    else()
        target_compile_options(RinVulkanICD PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set(RIN_VULKAN_ICD_LIBRARY
        "${CMAKE_SHARED_LIBRARY_PREFIX}rin-vulkan-icd${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(RIN_VULKAN_ICD_LIBRARY_RELATIVE_DIR "../../../lib")
    set(RIN_VULKAN_ICD_MANIFEST_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/share/vulkan/icd.d")
    file(MAKE_DIRECTORY "${RIN_VULKAN_ICD_MANIFEST_DIR}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/icd.d/rin-vulkan.json.in"
        "${RIN_VULKAN_ICD_MANIFEST_DIR}/rin-vulkan.json" @ONLY)
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${RINGPU_DIR}/include)
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
    if(RINVULKAN_BUILD_VALIDATION_SMOKE)
        add_executable(rinvulkan-loader-validation-test
            tests/rinvulkan_loader_validation_test.c)
        target_compile_features(rinvulkan-loader-validation-test PRIVATE
            c_std_11)
        target_include_directories(rinvulkan-loader-validation-test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(rinvulkan-loader-validation-test PRIVATE
            Vulkan::Vulkan ${CMAKE_DL_LIBS})
        add_dependencies(rinvulkan-loader-validation-test RinVulkanICD)
        if(MSVC)
            target_compile_options(rinvulkan-loader-validation-test PRIVATE
                /W4)
        else()
            target_compile_options(rinvulkan-loader-validation-test PRIVATE
                -Wall -Wextra -Wpedantic -Werror)
        endif()
        add_test(NAME rinvulkan-loader-validation
            COMMAND rinvulkan-loader-validation-test
                "$<TARGET_FILE:RinVulkanICD>")
        set_tests_properties(rinvulkan-loader-validation PROPERTIES
            ENVIRONMENT
                "VK_DRIVER_FILES=${RIN_VULKAN_ICD_MANIFEST_DIR}/rin-vulkan.json;VK_ICD_FILENAMES=${RIN_VULKAN_ICD_MANIFEST_DIR}/rin-vulkan.json")
    endif()
endif()
