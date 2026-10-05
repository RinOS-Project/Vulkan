/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_WSI_PLATFORM_H
#define RINVULKAN_PUBLIC_WSI_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#define RIN_VULKAN_WSI_PLATFORM_VERSION 1u
#define RIN_VULKAN_WSI_PLATFORM_V2_VERSION 2u
#define RIN_VULKAN_WSI_PLATFORM_V3_VERSION 3u
#define RIN_VULKAN_WSI_PLATFORM_V4_VERSION 4u
#define RIN_VULKAN_WSI_MAX_DISPLAYS 16u
#define RIN_VULKAN_WSI_MAX_MODES 64u
#define RIN_VULKAN_WSI_MAX_PLANES 16u
#define RIN_VULKAN_WSI_DISPLAY_NAME_SIZE 64u
#define RIN_VULKAN_WSI_DISPLAY_TRANSFORM_IDENTITY UINT32_C(0x00000001)
#define RIN_VULKAN_WSI_DISPLAY_PLANE_REORDER UINT32_C(0x00000002)
#define RIN_VULKAN_WSI_DISPLAY_PERSISTENT_CONTENT UINT32_C(0x00000004)
#define RIN_VULKAN_WSI_DISPLAY_FLAGS_KNOWN UINT32_C(0x00000007)
#define RIN_VULKAN_WSI_MAX_SURFACE_FORMATS 16u
#define RIN_VULKAN_WSI_MAX_SURFACE_PRESENT_MODES 8u

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

typedef struct RinVulkanWsiDisplayPlaneV2 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t plane_index;
    uint32_t current_stack_index;
    uint64_t current_display_cookie;
    uint64_t output_generation;
    uint64_t device_generation;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t reserved[2];
} RinVulkanWsiDisplayPlaneV2;

typedef struct RinVulkanWsiPlaneCapabilitiesV2 {
    uint32_t supported_alpha;
    int32_t min_src_x;
    int32_t min_src_y;
    int32_t max_src_x;
    int32_t max_src_y;
    uint32_t min_src_width;
    uint32_t min_src_height;
    uint32_t max_src_width;
    uint32_t max_src_height;
    int32_t min_dst_x;
    int32_t min_dst_y;
    int32_t max_dst_x;
    int32_t max_dst_y;
    uint32_t min_dst_width;
    uint32_t min_dst_height;
    uint32_t max_dst_width;
    uint32_t max_dst_height;
} RinVulkanWsiPlaneCapabilitiesV2;

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
typedef int (*RinVulkanWsiQueryPlanesV2Fn)(
    void* context, uint64_t device_generation, uint32_t capacity,
    uint32_t* count_out, RinVulkanWsiDisplayPlaneV2* planes_out);
typedef int (*RinVulkanWsiQueryPlaneDisplaysV2Fn)(
    void* context, uint64_t device_generation, uint32_t plane_index,
    uint32_t capacity, uint32_t* count_out,
    uint64_t* display_cookies_out);
typedef int (*RinVulkanWsiQueryPlaneCapabilitiesV2Fn)(
    void* context, uint64_t device_generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    RinVulkanWsiPlaneCapabilitiesV2* capabilities_out);
typedef int (*RinVulkanWsiQuerySurfaceSupportV3Fn)(
    void* context, uint64_t device_generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    uint32_t queue_family_index, uint32_t queue_flags, uint32_t queue_count,
    uint32_t* supported_out);

typedef struct RinVulkanWsiSurfaceFormatV4 {
    int32_t format;
    int32_t color_space;
} RinVulkanWsiSurfaceFormatV4;

/* Pointer-free, bounded surface properties reported by the output owner.
 * Extent values and bitmasks use Vulkan's standardized numeric values. */
typedef struct RinVulkanWsiSurfacePropertiesV4 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t min_image_count;
    uint32_t max_image_count;
    uint32_t current_extent_width;
    uint32_t current_extent_height;
    uint32_t min_image_extent_width;
    uint32_t min_image_extent_height;
    uint32_t max_image_extent_width;
    uint32_t max_image_extent_height;
    uint32_t max_image_array_layers;
    uint32_t supported_transforms;
    uint32_t current_transform;
    uint32_t supported_composite_alpha;
    uint32_t supported_usage_flags;
    uint32_t format_count;
    uint32_t present_mode_count;
    uint32_t reserved0[2];
    RinVulkanWsiSurfaceFormatV4 formats[RIN_VULKAN_WSI_MAX_SURFACE_FORMATS];
    int32_t present_modes[RIN_VULKAN_WSI_MAX_SURFACE_PRESENT_MODES];
    uint32_t reserved[4];
} RinVulkanWsiSurfacePropertiesV4;

typedef int (*RinVulkanWsiQuerySurfacePropertiesV4Fn)(
    void* context, uint64_t device_generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    RinVulkanWsiSurfacePropertiesV4* properties_out);

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

/* V2 preserves the V1 callback prefix and adds the OS-Core plane discovery
 * contract. query_planes returns records ordered by dense Vulkan planeIndex
 * (0..count-1); an unattached plane has current_display_cookie and
 * output_generation both zero. The V2 flags/reserved fields must be zero.
 * query_plane_supported_displays returns display cookies from the same
 * device generation. It is a separate ABI version: V1 callers remain
 * binary-stable. */
typedef struct RinVulkanWsiPlatformV2 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinVulkanWsiQueryDisplaysFn query_displays;
    RinVulkanWsiQueryModesFn query_modes;
    RinVulkanWsiPresentFn present;
    RinVulkanWsiPollPresentFn poll_present;
    RinVulkanWsiCancelPresentFn cancel_present;
    uint64_t reserved[4];
    RinVulkanWsiQueryPlanesV2Fn query_planes;
    RinVulkanWsiQueryPlaneDisplaysV2Fn query_plane_supported_displays;
    RinVulkanWsiQueryPlaneCapabilitiesV2Fn query_plane_capabilities;
    uint64_t reserved_v2[4];
} RinVulkanWsiPlatformV2;

/* V3 preserves the complete V2 prefix and adds an explicit query for whether
 * a concrete queue family can present to a generation-bound display plane.
 * A callback must derive the answer from the OS-Core output/queue routing; it
 * must not infer support merely from the presence of present callbacks. */
typedef struct RinVulkanWsiPlatformV3 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinVulkanWsiQueryDisplaysFn query_displays;
    RinVulkanWsiQueryModesFn query_modes;
    RinVulkanWsiPresentFn present;
    RinVulkanWsiPollPresentFn poll_present;
    RinVulkanWsiCancelPresentFn cancel_present;
    uint64_t reserved[4];
    RinVulkanWsiQueryPlanesV2Fn query_planes;
    RinVulkanWsiQueryPlaneDisplaysV2Fn query_plane_supported_displays;
    RinVulkanWsiQueryPlaneCapabilitiesV2Fn query_plane_capabilities;
    uint64_t reserved_v2[4];
    RinVulkanWsiQuerySurfaceSupportV3Fn query_surface_support;
    uint64_t reserved_v3[4];
} RinVulkanWsiPlatformV3;

/* V4 preserves the V3 prefix and requires the output owner to report the
 * actual surface limits, formats, and present modes for the selected output
 * generation. The ICD validates and converts this bounded record. */
typedef struct RinVulkanWsiPlatformV4 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinVulkanWsiQueryDisplaysFn query_displays;
    RinVulkanWsiQueryModesFn query_modes;
    RinVulkanWsiPresentFn present;
    RinVulkanWsiPollPresentFn poll_present;
    RinVulkanWsiCancelPresentFn cancel_present;
    uint64_t reserved[4];
    RinVulkanWsiQueryPlanesV2Fn query_planes;
    RinVulkanWsiQueryPlaneDisplaysV2Fn query_plane_supported_displays;
    RinVulkanWsiQueryPlaneCapabilitiesV2Fn query_plane_capabilities;
    uint64_t reserved_v2[4];
    RinVulkanWsiQuerySurfaceSupportV3Fn query_surface_support;
    uint64_t reserved_v3[4];
    RinVulkanWsiQuerySurfacePropertiesV4Fn query_surface_properties;
    uint64_t reserved_v4[4];
} RinVulkanWsiPlatformV4;

#if defined(__cplusplus)
static_assert(sizeof(RinVulkanWsiDisplayPlaneV2) == 64u,
              "Vulkan WSI plane ABI drift");
static_assert(sizeof(RinVulkanWsiPlaneCapabilitiesV2) == 68u,
              "Vulkan WSI plane capabilities ABI drift");
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
static_assert(offsetof(RinVulkanWsiPlatformV2, query_planes) ==
                  sizeof(RinVulkanWsiPlatformV1),
              "Vulkan WSI V2 callback prefix drift");
static_assert(sizeof(RinVulkanWsiPlatformV2) ==
                  (sizeof(void*) == 8u ? 144u : 108u),
              "Vulkan WSI platform V2 ABI drift");
static_assert(offsetof(RinVulkanWsiPlatformV3, query_surface_support) ==
                  sizeof(RinVulkanWsiPlatformV2),
              "Vulkan WSI V3 callback prefix drift");
static_assert(sizeof(RinVulkanWsiPlatformV3) ==
                  (sizeof(void*) == 8u ? 184u : 144u),
              "Vulkan WSI platform V3 ABI drift");
static_assert(sizeof(RinVulkanWsiSurfaceFormatV4) == 8u,
              "Vulkan WSI surface format ABI drift");
static_assert(sizeof(RinVulkanWsiSurfacePropertiesV4) == 252u,
              "Vulkan WSI surface properties ABI drift");
static_assert(offsetof(RinVulkanWsiPlatformV4, query_surface_properties) ==
                  sizeof(RinVulkanWsiPlatformV3),
              "Vulkan WSI V4 callback prefix drift");
static_assert(sizeof(RinVulkanWsiPlatformV4) ==
                  (sizeof(void*) == 8u ? 224u : 180u),
              "Vulkan WSI platform V4 ABI drift");
#else
_Static_assert(sizeof(RinVulkanWsiDisplayPlaneV2) == 64u,
               "Vulkan WSI plane ABI drift");
_Static_assert(sizeof(RinVulkanWsiPlaneCapabilitiesV2) == 68u,
               "Vulkan WSI plane capabilities ABI drift");
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
_Static_assert(offsetof(RinVulkanWsiPlatformV2, query_planes) ==
                   sizeof(RinVulkanWsiPlatformV1),
               "Vulkan WSI V2 callback prefix drift");
_Static_assert(sizeof(RinVulkanWsiPlatformV2) ==
                   (sizeof(void*) == 8u ? 144u : 108u),
               "Vulkan WSI platform V2 ABI drift");
_Static_assert(offsetof(RinVulkanWsiPlatformV3, query_surface_support) ==
                   sizeof(RinVulkanWsiPlatformV2),
               "Vulkan WSI V3 callback prefix drift");
_Static_assert(sizeof(RinVulkanWsiPlatformV3) ==
                   (sizeof(void*) == 8u ? 184u : 144u),
               "Vulkan WSI platform V3 ABI drift");
_Static_assert(sizeof(RinVulkanWsiSurfaceFormatV4) == 8u,
               "Vulkan WSI surface format ABI drift");
_Static_assert(sizeof(RinVulkanWsiSurfacePropertiesV4) == 252u,
               "Vulkan WSI surface properties ABI drift");
_Static_assert(offsetof(RinVulkanWsiPlatformV4, query_surface_properties) ==
                   sizeof(RinVulkanWsiPlatformV3),
               "Vulkan WSI V4 callback prefix drift");
_Static_assert(sizeof(RinVulkanWsiPlatformV4) ==
                   (sizeof(void*) == 8u ? 224u : 180u),
               "Vulkan WSI platform V4 ABI drift");
#endif

#endif /* RINVULKAN_PUBLIC_WSI_PLATFORM_H */
