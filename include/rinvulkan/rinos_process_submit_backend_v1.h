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

#ifdef __cplusplus
}
#endif

#endif
