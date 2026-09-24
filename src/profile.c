/* SPDX-License-Identifier: MIT */

#include <rinvulkan/profile.h>

#include <stddef.h>
#include <string.h>

#define RIN_GPU_VK_MIN_DEVICE_LOCAL UINT64_C(67108864)
#define RIN_GPU_VK_MIN_HOST_VISIBLE UINT64_C(16777216)
#define RIN_GPU_VK_MAX_REQUEST_MEMORY UINT64_C(1099511627776)
#define RIN_GPU_VK_MIN_BUFFER_SIZE UINT64_C(268435456)

static int all_zero(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t index;
    for (index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    return 1;
}

static int version_is_1_3(uint32_t version) {
    uint32_t variant = version >> 29u;
    uint32_t major = (version >> 22u) & 0x7fu;
    uint32_t minor = (version >> 12u) & 0x3ffu;
    return variant == 0u && major == 1u && minor == 3u;
}

static int device_name_valid(
        const char name[RIN_GPU_VULKAN_DEVICE_NAME_SIZE]) {
    uint32_t index;
    uint32_t length = RIN_GPU_VULKAN_DEVICE_NAME_SIZE;
    for (index = 0u; index < RIN_GPU_VULKAN_DEVICE_NAME_SIZE; ++index) {
        uint8_t byte = (uint8_t)name[index];
        if (byte == 0u) {
            length = index;
            break;
        }
        if (byte < 0x20u || byte > 0x7eu) return 0;
    }
    if (length == 0u || length == RIN_GPU_VULKAN_DEVICE_NAME_SIZE)
        return 0;
    for (index = length + 1u;
         index < RIN_GPU_VULKAN_DEVICE_NAME_SIZE; ++index) {
        if (name[index] != '\0') return 0;
    }
    return 1;
}

int rin_gpu_vulkan_plan_device(
        const RinGpuVulkanPhysicalDeviceV2* physical,
        const RinGpuVulkanCreateRequestV1* request,
        RinGpuVulkanDevicePlanV1* plan_out) {
    RinGpuVulkanPhysicalDeviceV2 device;
    RinGpuVulkanCreateRequestV1 wanted;
    RinGpuVulkanDevicePlanV1 plan;
    uint64_t required_features;
    uint32_t required_queues;
    uint32_t primary = UINT32_MAX;
    uint32_t transfer = UINT32_MAX;
    uint32_t local_heap = UINT32_MAX;
    uint32_t upload_type = UINT32_MAX;
    uint32_t has_present_queue = 0u;
    uint32_t index;

    if (!plan_out) return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!physical || !request) {
        memset(plan_out, 0, sizeof(*plan_out));
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    }
    device = *physical;
    wanted = *request;
    memset(plan_out, 0, sizeof(*plan_out));
    memset(&plan, 0, sizeof(plan));

    if (device.struct_size != sizeof(device) ||
        device.version != RIN_GPU_VULKAN_PHYSICAL_VERSION ||
        wanted.struct_size != sizeof(wanted) ||
        wanted.version != RIN_GPU_VULKAN_PROFILE_VERSION ||
        !version_is_1_3(device.api_version) ||
        !version_is_1_3(wanted.api_version) ||
        wanted.api_version > device.api_version ||
        (device.flags & ~RIN_GPU_VK_PHYSICAL_KNOWN) != 0u ||
        wanted.flags != 0u ||
        (device.features & ~RIN_GPU_VK_FEATURE_KNOWN) != 0u ||
        (wanted.required_features & ~RIN_GPU_VK_FEATURE_KNOWN) != 0u ||
        (wanted.optional_features & ~RIN_GPU_VK_FEATURE_KNOWN) != 0u ||
        (wanted.required_features & wanted.optional_features) != 0u ||
        (wanted.optional_features &
         RIN_GPU_VK_FEATURE_REQUIRED_1_3) != 0u ||
        (wanted.required_queue_flags & ~RIN_GPU_VK_QUEUE_KNOWN) != 0u ||
        (wanted.surface_required != 0u && wanted.surface_required != 1u) ||
        device.queue_family_count == 0u ||
        device.queue_family_count > RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES ||
        device.memory_heap_count == 0u ||
        device.memory_heap_count > RIN_GPU_VULKAN_MAX_MEMORY_HEAPS ||
        device.memory_type_count == 0u ||
        device.memory_type_count > RIN_GPU_VULKAN_MAX_MEMORY_TYPES ||
        wanted.reserved0 != 0u ||
        !all_zero(wanted.reserved, sizeof(wanted.reserved)) ||
        all_zero(device.device_uuid, sizeof(device.device_uuid)) ||
        all_zero(device.driver_digest, sizeof(device.driver_digest)) ||
        all_zero(device.pipeline_cache_uuid,
                 sizeof(device.pipeline_cache_uuid)) ||
        device.driver_version == 0u || device.vendor_id == 0u ||
        device.device_id == 0u ||
        device.device_type > RIN_GPU_VK_PHYSICAL_TYPE_MAX ||
        !device_name_valid(device.device_name) ||
        device.max_sampler_anisotropy != device.max_sampler_anisotropy ||
        device.max_sampler_anisotropy < 1.0f ||
        device.max_sampler_anisotropy > 64.0f ||
        device.iommu_domain_cookie == 0u || device.device_epoch == 0u ||
        wanted.max_in_flight == 0u ||
        wanted.max_in_flight > RIN_GPU_VULKAN_MAX_IN_FLIGHT ||
        wanted.minimum_device_local_bytes > RIN_GPU_VK_MAX_REQUEST_MEMORY ||
        wanted.minimum_host_visible_bytes > RIN_GPU_VK_MAX_REQUEST_MEMORY)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;

    if ((device.flags & (RIN_GPU_VK_PHYSICAL_DMA_ISOLATED |
                         RIN_GPU_VK_PHYSICAL_RESET_CAPABLE)) !=
        (RIN_GPU_VK_PHYSICAL_DMA_ISOLATED |
         RIN_GPU_VK_PHYSICAL_RESET_CAPABLE))
        return RIN_GPU_VULKAN_SECURITY;
    if (wanted.surface_required != 0u &&
        (device.flags & RIN_GPU_VK_PHYSICAL_PRESENT) == 0u)
        return RIN_GPU_VULKAN_UNSUPPORTED;
    if (device.max_image_dimension_2d < 4096u ||
        device.max_bound_descriptor_sets < 4u ||
        device.max_per_stage_resources < 128u ||
        device.max_push_constants_size < 128u ||
        device.max_memory_allocation_count < 4096u ||
        device.timestamp_period_ns_x1000 == 0u ||
        device.max_buffer_size < RIN_GPU_VK_MIN_BUFFER_SIZE)
        return RIN_GPU_VULKAN_LIMIT;

    required_features = wanted.required_features |
                        RIN_GPU_VK_FEATURE_REQUIRED_1_3;
    if ((device.features & required_features) != required_features)
        return RIN_GPU_VULKAN_UNSUPPORTED;
    required_queues = wanted.required_queue_flags |
                      RIN_GPU_VK_QUEUE_GRAPHICS |
                      RIN_GPU_VK_QUEUE_COMPUTE |
                      RIN_GPU_VK_QUEUE_TRANSFER;
    if (wanted.surface_required != 0u)
        required_queues |= RIN_GPU_VK_QUEUE_PRESENT;

    for (index = 0u; index < RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES; ++index) {
        const RinGpuVulkanQueueFamilyV1* queue =
            &device.queue_families[index];
        if (index >= device.queue_family_count) {
            if (!all_zero(queue, sizeof(*queue)))
                return RIN_GPU_VULKAN_INVALID_ARGUMENT;
            continue;
        }
        if ((queue->flags & ~RIN_GPU_VK_QUEUE_KNOWN) != 0u ||
            queue->flags == 0u || queue->queue_count == 0u ||
            queue->queue_count > 64u || queue->timestamp_valid_bits > 64u ||
            queue->reserved != 0u)
            return RIN_GPU_VULKAN_INVALID_ARGUMENT;
        if (primary == UINT32_MAX &&
            (queue->flags & required_queues) == required_queues)
            primary = index;
        if ((queue->flags & RIN_GPU_VK_QUEUE_PRESENT) != 0u)
            has_present_queue = 1u;
        if (transfer == UINT32_MAX &&
            (queue->flags & RIN_GPU_VK_QUEUE_TRANSFER) != 0u &&
            (queue->flags & (RIN_GPU_VK_QUEUE_GRAPHICS |
                             RIN_GPU_VK_QUEUE_COMPUTE)) == 0u)
            transfer = index;
    }
    if (has_present_queue !=
        ((device.flags & RIN_GPU_VK_PHYSICAL_PRESENT) != 0u))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (primary == UINT32_MAX) return RIN_GPU_VULKAN_UNSUPPORTED;
    if (transfer == UINT32_MAX) transfer = primary;

    for (index = 0u; index < RIN_GPU_VULKAN_MAX_MEMORY_HEAPS; ++index) {
        const RinGpuVulkanMemoryHeapV1* heap = &device.memory_heaps[index];
        if (index >= device.memory_heap_count) {
            if (!all_zero(heap, sizeof(*heap)))
                return RIN_GPU_VULKAN_INVALID_ARGUMENT;
            continue;
        }
        if (heap->size_bytes == 0u ||
            (heap->flags & ~RIN_GPU_VK_HEAP_KNOWN) != 0u ||
            heap->reserved != 0u)
            return RIN_GPU_VULKAN_INVALID_ARGUMENT;
        if (local_heap == UINT32_MAX &&
            (heap->flags & RIN_GPU_VK_HEAP_DEVICE_LOCAL) != 0u &&
            heap->size_bytes >= RIN_GPU_VK_MIN_DEVICE_LOCAL &&
            heap->size_bytes >= wanted.minimum_device_local_bytes)
            local_heap = index;
    }
    if (local_heap == UINT32_MAX) return RIN_GPU_VULKAN_LIMIT;

    for (index = 0u; index < RIN_GPU_VULKAN_MAX_MEMORY_TYPES; ++index) {
        const RinGpuVulkanMemoryTypeV1* type = &device.memory_types[index];
        uint32_t host_required = RIN_GPU_VK_MEMORY_HOST_VISIBLE |
                                 RIN_GPU_VK_MEMORY_HOST_COHERENT;
        if (index >= device.memory_type_count) {
            if (!all_zero(type, sizeof(*type)))
                return RIN_GPU_VULKAN_INVALID_ARGUMENT;
            continue;
        }
        if (type->heap_index >= device.memory_heap_count ||
            (type->property_flags & ~RIN_GPU_VK_MEMORY_KNOWN) != 0u ||
            type->property_flags == 0u ||
            ((type->property_flags & (RIN_GPU_VK_MEMORY_HOST_COHERENT |
                                      RIN_GPU_VK_MEMORY_HOST_CACHED)) != 0u &&
             (type->property_flags & RIN_GPU_VK_MEMORY_HOST_VISIBLE) == 0u) ||
            ((type->property_flags & RIN_GPU_VK_MEMORY_DEVICE_LOCAL) != 0u) !=
                ((device.memory_heaps[type->heap_index].flags &
                  RIN_GPU_VK_HEAP_DEVICE_LOCAL) != 0u))
            return RIN_GPU_VULKAN_INVALID_ARGUMENT;
        if (upload_type == UINT32_MAX &&
            (type->property_flags & host_required) == host_required &&
            device.memory_heaps[type->heap_index].size_bytes >=
                RIN_GPU_VK_MIN_HOST_VISIBLE &&
            device.memory_heaps[type->heap_index].size_bytes >=
                wanted.minimum_host_visible_bytes)
            upload_type = index;
    }
    if (upload_type == UINT32_MAX) return RIN_GPU_VULKAN_LIMIT;

    plan.struct_size = sizeof(plan);
    plan.version = RIN_GPU_VULKAN_PROFILE_VERSION;
    plan.api_version = wanted.api_version;
    plan.flags = wanted.surface_required ? RIN_GPU_VK_PLAN_PRESENT : 0u;
    if (transfer != primary)
        plan.flags |= RIN_GPU_VK_PLAN_DEDICATED_TRANSFER;
    plan.enabled_features = required_features |
        (wanted.optional_features & device.features);
    plan.primary_queue_family = primary;
    plan.transfer_queue_family = transfer;
    plan.device_local_heap = local_heap;
    plan.upload_memory_type = upload_type;
    plan.device_local_bytes = device.memory_heaps[local_heap].size_bytes;
    plan.host_visible_bytes =
        device.memory_heaps[device.memory_types[upload_type].heap_index]
            .size_bytes;
    plan.iommu_domain_cookie = device.iommu_domain_cookie;
    plan.device_epoch = device.device_epoch;
    memcpy(plan.device_uuid, device.device_uuid, sizeof(plan.device_uuid));
    memcpy(plan.driver_digest, device.driver_digest,
           sizeof(plan.driver_digest));
    plan.max_in_flight = wanted.max_in_flight;
    *plan_out = plan;
    return RIN_GPU_VULKAN_OK;
}
