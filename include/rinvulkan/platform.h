/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_PLATFORM_H
#define RINVULKAN_PUBLIC_PLATFORM_H

#include <stdint.h>

#include <rin/gpu_process_graphics_binding_v2.h>
#include <ringpu/compatibility.h>
#include <ringpu/backend_command_v1.h>

#define RIN_VULKAN_PRODUCT_PLATFORM_VERSION 1u
#define RIN_VULKAN_PRODUCT_PLATFORM_V2_VERSION 2u
#define RIN_VULKAN_PRODUCT_PLATFORM_V3_VERSION 3u
#define RIN_VULKAN_PRODUCT_PLATFORM_V4_VERSION 4u
#define RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION 5u
#define RIN_VULKAN_PRODUCT_PLATFORM_V6_VERSION 6u
#define RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION 7u
#define RIN_VULKAN_PRODUCT_PLATFORM_V8_VERSION 8u
#define RIN_VULKAN_PRODUCT_PLATFORM_V9_VERSION 9u
#define RIN_VULKAN_PRODUCT_PLATFORM_V10_VERSION 10u
#define RIN_VULKAN_PRODUCT_PLATFORM_V11_VERSION 11u
#define RIN_VULKAN_PRODUCT_PLATFORM_V12_VERSION 12u
#define RIN_VULKAN_PRODUCT_PLATFORM_V13_VERSION 13u
#define RIN_VULKAN_PRODUCT_PLATFORM_V14_VERSION 14u
#define RIN_VULKAN_PRODUCT_PROCESS_SAMPLER_DESC_V11_VERSION 1u
#define RIN_VULKAN_PRODUCT_PROCESS_GRAPHICS_BINDING_V12_VERSION 1u
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_V13_VERSION 1u
#define RIN_VULKAN_PRODUCT_PROCESS_GRAPHICS_BINDING_V14_VERSION 1u
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_TOKEN_TAG UINT64_C(0x5266)
#define RIN_VULKAN_PRODUCT_PROCESS_GRAPHICS_BIND_GROUP_TOKEN_TAG \
    UINT64_C(0x5265)
#define RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION 1u
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

/* The platform owner owns the adapter object and all physical execution. The
 * Vulkan library sees only this fixed, API-neutral product contract. The V1
 * prepare/submit callbacks are an inseparable pair. Both may be absent only
 * for a V6 canonical process-submit route; submissions outside that route
 * require a separately bound native V2/V3 callback pair or are unsupported.
 * Missing callbacks never imply software success. */
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
    /* An inseparable pair; both may be absent when the platform uses only
     * canonical V6 process submission and does not accept native queue waits. */
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
    /* An inseparable pair; both may be absent when the platform uses only
     * canonical V6 process submission and does not accept scoped waits. */
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

/* V6 adds a canonical process-submit route. The callback receives copied,
 * API-independent RinGPU commands and returns the physical driver's real
 * completion receipt. It is used only when the ICD has no native queue waits;
 * command buffers requiring another command family stay on their admitted
 * versioned path until that path can preserve their semantics. */
typedef struct RinVulkanProductProcessSubmitReceiptV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_id;
    uint32_t reserved0;
    uint64_t sequence;
    uint64_t completion_value;
    uint64_t device_epoch;
    uint64_t iommu_map_generation;
    uint64_t reserved[2];
} RinVulkanProductProcessSubmitReceiptV1;

typedef int (*RinVulkanProductSubmitProcessCommandsV6Fn)(
    void* context, uint32_t queue_id,
    const RinGpuBackendCommandV1* commands, uint32_t command_count,
    const RinVulkanProductResourceV1* resources, uint32_t resource_count,
    RinVulkanProductProcessSubmitReceiptV1* receipt_out);

typedef struct RinVulkanProductPlatformV6 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV5* base;
    void* context;
    RinVulkanProductSubmitProcessCommandsV6Fn submit_process_commands;
    uint64_t reserved[2];
} RinVulkanProductPlatformV6;

/* V7 adds process-owned compute-pipeline lifecycle to the canonical V6
 * process-submit route. The pipeline cookie is an opaque API object identity;
 * implementations must create a real process-owned shader/pipeline pair and
 * return the token that DISPATCH will carry. Destroy is not complete until
 * the corresponding process objects have actually been released. */
typedef int (*RinVulkanProductCreateProcessComputePipelineV7Fn)(
    void* context, uint64_t pipeline_cookie, const void* rin_shader_ir,
    uint64_t shader_ir_size, uint64_t* pipeline_token_out);
typedef int (*RinVulkanProductDestroyProcessComputePipelineV7Fn)(
    void* context, uint64_t pipeline_token);

typedef struct RinVulkanProductPlatformV7 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV6* base;
    void* context;
    RinVulkanProductCreateProcessComputePipelineV7Fn
        create_process_compute_pipeline;
    RinVulkanProductDestroyProcessComputePipelineV7Fn
        destroy_process_compute_pipeline;
    uint64_t reserved[2];
} RinVulkanProductPlatformV7;

/* V8 adds process-owned storage-buffer bind groups to the V6 process-submit
 * route. The cookies are opaque API resource identities; allocation ranges
 * are allocation-relative and never contain GPU virtual or physical
 * addresses. The SDK adapter must bind each resource before creating the
 * process object and keep both leases until the bind group is destroyed. */
#define RIN_VULKAN_PRODUCT_PROCESS_COMPUTE_BINDING_V8_VERSION 1u
typedef struct RinVulkanProductProcessComputeBindingV8 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t resource_index;
    uint32_t access;
    uint64_t resource_cookie;
    uint64_t allocation_handle;
    uint64_t allocation_offset;
    uint64_t resource_size_bytes;
    uint64_t offset;
    uint64_t size_bytes;
    uint32_t reserved[2];
} RinVulkanProductProcessComputeBindingV8;

typedef int (*RinVulkanProductCreateProcessComputeBindGroupV8Fn)(
    void* context, uint64_t bind_group_token, uint64_t pipeline_token,
    const RinVulkanProductProcessComputeBindingV8* bindings,
    uint32_t binding_count);
typedef int (*RinVulkanProductDestroyProcessComputeBindGroupV8Fn)(
    void* context, uint64_t bind_group_token);

typedef struct RinVulkanProductPlatformV8 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV7* base;
    void* context;
    RinVulkanProductCreateProcessComputeBindGroupV8Fn
        create_process_compute_bind_group;
    RinVulkanProductDestroyProcessComputeBindGroupV8Fn
        destroy_process_compute_bind_group;
    uint64_t reserved[2];
} RinVulkanProductPlatformV8;

/* V9 connects Vulkan query-pool lifetime and readback to process-owned
 * RinGPU queries. Values are API-independent device query values; callers
 * must not synthesize a device result from queue submission sequence data. */
#define RIN_VULKAN_PRODUCT_PROCESS_QUERY_RESULT_V9_VERSION 1u
#define RIN_VULKAN_PRODUCT_PROCESS_QUERY_VALUE_COUNT_V9 8u
#define RIN_VULKAN_PRODUCT_QUERY_TYPE_TIMESTAMP_V9 1u
typedef struct RinVulkanProductProcessQueryResultV9 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t query_type;
    uint32_t available;
    uint32_t reserved0;
    uint32_t reserved1;
    uint64_t values[RIN_VULKAN_PRODUCT_PROCESS_QUERY_VALUE_COUNT_V9];
} RinVulkanProductProcessQueryResultV9;

typedef int (*RinVulkanProductCreateProcessQueryV9Fn)(
    void* context, uint64_t query_cookie, uint32_t query_type);
typedef int (*RinVulkanProductDestroyProcessQueryV9Fn)(
    void* context, uint64_t query_cookie);
typedef int (*RinVulkanProductGetProcessQueryResultV9Fn)(
    void* context, uint64_t query_cookie,
    RinVulkanProductProcessQueryResultV9* result_out);

typedef struct RinVulkanProductPlatformV9 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV8* base;
    void* context;
    RinVulkanProductCreateProcessQueryV9Fn create_process_query;
    RinVulkanProductDestroyProcessQueryV9Fn destroy_process_query;
    RinVulkanProductGetProcessQueryResultV9Fn get_process_query_result;
    uint64_t reserved[2];
} RinVulkanProductPlatformV9;

/* V10 adds a process-owned, resource-free graphics-pipeline lifecycle to the
 * V9 product chain. Shader inputs and descriptors are not inferred by this
 * callback: callers pass translated RinShader IR and only the stable color
 * format/topology profile represented by the process SDK ABI. */
typedef int (*RinVulkanProductCreateProcessGraphicsPipelineV10Fn)(
    void* context, uint64_t pipeline_token,
    const void* vertex_shader_ir, uint64_t vertex_shader_ir_size,
    const void* fragment_shader_ir, uint64_t fragment_shader_ir_size,
    uint32_t color_format, uint32_t primitive_topology);
typedef int (*RinVulkanProductDestroyProcessGraphicsPipelineV10Fn)(
    void* context, uint64_t pipeline_token);

typedef struct RinVulkanProductPlatformV10 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV9* base;
    void* context;
    RinVulkanProductCreateProcessGraphicsPipelineV10Fn
        create_process_graphics_pipeline;
    RinVulkanProductDestroyProcessGraphicsPipelineV10Fn
        destroy_process_graphics_pipeline;
    uint64_t reserved[2];
} RinVulkanProductPlatformV10;

/* V11 adds process-owned sampler lifecycle. Values use this RinVk ABI's
 * stable Vulkan enum values; the RinOS process adapter performs the explicit
 * conversion to the API-neutral RinGPU sampler descriptor. */
typedef struct RinVulkanProductSamplerDescV11 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t min_filter;
    uint32_t mag_filter;
    uint32_t mipmap_mode;
    uint32_t address_mode_u;
    uint32_t address_mode_v;
    uint32_t address_mode_w;
    uint32_t compare_enable;
    uint32_t compare_op;
} RinVulkanProductSamplerDescV11;

typedef int (*RinVulkanProductCreateProcessSamplerV11Fn)(
    void* context, uint64_t sampler_token,
    const RinVulkanProductSamplerDescV11* descriptor);
typedef int (*RinVulkanProductDestroyProcessSamplerV11Fn)(
    void* context, uint64_t sampler_token);

typedef struct RinVulkanProductPlatformV11 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV10* base;
    void* context;
    RinVulkanProductCreateProcessSamplerV11Fn create_process_sampler;
    RinVulkanProductDestroyProcessSamplerV11Fn destroy_process_sampler;
    uint64_t reserved[2];
} RinVulkanProductPlatformV11;

/* V12 adds an API-neutral storage-buffer graphics bind-group lifecycle. The
 * adapter passes resource identity and allocation-relative ranges; it never
 * forwards a GPU VA, IOVA, or backend cookie as an address. */
typedef struct RinVulkanProductProcessGraphicsBindingV12 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t resource_index;
    uint32_t access;
    uint64_t resource_cookie;
    uint64_t allocation_handle;
    uint64_t allocation_offset;
    uint64_t resource_size_bytes;
    uint64_t offset;
    uint64_t size_bytes;
    uint32_t reserved[2];
} RinVulkanProductProcessGraphicsBindingV12;

typedef int (*RinVulkanProductCreateProcessGraphicsBindGroupV12Fn)(
    void* context, uint64_t bind_group_token, uint64_t pipeline_token,
    const RinVulkanProductProcessGraphicsBindingV12* bindings,
    uint32_t binding_count);
typedef int (*RinVulkanProductDestroyProcessGraphicsBindGroupV12Fn)(
    void* context, uint64_t bind_group_token);

typedef struct RinVulkanProductPlatformV12 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV11* base;
    void* context;
    RinVulkanProductCreateProcessGraphicsBindGroupV12Fn
        create_process_graphics_bind_group;
    RinVulkanProductDestroyProcessGraphicsBindGroupV12Fn
        destroy_process_graphics_bind_group;
    uint64_t reserved[2];
} RinVulkanProductPlatformV12;

/* V13 adds process-owned image lifetime. The descriptor is API-neutral:
 * format/usage/layout use the stable RinGPU process-image values, and the
 * allocation range is relative to the SDK allocation. Vulkan tiling,
 * modifiers, row pitch, and device addresses are deliberately not inferred
 * or carried here; the physical image provider owns that storage contract. */
typedef struct RinVulkanProductProcessImageV13 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t resource_cookie;
    uint64_t allocation_handle;
    uint64_t allocation_offset;
    uint64_t allocation_size_bytes;
    uint32_t format;
    uint32_t dimension;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mip_levels;
    uint32_t array_layers;
    uint32_t sample_count;
    uint32_t usage;
    uint32_t logical_layout;
    uint32_t aspect_mask;
    uint32_t base_mip_level;
    uint32_t level_count;
    uint32_t base_array_layer;
    uint32_t layer_count;
    uint32_t reserved0;
    uint64_t reserved[2];
} RinVulkanProductProcessImageV13;

#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_FORMAT_RGBA8_UNORM_V13 2u
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_FORMAT_D32_FLOAT_V13 4u
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_DIMENSION_2D_V13 2u
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_COPY_SOURCE_V13 UINT32_C(0x01)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_COPY_DESTINATION_V13 \
    UINT32_C(0x02)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_SAMPLED_V13 UINT32_C(0x04)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_STORAGE_V13 UINT32_C(0x08)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_COLOR_TARGET_V13 \
    UINT32_C(0x10)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_DEPTH_STENCIL_V13 \
    UINT32_C(0x20)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_USAGE_PRESENT_V13 UINT32_C(0x40)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_LAYOUT_UNDEFINED_V13 0u
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_ASPECT_COLOR_V13 UINT32_C(0x01)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_ASPECT_DEPTH_V13 UINT32_C(0x02)
#define RIN_VULKAN_PRODUCT_PROCESS_IMAGE_ASPECT_STENCIL_V13 UINT32_C(0x04)

typedef int (*RinVulkanProductCreateProcessImageV13Fn)(
    void* context, uint64_t image_token,
    const RinVulkanProductProcessImageV13* descriptor);
typedef int (*RinVulkanProductDestroyProcessImageV13Fn)(
    void* context, uint64_t image_token);

typedef struct RinVulkanProductPlatformV13 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV12* base;
    void* context;
    RinVulkanProductCreateProcessImageV13Fn create_process_image;
    RinVulkanProductDestroyProcessImageV13Fn destroy_process_image;
    uint64_t reserved[2];
} RinVulkanProductPlatformV13;

/* V14 adds process-owned graphics bindings using the canonical API-neutral
 * RinGPU V2 descriptor. Buffer entries additionally carry the allocation
 * identity/range needed to establish the SDK resource binding; image and
 * sampler entries carry process-owned tokens and exact logical metadata. */
typedef struct RinVulkanProductProcessGraphicsBindingV14 {
    uint32_t struct_size;
    uint32_t version;
    RinGpuProcessGraphicsBindingV2 binding;
    uint64_t allocation_handle;
    uint64_t allocation_offset;
    uint64_t resource_size_bytes;
} RinVulkanProductProcessGraphicsBindingV14;

typedef int (*RinVulkanProductCreateProcessGraphicsBindGroupV14Fn)(
    void* context, uint64_t bind_group_token, uint64_t pipeline_token,
    const RinVulkanProductProcessGraphicsBindingV14* bindings,
    uint32_t binding_count);
typedef int (*RinVulkanProductDestroyProcessGraphicsBindGroupV14Fn)(
    void* context, uint64_t bind_group_token);

typedef struct RinVulkanProductPlatformV14 {
    uint32_t struct_size;
    uint32_t version;
    RinVulkanProductPlatformV13* base;
    void* context;
    RinVulkanProductCreateProcessGraphicsBindGroupV14Fn
        create_process_graphics_bind_group;
    RinVulkanProductDestroyProcessGraphicsBindGroupV14Fn
        destroy_process_graphics_bind_group;
    uint64_t reserved[2];
} RinVulkanProductPlatformV14;

#ifdef __cplusplus
static_assert(sizeof(RinVulkanProductResourceV1) == 16u,
              "Vulkan product resource ABI drift");
static_assert(sizeof(RinVulkanProductProcessComputeBindingV8) == 72u,
              "Vulkan process compute binding V8 ABI drift");
static_assert(sizeof(RinVulkanProductProcessQueryResultV9) == 88u,
              "Vulkan process query result V9 ABI drift");
static_assert(sizeof(RinVulkanProductProcessGraphicsBindingV12) == 72u,
              "Vulkan process graphics binding V12 ABI drift");
static_assert(sizeof(RinVulkanProductProcessImageV13) == 120u,
              "Vulkan process image descriptor V13 ABI drift");
static_assert(sizeof(RinVulkanProductProcessGraphicsBindingV14) == 224u,
              "Vulkan process graphics binding V14 ABI drift");
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
_Static_assert(sizeof(RinVulkanProductProcessComputeBindingV8) == 72u,
               "Vulkan process compute binding V8 ABI drift");
_Static_assert(sizeof(RinVulkanProductProcessQueryResultV9) == 88u,
               "Vulkan process query result V9 ABI drift");
_Static_assert(sizeof(RinVulkanProductProcessGraphicsBindingV12) == 72u,
               "Vulkan process graphics binding V12 ABI drift");
_Static_assert(sizeof(RinVulkanProductProcessImageV13) == 120u,
               "Vulkan process image descriptor V13 ABI drift");
_Static_assert(sizeof(RinVulkanProductProcessGraphicsBindingV14) == 224u,
               "Vulkan process graphics binding V14 ABI drift");
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
