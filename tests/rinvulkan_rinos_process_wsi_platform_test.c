/* SPDX-License-Identifier: MIT */
#include <rinvulkan/rinos_process_wsi_platform_v1.h>
#include <rinvulkan/rinos_catalog_v1.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    RinVulkanRinOSCatalogV1 catalog;
    RinVulkanRinOSProcessWsiPlatformV1 adapter;
    RinVulkanWsiPlatformV6* platform;
    RinVulkanWsiPresentRequestV2 request;
    RinVulkanWsiPresentRequestV2 before;
    uint64_t* token_out;
    int result;

    memset(&catalog, 0, sizeof(catalog));
    memset(&adapter, 0, sizeof(adapter));
    memset(&request, 0, sizeof(request));
    catalog.struct_size = sizeof(catalog);
    catalog.version = RIN_VULKAN_RINOS_CATALOG_V1_VERSION;
    catalog.state = RIN_VULKAN_RINOS_CATALOG_STATE_ACTIVE;
    catalog.device_count = 1u;
    catalog.devices[0].device_id = UINT64_C(0x1111);
    catalog.devices[0].device_generation = UINT64_C(0x2222);
    catalog.devices[0].profile.struct_size =
        sizeof(catalog.devices[0].profile);
    catalog.devices[0].profile.version = RIN_GPU_VULKAN_PHYSICAL_VERSION;
    catalog.devices[0].profile.vendor_id = UINT32_C(0x8086);
    catalog.devices[0].profile.device_id = UINT32_C(0x7d55);
    catalog.devices[0].profile.device_epoch = 1u;
    catalog.devices[0].profile.device_uuid[0] = 1u;
    catalog.devices[0].profile.queue_family_count = 1u;
    catalog.devices[0].profile.queue_families[0].queue_count = 1u;
    catalog.devices[0].route_count = 1u;
    catalog.devices[0].routes[0].physical_queue_id = 1u;

    if (rin_vulkan_rinos_process_wsi_platform_v1_init(&adapter, &catalog) !=
        RIN_GPU_VULKAN_OK) {
        fputs("could not initialize process WSI adapter\n", stderr);
        return 1;
    }
    platform = rin_vulkan_rinos_process_wsi_platform_v1_get_platform(&adapter);
    if (!platform || !platform->present_v2) {
        fputs("process WSI V6 present callback is missing\n", stderr);
        return 1;
    }

    request.frame_id = UINT64_C(0x1122334455667788);
    before = request;
    token_out = (uint64_t*)((uint8_t*)&request +
                            offsetof(RinVulkanWsiPresentRequestV2,
                                     frame_id));
    result = platform->present_v2(platform->present_v2_context, &request,
                                  token_out);
    if (result != RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT ||
        memcmp(&request, &before, sizeof(request)) != 0) {
        fputs("aliased token output modified the WSI request\n", stderr);
        return 1;
    }

    puts("RinVulkan RinOS process-WSI V2 alias guard: PASS");
    return 0;
}
