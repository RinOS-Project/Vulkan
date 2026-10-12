/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_RINOS_CATALOG_V1_H
#define RINVULKAN_RINOS_CATALOG_V1_H

#include <stdint.h>

#include <rin/device.h>
#include <ringpu/cross_process_capability.h>
#include <rinvulkan/runtime.h>
#include <rinvulkan/rinos_process_submit_backend_v1.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_VULKAN_RINOS_CATALOG_V1_VERSION UINT32_C(1)
#define RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES \
    RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES

typedef struct RinVulkanRinOSCatalogDeviceV1 {
    uint64_t device_id;
    uint64_t device_generation;
    RinGpuVulkanPhysicalDeviceV2 profile;
    uint32_t route_count;
    uint32_t reserved;
    RinGpuVulkanQueueRouteIpcV1
        routes[RIN_GPU_VULKAN_CATALOG_MAX_QUEUE_ROUTES];
} RinVulkanRinOSCatalogDeviceV1;

typedef struct RinVulkanRinOSCatalogV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t state;
    uint32_t device_count;
    RinGpuCrossProcessCapabilityIpcClientV1 client;
    RinVulkanRinOSCatalogDeviceV1
        devices[RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES];
} RinVulkanRinOSCatalogV1;

/* The fields match VkDeviceQueueCreateInfo's family/count only. Pass entries
 * in the same order and with the same counts as the eventual vkCreateDevice
 * call so logical queue IDs match RinVk's flattened queue table. */
typedef struct RinVulkanRinOSQueueCreateInfoV1 {
    uint32_t queue_family_index;
    uint32_t queue_count;
} RinVulkanRinOSQueueCreateInfoV1;

/* Caller-zeroed storage is populated from the authenticated
 * ringpu-capability service. Unsupported GPUs are skipped; stale, malformed,
 * or transport-failed snapshots abort rather than publishing partial data. */
int rin_vulkan_rinos_catalog_connect_v1(
    RinVulkanRinOSCatalogV1* catalog);
int rin_vulkan_rinos_catalog_enumerate_v1(
    void* context, RinGpuVulkanPhysicalDeviceV2* devices,
    uint32_t capacity, uint32_t* count_out);
int rin_vulkan_rinos_catalog_build_queue_maps_v1(
    const RinVulkanRinOSCatalogV1* catalog, uint64_t device_id,
    uint64_t device_generation,
    const RinVulkanRinOSQueueCreateInfoV1* queue_infos,
    uint32_t queue_info_count,
    RinVulkanRinOSProcessQueueMapV4* mappings_out,
    uint32_t mapping_capacity, uint32_t* mapping_count_out);
/* A failed handle close leaves the catalog live so the caller can retry. */
int rin_vulkan_rinos_catalog_disconnect_v1(
    RinVulkanRinOSCatalogV1* catalog);

#ifdef __cplusplus
}
#endif

#endif
