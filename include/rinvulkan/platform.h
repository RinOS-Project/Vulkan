/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_PLATFORM_H
#define RINVULKAN_PUBLIC_PLATFORM_H

#include <stdint.h>

#define RIN_VULKAN_PRODUCT_PLATFORM_VERSION 1u
#define RIN_VULKAN_PRODUCT_PLATFORM_V2_VERSION 2u
#define RIN_VULKAN_PRODUCT_PLATFORM_V3_VERSION 3u
#define RIN_VULKAN_PRODUCT_PLATFORM_V4_VERSION 4u
#define RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION 5u
#define RIN_VULKAN_PRODUCT_MEMORY_VERSION 1u
#define RIN_VULKAN_PRODUCT_MAX_QUEUES 8u
#define RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION 8u
#define RIN_VULKAN_PRODUCT_SUBMISSION_V2_VERSION 2u
#define RIN_VULKAN_PRODUCT_SUBMISSION_V3_VERSION 3u
#define RIN_VULKAN_PRODUCT_SUBMISSION_WAIT_VERSION 1u
#define RIN_VULKAN_PRODUCT_SUBMISSION_WAIT_V2_VERSION 2u
#define RIN_VULKAN_PRODUCT_MAX_SUBMISSION_WAITS 8u

#define RIN_VULKAN_PRODUCT_SCOPE_GRAPHICS UINT64_C(0x00000001)
#define RIN_VULKAN_PRODUCT_SCOPE_COMPUTE UINT64_C(0x00000002)
#define RIN_VULKAN_PRODUCT_SCOPE_TRANSFER UINT64_C(0x00000004)
#define RIN_VULKAN_PRODUCT_SCOPE_HOST UINT64_C(0x00000008)
#define RIN_VULKAN_PRODUCT_SCOPE_ALL_COMMANDS UINT64_C(0x00000010)
#define RIN_VULKAN_PRODUCT_SCOPE_KNOWN \
    (RIN_VULKAN_PRODUCT_SCOPE_GRAPHICS | RIN_VULKAN_PRODUCT_SCOPE_COMPUTE | \
     RIN_VULKAN_PRODUCT_SCOPE_TRANSFER | RIN_VULKAN_PRODUCT_SCOPE_HOST | \
     RIN_VULKAN_PRODUCT_SCOPE_ALL_COMMANDS)

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
    RIN_VULKAN_PRODUCT_UNSUPPORTED = -9,
    RIN_VULKAN_PRODUCT_NO_SPACE = -10,
    RIN_VULKAN_PRODUCT_RECOVERY_REQUIRED = -11,
    RIN_VULKAN_PRODUCT_ACCESS_DENIED = -12
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

typedef struct RinVulkanProductSubmissionWaitV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_id;
    uint32_t flags;
    uint64_t completion_value;
} RinVulkanProductSubmissionWaitV1;

/* The V2 payload carries API-neutral queue timeline dependencies. The ICD
 * translates Vulkan semaphore state to these already-admitted queue points;
 * native owners lower the points to their own hardware wait records. */
typedef struct RinVulkanProductSubmissionV2 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductSubmissionV1 base;
    uint32_t wait_count;
    uint32_t reserved0;
    RinVulkanProductSubmissionWaitV1
        waits[RIN_VULKAN_PRODUCT_MAX_SUBMISSION_WAITS];
} RinVulkanProductSubmissionV2;

typedef struct RinVulkanProductSubmissionWaitV2 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_id;
    uint32_t flags;
    uint64_t completion_value;
    uint64_t execution_scope_mask;
} RinVulkanProductSubmissionWaitV2;

typedef struct RinVulkanProductSubmissionV3 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductSubmissionV1 base;
    uint32_t wait_count;
    uint32_t reserved0;
    RinVulkanProductSubmissionWaitV2
        waits[RIN_VULKAN_PRODUCT_MAX_SUBMISSION_WAITS];
} RinVulkanProductSubmissionV3;

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
typedef int (*RinVulkanProductPrepareSubmissionV2Fn)(
    void* context, uint32_t queue_id, uint64_t command_cookie,
    uint32_t wait_count, const RinVulkanProductSubmissionWaitV1* waits,
    RinVulkanProductSubmissionV2* submission_out);
typedef int (*RinVulkanProductSubmitV2Fn)(
    void* context, const RinVulkanProductSubmissionV2* submission,
    const RinVulkanProductResourceV1* resources, uint32_t resource_count);
typedef int (*RinVulkanProductPrepareSubmissionV3Fn)(
    void* context, uint32_t queue_id, uint64_t command_cookie,
    uint32_t wait_count, const RinVulkanProductSubmissionWaitV2* waits,
    RinVulkanProductSubmissionV3* submission_out);
typedef int (*RinVulkanProductSubmitV3Fn)(
    void* context, const RinVulkanProductSubmissionV3* submission,
    const RinVulkanProductResourceV1* resources, uint32_t resource_count);

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

/* V2 is a separate extension object so the V1 table and its layout remain
 * byte-for-byte stable. `base` must remain alive until this extension is
 * unbound. */
typedef struct RinVulkanProductPlatformV2 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV1* base;
    RinVulkanProductPrepareSubmissionV2Fn prepare_submission_v2;
    RinVulkanProductSubmitV2Fn submit_v2;
    uint64_t reserved[2];
} RinVulkanProductPlatformV2;

/* V3 adds a normalized consumer execution scope to each queue dependency.
 * The V2 table remains unchanged and is its first-class parent adapter. */
typedef struct RinVulkanProductPlatformV3 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV2* base;
    RinVulkanProductPrepareSubmissionV3Fn prepare_submission_v3;
    RinVulkanProductSubmitV3Fn submit_v3;
    uint64_t reserved[2];
} RinVulkanProductPlatformV3;

/* V4 adds process-address-space mapping for Vulkan host-visible allocations.
 * The callback returns an authenticated mapping lease; callers must retain it
 * until unmap succeeds. Mapping and cache synchronization stay in OS-Core and
 * are deliberately separate from RinGPU's common backend ABI. */
#define RIN_VULKAN_PRODUCT_CPU_ACCESS_READ UINT32_C(0x00000001)
#define RIN_VULKAN_PRODUCT_CPU_ACCESS_WRITE UINT32_C(0x00000002)
#define RIN_VULKAN_PRODUCT_CPU_ACCESS_KNOWN UINT32_C(0x00000003)
#define RIN_VULKAN_PRODUCT_SYNC_CPU_TO_DEVICE 1u
#define RIN_VULKAN_PRODUCT_SYNC_DEVICE_TO_CPU 2u

typedef int (*RinVulkanProductMapMemoryV4Fn)(
    void* context, uint64_t allocation_handle, uint32_t required_cpu_access,
    uint64_t* process_address_out, uint64_t* allocation_size_out,
    uint64_t* mapping_lease_out);
typedef int (*RinVulkanProductUnmapMemoryV4Fn)(
    void* context, uint64_t allocation_handle, uint64_t mapping_lease);
typedef int (*RinVulkanProductSyncMemoryV4Fn)(
    void* context, uint64_t allocation_handle, uint32_t action,
    uint64_t offset, uint64_t length);

typedef struct RinVulkanProductPlatformV4 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV3* base;
    void* context;
    RinVulkanProductMapMemoryV4Fn map_memory;
    RinVulkanProductUnmapMemoryV4Fn unmap_memory;
    RinVulkanProductSyncMemoryV4Fn sync_memory;
    uint64_t reserved[2];
} RinVulkanProductPlatformV4;

/* V5 binds Vulkan object cookies to their exact process-owned allocation
 * ranges. V4 stays byte-for-byte stable; the separate extension is required
 * by process-submit runtimes that authorize canonical commands by cookie. */
typedef int (*RinVulkanProductBindResourceV5Fn)(
    void* context, uint64_t resource_cookie, uint64_t allocation_handle,
    uint64_t allocation_offset, uint64_t size_bytes,
    uint32_t required_gpu_access);
typedef int (*RinVulkanProductUnbindResourceV5Fn)(
    void* context, uint64_t resource_cookie);

typedef struct RinVulkanProductPlatformV5 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV4* base;
    void* context;
    RinVulkanProductBindResourceV5Fn bind_resource;
    RinVulkanProductUnbindResourceV5Fn unbind_resource;
    uint64_t reserved[2];
} RinVulkanProductPlatformV5;

#ifdef __cplusplus
static_assert(sizeof(RinVulkanProductResourceV1) == 16u,
              "Vulkan product resource ABI drift");
static_assert(sizeof(RinVulkanProductReportV1) == 128u,
              "Vulkan product report ABI drift");
static_assert(sizeof(RinVulkanProductStatusV1) == 128u,
              "Vulkan product status ABI drift");
static_assert(sizeof(RinVulkanProductSubmissionWaitV1) == 24u,
              "Vulkan product submission wait ABI drift");
static_assert(sizeof(RinVulkanProductSubmissionV2) == 288u,
              "Vulkan product submission V2 ABI drift");
static_assert(sizeof(RinVulkanProductSubmissionWaitV2) == 32u,
              "Vulkan scoped submission wait ABI drift");
static_assert(sizeof(RinVulkanProductSubmissionV3) == 352u,
              "Vulkan product submission V3 ABI drift");
#else
_Static_assert(sizeof(RinVulkanProductResourceV1) == 16u,
               "Vulkan product resource ABI drift");
_Static_assert(sizeof(RinVulkanProductReportV1) == 128u,
               "Vulkan product report ABI drift");
_Static_assert(sizeof(RinVulkanProductStatusV1) == 128u,
               "Vulkan product status ABI drift");
_Static_assert(sizeof(RinVulkanProductSubmissionWaitV1) == 24u,
               "Vulkan product submission wait ABI drift");
_Static_assert(sizeof(RinVulkanProductSubmissionV2) == 288u,
               "Vulkan product submission V2 ABI drift");
_Static_assert(sizeof(RinVulkanProductSubmissionWaitV2) == 32u,
               "Vulkan scoped submission wait ABI drift");
_Static_assert(sizeof(RinVulkanProductSubmissionV3) == 352u,
               "Vulkan product submission V3 ABI drift");
#endif

#endif /* RINVULKAN_PUBLIC_PLATFORM_H */
