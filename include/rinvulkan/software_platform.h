/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_SOFTWARE_PLATFORM_H
#define RINVULKAN_PUBLIC_SOFTWARE_PLATFORM_H

#include <stdint.h>

#include <rinvulkan/command_runtime.h>
#include <rinvulkan/platform.h>

#define RIN_GPU_VULKAN_SOFTWARE_PLATFORM_VERSION 1u
#define RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS 64u
#define RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS 64u

/* Host-only RinVulkan product owner.  It is a real bounded executor for the
 * transfer packet ABI: allocations own zeroed byte storage, GPU addresses are
 * validated against that storage, and submit publishes a completion report
 * only after every copy has executed.  It is not a physical GPU backend. */
typedef struct RinGpuVulkanSoftwareAllocationV1 {
    uint64_t handle;
    uint64_t gpu_virtual_address;
    uint64_t size_bytes;
    uint32_t heap;
    uint32_t flags;
    uint8_t* bytes;
} RinGpuVulkanSoftwareAllocationV1;

typedef struct RinGpuVulkanSoftwarePlatformV1 {
    RinVulkanProductPlatformV1 platform;
    uint32_t initialized;
    uint32_t queue_count;
    uint64_t iommu_domain_cookie;
    uint64_t device_epoch;
    uint64_t iommu_map_generation;
    uint64_t next_allocation_handle;
    uint64_t next_gpu_virtual_address;
    uint64_t next_sequence;
    uint64_t completed_values[RIN_VULKAN_PRODUCT_MAX_QUEUES];
    uint64_t max_total_bytes;
    uint64_t total_bytes;
    uint32_t report_head;
    uint32_t report_tail;
    RinVulkanProductReportV1 reports[RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS];
    RinGpuVulkanSoftwareAllocationV1
        allocations[RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS];
} RinGpuVulkanSoftwarePlatformV1;

int rin_gpu_vulkan_software_platform_init(
    RinGpuVulkanSoftwarePlatformV1* platform, uint64_t iommu_domain_cookie,
    uint64_t device_epoch, uint32_t queue_count, uint64_t max_total_bytes);
int rin_gpu_vulkan_software_platform_shutdown(
    RinGpuVulkanSoftwarePlatformV1* platform);

#endif /* RINVULKAN_PUBLIC_SOFTWARE_PLATFORM_H */
