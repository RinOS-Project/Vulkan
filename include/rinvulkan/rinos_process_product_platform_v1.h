/* SPDX-License-Identifier: MIT */
/* RinOS SDK-backed process product-platform parent for RinVk V6-V8. */
#ifndef RINVULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_V1_H
#define RINVULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_V1_H

#include <stdint.h>

#include <rinvulkan/platform.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_VULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_V1_VERSION UINT32_C(1)

typedef struct RinVulkanRinOSProcessProductPlatformV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t state;
    uint32_t reserved0;
    uint64_t device_id;
    uint64_t device_generation;
    RinVulkanProductPlatformV1 platform_v1;
    RinVulkanProductPlatformV2 platform_v2;
    RinVulkanProductPlatformV3 platform_v3;
    RinVulkanProductPlatformV4 platform_v4;
    RinVulkanProductPlatformV5 platform_v5;
    uint64_t reserved[2];
} RinVulkanRinOSProcessProductPlatformV1;

/* Initialize zeroed caller-owned storage for the exact admitted SDK device.
 * V1 legacy submission and V2/V3 native wait callbacks are intentionally not
 * synthesized: bind this parent only with the V6 canonical process-submit
 * adapter when those native routes are unavailable. */
int rin_vulkan_rinos_process_product_platform_v1_init(
    RinVulkanRinOSProcessProductPlatformV1* platform,
    uint64_t device_id, uint64_t device_generation);
int rin_vulkan_rinos_process_product_platform_v1_valid(
    const RinVulkanRinOSProcessProductPlatformV1* platform);
RinVulkanProductPlatformV1*
rin_vulkan_rinos_process_product_platform_v1_base(
    RinVulkanRinOSProcessProductPlatformV1* platform);
RinVulkanProductPlatformV2*
rin_vulkan_rinos_process_product_platform_v1_v2(
    RinVulkanRinOSProcessProductPlatformV1* platform);
RinVulkanProductPlatformV3*
rin_vulkan_rinos_process_product_platform_v1_v3(
    RinVulkanRinOSProcessProductPlatformV1* platform);
RinVulkanProductPlatformV4*
rin_vulkan_rinos_process_product_platform_v1_v4(
    RinVulkanRinOSProcessProductPlatformV1* platform);
RinVulkanProductPlatformV5*
rin_vulkan_rinos_process_product_platform_v1_v5(
    RinVulkanRinOSProcessProductPlatformV1* platform);
int rin_vulkan_rinos_process_product_platform_v1_destroy(
    RinVulkanRinOSProcessProductPlatformV1* platform);

#ifdef __cplusplus
}
#endif

#endif /* RINVULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_V1_H */
