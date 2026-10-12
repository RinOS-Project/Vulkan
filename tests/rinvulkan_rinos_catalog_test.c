/* SPDX-License-Identifier: MIT */

#include <rinvulkan/rinos_catalog_v1.h>

#include <rin/base.h>
#include <rin/gpu.h>

#include <assert.h>
#include <string.h>

static uint32_t close_calls;
static RinResult close_result;

int rin_gpu_cross_process_capability_ipc_connect(
    RinGpuCrossProcessCapabilityIpcClientV1* client)
{
    if (!client) return RIN_GPU_CROSS_PROCESS_INVALID_ARGUMENT;
    memset(client, 0, sizeof(*client));
    client->channel = UINT64_C(7);
    client->next_request_id = 1u;
    return RIN_GPU_CROSS_PROCESS_OK;
}

int rin_gpu_cross_process_capability_ipc_query_vulkan_catalog_v1(
    RinGpuCrossProcessCapabilityIpcClientV1* client,
    const RinGpuVulkanCatalogQueryRequestV1* request,
    RinGpuVulkanCatalogSnapshotV1* snapshot_out)
{
    RinGpuVulkanPhysicalDeviceV2 profile;
    if (!client || client->channel != UINT64_C(7) || !request ||
        !snapshot_out || request->device_id != UINT64_C(0x1001) ||
        request->device_generation != UINT64_C(0x2001))
        return RIN_GPU_CROSS_PROCESS_INVALID_ARGUMENT;
    memset(&profile, 0, sizeof(profile));
    profile.struct_size = sizeof(profile);
    profile.version = RIN_GPU_VULKAN_PHYSICAL_VERSION;
    profile.api_version = RIN_GPU_VK_API_1_3;
    profile.flags = RIN_GPU_VK_PHYSICAL_DMA_ISOLATED |
                    RIN_GPU_VK_PHYSICAL_RESET_CAPABLE |
                    RIN_GPU_VK_PHYSICAL_PRESENT;
    profile.features = RIN_GPU_VK_FEATURE_KNOWN;
    profile.device_uuid[0] = 0x31u;
    profile.driver_digest[0] = 0x42u;
    profile.iommu_domain_cookie = UINT64_C(0x3001);
    profile.device_epoch = UINT64_C(0x4001);
    profile.queue_family_count = 2u;
    profile.queue_families[0].flags =
        RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
        RIN_GPU_VK_QUEUE_TRANSFER | RIN_GPU_VK_QUEUE_PRESENT;
    profile.queue_families[0].queue_count = 2u;
    profile.queue_families[0].timestamp_valid_bits = 64u;
    profile.queue_families[1].flags = RIN_GPU_VK_QUEUE_TRANSFER;
    profile.queue_families[1].queue_count = 1u;
    profile.queue_families[1].timestamp_valid_bits = 64u;
    profile.memory_heap_count = 2u;
    profile.memory_heaps[0].size_bytes = UINT64_C(512) * 1024u * 1024u;
    profile.memory_heaps[0].flags = RIN_GPU_VK_HEAP_DEVICE_LOCAL;
    profile.memory_heaps[1].size_bytes = UINT64_C(128) * 1024u * 1024u;
    profile.memory_type_count = 2u;
    profile.memory_types[0].heap_index = 0u;
    profile.memory_types[0].property_flags = RIN_GPU_VK_MEMORY_DEVICE_LOCAL;
    profile.memory_types[1].heap_index = 1u;
    profile.memory_types[1].property_flags =
        RIN_GPU_VK_MEMORY_HOST_VISIBLE | RIN_GPU_VK_MEMORY_HOST_COHERENT;
    profile.max_sampler_anisotropy = 1.0f;
    profile.max_image_dimension_2d = 4096u;
    profile.max_bound_descriptor_sets = 4u;
    profile.max_per_stage_resources = 128u;
    profile.max_push_constants_size = 128u;
    profile.max_memory_allocation_count = 4096u;
    profile.timestamp_period_ns_x1000 = 1000u;
    profile.max_buffer_size = UINT64_C(1) << 30u;
    profile.driver_version = RIN_GPU_VK_MAKE_VERSION(1u, 0u, 0u);
    profile.vendor_id = 0x8086u;
    profile.device_id = 0x7d55u;
    profile.device_type = RIN_GPU_VK_PHYSICAL_TYPE_INTEGRATED_GPU;
    memcpy(profile.device_name, "RinOS test GPU", sizeof("RinOS test GPU"));
    profile.pipeline_cache_uuid[0] = 0x53u;

    memset(snapshot_out, 0, sizeof(*snapshot_out));
    snapshot_out->struct_size = sizeof(*snapshot_out);
    snapshot_out->version = RIN_GPU_VULKAN_CATALOG_SNAPSHOT_V1_VERSION;
    snapshot_out->device_id = request->device_id;
    snapshot_out->device_generation = request->device_generation;
    snapshot_out->device_epoch = profile.device_epoch;
    snapshot_out->profile_size = sizeof(profile);
    snapshot_out->profile_version = profile.version;
    snapshot_out->route_count = 3u;
    snapshot_out->route_struct_size =
        sizeof(RinGpuVulkanQueueRouteIpcV1);
    memcpy(snapshot_out->profile, &profile, sizeof(profile));
    snapshot_out->routes[0].queue_family_index = 0u;
    snapshot_out->routes[0].queue_index = 0u;
    snapshot_out->routes[0].physical_queue_id = 3u;
    snapshot_out->routes[1].queue_family_index = 0u;
    snapshot_out->routes[1].queue_index = 1u;
    snapshot_out->routes[1].physical_queue_id = 5u;
    snapshot_out->routes[2].queue_family_index = 1u;
    snapshot_out->routes[2].queue_index = 0u;
    snapshot_out->routes[2].physical_queue_id = 7u;
    memset(&profile, 0, sizeof(profile));
    return RIN_GPU_CROSS_PROCESS_OK;
}

RinResult rin_device_enumerate_v1(uint32_t device_class, uint64_t index,
                                  RinDeviceInfoV1* info)
{
    if (device_class != RIN_SDK_DEVICE_CLASS_GPU || !info)
        return RIN_ERROR_INVALID_ARGUMENT;
    if (index != 0u) return RIN_ERROR_NOT_FOUND;
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(*info);
    info->version = RIN_SDK_STRUCT_VERSION_1;
    info->device_id = UINT64_C(0x1001);
    info->device_class = RIN_SDK_DEVICE_CLASS_GPU;
    info->generation = UINT64_C(0x2001);
    return RIN_SUCCESS;
}

RinResult rin_object_close_v1(RinObject object)
{
    assert(object == UINT64_C(7));
    ++close_calls;
    return close_result;
}

int main(void)
{
    RinVulkanRinOSCatalogV1 catalog;
    RinGpuVulkanRuntimeV1 runtime;
    RinVulkanRinOSQueueCreateInfoV1 queue_infos[2];
    RinVulkanRinOSProcessQueueMapV4 mappings[3];
    uint32_t mapping_count = UINT32_MAX;

    memset(&catalog, 0, sizeof(catalog));
    memset(&runtime, 0, sizeof(runtime));
    memset(queue_infos, 0, sizeof(queue_infos));
    memset(mappings, 0xa5, sizeof(mappings));
    assert(rin_vulkan_rinos_catalog_connect_v1(&catalog) ==
           RIN_GPU_VULKAN_OK);
    assert(catalog.device_count == 1u &&
           catalog.devices[0].profile.vendor_id == 0x8086u &&
           catalog.devices[0].profile.device_id == 0x7d55u);
    assert(rin_gpu_vulkan_runtime_init_from_catalog(
               &runtime, rin_vulkan_rinos_catalog_enumerate_v1, &catalog,
               UINT64_C(0xabcdef)) == RIN_GPU_VULKAN_OK);
    assert(runtime.physical_device_count == 1u &&
           runtime.physical_devices[0].device_epoch == UINT64_C(0x4001));

    queue_infos[0].queue_family_index = 0u;
    queue_infos[0].queue_count = 2u;
    queue_infos[1].queue_family_index = 1u;
    queue_infos[1].queue_count = 1u;
    assert(rin_vulkan_rinos_catalog_build_queue_maps_v1(
               &catalog, UINT64_C(0x1001), UINT64_C(0x2001), queue_infos,
               2u, mappings, 3u, &mapping_count) == RIN_GPU_VULKAN_OK);
    assert(mapping_count == 3u && mappings[0].logical_queue_id == 0u &&
           mappings[0].physical_queue_id == 3u &&
           mappings[1].logical_queue_id == 1u &&
           mappings[1].physical_queue_id == 5u &&
           mappings[2].logical_queue_id == 2u &&
           mappings[2].physical_queue_id == 7u);

    queue_infos[1].queue_count = 2u;
    mapping_count = UINT32_MAX;
    assert(rin_vulkan_rinos_catalog_build_queue_maps_v1(
               &catalog, UINT64_C(0x1001), UINT64_C(0x2001), queue_infos,
               2u, mappings, 3u, &mapping_count) ==
           RIN_GPU_VULKAN_UNSUPPORTED);
    assert(mapping_count == 0u && mappings[0].physical_queue_id == 0u);

    assert(rin_gpu_vulkan_runtime_shutdown(&runtime) == RIN_GPU_VULKAN_OK);
    close_result = RIN_ERROR_BUSY;
    assert(rin_vulkan_rinos_catalog_disconnect_v1(&catalog) ==
           RIN_GPU_VULKAN_BUSY);
    close_result = RIN_SUCCESS;
    assert(rin_vulkan_rinos_catalog_disconnect_v1(&catalog) ==
           RIN_GPU_VULKAN_OK);
    assert(close_calls == 2u && catalog.struct_size == 0u);
    return 0;
}
