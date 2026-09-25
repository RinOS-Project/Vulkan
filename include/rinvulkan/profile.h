/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_PROFILE_H
#define RINVULKAN_PUBLIC_PROFILE_H

#include <stdint.h>

#define RIN_GPU_VULKAN_PROFILE_VERSION 1u
#define RIN_GPU_VULKAN_PHYSICAL_VERSION 2u
#define RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES 8u
#define RIN_GPU_VULKAN_MAX_MEMORY_HEAPS 8u
#define RIN_GPU_VULKAN_MAX_MEMORY_TYPES 16u
#define RIN_GPU_VULKAN_MAX_IN_FLIGHT 64u
#define RIN_GPU_VULKAN_DEVICE_NAME_SIZE 24u

#define RIN_GPU_VK_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22u) | (((uint32_t)(minor)) << 12u) | \
     ((uint32_t)(patch)))
#define RIN_GPU_VK_API_1_3 RIN_GPU_VK_MAKE_VERSION(1u, 3u, 0u)

/* The public ICD exposes the host-validated 1.0 transfer/resource profile and
 * separately enumerated host extensions for timeline semaphore and bounded
 * synchronization2.  The
 * 1.3 value above remains the version of the internal product/profile
 * contract; it must not leak into Vulkan core advertisement until the
 * corresponding command/error/lifetime semantics are wired. */
#define RIN_GPU_VK_ICD_API_VERSION RIN_GPU_VK_MAKE_VERSION(1u, 0u, 0u)
#define RIN_GPU_VK_ICD_FEATURES UINT64_C(0x0000000000000003)

#define RIN_GPU_VK_PHYSICAL_DMA_ISOLATED UINT32_C(0x00000001)
#define RIN_GPU_VK_PHYSICAL_RESET_CAPABLE UINT32_C(0x00000002)
#define RIN_GPU_VK_PHYSICAL_PRESENT UINT32_C(0x00000004)
#define RIN_GPU_VK_PHYSICAL_KNOWN UINT32_C(0x00000007)

#define RIN_GPU_VK_QUEUE_GRAPHICS UINT32_C(0x00000001)
#define RIN_GPU_VK_QUEUE_COMPUTE UINT32_C(0x00000002)
#define RIN_GPU_VK_QUEUE_TRANSFER UINT32_C(0x00000004)
#define RIN_GPU_VK_QUEUE_PRESENT UINT32_C(0x00000008)
#define RIN_GPU_VK_QUEUE_KNOWN UINT32_C(0x0000000f)

#define RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE UINT64_C(0x0000000000000001)
#define RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2 UINT64_C(0x0000000000000002)
#define RIN_GPU_VK_FEATURE_DYNAMIC_RENDERING UINT64_C(0x0000000000000004)
#define RIN_GPU_VK_FEATURE_MAINTENANCE_4 UINT64_C(0x0000000000000008)
#define RIN_GPU_VK_FEATURE_BUFFER_DEVICE_ADDRESS UINT64_C(0x0000000000000010)
#define RIN_GPU_VK_FEATURE_DESCRIPTOR_INDEXING UINT64_C(0x0000000000000020)
#define RIN_GPU_VK_FEATURE_SAMPLER_ANISOTROPY UINT64_C(0x0000000000000040)
#define RIN_GPU_VK_FEATURE_KNOWN UINT64_C(0x000000000000007f)
#define RIN_GPU_VK_FEATURE_REQUIRED_1_3 UINT64_C(0x000000000000000f)

#define RIN_GPU_VK_HEAP_DEVICE_LOCAL UINT32_C(0x00000001)
#define RIN_GPU_VK_HEAP_KNOWN UINT32_C(0x00000001)

#define RIN_GPU_VK_MEMORY_DEVICE_LOCAL UINT32_C(0x00000001)
#define RIN_GPU_VK_MEMORY_HOST_VISIBLE UINT32_C(0x00000002)
#define RIN_GPU_VK_MEMORY_HOST_COHERENT UINT32_C(0x00000004)
#define RIN_GPU_VK_MEMORY_HOST_CACHED UINT32_C(0x00000008)
#define RIN_GPU_VK_MEMORY_KNOWN UINT32_C(0x0000000f)

#define RIN_GPU_VK_PLAN_PRESENT UINT32_C(0x00000001)
#define RIN_GPU_VK_PLAN_DEDICATED_TRANSFER UINT32_C(0x00000002)

#define RIN_GPU_VK_PHYSICAL_TYPE_OTHER 0u
#define RIN_GPU_VK_PHYSICAL_TYPE_INTEGRATED_GPU 1u
#define RIN_GPU_VK_PHYSICAL_TYPE_DISCRETE_GPU 2u
#define RIN_GPU_VK_PHYSICAL_TYPE_VIRTUAL_GPU 3u
#define RIN_GPU_VK_PHYSICAL_TYPE_CPU 4u
#define RIN_GPU_VK_PHYSICAL_TYPE_MAX RIN_GPU_VK_PHYSICAL_TYPE_CPU

typedef enum RinGpuVulkanProfileResult {
    RIN_GPU_VULKAN_INCOMPLETE = 1,
    RIN_GPU_VULKAN_OK = 0,
    RIN_GPU_VULKAN_INVALID_ARGUMENT = -1,
    RIN_GPU_VULKAN_UNSUPPORTED = -2,
    RIN_GPU_VULKAN_SECURITY = -3,
    RIN_GPU_VULKAN_LIMIT = -4,
    RIN_GPU_VULKAN_BUSY = -5,
    RIN_GPU_VULKAN_INVALID_HANDLE = -6,
    RIN_GPU_VULKAN_NOT_INITIALIZED = -7
} RinGpuVulkanProfileResult;

typedef struct RinGpuVulkanQueueFamilyV1 {
    uint32_t flags;
    uint32_t queue_count;
    uint32_t timestamp_valid_bits;
    uint32_t reserved;
} RinGpuVulkanQueueFamilyV1;

typedef struct RinGpuVulkanMemoryHeapV1 {
    uint64_t size_bytes;
    uint32_t flags;
    uint32_t reserved;
} RinGpuVulkanMemoryHeapV1;

typedef struct RinGpuVulkanMemoryTypeV1 {
    uint32_t heap_index;
    uint32_t property_flags;
} RinGpuVulkanMemoryTypeV1;

typedef struct RinGpuVulkanPhysicalDeviceV2 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t api_version;
    uint32_t flags;
    uint64_t features;
    uint8_t device_uuid[16];
    uint8_t driver_digest[32];
    uint64_t iommu_domain_cookie;
    uint64_t device_epoch;
    uint32_t queue_family_count;
    uint32_t memory_heap_count;
    uint32_t memory_type_count;
    float max_sampler_anisotropy;
    uint32_t max_image_dimension_2d;
    uint32_t max_bound_descriptor_sets;
    uint32_t max_per_stage_resources;
    uint32_t max_push_constants_size;
    uint32_t max_memory_allocation_count;
    uint32_t timestamp_period_ns_x1000;
    uint64_t max_buffer_size;
    RinGpuVulkanQueueFamilyV1
        queue_families[RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES];
    RinGpuVulkanMemoryHeapV1 memory_heaps[RIN_GPU_VULKAN_MAX_MEMORY_HEAPS];
    RinGpuVulkanMemoryTypeV1 memory_types[RIN_GPU_VULKAN_MAX_MEMORY_TYPES];
    uint32_t driver_version;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t device_type;
    char device_name[RIN_GPU_VULKAN_DEVICE_NAME_SIZE];
    uint8_t pipeline_cache_uuid[16];
} RinGpuVulkanPhysicalDeviceV2;

typedef struct RinGpuVulkanCreateRequestV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t api_version;
    uint32_t flags;
    uint64_t required_features;
    uint64_t optional_features;
    uint32_t required_queue_flags;
    uint32_t surface_required;
    uint64_t minimum_device_local_bytes;
    uint64_t minimum_host_visible_bytes;
    uint32_t max_in_flight;
    uint32_t reserved0;
    uint64_t reserved[8];
} RinGpuVulkanCreateRequestV1;

typedef struct RinGpuVulkanDevicePlanV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t api_version;
    uint32_t flags;
    uint64_t enabled_features;
    uint32_t primary_queue_family;
    uint32_t transfer_queue_family;
    uint32_t device_local_heap;
    uint32_t upload_memory_type;
    uint64_t device_local_bytes;
    uint64_t host_visible_bytes;
    uint64_t iommu_domain_cookie;
    uint64_t device_epoch;
    uint8_t device_uuid[16];
    uint8_t driver_digest[32];
    uint32_t max_in_flight;
    uint32_t reserved0;
    uint64_t reserved[8];
} RinGpuVulkanDevicePlanV1;

int rin_gpu_vulkan_plan_device(
    const RinGpuVulkanPhysicalDeviceV2* physical,
    const RinGpuVulkanCreateRequestV1* request,
    RinGpuVulkanDevicePlanV1* plan_out);

#if defined(__cplusplus)
static_assert(sizeof(RinGpuVulkanPhysicalDeviceV2) == 576u,
              "RinGPU Vulkan physical profile drift");
static_assert(sizeof(RinGpuVulkanCreateRequestV1) == 128u,
              "RinGPU Vulkan request drift");
static_assert(sizeof(RinGpuVulkanDevicePlanV1) == 192u,
              "RinGPU Vulkan plan drift");
#else
_Static_assert(sizeof(RinGpuVulkanPhysicalDeviceV2) == 576u,
               "RinGPU Vulkan physical profile drift");
_Static_assert(sizeof(RinGpuVulkanCreateRequestV1) == 128u,
               "RinGPU Vulkan request drift");
_Static_assert(sizeof(RinGpuVulkanDevicePlanV1) == 192u,
               "RinGPU Vulkan plan drift");
#endif

#endif
