/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_ICD_H
#define RINVULKAN_PUBLIC_ICD_H

#include <stddef.h>
#include <stdint.h>

#include <rinvulkan/platform.h>
#include <rinvulkan/runtime.h>
#include <rinvulkan/wsi_platform.h>

#if defined(_WIN32) && !defined(_WIN64)
#define RIN_VKAPI_CALL __stdcall
#else
#define RIN_VKAPI_CALL
#endif

#if defined(_WIN32)
#define RIN_VKAPI_ATTR __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define RIN_VKAPI_ATTR __attribute__((visibility("default")))
#else
#define RIN_VKAPI_ATTR
#endif

#define RIN_VK_ICD_LOADER_MAGIC UINT32_C(0x01cdc0de)
#define RIN_VK_ICD_INTERFACE_VERSION 5u
#define RIN_VK_MAX_EXTENSION_NAME_SIZE 256u
#define RIN_VK_MAX_DESCRIPTION_SIZE 256u

#define RIN_VK_PIPELINE_BIND_POINT_GRAPHICS 0u
#define RIN_VK_PIPELINE_BIND_POINT_COMPUTE 1u
#define RIN_VK_SHADER_STAGE_VERTEX_BIT UINT32_C(0x00000001)
#define RIN_VK_SHADER_STAGE_FRAGMENT_BIT UINT32_C(0x00000010)
#define RIN_VK_SHADER_STAGE_COMPUTE_BIT UINT32_C(0x00000020)

#define RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#define RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO 4
#define RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO 5
#define RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO 8
#define RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 9
#define RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 12
#define RIN_VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO 14
#define RIN_VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO 15
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO 17
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO 18
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO 19
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO 20
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO 22
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO 23
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO 24
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO 26
#define RIN_VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO 28
#define RIN_VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO 29
#define RIN_VK_STRUCTURE_TYPE_EVENT_CREATE_INFO 10
#define RIN_VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO 11
#define RIN_VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO 31
#define RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO 32
#define RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO 33
#define RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO 34
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO 30
#define RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 39
#define RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40
#define RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 42
#define RIN_VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO 16
#define RIN_VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER 44
#define RIN_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER 45
#define RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER 46
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES 50
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES 51
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES 52
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES 53
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES 54
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 1000059000
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 1000059001
#define RIN_VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO 1000207000
#define RIN_VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO 1000207001
#define RIN_VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO 1000207002
#define RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 1000314000
#define RIN_VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 1000314001
#define RIN_VK_STRUCTURE_TYPE_RENDERING_INFO 1000044000
#define RIN_VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO 1000044001
#define RIN_VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO 1000044002
#define RIN_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 1000314002
#define RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO 1000314003
#define RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2 1000314004
#define RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO 1000314005
#define RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO 1000314006
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES 1000314007
#define RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT 1000128000
#define RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT 1000128001
#define RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT 1000128002
#define RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT 1000128003
#define RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT 1000128004
#define RIN_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR 1000001000
#define RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR 1000001001
#define RIN_VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR 1000002000
#define RIN_VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR 1000002001

#define RIN_VK_SEMAPHORE_TYPE_BINARY 0u
#define RIN_VK_SEMAPHORE_TYPE_TIMELINE 1u
#define RIN_VK_SEMAPHORE_WAIT_ANY_BIT 0x00000001u
#define RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION \
    "VK_KHR_timeline_semaphore"
#define RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION \
    "VK_KHR_synchronization2"
#define RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION \
    "VK_KHR_dynamic_rendering"
#define RIN_VK_KHR_SURFACE_EXTENSION "VK_KHR_surface"
#define RIN_VK_KHR_DISPLAY_EXTENSION "VK_KHR_display"
#define RIN_VK_KHR_SWAPCHAIN_EXTENSION "VK_KHR_swapchain"
#define RIN_VK_KHR_SURFACE_SPEC_VERSION 25u
#define RIN_VK_KHR_DISPLAY_SPEC_VERSION 23u
#define RIN_VK_KHR_SWAPCHAIN_SPEC_VERSION 70u
#define RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR UINT32_C(0x00000001)
#define RIN_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR UINT32_C(0x00000001)
#define RIN_VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR UINT32_C(0x00000002)
#define RIN_VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR UINT32_C(0x00000004)
#define RIN_VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR UINT32_C(0x00000008)
#define RIN_VK_COMPOSITE_ALPHA_KNOWN_BITS_KHR UINT32_C(0x0000000f)
#define RIN_VK_PRESENT_MODE_IMMEDIATE_KHR 0
#define RIN_VK_PRESENT_MODE_MAILBOX_KHR 1
#define RIN_VK_PRESENT_MODE_FIFO_KHR 2
#define RIN_VK_PRESENT_MODE_FIFO_RELAXED_KHR 3
#define RIN_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 0
#define RIN_VK_EXT_DEBUG_UTILS_EXTENSION "VK_EXT_debug_utils"
#define RIN_VK_DEBUG_UTILS_SPEC_VERSION 2u
#define RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 0x00000001u
#define RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT 0x00000010u
#define RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 0x00000100u
#define RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT 0x00001000u
#define RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 0x00000001u
#define RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 0x00000002u
#define RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT 0x00000004u
#define RIN_VK_OBJECT_TYPE_INSTANCE 1
#define RIN_VK_OBJECT_TYPE_PHYSICAL_DEVICE 2
#define RIN_VK_OBJECT_TYPE_DEVICE 3
#define RIN_VK_OBJECT_TYPE_QUEUE 4
#define RIN_VK_OBJECT_TYPE_SEMAPHORE 5
#define RIN_VK_OBJECT_TYPE_COMMAND_BUFFER 6
#define RIN_VK_OBJECT_TYPE_FENCE 7
#define RIN_VK_OBJECT_TYPE_DEVICE_MEMORY 8
#define RIN_VK_OBJECT_TYPE_BUFFER 9
#define RIN_VK_OBJECT_TYPE_IMAGE 10
#define RIN_VK_OBJECT_TYPE_EVENT 11
#define RIN_VK_OBJECT_TYPE_QUERY_POOL 12
#define RIN_VK_OBJECT_TYPE_IMAGE_VIEW 14
#define RIN_VK_OBJECT_TYPE_SHADER_MODULE 15
#define RIN_VK_OBJECT_TYPE_PIPELINE_CACHE 16
#define RIN_VK_OBJECT_TYPE_PIPELINE_LAYOUT 17
#define RIN_VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT 20
#define RIN_VK_OBJECT_TYPE_SAMPLER 21
#define RIN_VK_OBJECT_TYPE_DESCRIPTOR_POOL 22
#define RIN_VK_OBJECT_TYPE_DESCRIPTOR_SET 23
#define RIN_VK_OBJECT_TYPE_COMMAND_POOL 25
#define RIN_VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER 1000128000
#define RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT UINT64_C(0x0000000000001000)
#define RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT UINT64_C(0x0000000000000004)
#define RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT \
    UINT64_C(0x0000000000000400)
#define RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT \
    UINT64_C(0x0000000000000800)
#define RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT UINT32_C(0x00000800)
#define RIN_VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT \
    RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT
#define RIN_VK_PIPELINE_STAGE_2_HOST_BIT UINT64_C(0x0000000000004000)
#define RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT UINT64_C(0x0000000000010000)
#define RIN_VK_PIPELINE_STAGE_2_COPY_BIT UINT64_C(0x0000000100000000)
#define RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT UINT64_C(0x0000000200000000)
#define RIN_VK_PIPELINE_STAGE_2_BLIT_BIT UINT64_C(0x0000000400000000)
#define RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT UINT64_C(0x0000000800000000)
#define RIN_VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT UINT32_C(0x00000001)
#define RIN_VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT UINT32_C(0x00000002)
#define RIN_VK_PIPELINE_STAGE_VERTEX_INPUT_BIT UINT32_C(0x00000004)
#define RIN_VK_PIPELINE_STAGE_VERTEX_SHADER_BIT UINT32_C(0x00000008)
#define RIN_VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT \
    UINT32_C(0x00000010)
#define RIN_VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT \
    UINT32_C(0x00000020)
#define RIN_VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT UINT32_C(0x00000040)
#define RIN_VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT UINT32_C(0x00000080)
#define RIN_VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT UINT32_C(0x00000100)
#define RIN_VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT UINT32_C(0x00000200)
#define RIN_VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT \
    UINT32_C(0x00000400)
#define RIN_VK_PIPELINE_STAGE_TRANSFER_BIT UINT32_C(0x00001000)
#define RIN_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT UINT32_C(0x00002000)
#define RIN_VK_PIPELINE_STAGE_HOST_BIT UINT32_C(0x00004000)
#define RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT UINT32_C(0x00008000)
#define RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT UINT32_C(0x00010000)
#define RIN_VK_ACCESS_2_TRANSFER_READ_BIT UINT64_C(0x0000000000000800)
#define RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT UINT64_C(0x0000000000001000)
#define RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT UINT64_C(0x0000000000000002)
#define RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT UINT64_C(0x0000000000000100)
#define RIN_VK_ACCESS_2_HOST_READ_BIT UINT64_C(0x0000000000002000)
#define RIN_VK_ACCESS_2_HOST_WRITE_BIT UINT64_C(0x0000000000004000)
#define RIN_VK_ACCESS_2_MEMORY_READ_BIT UINT64_C(0x0000000000008000)
#define RIN_VK_ACCESS_2_MEMORY_WRITE_BIT UINT64_C(0x0000000000010000)
#define RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT UINT64_C(0x0000000200000000)
#define RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT UINT64_C(0x0000000400000000)

#define RIN_VK_SUCCESS 0
#define RIN_VK_NOT_READY 1
#define RIN_VK_TIMEOUT 2
#define RIN_VK_INCOMPLETE 5
#define RIN_VK_ERROR_OUT_OF_HOST_MEMORY (-1)
#define RIN_VK_ERROR_OUT_OF_DEVICE_MEMORY (-2)
#define RIN_VK_ERROR_INITIALIZATION_FAILED (-3)
#define RIN_VK_ERROR_DEVICE_LOST (-4)
#define RIN_VK_ERROR_LAYER_NOT_PRESENT (-6)
#define RIN_VK_ERROR_EXTENSION_NOT_PRESENT (-7)
#define RIN_VK_ERROR_FEATURE_NOT_PRESENT (-8)
#define RIN_VK_ERROR_INCOMPATIBLE_DRIVER (-9)
#define RIN_VK_ERROR_TOO_MANY_OBJECTS (-10)
#define RIN_VK_ERROR_FORMAT_NOT_SUPPORTED (-11)
#define RIN_VK_ERROR_UNKNOWN (-13)
#define RIN_VK_SUBOPTIMAL_KHR 1000001003
#define RIN_VK_ERROR_SURFACE_LOST_KHR (-1000000000)
#define RIN_VK_ERROR_OUT_OF_DATE_KHR (-1000001004)
#define RIN_VK_ERROR_INCOMPATIBLE_DISPLAY_KHR (-1000003001)
#define RIN_VK_EVENT_RESET 0
#define RIN_VK_EVENT_SET 3

#define RIN_VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 0x00000001u
#define RIN_VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 0x00000002u
#define RIN_VK_COMMAND_POOL_CREATE_PROTECTED_BIT 0x00000004u
#define RIN_VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT 0x00000001u
#define RIN_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x00000001u
#define RIN_VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT 0x00000002u
#define RIN_VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT 0x00000004u
#define RIN_VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT 0x00000001u
#define RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
#define RIN_VK_COMMAND_BUFFER_LEVEL_SECONDARY 1

#define RIN_VK_QUERY_TYPE_OCCLUSION 0u
#define RIN_VK_QUERY_TYPE_PIPELINE_STATISTICS 1u
#define RIN_VK_QUERY_TYPE_TIMESTAMP 2u
#define RIN_VK_QUERY_CONTROL_PRECISE_BIT 0x00000001u
#define RIN_VK_QUERY_RESULT_64_BIT 0x00000001u
#define RIN_VK_QUERY_RESULT_WAIT_BIT 0x00000002u
#define RIN_VK_QUERY_RESULT_WITH_AVAILABILITY_BIT 0x00000004u
#define RIN_VK_QUERY_RESULT_PARTIAL_BIT 0x00000008u
#define RIN_VK_QUERY_RESULT_FLAGS_KNOWN \
    (RIN_VK_QUERY_RESULT_64_BIT | RIN_VK_QUERY_RESULT_WAIT_BIT | \
     RIN_VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | RIN_VK_QUERY_RESULT_PARTIAL_BIT)
#define RIN_VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT \
    0x00000001u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT \
    0x00000002u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT \
    0x00000004u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT \
    0x00000008u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT \
    0x00000010u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT \
    0x00000020u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT \
    0x00000040u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT \
    0x00000080u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT \
    0x00000100u
#define RIN_VK_QUERY_PIPELINE_STATISTIC_KNOWN \
    (RIN_VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT | \
     RIN_VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT)

typedef int32_t RinVkResult;
typedef int32_t RinVkStructureType;
typedef int32_t RinVkPhysicalDeviceType;
typedef uint32_t RinVkPipelineStageFlags;
typedef uint32_t RinVkAccessFlags;
typedef void (RIN_VKAPI_CALL *RinVkVoidFunction)(void);
typedef struct RinVkInstance_T* RinVkInstance;
typedef struct RinVkPhysicalDevice_T* RinVkPhysicalDevice;
typedef struct RinVkDevice_T* RinVkDevice;
typedef struct RinVkQueue_T* RinVkQueue;
#if UINTPTR_MAX == UINT64_MAX
typedef struct RinVkCommandPool_T* RinVkCommandPool;
#else
typedef uint64_t RinVkCommandPool;
#endif
typedef struct RinVkCommandBuffer_T* RinVkCommandBuffer;
typedef uint64_t RinVkBuffer;
typedef uint64_t RinVkImage;
typedef uint64_t RinVkImageView;
typedef uint64_t RinVkSurfaceKHR;
typedef uint64_t RinVkDisplayKHR;
typedef uint64_t RinVkDisplayModeKHR;
typedef uint64_t RinVkSwapchainKHR;
typedef int32_t RinVkPresentModeKHR;
typedef uint64_t RinVkSampler;
typedef uint64_t RinVkDeviceMemory;
typedef uint64_t RinVkFence;
typedef uint64_t RinVkSemaphore;
typedef uint64_t RinVkDescriptorSetLayout;
typedef uint64_t RinVkDescriptorPool;
typedef uint64_t RinVkDescriptorSet;
typedef uint64_t RinVkPipelineLayout;
typedef uint64_t RinVkPipelineCache;
typedef uint64_t RinVkPipeline;
typedef uint64_t RinVkShaderModule;
typedef uint64_t RinVkQueryPool;
typedef uint64_t RinVkEvent;
typedef uint64_t RinVkDebugUtilsMessengerEXT;

typedef struct RinVkDebugUtilsObjectNameInfoEXT {
    RinVkStructureType sType;
    const void* pNext;
    int32_t objectType;
    uint64_t objectHandle;
    const char* pObjectName;
} RinVkDebugUtilsObjectNameInfoEXT;

typedef struct RinVkDebugUtilsObjectTagInfoEXT {
    RinVkStructureType sType;
    const void* pNext;
    int32_t objectType;
    uint64_t objectHandle;
    uint64_t tagName;
    size_t tagSize;
    const void* pTag;
} RinVkDebugUtilsObjectTagInfoEXT;

typedef struct RinVkDebugUtilsLabelEXT {
    RinVkStructureType sType;
    const void* pNext;
    const char* pLabelName;
    float color[4];
} RinVkDebugUtilsLabelEXT;

typedef struct RinVkDebugUtilsObjectNameEXT {
    int32_t objectType;
    uint64_t objectHandle;
    const char* pObjectName;
} RinVkDebugUtilsObjectNameEXT;

typedef struct RinVkDebugUtilsLabelDataEXT {
    const char* pLabelName;
    float color[4];
} RinVkDebugUtilsLabelDataEXT;

typedef struct RinVkDebugUtilsMessengerCallbackDataEXT {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    const char* pMessageIdName;
    int32_t messageIdNumber;
    const char* pMessage;
    uint32_t queueLabelCount;
    const RinVkDebugUtilsLabelDataEXT* pQueueLabels;
    uint32_t cmdBufLabelCount;
    const RinVkDebugUtilsLabelDataEXT* pCmdBufLabels;
    uint32_t objectCount;
    const RinVkDebugUtilsObjectNameEXT* pObjects;
} RinVkDebugUtilsMessengerCallbackDataEXT;

typedef uint32_t (RIN_VKAPI_CALL *RinVkDebugUtilsMessengerCallbackEXT)(
    uint32_t message_severity, uint32_t message_types,
    const RinVkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

typedef struct RinVkDebugUtilsMessengerCreateInfoEXT {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t messageSeverity;
    uint32_t messageType;
    RinVkDebugUtilsMessengerCallbackEXT pfnUserCallback;
    void* pUserData;
} RinVkDebugUtilsMessengerCreateInfoEXT;

typedef struct RinVkPhysicalDeviceSynchronization2Features {
    RinVkStructureType sType;
    void* pNext;
    uint32_t synchronization2;
} RinVkPhysicalDeviceSynchronization2Features;

typedef struct RinVkApplicationInfo {
    RinVkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} RinVkApplicationInfo;

typedef struct RinVkInstanceCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    const RinVkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
} RinVkInstanceCreateInfo;

typedef struct RinVkExtensionProperties {
    char extensionName[RIN_VK_MAX_EXTENSION_NAME_SIZE];
    uint32_t specVersion;
} RinVkExtensionProperties;

typedef struct RinVkLayerProperties {
    char layerName[RIN_VK_MAX_EXTENSION_NAME_SIZE];
    uint32_t specVersion;
    uint32_t implementationVersion;
    char description[RIN_VK_MAX_DESCRIPTION_SIZE];
} RinVkLayerProperties;

typedef struct RinVkPhysicalDeviceFeatures {
    uint32_t robustBufferAccess;
    uint32_t fullDrawIndexUint32;
    uint32_t imageCubeArray;
    uint32_t independentBlend;
    uint32_t geometryShader;
    uint32_t tessellationShader;
    uint32_t sampleRateShading;
    uint32_t dualSrcBlend;
    uint32_t logicOp;
    uint32_t multiDrawIndirect;
    uint32_t drawIndirectFirstInstance;
    uint32_t depthClamp;
    uint32_t depthBiasClamp;
    uint32_t fillModeNonSolid;
    uint32_t depthBounds;
    uint32_t wideLines;
    uint32_t largePoints;
    uint32_t alphaToOne;
    uint32_t multiViewport;
    uint32_t samplerAnisotropy;
    uint32_t textureCompressionETC2;
    uint32_t textureCompressionASTC_LDR;
    uint32_t textureCompressionBC;
    uint32_t occlusionQueryPrecise;
    uint32_t pipelineStatisticsQuery;
    uint32_t vertexPipelineStoresAndAtomics;
    uint32_t fragmentStoresAndAtomics;
    uint32_t shaderTessellationAndGeometryPointSize;
    uint32_t shaderImageGatherExtended;
    uint32_t shaderStorageImageExtendedFormats;
    uint32_t shaderStorageImageMultisample;
    uint32_t shaderStorageImageReadWithoutFormat;
    uint32_t shaderStorageImageWriteWithoutFormat;
    uint32_t shaderUniformBufferArrayDynamicIndexing;
    uint32_t shaderSampledImageArrayDynamicIndexing;
    uint32_t shaderStorageBufferArrayDynamicIndexing;
    uint32_t shaderStorageImageArrayDynamicIndexing;
    uint32_t shaderClipDistance;
    uint32_t shaderCullDistance;
    uint32_t shaderFloat64;
    uint32_t shaderInt64;
    uint32_t shaderInt16;
    uint32_t shaderResourceResidency;
    uint32_t shaderResourceMinLod;
    uint32_t sparseBinding;
    uint32_t sparseResidencyBuffer;
    uint32_t sparseResidencyImage2D;
    uint32_t sparseResidencyImage3D;
    uint32_t sparseResidency2Samples;
    uint32_t sparseResidency4Samples;
    uint32_t sparseResidency8Samples;
    uint32_t sparseResidency16Samples;
    uint32_t sparseResidencyAliased;
    uint32_t variableMultisampleRate;
    uint32_t inheritedQueries;
} RinVkPhysicalDeviceFeatures;

typedef struct RinVkPhysicalDeviceFeatures2 {
    RinVkStructureType sType;
    void* pNext;
    RinVkPhysicalDeviceFeatures features;
} RinVkPhysicalDeviceFeatures2;

typedef struct RinVkPhysicalDeviceVulkan12Features {
    RinVkStructureType sType;
    void* pNext;
    uint32_t samplerMirrorClampToEdge;
    uint32_t drawIndirectCount;
    uint32_t storageBuffer8BitAccess;
    uint32_t uniformAndStorageBuffer8BitAccess;
    uint32_t storagePushConstant8;
    uint32_t shaderBufferInt64Atomics;
    uint32_t shaderSharedInt64Atomics;
    uint32_t shaderFloat16;
    uint32_t shaderInt8;
    uint32_t descriptorIndexing;
    uint32_t shaderInputAttachmentArrayDynamicIndexing;
    uint32_t shaderUniformTexelBufferArrayDynamicIndexing;
    uint32_t shaderStorageTexelBufferArrayDynamicIndexing;
    uint32_t shaderUniformBufferArrayNonUniformIndexing;
    uint32_t shaderSampledImageArrayNonUniformIndexing;
    uint32_t shaderStorageBufferArrayNonUniformIndexing;
    uint32_t shaderStorageImageArrayNonUniformIndexing;
    uint32_t shaderInputAttachmentArrayNonUniformIndexing;
    uint32_t shaderUniformTexelBufferArrayNonUniformIndexing;
    uint32_t shaderStorageTexelBufferArrayNonUniformIndexing;
    uint32_t descriptorBindingUniformBufferUpdateAfterBind;
    uint32_t descriptorBindingSampledImageUpdateAfterBind;
    uint32_t descriptorBindingStorageImageUpdateAfterBind;
    uint32_t descriptorBindingStorageBufferUpdateAfterBind;
    uint32_t descriptorBindingUniformTexelBufferUpdateAfterBind;
    uint32_t descriptorBindingStorageTexelBufferUpdateAfterBind;
    uint32_t descriptorBindingUpdateUnusedWhilePending;
    uint32_t descriptorBindingPartiallyBound;
    uint32_t descriptorBindingVariableDescriptorCount;
    uint32_t runtimeDescriptorArray;
    uint32_t samplerFilterMinmax;
    uint32_t scalarBlockLayout;
    uint32_t imagelessFramebuffer;
    uint32_t uniformBufferStandardLayout;
    uint32_t shaderSubgroupExtendedTypes;
    uint32_t separateDepthStencilLayouts;
    uint32_t hostQueryReset;
    uint32_t timelineSemaphore;
    uint32_t bufferDeviceAddress;
    uint32_t bufferDeviceAddressCaptureReplay;
    uint32_t bufferDeviceAddressMultiDevice;
    uint32_t vulkanMemoryModel;
    uint32_t vulkanMemoryModelDeviceScope;
    uint32_t vulkanMemoryModelAvailabilityVisibilityChains;
    uint32_t shaderOutputViewportIndex;
    uint32_t shaderOutputLayer;
    uint32_t subgroupBroadcastDynamicId;
} RinVkPhysicalDeviceVulkan12Features;

typedef struct RinVkPhysicalDeviceVulkan13Features {
    RinVkStructureType sType;
    void* pNext;
    uint32_t robustImageAccess;
    uint32_t inlineUniformBlock;
    uint32_t descriptorBindingInlineUniformBlockUpdateAfterBind;
    uint32_t pipelineCreationCacheControl;
    uint32_t privateData;
    uint32_t shaderDemoteToHelperInvocation;
    uint32_t shaderTerminateInvocation;
    uint32_t subgroupSizeControl;
    uint32_t computeFullSubgroups;
    uint32_t synchronization2;
    uint32_t textureCompressionASTC_HDR;
    uint32_t shaderZeroInitializeWorkgroupMemory;
    uint32_t dynamicRendering;
    uint32_t shaderIntegerDotProduct;
    uint32_t maintenance4;
} RinVkPhysicalDeviceVulkan13Features;

typedef struct RinVkPhysicalDeviceLimits {
    uint32_t maxImageDimension1D;
    uint32_t maxImageDimension2D;
    uint32_t maxImageDimension3D;
    uint32_t maxImageDimensionCube;
    uint32_t maxImageArrayLayers;
    uint32_t maxTexelBufferElements;
    uint32_t maxUniformBufferRange;
    uint32_t maxStorageBufferRange;
    uint32_t maxPushConstantsSize;
    uint32_t maxMemoryAllocationCount;
    uint32_t maxSamplerAllocationCount;
    uint64_t bufferImageGranularity;
    uint64_t sparseAddressSpaceSize;
    uint32_t maxBoundDescriptorSets;
    uint32_t maxPerStageDescriptorSamplers;
    uint32_t maxPerStageDescriptorUniformBuffers;
    uint32_t maxPerStageDescriptorStorageBuffers;
    uint32_t maxPerStageDescriptorSampledImages;
    uint32_t maxPerStageDescriptorStorageImages;
    uint32_t maxPerStageDescriptorInputAttachments;
    uint32_t maxPerStageResources;
    uint32_t maxDescriptorSetSamplers;
    uint32_t maxDescriptorSetUniformBuffers;
    uint32_t maxDescriptorSetUniformBuffersDynamic;
    uint32_t maxDescriptorSetStorageBuffers;
    uint32_t maxDescriptorSetStorageBuffersDynamic;
    uint32_t maxDescriptorSetSampledImages;
    uint32_t maxDescriptorSetStorageImages;
    uint32_t maxDescriptorSetInputAttachments;
    uint32_t maxVertexInputAttributes;
    uint32_t maxVertexInputBindings;
    uint32_t maxVertexInputAttributeOffset;
    uint32_t maxVertexInputBindingStride;
    uint32_t maxVertexOutputComponents;
    uint32_t maxTessellationGenerationLevel;
    uint32_t maxTessellationPatchSize;
    uint32_t maxTessellationControlPerVertexInputComponents;
    uint32_t maxTessellationControlPerVertexOutputComponents;
    uint32_t maxTessellationControlPerPatchOutputComponents;
    uint32_t maxTessellationControlTotalOutputComponents;
    uint32_t maxTessellationEvaluationInputComponents;
    uint32_t maxTessellationEvaluationOutputComponents;
    uint32_t maxGeometryShaderInvocations;
    uint32_t maxGeometryInputComponents;
    uint32_t maxGeometryOutputComponents;
    uint32_t maxGeometryOutputVertices;
    uint32_t maxGeometryTotalOutputComponents;
    uint32_t maxFragmentInputComponents;
    uint32_t maxFragmentOutputAttachments;
    uint32_t maxFragmentDualSrcAttachments;
    uint32_t maxFragmentCombinedOutputResources;
    uint32_t maxComputeSharedMemorySize;
    uint32_t maxComputeWorkGroupCount[3];
    uint32_t maxComputeWorkGroupInvocations;
    uint32_t maxComputeWorkGroupSize[3];
    uint32_t subPixelPrecisionBits;
    uint32_t subTexelPrecisionBits;
    uint32_t mipmapPrecisionBits;
    uint32_t maxDrawIndexedIndexValue;
    uint32_t maxDrawIndirectCount;
    float maxSamplerLodBias;
    float maxSamplerAnisotropy;
    uint32_t maxViewports;
    uint32_t maxViewportDimensions[2];
    float viewportBoundsRange[2];
    uint32_t viewportSubPixelBits;
    size_t minMemoryMapAlignment;
    uint64_t minTexelBufferOffsetAlignment;
    uint64_t minUniformBufferOffsetAlignment;
    uint64_t minStorageBufferOffsetAlignment;
    int32_t minTexelOffset;
    uint32_t maxTexelOffset;
    int32_t minTexelGatherOffset;
    uint32_t maxTexelGatherOffset;
    float minInterpolationOffset;
    float maxInterpolationOffset;
    uint32_t subPixelInterpolationOffsetBits;
    uint32_t maxFramebufferWidth;
    uint32_t maxFramebufferHeight;
    uint32_t maxFramebufferLayers;
    uint32_t framebufferColorSampleCounts;
    uint32_t framebufferDepthSampleCounts;
    uint32_t framebufferStencilSampleCounts;
    uint32_t framebufferNoAttachmentsSampleCounts;
    uint32_t maxColorAttachments;
    uint32_t sampledImageColorSampleCounts;
    uint32_t sampledImageIntegerSampleCounts;
    uint32_t sampledImageDepthSampleCounts;
    uint32_t sampledImageStencilSampleCounts;
    uint32_t storageImageSampleCounts;
    uint32_t maxSampleMaskWords;
    uint32_t timestampComputeAndGraphics;
    float timestampPeriod;
    uint32_t maxClipDistances;
    uint32_t maxCullDistances;
    uint32_t maxCombinedClipAndCullDistances;
    uint32_t discreteQueuePriorities;
    float pointSizeRange[2];
    float lineWidthRange[2];
    float pointSizeGranularity;
    float lineWidthGranularity;
    uint32_t strictLines;
    uint32_t standardSampleLocations;
    uint64_t optimalBufferCopyOffsetAlignment;
    uint64_t optimalBufferCopyRowPitchAlignment;
    uint64_t nonCoherentAtomSize;
} RinVkPhysicalDeviceLimits;

typedef struct RinVkPhysicalDeviceSparseProperties {
    uint32_t residencyStandard2DBlockShape;
    uint32_t residencyStandard2DMultisampleBlockShape;
    uint32_t residencyStandard3DBlockShape;
    uint32_t residencyAlignedMipSize;
    uint32_t residencyNonResidentStrict;
} RinVkPhysicalDeviceSparseProperties;

typedef struct RinVkFormatProperties {
    uint32_t linearTilingFeatures;
    uint32_t optimalTilingFeatures;
    uint32_t bufferFeatures;
} RinVkFormatProperties;

typedef struct RinVkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    RinVkPhysicalDeviceType deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
    RinVkPhysicalDeviceLimits limits;
    RinVkPhysicalDeviceSparseProperties sparseProperties;
} RinVkPhysicalDeviceProperties;

typedef struct RinVkPhysicalDeviceProperties2 {
    RinVkStructureType sType;
    void* pNext;
    RinVkPhysicalDeviceProperties properties;
} RinVkPhysicalDeviceProperties2;

typedef struct RinVkPhysicalDeviceVulkan11Properties {
    RinVkStructureType sType;
    void* pNext;
    uint8_t deviceUUID[16];
    uint8_t driverUUID[16];
    uint8_t deviceLUID[8];
    uint32_t deviceNodeMask;
    uint32_t deviceLUIDValid;
    uint32_t subgroupSize;
    uint32_t subgroupSupportedStages;
    uint32_t subgroupSupportedOperations;
    uint32_t subgroupQuadOperationsInAllStages;
    int32_t pointClippingBehavior;
    uint32_t maxMultiviewViewCount;
    uint32_t maxMultiviewInstanceIndex;
    uint32_t protectedNoFault;
    uint32_t maxPerSetDescriptors;
    uint64_t maxMemoryAllocationSize;
} RinVkPhysicalDeviceVulkan11Properties;

typedef struct RinVkConformanceVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t subminor;
    uint8_t patch;
} RinVkConformanceVersion;

typedef struct RinVkPhysicalDeviceVulkan12Properties {
    RinVkStructureType sType;
    void* pNext;
    int32_t driverID;
    char driverName[256];
    char driverInfo[256];
    RinVkConformanceVersion conformanceVersion;
    int32_t denormBehaviorIndependence;
    int32_t roundingModeIndependence;
    uint32_t shaderSignedZeroInfNanPreserveFloat16;
    uint32_t shaderSignedZeroInfNanPreserveFloat32;
    uint32_t shaderSignedZeroInfNanPreserveFloat64;
    uint32_t shaderDenormPreserveFloat16;
    uint32_t shaderDenormPreserveFloat32;
    uint32_t shaderDenormPreserveFloat64;
    uint32_t shaderDenormFlushToZeroFloat16;
    uint32_t shaderDenormFlushToZeroFloat32;
    uint32_t shaderDenormFlushToZeroFloat64;
    uint32_t shaderRoundingModeRTEFloat16;
    uint32_t shaderRoundingModeRTEFloat32;
    uint32_t shaderRoundingModeRTEFloat64;
    uint32_t shaderRoundingModeRTZFloat16;
    uint32_t shaderRoundingModeRTZFloat32;
    uint32_t shaderRoundingModeRTZFloat64;
    uint32_t maxUpdateAfterBindDescriptorsInAllPools;
    uint32_t shaderUniformBufferArrayNonUniformIndexingNative;
    uint32_t shaderSampledImageArrayNonUniformIndexingNative;
    uint32_t shaderStorageBufferArrayNonUniformIndexingNative;
    uint32_t shaderStorageImageArrayNonUniformIndexingNative;
    uint32_t shaderInputAttachmentArrayNonUniformIndexingNative;
    uint32_t robustBufferAccessUpdateAfterBind;
    uint32_t quadDivergentImplicitLod;
    uint32_t maxPerStageDescriptorUpdateAfterBindSamplers;
    uint32_t maxPerStageDescriptorUpdateAfterBindUniformBuffers;
    uint32_t maxPerStageDescriptorUpdateAfterBindStorageBuffers;
    uint32_t maxPerStageDescriptorUpdateAfterBindSampledImages;
    uint32_t maxPerStageDescriptorUpdateAfterBindStorageImages;
    uint32_t maxPerStageDescriptorUpdateAfterBindInputAttachments;
    uint32_t maxPerStageUpdateAfterBindResources;
    uint32_t maxDescriptorSetUpdateAfterBindSamplers;
    uint32_t maxDescriptorSetUpdateAfterBindUniformBuffers;
    uint32_t maxDescriptorSetUpdateAfterBindUniformBuffersDynamic;
    uint32_t maxDescriptorSetUpdateAfterBindStorageBuffers;
    uint32_t maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
    uint32_t maxDescriptorSetUpdateAfterBindSampledImages;
    uint32_t maxDescriptorSetUpdateAfterBindStorageImages;
    uint32_t maxDescriptorSetUpdateAfterBindInputAttachments;
    uint32_t supportedDepthResolveModes;
    uint32_t supportedStencilResolveModes;
    uint32_t independentResolveNone;
    uint32_t independentResolve;
    uint32_t filterMinmaxSingleComponentFormats;
    uint32_t filterMinmaxImageComponentMapping;
    uint64_t maxTimelineSemaphoreValueDifference;
    uint32_t framebufferIntegerColorSampleCounts;
} RinVkPhysicalDeviceVulkan12Properties;

typedef struct RinVkPhysicalDeviceVulkan13Properties {
    RinVkStructureType sType;
    void* pNext;
    uint32_t minSubgroupSize;
    uint32_t maxSubgroupSize;
    uint32_t maxComputeWorkgroupSubgroups;
    uint32_t requiredSubgroupSizeStages;
    uint32_t maxInlineUniformBlockSize;
    uint32_t maxPerStageDescriptorInlineUniformBlocks;
    uint32_t maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks;
    uint32_t maxDescriptorSetInlineUniformBlocks;
    uint32_t maxDescriptorSetUpdateAfterBindInlineUniformBlocks;
    uint32_t maxInlineUniformTotalSize;
    uint32_t integerDotProduct8BitUnsignedAccelerated;
    uint32_t integerDotProduct8BitSignedAccelerated;
    uint32_t integerDotProduct8BitMixedSignednessAccelerated;
    uint32_t integerDotProduct4x8BitPackedUnsignedAccelerated;
    uint32_t integerDotProduct4x8BitPackedSignedAccelerated;
    uint32_t integerDotProduct4x8BitPackedMixedSignednessAccelerated;
    uint32_t integerDotProduct16BitUnsignedAccelerated;
    uint32_t integerDotProduct16BitSignedAccelerated;
    uint32_t integerDotProduct16BitMixedSignednessAccelerated;
    uint32_t integerDotProduct32BitUnsignedAccelerated;
    uint32_t integerDotProduct32BitSignedAccelerated;
    uint32_t integerDotProduct32BitMixedSignednessAccelerated;
    uint32_t integerDotProduct64BitUnsignedAccelerated;
    uint32_t integerDotProduct64BitSignedAccelerated;
    uint32_t integerDotProduct64BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating8BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating8BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating16BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating16BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating32BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating32BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating64BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating64BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated;
    uint64_t storageTexelBufferOffsetAlignmentBytes;
    uint32_t storageTexelBufferOffsetSingleTexelAlignment;
    uint64_t uniformTexelBufferOffsetAlignmentBytes;
    uint32_t uniformTexelBufferOffsetSingleTexelAlignment;
    uint64_t maxBufferSize;
} RinVkPhysicalDeviceVulkan13Properties;

typedef struct RinVkExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} RinVkExtent3D;

typedef struct RinVkImageFormatProperties {
    RinVkExtent3D maxExtent;
    uint32_t maxMipLevels;
    uint32_t maxArrayLayers;
    uint32_t sampleCounts;
    uint64_t maxResourceSize;
} RinVkImageFormatProperties;

typedef struct RinVkSparseImageFormatProperties {
    uint32_t aspectMask;
    RinVkExtent3D imageGranularity;
    uint32_t flags;
} RinVkSparseImageFormatProperties;

typedef struct RinVkExtent2D {
    uint32_t width;
    uint32_t height;
} RinVkExtent2D;

typedef struct RinVkOffset2D {
    int32_t x;
    int32_t y;
} RinVkOffset2D;

typedef struct RinVkDisplayPropertiesKHR {
    RinVkDisplayKHR display;
    const char* displayName;
    RinVkExtent2D physicalDimensions;
    RinVkExtent2D physicalResolution;
    uint32_t supportedTransforms;
    uint32_t planeReorderPossible;
    uint32_t persistentContent;
} RinVkDisplayPropertiesKHR;

typedef struct RinVkDisplayModeParametersKHR {
    RinVkExtent2D visibleRegion;
    uint32_t refreshRate;
} RinVkDisplayModeParametersKHR;

typedef struct RinVkDisplayModePropertiesKHR {
    RinVkDisplayModeKHR displayMode;
    RinVkDisplayModeParametersKHR parameters;
} RinVkDisplayModePropertiesKHR;

typedef struct RinVkDisplayPlanePropertiesKHR {
    RinVkDisplayKHR currentDisplay;
    uint32_t currentStackIndex;
} RinVkDisplayPlanePropertiesKHR;

typedef struct RinVkDisplayPlaneCapabilitiesKHR {
    uint32_t supportedAlpha;
    RinVkOffset2D minSrcPosition;
    RinVkOffset2D maxSrcPosition;
    RinVkExtent2D minSrcExtent;
    RinVkExtent2D maxSrcExtent;
    RinVkOffset2D minDstPosition;
    RinVkOffset2D maxDstPosition;
    RinVkExtent2D minDstExtent;
    RinVkExtent2D maxDstExtent;
} RinVkDisplayPlaneCapabilitiesKHR;

#define RIN_VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR UINT32_C(0x00000001)
#define RIN_VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR UINT32_C(0x00000002)
#define RIN_VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR UINT32_C(0x00000004)
#define RIN_VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR \
    UINT32_C(0x00000008)
#define RIN_VK_DISPLAY_PLANE_ALPHA_KNOWN_BITS_KHR UINT32_C(0x0000000f)

typedef struct RinVkSurfaceCapabilitiesKHR {
    uint32_t minImageCount;
    uint32_t maxImageCount;
    RinVkExtent2D currentExtent;
    RinVkExtent2D minImageExtent;
    RinVkExtent2D maxImageExtent;
    uint32_t maxImageArrayLayers;
    uint32_t supportedTransforms;
    uint32_t currentTransform;
    uint32_t supportedCompositeAlpha;
    uint32_t supportedUsageFlags;
} RinVkSurfaceCapabilitiesKHR;

typedef struct RinVkSurfaceFormatKHR {
    int32_t format;
    int32_t colorSpace;
} RinVkSurfaceFormatKHR;

typedef struct RinVkDisplayModeCreateInfoKHR {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    RinVkDisplayModeParametersKHR parameters;
} RinVkDisplayModeCreateInfoKHR;

typedef struct RinVkDisplaySurfaceCreateInfoKHR {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    RinVkDisplayModeKHR displayMode;
    uint32_t planeIndex;
    uint32_t planeStackIndex;
    uint32_t transform;
    float globalAlpha;
    uint32_t alphaMode;
    RinVkExtent2D imageExtent;
} RinVkDisplaySurfaceCreateInfoKHR;

typedef struct RinVkSwapchainCreateInfoKHR {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    RinVkSurfaceKHR surface;
    uint32_t minImageCount;
    int32_t imageFormat;
    int32_t imageColorSpace;
    RinVkExtent2D imageExtent;
    uint32_t imageArrayLayers;
    uint32_t imageUsage;
    uint32_t imageSharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
    uint32_t preTransform;
    uint32_t compositeAlpha;
    uint32_t presentMode;
    uint32_t clipped;
    RinVkSwapchainKHR oldSwapchain;
} RinVkSwapchainCreateInfoKHR;

typedef struct RinVkPresentInfoKHR {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const RinVkSemaphore* pWaitSemaphores;
    uint32_t swapchainCount;
    const RinVkSwapchainKHR* pSwapchains;
    const uint32_t* pImageIndices;
    RinVkResult* pResults;
} RinVkPresentInfoKHR;

typedef struct RinVkQueueFamilyProperties {
    uint32_t queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    RinVkExtent3D minImageTransferGranularity;
} RinVkQueueFamilyProperties;

typedef struct RinVkMemoryType {
    uint32_t propertyFlags;
    uint32_t heapIndex;
} RinVkMemoryType;

typedef struct RinVkMemoryHeap {
    uint64_t size;
    uint32_t flags;
    uint32_t reserved;
} RinVkMemoryHeap;

typedef struct RinVkPhysicalDeviceMemoryProperties {
    uint32_t memoryTypeCount;
    RinVkMemoryType memoryTypes[32];
    uint32_t memoryHeapCount;
    RinVkMemoryHeap memoryHeaps[16];
} RinVkPhysicalDeviceMemoryProperties;

typedef struct RinVkDeviceQueueCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    const float* pQueuePriorities;
} RinVkDeviceQueueCreateInfo;

typedef struct RinVkDeviceCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueCreateInfoCount;
    const RinVkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const RinVkPhysicalDeviceFeatures* pEnabledFeatures;
} RinVkDeviceCreateInfo;

typedef struct RinVkCommandPoolCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueFamilyIndex;
} RinVkCommandPoolCreateInfo;

typedef struct RinVkCommandBufferAllocateInfo {
    RinVkStructureType sType;
    const void* pNext;
    RinVkCommandPool commandPool;
    int32_t level;
    uint32_t commandBufferCount;
} RinVkCommandBufferAllocateInfo;

typedef struct RinVkCommandBufferBeginInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    const void* pInheritanceInfo;
} RinVkCommandBufferBeginInfo;

typedef struct RinVkSubmitInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const uint64_t* pWaitSemaphores;
    const uint32_t* pWaitDstStageMask;
    uint32_t commandBufferCount;
    const RinVkCommandBuffer* pCommandBuffers;
    uint32_t signalSemaphoreCount;
    const uint64_t* pSignalSemaphores;
} RinVkSubmitInfo;

typedef struct RinVkSemaphoreSubmitInfo {
    RinVkStructureType sType;
    const void* pNext;
    RinVkSemaphore semaphore;
    uint64_t value;
    uint64_t stageMask;
    uint32_t deviceIndex;
    uint32_t reserved;
} RinVkSemaphoreSubmitInfo;

typedef struct RinVkCommandBufferSubmitInfo {
    RinVkStructureType sType;
    const void* pNext;
    RinVkCommandBuffer commandBuffer;
    uint32_t deviceMask;
    uint32_t reserved;
} RinVkCommandBufferSubmitInfo;

typedef struct RinVkSubmitInfo2 {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t waitSemaphoreInfoCount;
    const RinVkSemaphoreSubmitInfo* pWaitSemaphoreInfos;
    uint32_t commandBufferInfoCount;
    const RinVkCommandBufferSubmitInfo* pCommandBufferInfos;
    uint32_t signalSemaphoreInfoCount;
    const RinVkSemaphoreSubmitInfo* pSignalSemaphoreInfos;
} RinVkSubmitInfo2;

typedef struct RinVkMemoryBarrier2 {
    RinVkStructureType sType;
    const void* pNext;
    uint64_t srcStageMask;
    uint64_t srcAccessMask;
    uint64_t dstStageMask;
    uint64_t dstAccessMask;
} RinVkMemoryBarrier2;

typedef struct RinVkMemoryBarrier {
    RinVkStructureType sType;
    const void* pNext;
    RinVkAccessFlags srcAccessMask;
    RinVkAccessFlags dstAccessMask;
} RinVkMemoryBarrier;

typedef struct RinVkImageSubresourceRange {
    uint32_t aspectMask;
    uint32_t baseMipLevel;
    uint32_t levelCount;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
} RinVkImageSubresourceRange;

typedef struct RinVkBufferMemoryBarrier {
    RinVkStructureType sType;
    const void* pNext;
    RinVkAccessFlags srcAccessMask;
    RinVkAccessFlags dstAccessMask;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    RinVkBuffer buffer;
    uint64_t offset;
    uint64_t size;
} RinVkBufferMemoryBarrier;

typedef struct RinVkImageMemoryBarrier {
    RinVkStructureType sType;
    const void* pNext;
    RinVkAccessFlags srcAccessMask;
    RinVkAccessFlags dstAccessMask;
    uint32_t oldLayout;
    uint32_t newLayout;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    RinVkImage image;
    RinVkImageSubresourceRange subresourceRange;
} RinVkImageMemoryBarrier;

typedef struct RinVkBufferMemoryBarrier2 {
    RinVkStructureType sType;
    const void* pNext;
    uint64_t srcStageMask;
    uint64_t srcAccessMask;
    uint64_t dstStageMask;
    uint64_t dstAccessMask;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    RinVkBuffer buffer;
    uint64_t offset;
    uint64_t size;
} RinVkBufferMemoryBarrier2;

typedef struct RinVkImageMemoryBarrier2 {
    RinVkStructureType sType;
    const void* pNext;
    uint64_t srcStageMask;
    uint64_t srcAccessMask;
    uint64_t dstStageMask;
    uint64_t dstAccessMask;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    uint32_t oldLayout;
    uint32_t newLayout;
    RinVkImage image;
    RinVkImageSubresourceRange subresourceRange;
} RinVkImageMemoryBarrier2;

typedef struct RinVkDependencyInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t dependencyFlags;
    uint32_t memoryBarrierCount;
    const RinVkMemoryBarrier2* pMemoryBarriers;
    uint32_t bufferMemoryBarrierCount;
    const RinVkBufferMemoryBarrier2* pBufferMemoryBarriers;
    uint32_t imageMemoryBarrierCount;
    const RinVkImageMemoryBarrier2* pImageMemoryBarriers;
} RinVkDependencyInfo;

typedef struct RinVkTimelineSemaphoreSubmitInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreValueCount;
    const uint64_t* pWaitSemaphoreValues;
    uint32_t signalSemaphoreValueCount;
    const uint64_t* pSignalSemaphoreValues;
} RinVkTimelineSemaphoreSubmitInfo;

typedef struct RinVkFenceCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
} RinVkFenceCreateInfo;

typedef struct RinVkSemaphoreCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
} RinVkSemaphoreCreateInfo;

typedef struct RinVkSemaphoreTypeCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t semaphoreType;
    uint64_t initialValue;
} RinVkSemaphoreTypeCreateInfo;

typedef struct RinVkSemaphoreWaitInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t semaphoreCount;
    const RinVkSemaphore* pSemaphores;
    const uint64_t* pValues;
} RinVkSemaphoreWaitInfo;

typedef struct RinVkQueryPoolCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queryType;
    uint32_t queryCount;
    uint32_t pipelineStatistics;
} RinVkQueryPoolCreateInfo;

typedef struct RinVkEventCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
} RinVkEventCreateInfo;

typedef struct RinVkCommandBufferInheritanceInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint64_t renderPass;
    uint32_t subpass;
    uint64_t framebuffer;
    uint32_t occlusionQueryEnable;
    uint32_t queryFlags;
    uint32_t pipelineStatistics;
} RinVkCommandBufferInheritanceInfo;

typedef struct RinVkMemoryAllocateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint64_t allocationSize;
    uint32_t memoryTypeIndex;
} RinVkMemoryAllocateInfo;

typedef struct RinVkBufferCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint64_t size;
    uint32_t usage;
    uint32_t sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
} RinVkBufferCreateInfo;

typedef struct RinVkImageCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t imageType;
    int32_t format;
    RinVkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    uint32_t samples;
    uint32_t tiling;
    uint32_t usage;
    uint32_t sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
} RinVkImageCreateInfo;

typedef struct RinVkMemoryRequirements {
    uint64_t size;
    uint64_t alignment;
    uint32_t memoryTypeBits;
} RinVkMemoryRequirements;

typedef struct RinVkBufferCopy {
    uint64_t srcOffset;
    uint64_t dstOffset;
    uint64_t size;
} RinVkBufferCopy;

typedef struct RinVkImageSubresourceLayers {
    uint32_t aspectMask;
    uint32_t mipLevel;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
} RinVkImageSubresourceLayers;

typedef struct RinVkOffset3D {
    int32_t x;
    int32_t y;
    int32_t z;
} RinVkOffset3D;

typedef struct RinVkBufferImageCopy {
    uint64_t bufferOffset;
    uint32_t bufferRowLength;
    uint32_t bufferImageHeight;
    RinVkImageSubresourceLayers imageSubresource;
    RinVkOffset3D imageOffset;
    RinVkExtent3D imageExtent;
} RinVkBufferImageCopy;

typedef struct RinVkImageCopy {
    RinVkImageSubresourceLayers srcSubresource;
    RinVkOffset3D srcOffset;
    RinVkImageSubresourceLayers dstSubresource;
    RinVkOffset3D dstOffset;
    RinVkExtent3D extent;
} RinVkImageCopy;

typedef struct RinVkImageBlit {
    RinVkImageSubresourceLayers srcSubresource;
    RinVkOffset3D srcOffsets[2];
    RinVkImageSubresourceLayers dstSubresource;
    RinVkOffset3D dstOffsets[2];
} RinVkImageBlit;

typedef struct RinVkImageResolve {
    RinVkImageSubresourceLayers srcSubresource;
    RinVkOffset3D srcOffset;
    RinVkImageSubresourceLayers dstSubresource;
    RinVkOffset3D dstOffset;
    RinVkExtent3D extent;
} RinVkImageResolve;

typedef union RinVkClearColorValue {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
} RinVkClearColorValue;

typedef struct RinVkClearDepthStencilValue {
    float depth;
    uint32_t stencil;
} RinVkClearDepthStencilValue;

typedef struct RinVkDescriptorSetLayoutBinding {
    uint32_t binding;
    uint32_t descriptorType;
    uint32_t descriptorCount;
    uint32_t stageFlags;
    const void* pImmutableSamplers;
} RinVkDescriptorSetLayoutBinding;

typedef struct RinVkDescriptorSetLayoutCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t bindingCount;
    const RinVkDescriptorSetLayoutBinding* pBindings;
} RinVkDescriptorSetLayoutCreateInfo;

typedef struct RinVkDescriptorPoolSize {
    uint32_t type;
    uint32_t descriptorCount;
} RinVkDescriptorPoolSize;

typedef struct RinVkDescriptorPoolCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t maxSets;
    uint32_t poolSizeCount;
    const RinVkDescriptorPoolSize* pPoolSizes;
} RinVkDescriptorPoolCreateInfo;

typedef struct RinVkDescriptorSetAllocateInfo {
    RinVkStructureType sType;
    const void* pNext;
    RinVkDescriptorPool descriptorPool;
    uint32_t descriptorSetCount;
    const RinVkDescriptorSetLayout* pSetLayouts;
} RinVkDescriptorSetAllocateInfo;

typedef struct RinVkPipelineLayoutCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t setLayoutCount;
    const RinVkDescriptorSetLayout* pSetLayouts;
    uint32_t pushConstantRangeCount;
    const void* pPushConstantRanges;
} RinVkPipelineLayoutCreateInfo;

typedef struct RinVkPipelineCacheCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    size_t initialDataSize;
    const void* pInitialData;
} RinVkPipelineCacheCreateInfo;

typedef struct RinVkShaderModuleCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    size_t codeSize;
    const uint32_t* pCode;
} RinVkShaderModuleCreateInfo;

typedef struct RinVkPipelineShaderStageCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t stage;
    RinVkShaderModule module;
    const char* pName;
    const void* pSpecializationInfo;
} RinVkPipelineShaderStageCreateInfo;

typedef struct RinVkComputePipelineCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    RinVkPipelineShaderStageCreateInfo stage;
    RinVkPipelineLayout layout;
    RinVkPipeline basePipelineHandle;
    int32_t basePipelineIndex;
} RinVkComputePipelineCreateInfo;

typedef struct RinVkVertexInputBindingDescription {
    uint32_t binding;
    uint32_t stride;
    uint32_t inputRate;
} RinVkVertexInputBindingDescription;

typedef struct RinVkVertexInputAttributeDescription {
    uint32_t location;
    uint32_t binding;
    uint32_t format;
    uint32_t offset;
} RinVkVertexInputAttributeDescription;

typedef struct RinVkPipelineVertexInputStateCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t vertexBindingDescriptionCount;
    const RinVkVertexInputBindingDescription* pVertexBindingDescriptions;
    uint32_t vertexAttributeDescriptionCount;
    const RinVkVertexInputAttributeDescription* pVertexAttributeDescriptions;
} RinVkPipelineVertexInputStateCreateInfo;

typedef struct RinVkPipelineInputAssemblyStateCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t topology;
    uint32_t primitiveRestartEnable;
} RinVkPipelineInputAssemblyStateCreateInfo;

typedef struct RinVkViewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
} RinVkViewport;

typedef struct RinVkRect2D {
    RinVkOffset2D offset;
    RinVkExtent2D extent;
} RinVkRect2D;

typedef struct RinVkPipelineViewportStateCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t viewportCount;
    const RinVkViewport* pViewports;
    uint32_t scissorCount;
    const RinVkRect2D* pScissors;
} RinVkPipelineViewportStateCreateInfo;

typedef struct RinVkPipelineRasterizationStateCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t depthClampEnable;
    uint32_t rasterizerDiscardEnable;
    uint32_t polygonMode;
    uint32_t cullMode;
    uint32_t frontFace;
    uint32_t depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth;
} RinVkPipelineRasterizationStateCreateInfo;

typedef struct RinVkPipelineMultisampleStateCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t rasterizationSamples;
    uint32_t sampleShadingEnable;
    float minSampleShading;
    const uint32_t* pSampleMask;
    uint32_t alphaToCoverageEnable;
    uint32_t alphaToOneEnable;
} RinVkPipelineMultisampleStateCreateInfo;

typedef struct RinVkPipelineColorBlendAttachmentState {
    uint32_t blendEnable;
    uint32_t srcColorBlendFactor;
    uint32_t dstColorBlendFactor;
    uint32_t colorBlendOp;
    uint32_t srcAlphaBlendFactor;
    uint32_t dstAlphaBlendFactor;
    uint32_t alphaBlendOp;
    uint32_t colorWriteMask;
} RinVkPipelineColorBlendAttachmentState;

typedef struct RinVkPipelineColorBlendStateCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t logicOpEnable;
    uint32_t logicOp;
    uint32_t attachmentCount;
    const RinVkPipelineColorBlendAttachmentState* pAttachments;
    float blendConstants[4];
} RinVkPipelineColorBlendStateCreateInfo;

typedef struct RinVkPipelineRenderingCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t viewMask;
    uint32_t colorAttachmentCount;
    const int32_t* pColorAttachmentFormats;
    int32_t depthAttachmentFormat;
    int32_t stencilAttachmentFormat;
} RinVkPipelineRenderingCreateInfo;

typedef struct RinVkGraphicsPipelineCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t stageCount;
    const RinVkPipelineShaderStageCreateInfo* pStages;
    const RinVkPipelineVertexInputStateCreateInfo* pVertexInputState;
    const RinVkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
    const void* pTessellationState;
    const RinVkPipelineViewportStateCreateInfo* pViewportState;
    const RinVkPipelineRasterizationStateCreateInfo* pRasterizationState;
    const RinVkPipelineMultisampleStateCreateInfo* pMultisampleState;
    const void* pDepthStencilState;
    const RinVkPipelineColorBlendStateCreateInfo* pColorBlendState;
    const void* pDynamicState;
    RinVkPipelineLayout layout;
    uint64_t renderPass;
    uint32_t subpass;
    RinVkPipeline basePipelineHandle;
    int32_t basePipelineIndex;
} RinVkGraphicsPipelineCreateInfo;

typedef union RinVkClearValue {
    RinVkClearColorValue color;
    RinVkClearDepthStencilValue depthStencil;
} RinVkClearValue;

typedef struct RinVkRenderingAttachmentInfo {
    RinVkStructureType sType;
    const void* pNext;
    RinVkImageView imageView;
    uint32_t imageLayout;
    uint32_t resolveMode;
    RinVkImageView resolveImageView;
    uint32_t resolveImageLayout;
    uint32_t loadOp;
    uint32_t storeOp;
    RinVkClearValue clearValue;
} RinVkRenderingAttachmentInfo;

typedef struct RinVkRenderingInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    RinVkRect2D renderArea;
    uint32_t layerCount;
    uint32_t viewMask;
    uint32_t colorAttachmentCount;
    const RinVkRenderingAttachmentInfo* pColorAttachments;
    const RinVkRenderingAttachmentInfo* pDepthAttachment;
    const RinVkRenderingAttachmentInfo* pStencilAttachment;
} RinVkRenderingInfo;

typedef struct RinVkDescriptorBufferInfo {
    RinVkBuffer buffer;
    uint64_t offset;
    uint64_t range;
} RinVkDescriptorBufferInfo;

typedef struct RinVkDescriptorImageInfo {
    RinVkSampler sampler;
    RinVkImageView imageView;
    uint32_t imageLayout;
} RinVkDescriptorImageInfo;

typedef struct RinVkWriteDescriptorSet {
    RinVkStructureType sType;
    const void* pNext;
    RinVkDescriptorSet dstSet;
    uint32_t dstBinding;
    uint32_t dstArrayElement;
    uint32_t descriptorCount;
    uint32_t descriptorType;
    const RinVkDescriptorImageInfo* pImageInfo;
    const RinVkDescriptorBufferInfo* pBufferInfo;
    const RinVkBuffer* pTexelBufferView;
} RinVkWriteDescriptorSet;

typedef struct RinVkImageViewCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    RinVkImage image;
    uint32_t viewType;
    int32_t format;
    RinVkImageSubresourceRange subresourceRange;
} RinVkImageViewCreateInfo;

typedef struct RinVkSamplerCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t magFilter;
    uint32_t minFilter;
    uint32_t mipmapMode;
    uint32_t addressModeU;
    uint32_t addressModeV;
    uint32_t addressModeW;
    uint32_t compareEnable;
    uint32_t compareOp;
} RinVkSamplerCreateInfo;

#define RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT UINT32_C(0x00000001)
#define RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT UINT32_C(0x00000002)
#define RIN_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT UINT32_C(0x00000010)
#define RIN_VK_BUFFER_USAGE_STORAGE_BUFFER_BIT UINT32_C(0x00000020)
#define RIN_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT UINT32_C(0x00000080)
#define RIN_VK_BUFFER_USAGE_KNOWN \
    (RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT | RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT | \
     RIN_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | \
     RIN_VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | \
     RIN_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
#define RIN_VK_IMAGE_TYPE_2D 1u
#define RIN_VK_FORMAT_R8G8B8A8_UNORM 37
#define RIN_VK_FORMAT_D32_SFLOAT 126
#define RIN_VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT UINT32_C(0x00000080)
#define RIN_VK_IMAGE_TILING_OPTIMAL 0u
#define RIN_VK_SAMPLE_COUNT_1_BIT 1u
#define RIN_VK_SAMPLE_COUNT_2_BIT 2u
#define RIN_VK_SAMPLE_COUNT_4_BIT 4u
#define RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT UINT32_C(0x00000001)
#define RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT UINT32_C(0x00000002)
#define RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT UINT32_C(0x00000010)
#define RIN_VK_IMAGE_USAGE_KNOWN \
    (RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT | RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT | \
     RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
#define RIN_VK_IMAGE_ASPECT_COLOR_BIT UINT32_C(0x00000001)
#define RIN_VK_IMAGE_ASPECT_DEPTH_BIT UINT32_C(0x00000002)
#define RIN_VK_IMAGE_VIEW_TYPE_2D 1u
#define RIN_VK_IMAGE_VIEW_TYPE_2D_ARRAY 5u
#define RIN_VK_IMAGE_LAYOUT_GENERAL UINT32_C(1)
#define RIN_VK_IMAGE_LAYOUT_UNDEFINED UINT32_C(0)
#define RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR UINT32_C(1000001002)
#define RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL UINT32_C(6)
#define RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL UINT32_C(7)
#define RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL UINT32_C(2)
#define RIN_VK_FORMAT_R32_SFLOAT 100u
#define RIN_VK_FORMAT_R32G32_SFLOAT 103u
#define RIN_VK_FORMAT_R32G32B32_SFLOAT 106u
#define RIN_VK_FORMAT_R32G32B32A32_SFLOAT 109u
#define RIN_VK_PRIMITIVE_TOPOLOGY_POINT_LIST 0u
#define RIN_VK_PRIMITIVE_TOPOLOGY_LINE_LIST 1u
#define RIN_VK_PRIMITIVE_TOPOLOGY_LINE_STRIP 2u
#define RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST 3u
#define RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP 4u
#define RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN 5u
#define RIN_VK_POLYGON_MODE_FILL 0u
#define RIN_VK_CULL_MODE_NONE 0u
#define RIN_VK_CULL_MODE_FRONT_BIT 1u
#define RIN_VK_CULL_MODE_BACK_BIT 2u
#define RIN_VK_FRONT_FACE_COUNTER_CLOCKWISE 0u
#define RIN_VK_FRONT_FACE_CLOCKWISE 1u
#define RIN_VK_VERTEX_INPUT_RATE_VERTEX 0u
#define RIN_VK_SAMPLE_COUNT_1_BIT 1u
#define RIN_VK_ATTACHMENT_LOAD_OP_CLEAR 1u
#define RIN_VK_ATTACHMENT_STORE_OP_STORE 0u
#define RIN_VK_BLEND_FACTOR_ZERO 0u
#define RIN_VK_BLEND_FACTOR_ONE 1u
#define RIN_VK_BLEND_FACTOR_SRC_COLOR 2u
#define RIN_VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR 3u
#define RIN_VK_BLEND_FACTOR_DST_COLOR 4u
#define RIN_VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR 5u
#define RIN_VK_BLEND_FACTOR_SRC_ALPHA 6u
#define RIN_VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA 7u
#define RIN_VK_BLEND_FACTOR_DST_ALPHA 8u
#define RIN_VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA 9u
#define RIN_VK_BLEND_FACTOR_CONSTANT_COLOR 10u
#define RIN_VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR 11u
#define RIN_VK_BLEND_FACTOR_CONSTANT_ALPHA 12u
#define RIN_VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA 13u
#define RIN_VK_BLEND_FACTOR_SRC_ALPHA_SATURATE 14u
#define RIN_VK_BLEND_OP_ADD 0u
#define RIN_VK_BLEND_OP_SUBTRACT 1u
#define RIN_VK_BLEND_OP_REVERSE_SUBTRACT 2u
#define RIN_VK_BLEND_OP_MIN 3u
#define RIN_VK_BLEND_OP_MAX 4u
#define RIN_VK_COLOR_COMPONENT_R_BIT 0x1u
#define RIN_VK_COLOR_COMPONENT_G_BIT 0x2u
#define RIN_VK_COLOR_COMPONENT_B_BIT 0x4u
#define RIN_VK_COLOR_COMPONENT_A_BIT 0x8u
#define RIN_VK_QUEUE_FAMILY_IGNORED UINT32_MAX
#define RIN_VK_REMAINING_MIP_LEVELS UINT32_MAX
#define RIN_VK_REMAINING_ARRAY_LAYERS UINT32_MAX
#define RIN_VK_WHOLE_SIZE UINT64_MAX
#define RIN_VK_FILTER_NEAREST 0u
#define RIN_VK_FILTER_LINEAR 1u
#define RIN_VK_SHARING_MODE_EXCLUSIVE 0u
#define RIN_VK_FENCE_CREATE_SIGNALED_BIT UINT32_C(0x00000001)
#define RIN_VK_FENCE_CREATE_KNOWN RIN_VK_FENCE_CREATE_SIGNALED_BIT
#define RIN_VK_SEMAPHORE_CREATE_KNOWN 0u
#define RIN_VK_DESCRIPTOR_TYPE_SAMPLER 0u
#define RIN_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER 1u
#define RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE 2u
#define RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE 3u
#define RIN_VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER 4u
#define RIN_VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER 5u
#define RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER 6u
#define RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER 7u
#define RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC 8u
#define RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC 9u
#define RIN_VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT 10u
#define RIN_VK_DESCRIPTOR_TYPE_KNOWN_MAX RIN_VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
#define RIN_VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT UINT32_C(0x00000001)

#if UINTPTR_MAX == UINT64_MAX
#define RIN_VK_APPLICATION_INFO_SIZE 48u
#define RIN_VK_INSTANCE_CREATE_INFO_SIZE 64u
#define RIN_VK_FENCE_CREATE_INFO_SIZE 24u
#define RIN_VK_SEMAPHORE_CREATE_INFO_SIZE 24u
#define RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE 40u
#define RIN_VK_DEVICE_CREATE_INFO_SIZE 72u
#define RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE 24u
#define RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE 32u
#define RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE 32u
#define RIN_VK_SUBMIT_INFO_SIZE 72u
#define RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE 504u
#define RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE 824u
#define RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET 304u
#define RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET 424u
#define RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET 296u
#define RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET 800u
#define RIN_VK_PHYSICAL_FEATURES_2_SIZE 240u
#define RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET 16u
#define RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE 208u
#define RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE 80u
#define RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET 52u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET 164u
#define RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET 168u
#define RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET 52u
#define RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET 64u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET 72u
#define RIN_VK_PHYSICAL_PROPERTIES_2_SIZE 840u
#define RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET 16u
#define RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE 112u
#define RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET 104u
#define RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE 736u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET 720u
#define RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE 216u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET 208u
#else
#define RIN_VK_APPLICATION_INFO_SIZE 28u
#define RIN_VK_INSTANCE_CREATE_INFO_SIZE 32u
#define RIN_VK_FENCE_CREATE_INFO_SIZE 12u
#define RIN_VK_SEMAPHORE_CREATE_INFO_SIZE 12u
#define RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE 24u
#define RIN_VK_DEVICE_CREATE_INFO_SIZE 40u
#define RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE 16u
#define RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE 24u
#define RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE 16u
#define RIN_VK_SUBMIT_INFO_SIZE 36u
#define RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE 488u
#define RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE 800u
#define RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET 296u
#define RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET 412u
#define RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET 292u
#define RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET 780u
#define RIN_VK_PHYSICAL_FEATURES_2_SIZE 228u
#define RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET 8u
#define RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE 196u
#define RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE 68u
#define RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET 44u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET 156u
#define RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET 160u
#define RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET 44u
#define RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET 56u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET 64u
#define RIN_VK_PHYSICAL_PROPERTIES_2_SIZE 808u
#define RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET 8u
#define RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE 100u
#define RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET 92u
#define RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE 724u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET 712u
#define RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE 200u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET 192u
#endif

#if defined(__cplusplus)
#if UINTPTR_MAX == UINT64_MAX
static_assert(sizeof(RinVkShaderModuleCreateInfo) == 40u,
              "Vulkan shader-module create info ABI drift");
#else
static_assert(sizeof(RinVkShaderModuleCreateInfo) == 20u,
              "Vulkan shader-module create info ABI drift");
#endif
static_assert(sizeof(RinVkApplicationInfo) == RIN_VK_APPLICATION_INFO_SIZE,
              "Vulkan application info ABI drift");
static_assert(sizeof(RinVkInstanceCreateInfo) ==
                  RIN_VK_INSTANCE_CREATE_INFO_SIZE,
              "Vulkan instance create ABI drift");
static_assert(sizeof(RinVkExtensionProperties) == 260u,
              "Vulkan extension properties ABI drift");
static_assert(sizeof(RinVkLayerProperties) == 520u,
              "Vulkan layer properties ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceFeatures) == 220u,
              "Vulkan physical features ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceFeatures2) ==
                  RIN_VK_PHYSICAL_FEATURES_2_SIZE,
              "Vulkan physical features2 ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceFeatures2, features) ==
                  RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET,
              "Vulkan physical features2 offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan12Features) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE,
              "Vulkan 1.2 physical features ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan13Features) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE,
              "Vulkan 1.3 physical features ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                       descriptorIndexing) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET,
              "Vulkan 1.2 descriptor-indexing offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                       timelineSemaphore) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET,
              "Vulkan 1.2 timeline-semaphore offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                       bufferDeviceAddress) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET,
              "Vulkan 1.2 buffer-device-address offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                       synchronization2) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET,
              "Vulkan 1.3 synchronization2 offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                       dynamicRendering) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET,
              "Vulkan 1.3 dynamic-rendering offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                       maintenance4) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET,
              "Vulkan 1.3 maintenance4 offset drift");
static_assert(sizeof(RinVkPhysicalDeviceLimits) ==
                  RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE,
              "Vulkan physical limits ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceSparseProperties) == 20u,
              "Vulkan sparse properties ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceProperties) ==
                  RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE,
              "Vulkan physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceLimits, minMemoryMapAlignment) ==
                  RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET,
              "Vulkan physical limits map-alignment offset drift");
static_assert(offsetof(RinVkPhysicalDeviceLimits, timestampPeriod) ==
                  RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET,
              "Vulkan physical limits timestamp offset drift");
static_assert(offsetof(RinVkPhysicalDeviceProperties, limits) ==
                  RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET,
              "Vulkan physical properties limits offset drift");
static_assert(offsetof(RinVkPhysicalDeviceProperties, sparseProperties) ==
                  RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET,
              "Vulkan physical properties sparse offset drift");
static_assert(sizeof(RinVkPhysicalDeviceProperties2) ==
                  RIN_VK_PHYSICAL_PROPERTIES_2_SIZE,
              "Vulkan physical properties2 ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceProperties2, properties) ==
                  RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET,
              "Vulkan physical properties2 offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan11Properties) ==
                  RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE,
              "Vulkan 1.1 physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan11Properties,
                       maxMemoryAllocationSize) ==
                  RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET,
              "Vulkan 1.1 maximum allocation offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan12Properties) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE,
              "Vulkan 1.2 physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Properties,
                       maxTimelineSemaphoreValueDifference) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET,
              "Vulkan 1.2 timeline limit offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan13Properties) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE,
              "Vulkan 1.3 physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Properties,
                       maxBufferSize) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET,
              "Vulkan 1.3 maximum buffer offset drift");
static_assert(sizeof(RinVkQueueFamilyProperties) == 24u,
              "Vulkan queue family properties ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceMemoryProperties) == 520u,
              "Vulkan memory properties ABI drift");
static_assert(sizeof(RinVkDeviceQueueCreateInfo) ==
                  RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE,
              "Vulkan device queue create ABI drift");
static_assert(sizeof(RinVkDeviceCreateInfo) ==
                  RIN_VK_DEVICE_CREATE_INFO_SIZE,
              "Vulkan device create ABI drift");
static_assert(sizeof(RinVkCommandPoolCreateInfo) ==
                  RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE,
              "Vulkan command-pool create ABI drift");
static_assert(sizeof(RinVkCommandBufferAllocateInfo) ==
                  RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE,
              "Vulkan command-buffer allocate ABI drift");
static_assert(sizeof(RinVkCommandBufferBeginInfo) ==
                  RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE,
              "Vulkan command-buffer begin ABI drift");
static_assert(sizeof(RinVkSubmitInfo) == RIN_VK_SUBMIT_INFO_SIZE,
              "Vulkan submit ABI drift");
static_assert(sizeof(RinVkFenceCreateInfo) == RIN_VK_FENCE_CREATE_INFO_SIZE,
              "Vulkan fence-create ABI drift");
static_assert(sizeof(RinVkSemaphoreCreateInfo) ==
                  RIN_VK_SEMAPHORE_CREATE_INFO_SIZE,
              "Vulkan semaphore-create ABI drift");
static_assert(sizeof(RinVkBufferCopy) == 24u,
              "Vulkan buffer-copy ABI drift");
static_assert(sizeof(RinVkImageBlit) == 80u,
              "Vulkan image-blit ABI drift");
static_assert(sizeof(RinVkImageResolve) == 68u,
              "Vulkan image-resolve ABI drift");
static_assert(sizeof(RinVkClearColorValue) == 16u,
              "Vulkan clear-color ABI drift");
static_assert(sizeof(RinVkClearDepthStencilValue) == 8u,
              "Vulkan clear-depth ABI drift");
static_assert(sizeof(RinVkBufferMemoryBarrier2) == 80u,
              "Vulkan buffer-memory-barrier2 ABI drift");
static_assert(sizeof(RinVkImageMemoryBarrier2) == 96u,
              "Vulkan image-memory-barrier2 ABI drift");
static_assert(sizeof(RinVkImageSubresourceRange) == 20u,
              "Vulkan image-subresource-range ABI drift");
static_assert(sizeof(RinVkCommandPool) == 8u,
              "Vulkan command-pool handle ABI drift");
static_assert(sizeof(RinVkCommandBuffer) == sizeof(void*),
              "Vulkan command-buffer handle ABI drift");
#else
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(RinVkShaderModuleCreateInfo) == 40u,
               "Vulkan shader-module create info ABI drift");
#else
_Static_assert(sizeof(RinVkShaderModuleCreateInfo) == 20u,
               "Vulkan shader-module create info ABI drift");
#endif
_Static_assert(sizeof(RinVkApplicationInfo) == RIN_VK_APPLICATION_INFO_SIZE,
               "Vulkan application info ABI drift");
_Static_assert(sizeof(RinVkInstanceCreateInfo) ==
                   RIN_VK_INSTANCE_CREATE_INFO_SIZE,
               "Vulkan instance create ABI drift");
_Static_assert(sizeof(RinVkExtensionProperties) == 260u,
               "Vulkan extension properties ABI drift");
_Static_assert(sizeof(RinVkLayerProperties) == 520u,
               "Vulkan layer properties ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceFeatures) == 220u,
               "Vulkan physical features ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceFeatures2) ==
                   RIN_VK_PHYSICAL_FEATURES_2_SIZE,
               "Vulkan physical features2 ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceFeatures2, features) ==
                   RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET,
               "Vulkan physical features2 offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan12Features) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE,
               "Vulkan 1.2 physical features ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan13Features) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE,
               "Vulkan 1.3 physical features ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                        descriptorIndexing) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET,
               "Vulkan 1.2 descriptor-indexing offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                        timelineSemaphore) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET,
               "Vulkan 1.2 timeline-semaphore offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                        bufferDeviceAddress) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET,
               "Vulkan 1.2 buffer-device-address offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                        synchronization2) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET,
               "Vulkan 1.3 synchronization2 offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                        dynamicRendering) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET,
               "Vulkan 1.3 dynamic-rendering offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                        maintenance4) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET,
               "Vulkan 1.3 maintenance4 offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceLimits) ==
                   RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE,
               "Vulkan physical limits ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceSparseProperties) == 20u,
               "Vulkan sparse properties ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceProperties) ==
                   RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE,
               "Vulkan physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceLimits, minMemoryMapAlignment) ==
                   RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET,
               "Vulkan physical limits map-alignment offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceLimits, timestampPeriod) ==
                   RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET,
               "Vulkan physical limits timestamp offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceProperties, limits) ==
                   RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET,
               "Vulkan physical properties limits offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceProperties, sparseProperties) ==
                   RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET,
               "Vulkan physical properties sparse offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceProperties2) ==
                   RIN_VK_PHYSICAL_PROPERTIES_2_SIZE,
               "Vulkan physical properties2 ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceProperties2, properties) ==
                   RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET,
               "Vulkan physical properties2 offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan11Properties) ==
                   RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE,
               "Vulkan 1.1 physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan11Properties,
                        maxMemoryAllocationSize) ==
                   RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET,
               "Vulkan 1.1 maximum allocation offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan12Properties) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE,
               "Vulkan 1.2 physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Properties,
                        maxTimelineSemaphoreValueDifference) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET,
               "Vulkan 1.2 timeline limit offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan13Properties) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE,
               "Vulkan 1.3 physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Properties,
                        maxBufferSize) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET,
               "Vulkan 1.3 maximum buffer offset drift");
_Static_assert(sizeof(RinVkQueueFamilyProperties) == 24u,
               "Vulkan queue family properties ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceMemoryProperties) == 520u,
               "Vulkan memory properties ABI drift");
_Static_assert(sizeof(RinVkDeviceQueueCreateInfo) ==
                   RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE,
               "Vulkan device queue create ABI drift");
_Static_assert(sizeof(RinVkDeviceCreateInfo) ==
                   RIN_VK_DEVICE_CREATE_INFO_SIZE,
               "Vulkan device create ABI drift");
_Static_assert(sizeof(RinVkCommandPoolCreateInfo) ==
                   RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE,
               "Vulkan command-pool create ABI drift");
_Static_assert(sizeof(RinVkCommandBufferAllocateInfo) ==
                   RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE,
               "Vulkan command-buffer allocate ABI drift");
_Static_assert(sizeof(RinVkCommandBufferBeginInfo) ==
                   RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE,
               "Vulkan command-buffer begin ABI drift");
_Static_assert(sizeof(RinVkSubmitInfo) == RIN_VK_SUBMIT_INFO_SIZE,
               "Vulkan submit ABI drift");
_Static_assert(sizeof(RinVkFenceCreateInfo) == RIN_VK_FENCE_CREATE_INFO_SIZE,
               "Vulkan fence-create ABI drift");
_Static_assert(sizeof(RinVkSemaphoreCreateInfo) ==
                   RIN_VK_SEMAPHORE_CREATE_INFO_SIZE,
               "Vulkan semaphore-create ABI drift");
_Static_assert(sizeof(RinVkBufferCopy) == 24u,
                "Vulkan buffer-copy ABI drift");
_Static_assert(sizeof(RinVkImageBlit) == 80u,
               "Vulkan image-blit ABI drift");
_Static_assert(sizeof(RinVkImageResolve) == 68u,
               "Vulkan image-resolve ABI drift");
_Static_assert(sizeof(RinVkClearColorValue) == 16u,
               "Vulkan clear-color ABI drift");
_Static_assert(sizeof(RinVkClearDepthStencilValue) == 8u,
               "Vulkan clear-depth ABI drift");
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(RinVkDebugUtilsObjectNameInfoEXT) == 40u,
               "Vulkan debug object-name ABI drift");
_Static_assert(sizeof(RinVkDebugUtilsObjectTagInfoEXT) == 56u,
               "Vulkan debug object-tag ABI drift");
_Static_assert(sizeof(RinVkDebugUtilsLabelEXT) == 40u,
               "Vulkan debug label ABI drift");
_Static_assert(sizeof(RinVkDebugUtilsObjectNameEXT) == 24u,
               "Vulkan debug callback object ABI drift");
_Static_assert(sizeof(RinVkDebugUtilsLabelDataEXT) == 24u,
               "Vulkan debug callback label ABI drift");
_Static_assert(sizeof(RinVkDebugUtilsMessengerCallbackDataEXT) == 96u,
               "Vulkan debug callback data ABI drift");
_Static_assert(sizeof(RinVkDebugUtilsMessengerCreateInfoEXT) == 48u,
               "Vulkan debug messenger create ABI drift");
#endif
_Static_assert(sizeof(RinVkBufferMemoryBarrier2) == 80u,
               "Vulkan buffer-memory-barrier2 ABI drift");
_Static_assert(sizeof(RinVkImageMemoryBarrier2) == 96u,
               "Vulkan image-memory-barrier2 ABI drift");
_Static_assert(sizeof(RinVkImageSubresourceRange) == 20u,
               "Vulkan image-subresource-range ABI drift");
_Static_assert(sizeof(RinVkCommandPool) == 8u,
               "Vulkan command-pool handle ABI drift");
_Static_assert(sizeof(RinVkCommandBuffer) == sizeof(void*),
               "Vulkan command-buffer handle ABI drift");
#endif

int rin_gpu_vulkan_icd_bind_runtime(RinGpuVulkanRuntimeV1* runtime);
int rin_gpu_vulkan_icd_unbind_runtime(RinGpuVulkanRuntimeV1* runtime);
/* Resource operations are enabled only after binding the real RinGPU product
 * owner.  The binding verifies IOMMU/domain epoch per logical device before
 * every allocation; a missing or stale owner is never treated as software
 * memory. */
int rin_gpu_vulkan_icd_bind_product_platform(
    RinVulkanProductPlatformV1* platform);
int rin_gpu_vulkan_icd_unbind_product_platform(
    RinVulkanProductPlatformV1* platform);
int rin_gpu_vulkan_icd_bind_product_platform_v2(
    RinVulkanProductPlatformV2* platform);
int rin_gpu_vulkan_icd_unbind_product_platform_v2(
    RinVulkanProductPlatformV2* platform);
int rin_gpu_vulkan_icd_bind_wsi_platform(RinVulkanWsiPlatformV1* platform);
int rin_gpu_vulkan_icd_unbind_wsi_platform(RinVulkanWsiPlatformV1* platform);
int rin_gpu_vulkan_icd_bind_wsi_platform_v2(
    RinVulkanWsiPlatformV2* platform);
int rin_gpu_vulkan_icd_unbind_wsi_platform_v2(
    RinVulkanWsiPlatformV2* platform);
int rin_gpu_vulkan_icd_bind_wsi_platform_v3(
    RinVulkanWsiPlatformV3* platform);
int rin_gpu_vulkan_icd_unbind_wsi_platform_v3(
    RinVulkanWsiPlatformV3* platform);
int rin_gpu_vulkan_icd_bind_wsi_platform_v4(
    RinVulkanWsiPlatformV4* platform);
int rin_gpu_vulkan_icd_unbind_wsi_platform_v4(
    RinVulkanWsiPlatformV4* platform);
/* Product completion is driven by the RinGPU service loop, not by a forged
 * synchronous Vulkan wait.  It retires only work reported complete by the
 * product runtime and leaves still-pending command buffers leased. */
RinVkResult rin_gpu_vulkan_icd_maintain(RinVkDevice device);

RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* version);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vk_icdGetInstanceProcAddr(RinVkInstance instance, const char* name);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vk_icdGetPhysicalDeviceProcAddr(RinVkInstance instance, const char* name);

RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateInstanceVersion(uint32_t* api_version);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateInstanceExtensionProperties(
    const char* layer_name, uint32_t* property_count,
    RinVkExtensionProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDebugUtilsMessengerEXT(
    RinVkInstance instance,
    const RinVkDebugUtilsMessengerCreateInfoEXT* create_info,
    const void* allocator, RinVkDebugUtilsMessengerEXT* messenger_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyDebugUtilsMessengerEXT(RinVkInstance instance,
                                RinVkDebugUtilsMessengerEXT messenger,
                                const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkSetDebugUtilsObjectNameEXT(
    RinVkDevice device,
    const RinVkDebugUtilsObjectNameInfoEXT* name_info);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkSetDebugUtilsObjectTagEXT(
    RinVkDevice device,
    const RinVkDebugUtilsObjectTagInfoEXT* tag_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkSubmitDebugUtilsMessageEXT(
    RinVkInstance instance, uint32_t message_severity,
    uint32_t message_types,
    const RinVkDebugUtilsMessengerCallbackDataEXT* callback_data);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBeginDebugUtilsLabelEXT(
    RinVkCommandBuffer command_buffer,
    const RinVkDebugUtilsLabelEXT* label_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdEndDebugUtilsLabelEXT(RinVkCommandBuffer command_buffer);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdInsertDebugUtilsLabelEXT(
    RinVkCommandBuffer command_buffer,
    const RinVkDebugUtilsLabelEXT* label_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkQueueBeginDebugUtilsLabelEXT(RinVkQueue queue,
                               const RinVkDebugUtilsLabelEXT* label_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkQueueEndDebugUtilsLabelEXT(RinVkQueue queue);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkQueueInsertDebugUtilsLabelEXT(RinVkQueue queue,
                                const RinVkDebugUtilsLabelEXT* label_info);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateDeviceExtensionProperties(
    RinVkPhysicalDevice physical_device, const char* layer_name,
    uint32_t* property_count, RinVkExtensionProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateInstanceLayerProperties(uint32_t* property_count,
                                   RinVkLayerProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateInstance(const RinVkInstanceCreateInfo* create_info,
                 const void* allocator, RinVkInstance* instance_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyInstance(RinVkInstance instance, const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumeratePhysicalDevices(RinVkInstance instance,
                           uint32_t* physical_device_count,
                           RinVkPhysicalDevice* physical_devices);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceFeatures(RinVkPhysicalDevice physical_device,
                            RinVkPhysicalDeviceFeatures* features);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceFeatures2(RinVkPhysicalDevice physical_device,
                             RinVkPhysicalDeviceFeatures2* features);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceProperties(RinVkPhysicalDevice physical_device,
                              RinVkPhysicalDeviceProperties* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceProperties2(
    RinVkPhysicalDevice physical_device,
    RinVkPhysicalDeviceProperties2* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceFormatProperties(
    RinVkPhysicalDevice physical_device, int32_t format,
    RinVkFormatProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceImageFormatProperties(
    RinVkPhysicalDevice physical_device, int32_t format, uint32_t image_type,
    uint32_t tiling, uint32_t usage, uint32_t flags,
    RinVkImageFormatProperties* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceSparseImageFormatProperties(
    RinVkPhysicalDevice physical_device, int32_t format, uint32_t image_type,
    uint32_t samples, uint32_t usage, uint32_t tiling,
    uint32_t* property_count, RinVkSparseImageFormatProperties* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceQueueFamilyProperties(
    RinVkPhysicalDevice physical_device, uint32_t* property_count,
    RinVkQueueFamilyProperties* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceMemoryProperties(
    RinVkPhysicalDevice physical_device,
    RinVkPhysicalDeviceMemoryProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceDisplayPropertiesKHR(
    RinVkPhysicalDevice physical_device, uint32_t* property_count,
    RinVkDisplayPropertiesKHR* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetDisplayModePropertiesKHR(
    RinVkPhysicalDevice physical_device, RinVkDisplayKHR display,
    uint32_t* property_count, RinVkDisplayModePropertiesKHR* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDisplayModeKHR(
    RinVkPhysicalDevice physical_device, RinVkDisplayKHR display,
    const RinVkDisplayModeCreateInfoKHR* create_info, const void* allocator,
    RinVkDisplayModeKHR* mode_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDisplayPlaneSurfaceKHR(
    RinVkInstance instance, const RinVkDisplaySurfaceCreateInfoKHR* create_info,
    const void* allocator, RinVkSurfaceKHR* surface_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroySurfaceKHR(RinVkInstance instance, RinVkSurfaceKHR surface,
                    const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceSurfaceSupportKHR(
    RinVkPhysicalDevice physical_device, uint32_t queue_family_index,
    RinVkSurfaceKHR surface, uint32_t* supported_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface,
    RinVkSurfaceCapabilitiesKHR* capabilities_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceSurfaceFormatsKHR(
    RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface,
    uint32_t* format_count, RinVkSurfaceFormatKHR* formats);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceSurfacePresentModesKHR(
    RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface,
    uint32_t* present_mode_count, RinVkPresentModeKHR* present_modes);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateSwapchainKHR(RinVkDevice device,
                     const RinVkSwapchainCreateInfoKHR* create_info,
                     const void* allocator,
                     RinVkSwapchainKHR* swapchain_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroySwapchainKHR(RinVkDevice device, RinVkSwapchainKHR swapchain,
                      const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetSwapchainImagesKHR(RinVkDevice device, RinVkSwapchainKHR swapchain,
                        uint32_t* image_count, RinVkImage* images);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkAcquireNextImageKHR(RinVkDevice device, RinVkSwapchainKHR swapchain,
                      uint64_t timeout, RinVkSemaphore semaphore,
                      RinVkFence fence, uint32_t* image_index_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueuePresentKHR(RinVkQueue queue, const RinVkPresentInfoKHR* present_info);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
    RinVkPhysicalDevice physical_device, uint32_t* property_count,
    RinVkDisplayPlanePropertiesKHR* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetDisplayPlaneSupportedDisplaysKHR(
    RinVkPhysicalDevice physical_device, uint32_t plane_index,
    uint32_t* display_count, RinVkDisplayKHR* displays);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetDisplayPlaneCapabilitiesKHR(
    RinVkPhysicalDevice physical_device, RinVkDisplayModeKHR mode,
    uint32_t plane_index, RinVkDisplayPlaneCapabilitiesKHR* capabilities);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDevice(RinVkPhysicalDevice physical_device,
               const RinVkDeviceCreateInfo* create_info,
               const void* allocator, RinVkDevice* device_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyDevice(RinVkDevice device, const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetDeviceQueue(RinVkDevice device, uint32_t queue_family_index,
                 uint32_t queue_index, RinVkQueue* queue_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkDeviceWaitIdle(RinVkDevice device);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueueWaitIdle(RinVkQueue queue);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateFence(RinVkDevice device, const RinVkFenceCreateInfo* create_info,
              const void* allocator, RinVkFence* fence_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyFence(RinVkDevice device, RinVkFence fence, const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetFences(RinVkDevice device, uint32_t fence_count,
              const RinVkFence* fences);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetFenceStatus(RinVkDevice device, RinVkFence fence);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkWaitForFences(RinVkDevice device, uint32_t fence_count,
                const RinVkFence* fences, uint32_t wait_all,
                uint64_t timeout);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateSemaphore(RinVkDevice device,
                  const RinVkSemaphoreCreateInfo* create_info,
                  const void* allocator, RinVkSemaphore* semaphore_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroySemaphore(RinVkDevice device, RinVkSemaphore semaphore,
                   const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetSemaphoreCounterValue(RinVkDevice device, RinVkSemaphore semaphore,
                           uint64_t* value_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkSignalSemaphore(RinVkDevice device, RinVkSemaphore semaphore,
                  uint64_t value);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkWaitSemaphores(RinVkDevice device, const RinVkSemaphoreWaitInfo* wait_info,
                 uint64_t timeout);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateCommandPool(RinVkDevice device,
                    const RinVkCommandPoolCreateInfo* create_info,
                    const void* allocator, RinVkCommandPool* pool_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyCommandPool(RinVkDevice device, RinVkCommandPool command_pool,
                     const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetCommandPool(RinVkDevice device, RinVkCommandPool command_pool,
                   uint32_t flags);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkAllocateCommandBuffers(
    RinVkDevice device, const RinVkCommandBufferAllocateInfo* allocate_info,
    RinVkCommandBuffer* command_buffers);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkFreeCommandBuffers(RinVkDevice device, RinVkCommandPool command_pool,
                     uint32_t command_buffer_count,
                     const RinVkCommandBuffer* command_buffers);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkBeginCommandBuffer(RinVkCommandBuffer command_buffer,
                     const RinVkCommandBufferBeginInfo* begin_info);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEndCommandBuffer(RinVkCommandBuffer command_buffer);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetCommandBuffer(RinVkCommandBuffer command_buffer, uint32_t flags);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyBuffer(RinVkCommandBuffer command_buffer, RinVkBuffer src_buffer,
                RinVkBuffer dst_buffer, uint32_t region_count,
                const RinVkBufferCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyImage(RinVkCommandBuffer command_buffer, RinVkImage src_image,
               uint32_t src_image_layout, RinVkImage dst_image,
               uint32_t dst_image_layout, uint32_t region_count,
               const RinVkImageCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyBufferToImage(RinVkCommandBuffer command_buffer,
                       RinVkBuffer src_buffer, RinVkImage dst_image,
                       uint32_t dst_image_layout, uint32_t region_count,
                       const RinVkBufferImageCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyImageToBuffer(RinVkCommandBuffer command_buffer,
                        RinVkImage src_image, uint32_t src_image_layout,
                        RinVkBuffer dst_buffer, uint32_t region_count,
                        const RinVkBufferImageCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBlitImage(RinVkCommandBuffer command_buffer, RinVkImage src_image,
               uint32_t src_image_layout, RinVkImage dst_image,
               uint32_t dst_image_layout, uint32_t region_count,
               const RinVkImageBlit* regions, uint32_t filter);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdResolveImage(RinVkCommandBuffer command_buffer, RinVkImage src_image,
                  uint32_t src_image_layout, RinVkImage dst_image,
                  uint32_t dst_image_layout, uint32_t region_count,
                  const RinVkImageResolve* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdClearColorImage(RinVkCommandBuffer command_buffer, RinVkImage image,
                     uint32_t image_layout,
                     const RinVkClearColorValue* color,
                     uint32_t range_count,
                     const RinVkImageSubresourceRange* ranges);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdClearDepthStencilImage(
    RinVkCommandBuffer command_buffer, RinVkImage image,
    uint32_t image_layout, const RinVkClearDepthStencilValue* value,
    uint32_t range_count, const RinVkImageSubresourceRange* ranges);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdPipelineBarrier2(RinVkCommandBuffer command_buffer,
                      const RinVkDependencyInfo* dependency_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdPipelineBarrier2KHR(RinVkCommandBuffer command_buffer,
                         const RinVkDependencyInfo* dependency_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdSetEvent2(RinVkCommandBuffer command_buffer, RinVkEvent event,
               const RinVkDependencyInfo* dependency_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdSetEvent2KHR(RinVkCommandBuffer command_buffer, RinVkEvent event,
                  const RinVkDependencyInfo* dependency_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdWaitEvents2(RinVkCommandBuffer command_buffer, uint32_t event_count,
                 const RinVkEvent* events,
                 const RinVkDependencyInfo* dependency_infos);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdWaitEvents2KHR(RinVkCommandBuffer command_buffer, uint32_t event_count,
                    const RinVkEvent* events,
                    const RinVkDependencyInfo* dependency_infos);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueueSubmit(RinVkQueue queue, uint32_t submit_count,
              const RinVkSubmitInfo* submits, uint64_t fence);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueueSubmit2(RinVkQueue queue, uint32_t submit_count,
               const RinVkSubmitInfo2* submits, uint64_t fence);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueueSubmit2KHR(RinVkQueue queue, uint32_t submit_count,
                  const RinVkSubmitInfo2* submits, uint64_t fence);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkAllocateMemory(RinVkDevice device,
                 const RinVkMemoryAllocateInfo* allocate_info,
                 const void* allocator, RinVkDeviceMemory* memory_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkFreeMemory(RinVkDevice device, RinVkDeviceMemory memory,
             const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateBuffer(RinVkDevice device, const RinVkBufferCreateInfo* create_info,
               const void* allocator, RinVkBuffer* buffer_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyBuffer(RinVkDevice device, RinVkBuffer buffer,
                const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetBufferMemoryRequirements(RinVkDevice device, RinVkBuffer buffer,
                              RinVkMemoryRequirements* requirements);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkBindBufferMemory(RinVkDevice device, RinVkBuffer buffer,
                   RinVkDeviceMemory memory, uint64_t memory_offset);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateImage(RinVkDevice device, const RinVkImageCreateInfo* create_info,
              const void* allocator, RinVkImage* image_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyImage(RinVkDevice device, RinVkImage image, const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetImageMemoryRequirements(RinVkDevice device, RinVkImage image,
                             RinVkMemoryRequirements* requirements);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkBindImageMemory(RinVkDevice device, RinVkImage image,
                   RinVkDeviceMemory memory, uint64_t memory_offset);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateImageView(RinVkDevice device,
                  const RinVkImageViewCreateInfo* create_info,
                  const void* allocator, RinVkImageView* view_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyImageView(RinVkDevice device, RinVkImageView view,
                   const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateSampler(RinVkDevice device, const RinVkSamplerCreateInfo* create_info,
                const void* allocator, RinVkSampler* sampler_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroySampler(RinVkDevice device, RinVkSampler sampler,
                 const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDescriptorSetLayout(
    RinVkDevice device, const RinVkDescriptorSetLayoutCreateInfo* create_info,
    const void* allocator, RinVkDescriptorSetLayout* layout_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyDescriptorSetLayout(RinVkDevice device,
                             RinVkDescriptorSetLayout layout,
                             const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDescriptorPool(RinVkDevice device,
                       const RinVkDescriptorPoolCreateInfo* create_info,
                       const void* allocator, RinVkDescriptorPool* pool_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyDescriptorPool(RinVkDevice device, RinVkDescriptorPool pool,
                        const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkAllocateDescriptorSets(RinVkDevice device,
                         const RinVkDescriptorSetAllocateInfo* allocate_info,
                         RinVkDescriptorSet* sets_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkFreeDescriptorSets(RinVkDevice device, RinVkDescriptorPool pool,
                     uint32_t set_count, const RinVkDescriptorSet* sets);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkUpdateDescriptorSets(RinVkDevice device, uint32_t write_count,
                       const RinVkWriteDescriptorSet* writes,
                       uint32_t copy_count, const void* copies);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreatePipelineLayout(RinVkDevice device,
                       const RinVkPipelineLayoutCreateInfo* create_info,
                       const void* allocator, RinVkPipelineLayout* layout_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyPipelineLayout(RinVkDevice device, RinVkPipelineLayout layout,
                        const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateComputePipelines(RinVkDevice device, RinVkPipelineCache pipeline_cache,
                         uint32_t create_info_count,
                         const RinVkComputePipelineCreateInfo* create_infos,
                         const void* allocator, RinVkPipeline* pipelines);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateGraphicsPipelines(
    RinVkDevice device, RinVkPipelineCache pipeline_cache,
    uint32_t create_info_count,
    const RinVkGraphicsPipelineCreateInfo* create_infos,
    const void* allocator, RinVkPipeline* pipelines);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyPipeline(RinVkDevice device, RinVkPipeline pipeline,
                  const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreatePipelineCache(RinVkDevice device,
                      const RinVkPipelineCacheCreateInfo* create_info,
                      const void* allocator, RinVkPipelineCache* cache_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyPipelineCache(RinVkDevice device, RinVkPipelineCache cache,
                       const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetPipelineCacheData(RinVkDevice device, RinVkPipelineCache cache,
                       size_t* data_size, void* data);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkMergePipelineCaches(RinVkDevice device, RinVkPipelineCache dst_cache,
                      uint32_t src_cache_count,
                      const RinVkPipelineCache* src_caches);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateShaderModule(RinVkDevice device,
                     const RinVkShaderModuleCreateInfo* create_info,
                     const void* allocator,
                     RinVkShaderModule* shader_module_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyShaderModule(RinVkDevice device, RinVkShaderModule shader_module,
                      const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBindDescriptorSets(RinVkCommandBuffer command_buffer,
                         uint32_t pipeline_bind_point,
                         RinVkPipelineLayout layout, uint32_t first_set,
                         uint32_t descriptor_set_count,
                         const RinVkDescriptorSet* descriptor_sets,
                         uint32_t dynamic_offset_count,
                         const uint32_t* dynamic_offsets);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBindPipeline(RinVkCommandBuffer command_buffer,
                  uint32_t pipeline_bind_point, RinVkPipeline pipeline);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBeginRendering(RinVkCommandBuffer command_buffer,
                    const RinVkRenderingInfo* rendering_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBeginRenderingKHR(RinVkCommandBuffer command_buffer,
                       const RinVkRenderingInfo* rendering_info);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdEndRendering(RinVkCommandBuffer command_buffer);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdEndRenderingKHR(RinVkCommandBuffer command_buffer);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBindVertexBuffers(RinVkCommandBuffer command_buffer,
                       uint32_t first_binding, uint32_t binding_count,
                       const RinVkBuffer* buffers, const uint64_t* offsets);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdDraw(RinVkCommandBuffer command_buffer, uint32_t vertex_count,
          uint32_t instance_count, uint32_t first_vertex,
          uint32_t first_instance);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdDispatch(RinVkCommandBuffer command_buffer, uint32_t group_count_x,
              uint32_t group_count_y, uint32_t group_count_z);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateQueryPool(RinVkDevice device,
                  const RinVkQueryPoolCreateInfo* create_info,
                  const void* allocator, RinVkQueryPool* query_pool_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyQueryPool(RinVkDevice device, RinVkQueryPool query_pool,
                   const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetQueryPoolResults(RinVkDevice device, RinVkQueryPool query_pool,
                      uint32_t first_query, uint32_t query_count,
                      size_t data_size, void* data, uint64_t stride,
                      uint32_t flags);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdResetQueryPool(RinVkCommandBuffer command_buffer,
                    RinVkQueryPool query_pool, uint32_t first_query,
                    uint32_t query_count);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdBeginQuery(RinVkCommandBuffer command_buffer, RinVkQueryPool query_pool,
                uint32_t query, uint32_t flags);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdEndQuery(RinVkCommandBuffer command_buffer, RinVkQueryPool query_pool,
              uint32_t query);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdWriteTimestamp(RinVkCommandBuffer command_buffer, uint64_t stage,
                    RinVkQueryPool query_pool, uint32_t query);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdWriteTimestamp2(RinVkCommandBuffer command_buffer, uint64_t stage,
                     RinVkQueryPool query_pool, uint32_t query);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdWriteTimestamp2KHR(RinVkCommandBuffer command_buffer, uint64_t stage,
                        RinVkQueryPool query_pool, uint32_t query);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateEvent(RinVkDevice device, const RinVkEventCreateInfo* create_info,
              const void* allocator, RinVkEvent* event_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyEvent(RinVkDevice device, RinVkEvent event, const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetEventStatus(RinVkDevice device, RinVkEvent event);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkSetEvent(RinVkDevice device, RinVkEvent event);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetEvent(RinVkDevice device, RinVkEvent event);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdSetEvent(RinVkCommandBuffer command_buffer, RinVkEvent event,
              RinVkPipelineStageFlags stage);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdResetEvent2(RinVkCommandBuffer command_buffer, RinVkEvent event,
                 uint64_t stage_mask);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdResetEvent2KHR(RinVkCommandBuffer command_buffer, RinVkEvent event,
                    uint64_t stage_mask);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdResetEvent(RinVkCommandBuffer command_buffer, RinVkEvent event,
                RinVkPipelineStageFlags stage);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdWaitEvents(RinVkCommandBuffer command_buffer, uint32_t event_count,
                const RinVkEvent* events,
                RinVkPipelineStageFlags src_stage_mask,
                RinVkPipelineStageFlags dst_stage_mask,
                uint32_t memory_barrier_count,
                const RinVkMemoryBarrier* memory_barriers,
                uint32_t buffer_barrier_count,
                const RinVkBufferMemoryBarrier* buffer_barriers,
                uint32_t image_barrier_count,
                const RinVkImageMemoryBarrier* image_barriers);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vkGetDeviceProcAddr(RinVkDevice device, const char* name);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vkGetInstanceProcAddr(RinVkInstance instance, const char* name);

#endif
