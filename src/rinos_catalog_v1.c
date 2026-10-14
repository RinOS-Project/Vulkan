/* SPDX-License-Identifier: MIT */

#include <rinvulkan/rinos_catalog_v1.h>

#include <rin/base.h>
#include <rin/gpu.h>
#include <rinvulkan/profile.h>

#include <stddef.h>
#include <string.h>

static int catalog_range_valid(const void* pointer, size_t size)
{
    uintptr_t address;
    if (!pointer || size == 0u) return 0;
    address = (uintptr_t)pointer;
    return address <= UINTPTR_MAX - size;
}

static int catalog_ranges_overlap(const void* left, size_t left_size,
                                  const void* right, size_t right_size)
{
    uintptr_t left_address;
    uintptr_t right_address;
    if (!left || !right || left_size == 0u || right_size == 0u) return 0;
    left_address = (uintptr_t)left;
    right_address = (uintptr_t)right;
    if (left_address > UINTPTR_MAX - left_size ||
        right_address > UINTPTR_MAX - right_size)
        return 1;
    return left_address < right_address + right_size &&
           right_address < left_address + left_size;
}

static int catalog_live(const RinVulkanRinOSCatalogV1* catalog)
{
    return catalog && catalog_range_valid(catalog, sizeof(*catalog)) &&
           catalog->struct_size == sizeof(*catalog) &&
           catalog->version == RIN_VULKAN_RINOS_CATALOG_V1_VERSION &&
           catalog->state == RIN_VULKAN_RINOS_CATALOG_STATE_ACTIVE &&
           catalog->device_count <= RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES &&
           catalog->client.channel != RIN_HANDLE_INVALID &&
           catalog->client.next_request_id != 0u &&
           catalog->client.reserved[0] == 0u &&
           catalog->client.reserved[1] == 0u;
}

static int catalog_active(const RinVulkanRinOSCatalogV1* catalog)
{
    return catalog_live(catalog) && catalog->device_count != 0u;
}

static int catalog_profile_routes_valid(
    const RinGpuVulkanCatalogSnapshotV1* snapshot,
    const RinGpuVulkanPhysicalDeviceV2* profile)
{
    uint32_t index;
    if (!snapshot || !profile ||
        profile->struct_size != sizeof(*profile) ||
        profile->version != RIN_GPU_VULKAN_PHYSICAL_VERSION ||
        profile->device_epoch != snapshot->device_epoch ||
        profile->queue_family_count == 0u ||
        profile->queue_family_count > RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES ||
        snapshot->route_count == 0u ||
        snapshot->route_count > RIN_GPU_VULKAN_CATALOG_MAX_QUEUE_ROUTES)
        return 0;
    for (index = 0u; index < snapshot->route_count; ++index) {
        const RinGpuVulkanQueueRouteIpcV1* route = &snapshot->routes[index];
        uint32_t previous;
        if (route->reserved != 0u ||
            route->queue_family_index >= profile->queue_family_count ||
            route->queue_index >=
                profile->queue_families[route->queue_family_index].queue_count)
            return 0;
        for (previous = 0u; previous < index; ++previous) {
            const RinGpuVulkanQueueRouteIpcV1* earlier =
                &snapshot->routes[previous];
            if (earlier->queue_family_index == route->queue_family_index &&
                earlier->queue_index == route->queue_index)
                return 0;
        }
    }
    for (; index < RIN_GPU_VULKAN_CATALOG_MAX_QUEUE_ROUTES; ++index) {
        const RinGpuVulkanQueueRouteIpcV1* route = &snapshot->routes[index];
        if (route->queue_family_index != 0u || route->queue_index != 0u ||
            route->physical_queue_id != 0u || route->reserved != 0u)
            return 0;
    }
    return 1;
}

static int catalog_disconnect_partial(RinVulkanRinOSCatalogV1* catalog,
                                      int result)
{
    int close_result;
    if (catalog->client.channel == RIN_HANDLE_INVALID) {
        memset(catalog, 0, sizeof(*catalog));
        return result;
    }
    close_result = rin_object_close_v1((RinObject)catalog->client.channel);
    if (close_result == RIN_SUCCESS) {
        memset(catalog, 0, sizeof(*catalog));
        return result;
    }
    catalog->struct_size = sizeof(*catalog);
    catalog->version = RIN_VULKAN_RINOS_CATALOG_V1_VERSION;
    catalog->state = RIN_VULKAN_RINOS_CATALOG_STATE_ACTIVE;
    return close_result == RIN_ERROR_BUSY ? RIN_GPU_VULKAN_BUSY
                                          : RIN_GPU_VULKAN_NOT_INITIALIZED;
}

int rin_vulkan_rinos_catalog_connect_v1(
    RinVulkanRinOSCatalogV1* catalog)
{
    uint64_t index;
    int result;
    if (!catalog || !catalog_range_valid(catalog, sizeof(*catalog)) ||
        catalog->struct_size != 0u || catalog->version != 0u ||
        catalog->state != 0u || catalog->device_count != 0u)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    memset(catalog, 0, sizeof(*catalog));
    catalog->client.channel = RIN_HANDLE_INVALID;
    result = rin_gpu_cross_process_capability_ipc_connect(&catalog->client);
    if (result != RIN_GPU_CROSS_PROCESS_OK)
        return result == RIN_GPU_CROSS_PROCESS_BUSY
                   ? RIN_GPU_VULKAN_BUSY
                   : RIN_GPU_VULKAN_NOT_INITIALIZED;

    for (index = 0u; index <= RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES;
         ++index) {
        RinDeviceInfoV1 device;
        RinGpuVulkanCatalogQueryRequestV1 request;
        RinGpuVulkanCatalogSnapshotV1 snapshot;
        RinGpuVulkanPhysicalDeviceV2 profile;
        uint32_t previous;
        result = RIN_GPU_VULKAN_NOT_INITIALIZED;
        memset(&device, 0, sizeof(device));
        device.struct_size = sizeof(device);
        device.version = RIN_SDK_STRUCT_VERSION_1;
        {
            const RinResult sdk_result = rin_device_enumerate_v1(
                RIN_SDK_DEVICE_CLASS_GPU, index, &device);
            if (sdk_result == RIN_ERROR_NOT_FOUND) {
                result = RIN_GPU_VULKAN_OK;
                break;
            }
            if (sdk_result != RIN_SUCCESS) {
                result = sdk_result == RIN_ERROR_BUSY
                             ? RIN_GPU_VULKAN_BUSY
                             : RIN_GPU_VULKAN_NOT_INITIALIZED;
                goto fail;
            }
        }
        if (index == RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES) {
            result = RIN_GPU_VULKAN_LIMIT;
            goto fail;
        }
        if (device.struct_size != sizeof(device) ||
            device.version != RIN_SDK_STRUCT_VERSION_1 ||
            device.device_id == 0u || device.generation == 0u ||
            device.device_class != RIN_SDK_DEVICE_CLASS_GPU) {
            result = RIN_GPU_VULKAN_SECURITY;
            goto fail;
        }
        for (previous = 0u; previous < catalog->device_count; ++previous) {
            if (catalog->devices[previous].device_id == device.device_id) {
                result = RIN_GPU_VULKAN_SECURITY;
                goto fail;
            }
        }
        memset(&request, 0, sizeof(request));
        request.struct_size = sizeof(request);
        request.version = RIN_GPU_VULKAN_CATALOG_QUERY_V1_VERSION;
        request.device_id = device.device_id;
        request.device_generation = device.generation;
        memset(&snapshot, 0, sizeof(snapshot));
        result = rin_gpu_cross_process_capability_ipc_query_vulkan_catalog_v1(
            &catalog->client, &request, &snapshot);
        if (result == RIN_GPU_CROSS_PROCESS_UNSUPPORTED) {
            result = RIN_GPU_VULKAN_OK;
            continue;
        }
        if (result != RIN_GPU_CROSS_PROCESS_OK) {
            result = result == RIN_GPU_CROSS_PROCESS_BUSY
                         ? RIN_GPU_VULKAN_BUSY
                         : RIN_GPU_VULKAN_NOT_INITIALIZED;
            goto fail;
        }
        if (snapshot.device_id != device.device_id ||
            snapshot.device_generation != device.generation ||
            snapshot.profile_size != sizeof(profile) ||
            snapshot.profile_version != RIN_GPU_VULKAN_PHYSICAL_VERSION) {
            result = RIN_GPU_VULKAN_SECURITY;
            goto fail;
        }
        memcpy(&profile, snapshot.profile, sizeof(profile));
        if (!catalog_profile_routes_valid(&snapshot, &profile)) {
            memset(&profile, 0, sizeof(profile));
            result = RIN_GPU_VULKAN_SECURITY;
            goto fail;
        }
        catalog->devices[catalog->device_count].device_id = device.device_id;
        catalog->devices[catalog->device_count].device_generation =
            device.generation;
        catalog->devices[catalog->device_count].profile = profile;
        catalog->devices[catalog->device_count].route_count =
            snapshot.route_count;
        memcpy(catalog->devices[catalog->device_count].routes,
               snapshot.routes,
               sizeof(snapshot.routes));
        ++catalog->device_count;
        memset(&profile, 0, sizeof(profile));
        memset(&snapshot, 0, sizeof(snapshot));
        memset(&request, 0, sizeof(request));
        memset(&device, 0, sizeof(device));
    }
    if (catalog->device_count == 0u) {
        result = RIN_GPU_VULKAN_UNSUPPORTED;
        goto fail;
    }
    catalog->struct_size = sizeof(*catalog);
    catalog->version = RIN_VULKAN_RINOS_CATALOG_V1_VERSION;
    catalog->state = RIN_VULKAN_RINOS_CATALOG_STATE_ACTIVE;
    return RIN_GPU_VULKAN_OK;

fail:
    return catalog_disconnect_partial(catalog, result);
}

int rin_vulkan_rinos_catalog_enumerate_v1(
    void* context, RinGpuVulkanPhysicalDeviceV2* devices,
    uint32_t capacity, uint32_t* count_out)
{
    const RinVulkanRinOSCatalogV1* catalog =
        (const RinVulkanRinOSCatalogV1*)context;
    if (!count_out || !catalog_active(catalog) ||
        !catalog_range_valid(count_out, sizeof(*count_out)) ||
        (catalog->device_count != 0u && !devices) ||
        (devices && !catalog_range_valid(
                        devices, (size_t)capacity * sizeof(*devices))) ||
        catalog_ranges_overlap(count_out, sizeof(*count_out), catalog,
                               sizeof(*catalog)) ||
        (devices &&
         (catalog_ranges_overlap(devices,
                                 (size_t)capacity * sizeof(*devices), catalog,
                                 sizeof(*catalog)) ||
          catalog_ranges_overlap(count_out, sizeof(*count_out), devices,
                                 (size_t)capacity * sizeof(*devices)))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    *count_out = 0u;
    if (capacity < catalog->device_count) return RIN_GPU_VULKAN_LIMIT;
    memcpy(devices, &catalog->devices[0].profile,
           sizeof(catalog->devices[0].profile));
    for (uint32_t index = 1u; index < catalog->device_count; ++index)
        devices[index] = catalog->devices[index].profile;
    *count_out = catalog->device_count;
    return RIN_GPU_VULKAN_OK;
}

int rin_vulkan_rinos_catalog_build_queue_maps_v1(
    const RinVulkanRinOSCatalogV1* catalog, uint64_t device_id,
    uint64_t device_generation,
    const RinVulkanRinOSQueueCreateInfoV1* queue_infos,
    uint32_t queue_info_count,
    RinVulkanRinOSProcessQueueMapV4* mappings_out,
    uint32_t mapping_capacity, uint32_t* mapping_count_out)
{
    const RinVulkanRinOSCatalogDeviceV1* device = NULL;
    uint32_t total_queues = 0u;
    uint32_t index;
    if (!mapping_count_out || !catalog_active(catalog) || device_id == 0u ||
        device_generation == 0u || !queue_infos || queue_info_count == 0u ||
        queue_info_count > RIN_VULKAN_PRODUCT_MAX_QUEUES ||
        !mappings_out || mapping_capacity == 0u ||
        mapping_capacity > RIN_VULKAN_PRODUCT_MAX_QUEUES ||
        !catalog_range_valid(mapping_count_out, sizeof(*mapping_count_out)) ||
        !catalog_range_valid(queue_infos,
                             (size_t)queue_info_count * sizeof(*queue_infos)) ||
        !catalog_range_valid(mappings_out,
                             (size_t)mapping_capacity * sizeof(*mappings_out)) ||
        catalog_ranges_overlap(catalog, sizeof(*catalog), queue_infos,
                               (size_t)queue_info_count * sizeof(*queue_infos)) ||
        catalog_ranges_overlap(catalog, sizeof(*catalog), mappings_out,
                               (size_t)mapping_capacity * sizeof(*mappings_out)) ||
        catalog_ranges_overlap(mapping_count_out, sizeof(*mapping_count_out),
                               catalog, sizeof(*catalog)) ||
        catalog_ranges_overlap(mapping_count_out, sizeof(*mapping_count_out),
                               queue_infos,
                               (size_t)queue_info_count * sizeof(*queue_infos)) ||
        catalog_ranges_overlap(mapping_count_out, sizeof(*mapping_count_out),
                               mappings_out,
                               (size_t)mapping_capacity * sizeof(*mappings_out)) ||
        catalog_ranges_overlap(mappings_out,
                               (size_t)mapping_capacity * sizeof(*mappings_out),
                               queue_infos,
                               (size_t)queue_info_count * sizeof(*queue_infos)))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    *mapping_count_out = 0u;
    memset(mappings_out, 0,
           (size_t)mapping_capacity * sizeof(*mappings_out));
    for (index = 0u; index < catalog->device_count; ++index) {
        if (catalog->devices[index].device_id == device_id &&
            catalog->devices[index].device_generation == device_generation) {
            device = &catalog->devices[index];
            break;
        }
    }
    if (!device) return RIN_GPU_VULKAN_NOT_INITIALIZED;
    for (index = 0u; index < queue_info_count; ++index) {
        const RinVulkanRinOSQueueCreateInfoV1* info = &queue_infos[index];
        uint32_t prior;
        if (info->queue_family_index >= device->profile.queue_family_count ||
            info->queue_count == 0u ||
            info->queue_count > device->profile.queue_families[
                                    info->queue_family_index].queue_count ||
            info->queue_count > RIN_VULKAN_PRODUCT_MAX_QUEUES - total_queues)
            return RIN_GPU_VULKAN_UNSUPPORTED;
        for (prior = 0u; prior < index; ++prior)
            if (queue_infos[prior].queue_family_index ==
                info->queue_family_index)
                return RIN_GPU_VULKAN_INVALID_ARGUMENT;
        total_queues += info->queue_count;
    }
    if (mapping_capacity < total_queues) return RIN_GPU_VULKAN_LIMIT;
    total_queues = 0u;
    for (index = 0u; index < queue_info_count; ++index) {
        const RinVulkanRinOSQueueCreateInfoV1* info = &queue_infos[index];
        for (uint32_t queue_index = 0u; queue_index < info->queue_count;
             ++queue_index) {
            uint32_t route_index;
            for (route_index = 0u; route_index < device->route_count;
                 ++route_index) {
                const RinGpuVulkanQueueRouteIpcV1* route =
                    &device->routes[route_index];
                if (route->queue_family_index == info->queue_family_index &&
                    route->queue_index == queue_index)
                    break;
            }
            if (route_index == device->route_count) {
                memset(mappings_out, 0,
                       (size_t)mapping_capacity * sizeof(*mappings_out));
                return RIN_GPU_VULKAN_UNSUPPORTED;
            }
            mappings_out[total_queues].logical_queue_id = total_queues;
            mappings_out[total_queues].physical_queue_id =
                device->routes[route_index].physical_queue_id;
            ++total_queues;
        }
    }
    *mapping_count_out = total_queues;
    return RIN_GPU_VULKAN_OK;
}

int rin_vulkan_rinos_catalog_disconnect_v1(
    RinVulkanRinOSCatalogV1* catalog)
{
    int result;
    if (!catalog_live(catalog))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    result = rin_object_close_v1((RinObject)catalog->client.channel);
    if (result != RIN_SUCCESS)
        return result == RIN_ERROR_BUSY ? RIN_GPU_VULKAN_BUSY
                                        : RIN_GPU_VULKAN_NOT_INITIALIZED;
    memset(catalog, 0, sizeof(*catalog));
    return RIN_GPU_VULKAN_OK;
}
