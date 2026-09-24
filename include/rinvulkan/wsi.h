/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_WSI_H
#define RINVULKAN_PUBLIC_WSI_H

#include <ringpu/presentation.h>

#include <stdint.h>

#define RIN_GPU_VULKAN_WSI_VERSION 1u
#define RIN_GPU_VULKAN_WSI_RUNTIME_STATE_QWORDS 4608u
#define RIN_GPU_VULKAN_WSI_MAX_SURFACES 8u
#define RIN_GPU_VULKAN_WSI_MIN_IMAGES 2u
#define RIN_GPU_VULKAN_WSI_MAX_IMAGES 8u

typedef enum RinGpuVulkanWsiResult {
    RIN_GPU_VULKAN_WSI_OK = 0,
    RIN_GPU_VULKAN_WSI_SUBOPTIMAL = 1,
    RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT = -1,
    RIN_GPU_VULKAN_WSI_NOT_READY = -2,
    RIN_GPU_VULKAN_WSI_OUT_OF_DATE = -3,
    RIN_GPU_VULKAN_WSI_DEVICE_LOST = -4,
    RIN_GPU_VULKAN_WSI_BUSY = -5,
    RIN_GPU_VULKAN_WSI_BACKEND = -6,
    RIN_GPU_VULKAN_WSI_UNSUPPORTED = -7,
    RIN_GPU_VULKAN_WSI_LIMIT = -8
} RinGpuVulkanWsiResult;

typedef struct RinGpuVulkanWsiAcquireV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t surface_id;
    uint32_t image_index;
    uint64_t image_token;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t frame_id;
    uint64_t reserved[2];
} RinGpuVulkanWsiAcquireV1;

typedef struct RinGpuVulkanWsiPresentV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t surface_id;
    uint32_t image_index;
    uint64_t image_token;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t frame_id;
    uint32_t flags;
    uint32_t damage_count;
    RinGpuPresentationDamageRectV1
        damage[RIN_GPU_PRESENTATION_MAX_DAMAGE_RECTS];
    uint64_t reserved[2];
} RinGpuVulkanWsiPresentV1;

typedef struct RinGpuVulkanWsiRuntime {
    uint64_t opaque[RIN_GPU_VULKAN_WSI_RUNTIME_STATE_QWORDS];
} RinGpuVulkanWsiRuntime;

#ifdef __cplusplus
extern "C" {
#endif

/* This owner is deliberately above RinGPU core.  It translates a bounded
 * Vulkan-like surface/swapchain contract into the compositor presentation
 * owner and never stores a native surface pointer. */
int rin_gpu_vulkan_wsi_runtime_init(
    RinGpuVulkanWsiRuntime* runtime, uint64_t device_generation,
    const RinGpuPresentationBackendV1* backend);
int rin_gpu_vulkan_wsi_create_swapchain(
    RinGpuVulkanWsiRuntime* runtime, const RinGpuPresentationOutputV1* output,
    uint32_t image_count, uint32_t mode, uint32_t* surface_id_out);
int rin_gpu_vulkan_wsi_resize_surface(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id,
    const RinGpuPresentationOutputV1* output, uint32_t image_count);
int rin_gpu_vulkan_wsi_notify_output_change(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id,
    const RinGpuPresentationOutputV1* observed_output);
int rin_gpu_vulkan_wsi_remove_surface(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id);
int rin_gpu_vulkan_wsi_acquire_next_image(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id,
    RinGpuVulkanWsiAcquireV1* acquire_out);
int rin_gpu_vulkan_wsi_present(
    RinGpuVulkanWsiRuntime* runtime,
    const RinGpuVulkanWsiPresentV1* present, uint64_t* fence_value_out);
int rin_gpu_vulkan_wsi_complete(
    RinGpuVulkanWsiRuntime* runtime,
    const RinGpuPresentationCompletionV1* completion);
int rin_gpu_vulkan_wsi_device_lost(RinGpuVulkanWsiRuntime* runtime);
int rin_gpu_vulkan_wsi_device_reset(
    RinGpuVulkanWsiRuntime* runtime, uint64_t next_device_generation);
int rin_gpu_vulkan_wsi_get_presentation_status(
    RinGpuVulkanWsiRuntime* runtime,
    RinGpuPresentationStatusV1* status_out);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(RinGpuVulkanWsiAcquireV1) == 64u,
              "RinGPU Vulkan WSI acquire drift");
static_assert(sizeof(RinGpuVulkanWsiPresentV1) == 328u,
              "RinGPU Vulkan WSI present drift");
#else
_Static_assert(sizeof(RinGpuVulkanWsiAcquireV1) == 64u,
               "RinGPU Vulkan WSI acquire drift");
_Static_assert(sizeof(RinGpuVulkanWsiPresentV1) == 328u,
               "RinGPU Vulkan WSI present drift");
#endif

#endif /* RINVULKAN_PUBLIC_WSI_H */
