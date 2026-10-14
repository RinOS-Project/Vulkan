/* SPDX-License-Identifier: MIT */
/* RinOS SDK process adapter for the direct-display WSI V6 contract. */
#ifndef RINVULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_H
#define RINVULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_H

#include <stdint.h>

#include <rinvulkan/icd.h>
#include <rinvulkan/rinos_catalog_v1.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_VERSION UINT32_C(1)
#define RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAGIC UINT32_C(0x31575352)
#define RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS UINT32_C(64)

typedef struct RinVulkanRinOSProcessWsiPresentV1 {
    uint64_t present_token;
    uint64_t sdk_present_token;
    uint64_t display_cookie;
    uint64_t device_generation;
    uint64_t output_generation;
    uint32_t state;
    uint32_t reserved;
} RinVulkanRinOSProcessWsiPresentV1;

typedef struct RinVulkanRinOSProcessWsiPlatformV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t state;
    uint32_t device_count;
    volatile uint32_t active_calls;
    volatile uint32_t present_lock;
    uint32_t reserved0;
    uint32_t reserved1;
    uint64_t next_present_token;
    RinVulkanRinOSCatalogDeviceV1
        devices[RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES];
    RinVulkanRinOSProcessWsiPresentV1 presents[
        RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS];
    RinVulkanWsiPlatformV6 platform;
    uint64_t reserved[2];
} RinVulkanRinOSProcessWsiPlatformV1;

/* Build a process-facing V6 table from an authenticated RinOS catalog
 * snapshot. Calls are routed by the exact device generation and physical
 * identity; no device is selected by array order. The V1 present callback is
 * intentionally absent: V6 presentation carries its required image layout
 * through present_v2, without projecting it into the V1 ABI. */
int rin_vulkan_rinos_process_wsi_platform_v1_init(
    RinVulkanRinOSProcessWsiPlatformV1* adapter,
    const RinVulkanRinOSCatalogV1* catalog);
int rin_vulkan_rinos_process_wsi_platform_v1_valid(
    const RinVulkanRinOSProcessWsiPlatformV1* adapter);
RinVulkanWsiPlatformV6*
rin_vulkan_rinos_process_wsi_platform_v1_get_platform(
    RinVulkanRinOSProcessWsiPlatformV1* adapter);
int rin_vulkan_rinos_process_wsi_platform_v1_bind(
    RinVulkanRinOSProcessWsiPlatformV1* adapter);
int rin_vulkan_rinos_process_wsi_platform_v1_unbind(
    RinVulkanRinOSProcessWsiPlatformV1* adapter);
int rin_vulkan_rinos_process_wsi_platform_v1_destroy(
    RinVulkanRinOSProcessWsiPlatformV1* adapter);

#ifdef __cplusplus
}
#endif

#endif /* RINVULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_H */
