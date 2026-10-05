/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_WSI_PLATFORM_H
#define RINVULKAN_PUBLIC_WSI_PLATFORM_H

#include <stdint.h>

#define RIN_VULKAN_WSI_PLATFORM_VERSION 1u
#define RIN_VULKAN_WSI_MAX_DISPLAYS 16u
#define RIN_VULKAN_WSI_MAX_MODES 64u
#define RIN_VULKAN_WSI_DISPLAY_NAME_SIZE 64u
#define RIN_VULKAN_WSI_DISPLAY_TRANSFORM_IDENTITY UINT32_C(0x00000001)
#define RIN_VULKAN_WSI_DISPLAY_PLANE_REORDER UINT32_C(0x00000002)
#define RIN_VULKAN_WSI_DISPLAY_PERSISTENT_CONTENT UINT32_C(0x00000004)
#define RIN_VULKAN_WSI_DISPLAY_FLAGS_KNOWN UINT32_C(0x00000007)

typedef enum RinVulkanWsiPlatformResult {
    RIN_VULKAN_WSI_PLATFORM_OK = 0,
    RIN_VULKAN_WSI_PLATFORM_INCOMPLETE = 1,
    RIN_VULKAN_WSI_PLATFORM_NOT_READY = 2,
    RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE = -1,
    RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST = -2,
    RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED = -3,
    RIN_VULKAN_WSI_PLATFORM_BACKEND = -4,
    RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT = -5,
    RIN_VULKAN_WSI_PLATFORM_LIMIT = -6
} RinVulkanWsiPlatformResult;

typedef struct RinVulkanWsiDisplayV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t display_cookie;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t current_mode_cookie;
    uint32_t width;
    uint32_t height;
    uint32_t refresh_millihertz;
    uint32_t format;
    uint32_t physical_width_mm;
    uint32_t physical_height_mm;
    uint32_t flags;
    uint32_t plane_count;
    uint32_t mode_count;
    char display_name[RIN_VULKAN_WSI_DISPLAY_NAME_SIZE];
    uint64_t reserved[2];
} RinVulkanWsiDisplayV1;

typedef struct RinVulkanWsiModeV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t display_cookie;
    uint64_t mode_cookie;
    uint64_t output_generation;
    uint32_t width;
    uint32_t height;
    uint32_t refresh_millihertz;
    uint32_t format;
    uint32_t flags;
    uint64_t reserved[1];
} RinVulkanWsiModeV1;

typedef struct RinVulkanWsiPresentRequestV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t display_cookie;
    uint64_t mode_cookie;
    uint64_t allocation_handle;
    uint64_t allocation_offset;
    uint64_t allocation_size;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t frame_id;
    uint32_t image_index;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t present_mode;
    uint32_t flags;
    uint64_t reserved[2];
} RinVulkanWsiPresentRequestV1;

typedef struct RinVulkanWsiPresentStatusV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t state;
    uint32_t reserved0;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t reserved[2];
} RinVulkanWsiPresentStatusV1;

#define RIN_VULKAN_WSI_PRESENT_PENDING 1u
#define RIN_VULKAN_WSI_PRESENT_COMPLETE 2u

typedef int (*RinVulkanWsiQueryDisplaysFn)(
    void* context, uint64_t device_generation, uint32_t capacity,
    uint32_t* count_out, RinVulkanWsiDisplayV1* displays_out);
typedef int (*RinVulkanWsiQueryModesFn)(
    void* context, uint64_t device_generation, uint64_t display_cookie,
    uint64_t output_generation, uint32_t capacity, uint32_t* count_out,
    RinVulkanWsiModeV1* modes_out);
/* Success means the OS-Core display owner has retained the exact allocation
 * until poll_present reports COMPLETE or cancel_present retires its token. */
typedef int (*RinVulkanWsiPresentFn)(
    void* context, const RinVulkanWsiPresentRequestV1* request,
    uint64_t* present_token_out);
typedef int (*RinVulkanWsiPollPresentFn)(
    void* context, uint64_t display_cookie, uint64_t present_token,
    RinVulkanWsiPresentStatusV1* status_out);
typedef int (*RinVulkanWsiCancelPresentFn)(
    void* context, uint64_t display_cookie, uint64_t present_token);

/* This is an OS-Core adapter, not another common backend operation table.
 * The RinGPU command and resource contracts remain authoritative; this
 * versioned boundary supplies display topology and scanout-specific work. */
typedef struct RinVulkanWsiPlatformV1 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinVulkanWsiQueryDisplaysFn query_displays;
    RinVulkanWsiQueryModesFn query_modes;
    RinVulkanWsiPresentFn present;
    RinVulkanWsiPollPresentFn poll_present;
    RinVulkanWsiCancelPresentFn cancel_present;
    uint64_t reserved[4];
} RinVulkanWsiPlatformV1;

#if defined(__cplusplus)
static_assert(sizeof(RinVulkanWsiDisplayV1) == 160u,
              "Vulkan WSI display ABI drift");
static_assert(sizeof(RinVulkanWsiModeV1) == 64u,
              "Vulkan WSI mode ABI drift");
static_assert(sizeof(RinVulkanWsiPresentRequestV1) == 112u,
              "Vulkan WSI present request ABI drift");
static_assert(sizeof(RinVulkanWsiPresentStatusV1) == 48u,
              "Vulkan WSI present status ABI drift");
static_assert(sizeof(RinVulkanWsiPlatformV1) ==
                  (sizeof(void*) == 8u ? 88u : 64u),
              "Vulkan WSI platform ABI drift");
#else
_Static_assert(sizeof(RinVulkanWsiDisplayV1) == 160u,
               "Vulkan WSI display ABI drift");
_Static_assert(sizeof(RinVulkanWsiModeV1) == 64u,
               "Vulkan WSI mode ABI drift");
_Static_assert(sizeof(RinVulkanWsiPresentRequestV1) == 112u,
               "Vulkan WSI present request ABI drift");
_Static_assert(sizeof(RinVulkanWsiPresentStatusV1) == 48u,
               "Vulkan WSI present status ABI drift");
_Static_assert(sizeof(RinVulkanWsiPlatformV1) ==
                   (sizeof(void*) == 8u ? 88u : 64u),
               "Vulkan WSI platform ABI drift");
#endif

#endif /* RINVULKAN_PUBLIC_WSI_PLATFORM_H */
