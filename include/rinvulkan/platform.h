/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_PLATFORM_H
#define RINVULKAN_PUBLIC_PLATFORM_H

#include <stdint.h>

#define RIN_VULKAN_PRODUCT_PLATFORM_VERSION 1u
#define RIN_VULKAN_PRODUCT_MEMORY_VERSION 1u
#define RIN_VULKAN_PRODUCT_MAX_QUEUES 8u
#define RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION 8u

#define RIN_VULKAN_PRODUCT_STATUS_READY UINT32_C(0x00000001)
#define RIN_VULKAN_PRODUCT_STATUS_SUSPENDED UINT32_C(0x00000002)
#define RIN_VULKAN_PRODUCT_STATUS_RECOVERY_REQUIRED UINT32_C(0x00000004)
#define RIN_VULKAN_PRODUCT_STATUS_LOST UINT32_C(0x00000008)

#define RIN_VULKAN_PRODUCT_REPORT_RESET UINT32_C(0x00000001)
#define RIN_VULKAN_PRODUCT_REPORT_WATCHDOG UINT32_C(0x00000002)
#define RIN_VULKAN_PRODUCT_REPORT_DEVICE_FAULT UINT32_C(0x00000004)
#define RIN_VULKAN_PRODUCT_REPORT_MEMORY_REBOUND UINT32_C(0x00000008)
#define RIN_VULKAN_PRODUCT_REPORT_KNOWN \
    (RIN_VULKAN_PRODUCT_REPORT_RESET | RIN_VULKAN_PRODUCT_REPORT_WATCHDOG | \
     RIN_VULKAN_PRODUCT_REPORT_DEVICE_FAULT | \
     RIN_VULKAN_PRODUCT_REPORT_MEMORY_REBOUND)

#define RIN_VULKAN_PRODUCT_MEMORY_HEAP_LOCAL 1u
#define RIN_VULKAN_PRODUCT_MEMORY_HEAP_SYSTEM 2u
#define RIN_VULKAN_PRODUCT_MEMORY_GPU_READ UINT32_C(0x00000001)
#define RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE UINT32_C(0x00000002)
#define RIN_VULKAN_PRODUCT_MEMORY_CPU_VISIBLE UINT32_C(0x00000004)
#define RIN_VULKAN_PRODUCT_MEMORY_ZEROED UINT32_C(0x00000008)
#define RIN_VULKAN_PRODUCT_MEMORY_ALLOCATION_ACTIVE 1u

typedef enum RinVulkanProductResult {
    RIN_VULKAN_PRODUCT_OK = 0,
    RIN_VULKAN_PRODUCT_INVALID_ARGUMENT = -1,
    RIN_VULKAN_PRODUCT_STATE = -2,
    RIN_VULKAN_PRODUCT_BUSY = -3,
    RIN_VULKAN_PRODUCT_BACKEND_FAILED = -4,
    RIN_VULKAN_PRODUCT_PROTOCOL = -5,
    RIN_VULKAN_PRODUCT_LOST = -6,
    RIN_VULKAN_PRODUCT_TIMEOUT = -7,
    RIN_VULKAN_PRODUCT_LIMIT = -8,
    RIN_VULKAN_PRODUCT_NO_SPACE = -10,
    RIN_VULKAN_PRODUCT_RECOVERY_REQUIRED = -11
} RinVulkanProductResult;

typedef struct RinVulkanProductResourceV1 {
    uint64_t allocation_handle;
    uint32_t required_gpu_access;
    uint32_t reserved;
} RinVulkanProductResourceV1;

typedef struct RinVulkanProductReportV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t flags;
    uint32_t completed_count;
    uint32_t failed_count;
    uint32_t in_flight_count;
    uint32_t reset_count;
    uint32_t reserved0;
    uint64_t device_epoch;
    uint64_t iommu_domain_cookie;
    uint64_t iommu_map_generation;
    uint64_t observed_monotonic_ns;
    uint64_t completed_values[RIN_VULKAN_PRODUCT_MAX_QUEUES];
} RinVulkanProductReportV1;

typedef struct RinVulkanProductStatusV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t flags;
    uint32_t queue_count;
    uint64_t iommu_domain_cookie;
    uint64_t iommu_map_generation;
    uint64_t device_epoch;
    uint32_t pending_submission_count;
    uint32_t active_resource_lease_count;
    uint32_t active_allocation_count;
    uint32_t cleanup_pending_count;
    uint32_t reset_count;
    uint32_t max_resources_per_submission;
    uint64_t completed_values[RIN_VULKAN_PRODUCT_MAX_QUEUES];
} RinVulkanProductStatusV1;

typedef struct RinVulkanProductAllocationDescV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t heap;
    uint32_t flags;
    uint64_t size_bytes;
    uint64_t alignment;
    uint64_t reserved[4];
} RinVulkanProductAllocationDescV1;

typedef struct RinVulkanProductAllocationInfoV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t heap;
    uint32_t flags;
    uint64_t allocation_handle;
    uint64_t gpu_virtual_address;
    uint64_t heap_offset;
    uint64_t requested_size_bytes;
    uint64_t allocation_size_bytes;
    uint64_t alignment;
    uint64_t iommu_map_generation;
    uint64_t device_epoch;
    uint32_t lease_count;
    uint32_t state;
    uint64_t reserved;
} RinVulkanProductAllocationInfoV1;

typedef struct RinVulkanProductSubmissionV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_id;
    uint32_t flags;
    uint64_t sequence;
    uint64_t command_cookie;
    uint64_t completion_value;
    uint64_t iommu_domain_cookie;
    uint64_t iommu_map_generation;
    uint64_t device_epoch;
    uint64_t deadline_ns;
    uint64_t reserved;
} RinVulkanProductSubmissionV1;

typedef int (*RinVulkanProductGetStatusFn)(
    void* context, RinVulkanProductStatusV1* status_out);
typedef int (*RinVulkanProductPollFn)(
    void* context, RinVulkanProductReportV1* report_out);
typedef int (*RinVulkanProductDestroyAllocationFn)(
    void* context, uint64_t allocation_handle);
typedef int (*RinVulkanProductPrepareSubmissionFn)(
    void* context, uint32_t queue_id, uint64_t command_cookie,
    RinVulkanProductSubmissionV1* submission_out);
typedef int (*RinVulkanProductSubmitFn)(
    void* context, const RinVulkanProductSubmissionV1* submission,
    const RinVulkanProductResourceV1* resources, uint32_t resource_count);
typedef int (*RinVulkanProductAllocateFn)(
    void* context, const RinVulkanProductAllocationDescV1* descriptor,
    uint64_t* allocation_handle_out);
typedef int (*RinVulkanProductQueryAllocationFn)(
    void* context, uint64_t allocation_handle,
    RinVulkanProductAllocationInfoV1* info_out);

/* OS-Core owns the adapter object and all physical execution.  The Vulkan
 * library sees only this fixed, API-neutral product contract.  A missing
 * callback is rejected during binding; no callback is interpreted as a
 * software-success path. */
typedef struct RinVulkanProductPlatformV1 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinVulkanProductGetStatusFn get_status;
    RinVulkanProductPollFn poll;
    RinVulkanProductDestroyAllocationFn destroy_allocation;
    RinVulkanProductPrepareSubmissionFn prepare_submission;
    RinVulkanProductSubmitFn submit;
    RinVulkanProductAllocateFn allocate;
    RinVulkanProductQueryAllocationFn query_allocation;
    uint64_t reserved[4];
} RinVulkanProductPlatformV1;

#ifdef __cplusplus
static_assert(sizeof(RinVulkanProductResourceV1) == 16u,
              "Vulkan product resource ABI drift");
static_assert(sizeof(RinVulkanProductReportV1) == 128u,
              "Vulkan product report ABI drift");
static_assert(sizeof(RinVulkanProductStatusV1) == 128u,
              "Vulkan product status ABI drift");
#else
_Static_assert(sizeof(RinVulkanProductResourceV1) == 16u,
               "Vulkan product resource ABI drift");
_Static_assert(sizeof(RinVulkanProductReportV1) == 128u,
               "Vulkan product report ABI drift");
_Static_assert(sizeof(RinVulkanProductStatusV1) == 128u,
               "Vulkan product status ABI drift");
#endif

#endif /* RINVULKAN_PUBLIC_PLATFORM_H */
