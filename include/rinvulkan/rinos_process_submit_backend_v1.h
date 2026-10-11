/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_H
#define RINVULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_H

#include <stdint.h>

#include <rin/gpu.h>
#include <rinvulkan/platform.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_VERSION UINT32_C(1)
#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_MAX_QUEUES \
    RIN_VULKAN_PRODUCT_MAX_QUEUES
#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_VERSION UINT32_C(2)
#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_MAX_COMPUTE_PIPELINES \
    UINT32_C(64)
#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_VERSION UINT32_C(3)
#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS \
    UINT32_C(64)
#define RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS \
    RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1
#define RIN_VULKAN_RINOS_PROCESS_RESOURCE_COOKIE_TAG UINT64_C(0x5261)
/* Reserved process-token tag for the private shader-module child associated
 * with a RinVk pipeline token. */
#define RIN_VULKAN_RINOS_PROCESS_SHADER_MODULE_TOKEN_TAG UINT64_C(0x5260)

typedef struct RinVulkanRinOSProcessQueueV1 {
    uint32_t queue_id;
    uint32_t active;
    RinGpuProcessQueueV1 token;
} RinVulkanRinOSProcessQueueV1;

typedef struct RinVulkanRinOSProcessSubmitBackendV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t state;
    volatile uint32_t lock;
    uint64_t device_id;
    uint64_t device_generation;
    uint32_t queue_count;
    uint32_t reserved0;
    RinVulkanProductPlatformV6 platform;
    RinVulkanRinOSProcessQueueV1
        queues[RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_MAX_QUEUES];
    uint64_t reserved[2];
} RinVulkanRinOSProcessSubmitBackendV1;

typedef struct RinVulkanRinOSProcessComputePipelineV2 {
    uint64_t pipeline_token;
    uint64_t shader_module_token;
    uint32_t pipeline_active;
    uint32_t shader_module_active;
} RinVulkanRinOSProcessComputePipelineV2;

/* V2 composes the stable V1 queue-submit adapter with process-owned compute
 * pipeline lifecycle. Keep V1 unchanged for existing transfer clients. */
typedef struct RinVulkanRinOSProcessSubmitBackendV2 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t state;
    volatile uint32_t lock;
    RinVulkanRinOSProcessSubmitBackendV1 submit_backend;
    RinVulkanProductPlatformV7 platform;
    RinVulkanRinOSProcessComputePipelineV2 compute_pipelines[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_MAX_COMPUTE_PIPELINES];
    uint64_t reserved[2];
} RinVulkanRinOSProcessSubmitBackendV2;

typedef struct RinVulkanRinOSProcessComputeBindingV3 {
    uint64_t resource_cookie;
    uint32_t active;
    uint32_t reserved;
} RinVulkanRinOSProcessComputeBindingV3;

typedef struct RinVulkanRinOSProcessComputeBindGroupV3 {
    uint64_t bind_group_token;
    uint64_t pipeline_token;
    uint32_t bind_group_active;
    uint32_t binding_count;
    RinVulkanRinOSProcessComputeBindingV3 bindings[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS];
} RinVulkanRinOSProcessComputeBindGroupV3;

/* V3 composes the stable V2 pipeline/queue adapter with process-owned
 * resource and bind-group lifecycle. Each group owns distinct resource
 * cookies, so its allocation leases are released only after the group object
 * is actually destroyed. */
typedef struct RinVulkanRinOSProcessSubmitBackendV3 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t state;
    volatile uint32_t lock;
    RinVulkanRinOSProcessSubmitBackendV2 compute_backend;
    RinVulkanProductPlatformV7 platform_v7;
    RinVulkanProductPlatformV8 platform_v8;
    RinVulkanRinOSProcessComputeBindGroupV3 bind_groups[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS];
    uint64_t reserved[2];
} RinVulkanRinOSProcessSubmitBackendV3;

/* Initialize caller-zeroed storage for the current RinOS process. Each queue
 * ID is bound through the authenticated RinOS SDK; the returned V6 callback
 * submits canonical RinGPU records through the process-submit syscall. The
 * V5 parent remains caller-owned and must outlive this backend registration. */
int rin_vulkan_rinos_process_submit_backend_v1_init(
    RinVulkanRinOSProcessSubmitBackendV1* backend,
    RinVulkanProductPlatformV5* parent,
    uint64_t device_id, uint64_t device_generation,
    const uint32_t* queue_ids, uint32_t queue_count);

int rin_vulkan_rinos_process_submit_backend_v1_valid(
    const RinVulkanRinOSProcessSubmitBackendV1* backend);

RinVulkanProductPlatformV6*
rin_vulkan_rinos_process_submit_backend_v1_platform(
    RinVulkanRinOSProcessSubmitBackendV1* backend);

/* Quiesce and unbind the V6 platform before detach. A failed queue-token
 * release is retained for a later retry and never reported as successful. */
int rin_vulkan_rinos_process_submit_backend_v1_detach(
    RinVulkanRinOSProcessSubmitBackendV1* backend);

/* V2 owns the V1 queue adapter and its process compute objects as one
 * lifecycle. Detach retries real object/queue releases and remains pending
 * if any SDK release fails. */
int rin_vulkan_rinos_process_submit_backend_v2_init(
    RinVulkanRinOSProcessSubmitBackendV2* backend,
    RinVulkanProductPlatformV5* parent, uint64_t device_id,
    uint64_t device_generation, const uint32_t* queue_ids,
    uint32_t queue_count);
int rin_vulkan_rinos_process_submit_backend_v2_valid(
    const RinVulkanRinOSProcessSubmitBackendV2* backend);
RinVulkanProductPlatformV7*
rin_vulkan_rinos_process_submit_backend_v2_platform(
    RinVulkanRinOSProcessSubmitBackendV2* backend);
int rin_vulkan_rinos_process_submit_backend_v2_detach(
    RinVulkanRinOSProcessSubmitBackendV2* backend);

int rin_vulkan_rinos_process_submit_backend_v3_init(
    RinVulkanRinOSProcessSubmitBackendV3* backend,
    RinVulkanProductPlatformV5* parent, uint64_t device_id,
    uint64_t device_generation, const uint32_t* queue_ids,
    uint32_t queue_count);
int rin_vulkan_rinos_process_submit_backend_v3_valid(
    const RinVulkanRinOSProcessSubmitBackendV3* backend);
RinVulkanProductPlatformV7*
rin_vulkan_rinos_process_submit_backend_v3_platform_v7(
    RinVulkanRinOSProcessSubmitBackendV3* backend);
RinVulkanProductPlatformV8*
rin_vulkan_rinos_process_submit_backend_v3_platform_v8(
    RinVulkanRinOSProcessSubmitBackendV3* backend);
int rin_vulkan_rinos_process_submit_backend_v3_detach(
    RinVulkanRinOSProcessSubmitBackendV3* backend);

#ifdef __cplusplus
}
#endif

#endif
