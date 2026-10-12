/* SPDX-License-Identifier: MIT */
#include <rinvulkan/rinos_process_product_platform_v1.h>

#include <rin/gpu.h>

#include <stddef.h>
#include <string.h>

#define RIN_VULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_STATE_ACTIVE UINT32_C(1)

static int rinos_product_range_valid(const void* pointer, size_t size)
{
    uintptr_t start;
    if (!pointer || size == 0u) return 0;
    start = (uintptr_t)pointer;
    return size - 1u <= UINTPTR_MAX - start;
}

static int rinos_product_ranges_overlap(const void* first, size_t first_size,
                                       const void* second,
                                       size_t second_size)
{
    uintptr_t first_start;
    uintptr_t second_start;
    if (!rinos_product_range_valid(first, first_size) ||
        !rinos_product_range_valid(second, second_size))
        return 1;
    first_start = (uintptr_t)first;
    second_start = (uintptr_t)second;
    return first_start <= second_start
               ? second_start - first_start < first_size
               : first_start - second_start < second_size;
}

static int rinos_product_bytes_zero(const void* pointer, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)pointer;
    size_t index;
    if (!rinos_product_range_valid(pointer, size)) return 0;
    for (index = 0u; index < size; ++index)
        if (bytes[index] != 0u) return 0;
    return 1;
}

static int rinos_product_platform_live(
    const RinVulkanRinOSProcessProductPlatformV1* platform)
{
    return platform && platform->struct_size == sizeof(*platform) &&
           platform->version ==
               RIN_VULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_V1_VERSION &&
           platform->state ==
               RIN_VULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_STATE_ACTIVE &&
           platform->reserved0 == 0u && platform->device_id != 0u &&
           platform->device_generation != 0u &&
           platform->reserved[0] == 0u && platform->reserved[1] == 0u;
}

static int rinos_product_sdk_result(RinResult result)
{
    switch (result) {
    case RIN_SUCCESS:
        return RIN_VULKAN_PRODUCT_OK;
    case RIN_ERROR_INVALID_ARGUMENT:
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    case RIN_ERROR_ACCESS_DENIED:
        return RIN_VULKAN_PRODUCT_ACCESS_DENIED;
    case RIN_ERROR_BUSY:
    case RIN_ERROR_WOULD_BLOCK:
        return RIN_VULKAN_PRODUCT_BUSY;
    case RIN_ERROR_TIMEOUT:
        return RIN_VULKAN_PRODUCT_TIMEOUT;
    case RIN_ERROR_NOT_SUPPORTED:
        return RIN_VULKAN_PRODUCT_UNSUPPORTED;
    case RIN_ERROR_STALE_HANDLE:
        return RIN_VULKAN_PRODUCT_LOST;
    case RIN_ERROR_ABI_MISMATCH:
    case RIN_ERROR_INTEGRITY:
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    default:
        return RIN_VULKAN_PRODUCT_BACKEND_FAILED;
    }
}

static uint32_t rinos_product_status_flags(uint32_t flags)
{
    uint32_t result = 0u;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_STATUS_READY_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_STATUS_READY;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_STATUS_SUSPENDED_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_STATUS_SUSPENDED;
    if ((flags &
         RIN_GPU_PROCESS_PRODUCT_STATUS_RECOVERY_REQUIRED_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_STATUS_RECOVERY_REQUIRED;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_STATUS_LOST_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_STATUS_LOST;
    return result;
}

static uint32_t rinos_product_report_flags(uint32_t flags)
{
    uint32_t result = 0u;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_REPORT_RESET_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_REPORT_RESET;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_REPORT_WATCHDOG_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_REPORT_WATCHDOG;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_REPORT_DEVICE_FAULT_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_REPORT_DEVICE_FAULT;
    if ((flags & RIN_GPU_PROCESS_PRODUCT_REPORT_MEMORY_REBOUND_V1) != 0u)
        result |= RIN_VULKAN_PRODUCT_REPORT_MEMORY_REBOUND;
    return result;
}

static int rinos_product_get_status(
    void* context, RinVulkanProductStatusV1* status_out)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    RinGpuProcessProductStatusV1 sdk_status;
    RinResult sdk_result;

    if (status_out) memset(status_out, 0, sizeof(*status_out));
    if (!rinos_product_platform_live(platform) || !status_out ||
        !rinos_product_range_valid(status_out, sizeof(*status_out)) ||
        rinos_product_ranges_overlap(platform, sizeof(*platform), status_out,
                                     sizeof(*status_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(&sdk_status, 0, sizeof(sdk_status));
    sdk_status.struct_size = sizeof(sdk_status);
    sdk_status.version = RIN_GPU_PROCESS_PRODUCT_STATUS_VERSION_V1;
    sdk_result = rin_gpu_process_product_get_status_v1(
        platform->device_id, platform->device_generation, &sdk_status);
    if (sdk_result != RIN_SUCCESS) {
        memset(&sdk_status, 0, sizeof(sdk_status));
        return rinos_product_sdk_result(sdk_result);
    }
    if (sdk_status.struct_size != sizeof(sdk_status) ||
        sdk_status.version != RIN_GPU_PROCESS_PRODUCT_STATUS_VERSION_V1 ||
        sdk_status.flags == 0u ||
        (sdk_status.flags & ~RIN_GPU_PROCESS_PRODUCT_STATUS_KNOWN_V1) != 0u ||
        sdk_status.queue_count == 0u ||
        sdk_status.queue_count > RIN_VULKAN_PRODUCT_MAX_QUEUES ||
        sdk_status.iommu_domain_cookie == 0u ||
        sdk_status.iommu_map_generation == 0u ||
        sdk_status.device_epoch == 0u ||
        sdk_status.max_resources_per_submission !=
            RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION) {
        memset(&sdk_status, 0, sizeof(sdk_status));
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    }

    status_out->struct_size = sizeof(*status_out);
    status_out->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    status_out->flags = rinos_product_status_flags(sdk_status.flags);
    status_out->queue_count = sdk_status.queue_count;
    status_out->iommu_domain_cookie = sdk_status.iommu_domain_cookie;
    status_out->iommu_map_generation = sdk_status.iommu_map_generation;
    status_out->device_epoch = sdk_status.device_epoch;
    status_out->pending_submission_count =
        sdk_status.pending_submission_count;
    status_out->active_resource_lease_count =
        sdk_status.active_resource_lease_count;
    status_out->active_allocation_count = sdk_status.active_allocation_count;
    status_out->cleanup_pending_count = sdk_status.cleanup_pending_count;
    status_out->reset_count = sdk_status.reset_count;
    status_out->max_resources_per_submission =
        sdk_status.max_resources_per_submission;
    memcpy(status_out->completed_values, sdk_status.completed_values,
           sizeof(status_out->completed_values));
    memset(&sdk_status, 0, sizeof(sdk_status));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_product_poll(void* context,
                              RinVulkanProductReportV1* report_out)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    RinGpuProcessProductReportV1 sdk_report;
    RinResult sdk_result;

    if (report_out) memset(report_out, 0, sizeof(*report_out));
    if (!rinos_product_platform_live(platform) || !report_out ||
        !rinos_product_range_valid(report_out, sizeof(*report_out)) ||
        rinos_product_ranges_overlap(platform, sizeof(*platform), report_out,
                                     sizeof(*report_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(&sdk_report, 0, sizeof(sdk_report));
    sdk_report.struct_size = sizeof(sdk_report);
    sdk_report.version = RIN_GPU_PROCESS_PRODUCT_REPORT_VERSION_V1;
    sdk_result = rin_gpu_process_product_poll_v1(
        platform->device_id, platform->device_generation, &sdk_report);
    if (sdk_result != RIN_SUCCESS) {
        memset(&sdk_report, 0, sizeof(sdk_report));
        return rinos_product_sdk_result(sdk_result);
    }
    if (sdk_report.struct_size != sizeof(sdk_report) ||
        sdk_report.version != RIN_GPU_PROCESS_PRODUCT_REPORT_VERSION_V1 ||
        (sdk_report.flags & ~RIN_GPU_PROCESS_PRODUCT_REPORT_KNOWN_V1) != 0u ||
        sdk_report.device_epoch == 0u ||
        sdk_report.iommu_domain_cookie == 0u ||
        sdk_report.iommu_map_generation == 0u) {
        memset(&sdk_report, 0, sizeof(sdk_report));
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    }

    report_out->struct_size = sizeof(*report_out);
    report_out->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    report_out->flags = rinos_product_report_flags(sdk_report.flags);
    report_out->completed_count = sdk_report.completed_count;
    report_out->failed_count = sdk_report.failed_count;
    report_out->in_flight_count = sdk_report.pending_submission_count;
    report_out->reset_count = sdk_report.reset_count;
    report_out->device_epoch = sdk_report.device_epoch;
    report_out->iommu_domain_cookie = sdk_report.iommu_domain_cookie;
    report_out->iommu_map_generation = sdk_report.iommu_map_generation;
    report_out->observed_monotonic_ns = sdk_report.observed_monotonic_ns;
    memcpy(report_out->completed_values, sdk_report.completed_values,
           sizeof(report_out->completed_values));
    memset(&sdk_report, 0, sizeof(sdk_report));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_product_destroy_allocation(void* context,
                                             uint64_t allocation_handle)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    if (!rinos_product_platform_live(platform) || allocation_handle == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    return rinos_product_sdk_result(rin_gpu_memory_destroy_v1(
        platform->device_id, platform->device_generation,
        allocation_handle));
}

static int rinos_product_allocate(
    void* context, const RinVulkanProductAllocationDescV1* descriptor,
    uint64_t* allocation_handle_out)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    RinGpuAllocationDescV1 sdk_descriptor;
    RinGpuAllocationV1 allocation = 0u;

    if (allocation_handle_out) *allocation_handle_out = 0u;
    if (!rinos_product_platform_live(platform) || !descriptor ||
        !allocation_handle_out || descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_VULKAN_PRODUCT_MEMORY_VERSION ||
        (descriptor->heap != RIN_VULKAN_PRODUCT_MEMORY_HEAP_LOCAL &&
         descriptor->heap != RIN_VULKAN_PRODUCT_MEMORY_HEAP_SYSTEM) ||
        descriptor->size_bytes == 0u || descriptor->alignment == 0u ||
        (descriptor->flags &
         ~(RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
           RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE |
           RIN_VULKAN_PRODUCT_MEMORY_CPU_VISIBLE |
           RIN_VULKAN_PRODUCT_MEMORY_ZEROED)) != 0u ||
        descriptor->reserved[0] != 0u || descriptor->reserved[1] != 0u ||
        descriptor->reserved[2] != 0u || descriptor->reserved[3] != 0u ||
        rinos_product_ranges_overlap(platform, sizeof(*platform), descriptor,
                                     sizeof(*descriptor)) ||
        rinos_product_ranges_overlap(platform, sizeof(*platform),
                                     allocation_handle_out,
                                     sizeof(*allocation_handle_out)) ||
        rinos_product_ranges_overlap(descriptor, sizeof(*descriptor),
                                     allocation_handle_out,
                                     sizeof(*allocation_handle_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;

    memset(&sdk_descriptor, 0, sizeof(sdk_descriptor));
    sdk_descriptor.struct_size = sizeof(sdk_descriptor);
    sdk_descriptor.version = RIN_GPU_ALLOCATION_DESC_VERSION_V1;
    sdk_descriptor.heap = descriptor->heap ==
                                  RIN_VULKAN_PRODUCT_MEMORY_HEAP_LOCAL
                              ? RIN_GPU_HEAP_LOCAL
                              : RIN_GPU_HEAP_SYSTEM;
    if ((descriptor->flags & RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) != 0u)
        sdk_descriptor.flags |= RIN_GPU_ALLOCATION_GPU_READ;
    if ((descriptor->flags & RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) != 0u)
        sdk_descriptor.flags |= RIN_GPU_ALLOCATION_GPU_WRITE;
    if ((descriptor->flags & RIN_VULKAN_PRODUCT_MEMORY_CPU_VISIBLE) != 0u)
        sdk_descriptor.flags |= RIN_GPU_ALLOCATION_CPU_VISIBLE;
    if ((descriptor->flags & RIN_VULKAN_PRODUCT_MEMORY_ZEROED) != 0u)
        sdk_descriptor.flags |= RIN_GPU_ALLOCATION_ZEROED;
    sdk_descriptor.size_bytes = descriptor->size_bytes;
    sdk_descriptor.alignment = descriptor->alignment;
    {
        const RinResult result = rin_gpu_memory_allocate_v1(
            platform->device_id, platform->device_generation,
            &sdk_descriptor, &allocation);
        memset(&sdk_descriptor, 0, sizeof(sdk_descriptor));
        if (result != RIN_SUCCESS) return rinos_product_sdk_result(result);
    }
    if (allocation == 0u) return RIN_VULKAN_PRODUCT_PROTOCOL;
    *allocation_handle_out = allocation;
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_product_query_allocation(
    void* context, uint64_t allocation_handle,
    RinVulkanProductAllocationInfoV1* info_out)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    RinGpuAllocationInfoV1 sdk_info;
    RinResult sdk_result;

    if (info_out) memset(info_out, 0, sizeof(*info_out));
    if (!rinos_product_platform_live(platform) || allocation_handle == 0u ||
        !info_out || !rinos_product_range_valid(info_out, sizeof(*info_out)) ||
        rinos_product_ranges_overlap(platform, sizeof(*platform), info_out,
                                     sizeof(*info_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(&sdk_info, 0, sizeof(sdk_info));
    sdk_info.struct_size = sizeof(sdk_info);
    sdk_info.version = RIN_GPU_ALLOCATION_INFO_VERSION_V1;
    sdk_result = rin_gpu_memory_query_v1(
        platform->device_id, platform->device_generation, allocation_handle,
        &sdk_info);
    if (sdk_result != RIN_SUCCESS) {
        memset(&sdk_info, 0, sizeof(sdk_info));
        return rinos_product_sdk_result(sdk_result);
    }
    if (sdk_info.struct_size != sizeof(sdk_info) ||
        sdk_info.version != RIN_GPU_ALLOCATION_INFO_VERSION_V1 ||
        sdk_info.allocation != allocation_handle ||
        (sdk_info.heap != RIN_GPU_HEAP_LOCAL &&
         sdk_info.heap != RIN_GPU_HEAP_SYSTEM) ||
        sdk_info.state != RIN_GPU_ALLOCATION_STATE_ACTIVE_V1 ||
        sdk_info.requested_size_bytes == 0u ||
        sdk_info.allocation_size_bytes < sdk_info.requested_size_bytes ||
        sdk_info.alignment == 0u || sdk_info.device_epoch == 0u ||
        sdk_info.iommu_map_generation == 0u || sdk_info.reserved != 0u) {
        memset(&sdk_info, 0, sizeof(sdk_info));
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    }
    info_out->struct_size = sizeof(*info_out);
    info_out->version = RIN_VULKAN_PRODUCT_MEMORY_VERSION;
    info_out->heap = sdk_info.heap == RIN_GPU_HEAP_LOCAL
                         ? RIN_VULKAN_PRODUCT_MEMORY_HEAP_LOCAL
                         : RIN_VULKAN_PRODUCT_MEMORY_HEAP_SYSTEM;
    if ((sdk_info.flags & RIN_GPU_ALLOCATION_GPU_READ) != 0u)
        info_out->flags |= RIN_VULKAN_PRODUCT_MEMORY_GPU_READ;
    if ((sdk_info.flags & RIN_GPU_ALLOCATION_GPU_WRITE) != 0u)
        info_out->flags |= RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE;
    if ((sdk_info.flags & RIN_GPU_ALLOCATION_CPU_VISIBLE) != 0u)
        info_out->flags |= RIN_VULKAN_PRODUCT_MEMORY_CPU_VISIBLE;
    if ((sdk_info.flags & RIN_GPU_ALLOCATION_ZEROED) != 0u)
        info_out->flags |= RIN_VULKAN_PRODUCT_MEMORY_ZEROED;
    info_out->allocation_handle = sdk_info.allocation;
    info_out->gpu_virtual_address = sdk_info.gpu_virtual_address;
    info_out->heap_offset = sdk_info.heap_offset;
    info_out->requested_size_bytes = sdk_info.requested_size_bytes;
    info_out->allocation_size_bytes = sdk_info.allocation_size_bytes;
    info_out->alignment = sdk_info.alignment;
    info_out->iommu_map_generation = sdk_info.iommu_map_generation;
    info_out->device_epoch = sdk_info.device_epoch;
    info_out->lease_count = sdk_info.lease_count;
    info_out->state = RIN_VULKAN_PRODUCT_MEMORY_ALLOCATION_ACTIVE;
    memset(&sdk_info, 0, sizeof(sdk_info));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_product_map_memory(
    void* context, uint64_t allocation_handle, uint32_t required_cpu_access,
    uint64_t* process_address_out, uint64_t* allocation_size_out,
    uint64_t* mapping_lease_out)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    RinGpuMemoryMappingV1 mapping;
    RinResult result;

    if (process_address_out) *process_address_out = 0u;
    if (allocation_size_out) *allocation_size_out = 0u;
    if (mapping_lease_out) *mapping_lease_out = 0u;
    if (!rinos_product_platform_live(platform) || allocation_handle == 0u ||
        !process_address_out || !allocation_size_out || !mapping_lease_out ||
        required_cpu_access == 0u ||
        (required_cpu_access & ~RIN_VULKAN_PRODUCT_CPU_ACCESS_KNOWN) != 0u ||
        rinos_product_ranges_overlap(platform, sizeof(*platform),
                                     process_address_out,
                                     sizeof(*process_address_out)) ||
        rinos_product_ranges_overlap(platform, sizeof(*platform),
                                     allocation_size_out,
                                     sizeof(*allocation_size_out)) ||
        rinos_product_ranges_overlap(platform, sizeof(*platform),
                                     mapping_lease_out,
                                     sizeof(*mapping_lease_out)) ||
        rinos_product_ranges_overlap(process_address_out,
                                     sizeof(*process_address_out),
                                     allocation_size_out,
                                     sizeof(*allocation_size_out)) ||
        rinos_product_ranges_overlap(process_address_out,
                                     sizeof(*process_address_out),
                                     mapping_lease_out,
                                     sizeof(*mapping_lease_out)) ||
        rinos_product_ranges_overlap(allocation_size_out,
                                     sizeof(*allocation_size_out),
                                     mapping_lease_out,
                                     sizeof(*mapping_lease_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(&mapping, 0, sizeof(mapping));
    mapping.struct_size = sizeof(mapping);
    mapping.version = RIN_GPU_MEMORY_MAPPING_VERSION_V1;
    result = rin_gpu_memory_map_v1(
        platform->device_id, platform->device_generation, allocation_handle,
        required_cpu_access, &mapping);
    if (result != RIN_SUCCESS) {
        memset(&mapping, 0, sizeof(mapping));
        return rinos_product_sdk_result(result);
    }
    if (mapping.struct_size != sizeof(mapping) ||
        mapping.version != RIN_GPU_MEMORY_MAPPING_VERSION_V1 ||
        mapping.allocation != allocation_handle || mapping.address == 0u ||
        mapping.size_bytes == 0u || mapping.mapping == 0u ||
        mapping.device_generation != platform->device_generation ||
        mapping.reserved != 0u) {
        /* Preserve the real lease for vkMapMemory's established retry path.
         * Never discard an acquired mapping merely because its metadata is
         * malformed, and never expose its address on this error path. */
        *mapping_lease_out = mapping.mapping;
        memset(&mapping, 0, sizeof(mapping));
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    }
    *process_address_out = mapping.address;
    *allocation_size_out = mapping.size_bytes;
    *mapping_lease_out = mapping.mapping;
    memset(&mapping, 0, sizeof(mapping));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_product_unmap_memory(void* context,
                                      uint64_t allocation_handle,
                                      uint64_t mapping_lease)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    if (!rinos_product_platform_live(platform) || allocation_handle == 0u ||
        mapping_lease == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    return rinos_product_sdk_result(rin_gpu_memory_unmap_v1(
        platform->device_id, platform->device_generation, allocation_handle,
        mapping_lease));
}

static int rinos_product_sync_memory(void* context,
                                    uint64_t allocation_handle,
                                    uint32_t action, uint64_t offset,
                                    uint64_t length)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    uint32_t sdk_action;
    if (!rinos_product_platform_live(platform) || allocation_handle == 0u ||
        length == 0u || offset > UINT64_MAX - length)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (action == RIN_VULKAN_PRODUCT_SYNC_CPU_TO_DEVICE)
        sdk_action = RIN_GPU_SYNC_CPU_TO_DEVICE;
    else if (action == RIN_VULKAN_PRODUCT_SYNC_DEVICE_TO_CPU)
        sdk_action = RIN_GPU_SYNC_DEVICE_TO_CPU;
    else
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    return rinos_product_sdk_result(rin_gpu_memory_sync_v1(
        platform->device_id, platform->device_generation, allocation_handle,
        sdk_action, offset, length));
}

static int rinos_product_bind_resource(
    void* context, uint64_t resource_cookie, uint64_t allocation_handle,
    uint64_t allocation_offset, uint64_t size_bytes,
    uint32_t required_gpu_access)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    RinGpuProcessResourceBindingV1 binding;
    if (!rinos_product_platform_live(platform) || resource_cookie == 0u ||
        allocation_handle == 0u || size_bytes == 0u ||
        allocation_offset > UINT64_MAX - size_bytes ||
        required_gpu_access == 0u ||
        (required_gpu_access &
         ~(RIN_GPU_ALLOCATION_GPU_READ | RIN_GPU_ALLOCATION_GPU_WRITE)) != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(&binding, 0, sizeof(binding));
    binding.struct_size = sizeof(binding);
    binding.version = RIN_GPU_PROCESS_RESOURCE_BINDING_VERSION_V1;
    binding.resource_cookie = resource_cookie;
    binding.allocation = allocation_handle;
    binding.allocation_offset = allocation_offset;
    binding.size_bytes = size_bytes;
    binding.required_gpu_access = required_gpu_access;
    return rinos_product_sdk_result(rin_gpu_process_resource_bind_v1(
        platform->device_id, platform->device_generation, &binding));
}

static int rinos_product_unbind_resource(void* context,
                                        uint64_t resource_cookie)
{
    RinVulkanRinOSProcessProductPlatformV1* platform =
        (RinVulkanRinOSProcessProductPlatformV1*)context;
    if (!rinos_product_platform_live(platform) || resource_cookie == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    return rinos_product_sdk_result(rin_gpu_process_resource_unbind_v1(
        platform->device_id, platform->device_generation, resource_cookie));
}

int rin_vulkan_rinos_process_product_platform_v1_init(
    RinVulkanRinOSProcessProductPlatformV1* platform,
    uint64_t device_id, uint64_t device_generation)
{
    if (!platform || device_id == 0u || device_generation == 0u ||
        !rinos_product_range_valid(platform, sizeof(*platform)) ||
        !rinos_product_bytes_zero(platform, sizeof(*platform)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(platform, 0, sizeof(*platform));
    platform->struct_size = sizeof(*platform);
    platform->version =
        RIN_VULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_V1_VERSION;
    platform->state = RIN_VULKAN_RINOS_PROCESS_PRODUCT_PLATFORM_STATE_ACTIVE;
    platform->device_id = device_id;
    platform->device_generation = device_generation;

    platform->platform_v1.struct_size = sizeof(platform->platform_v1);
    platform->platform_v1.version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    platform->platform_v1.context = platform;
    platform->platform_v1.get_status = rinos_product_get_status;
    platform->platform_v1.poll = rinos_product_poll;
    platform->platform_v1.destroy_allocation =
        rinos_product_destroy_allocation;
    platform->platform_v1.allocate = rinos_product_allocate;
    platform->platform_v1.query_allocation = rinos_product_query_allocation;

    platform->platform_v2.struct_size = sizeof(platform->platform_v2);
    platform->platform_v2.version = RIN_VULKAN_PRODUCT_PLATFORM_V2_VERSION;
    platform->platform_v2.base = &platform->platform_v1;

    platform->platform_v3.struct_size = sizeof(platform->platform_v3);
    platform->platform_v3.version = RIN_VULKAN_PRODUCT_PLATFORM_V3_VERSION;
    platform->platform_v3.base = &platform->platform_v2;

    platform->platform_v4.struct_size = sizeof(platform->platform_v4);
    platform->platform_v4.version = RIN_VULKAN_PRODUCT_PLATFORM_V4_VERSION;
    platform->platform_v4.base = &platform->platform_v3;
    platform->platform_v4.context = platform;
    platform->platform_v4.map_memory = rinos_product_map_memory;
    platform->platform_v4.unmap_memory = rinos_product_unmap_memory;
    platform->platform_v4.sync_memory = rinos_product_sync_memory;

    platform->platform_v5.struct_size = sizeof(platform->platform_v5);
    platform->platform_v5.version = RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION;
    platform->platform_v5.base = &platform->platform_v4;
    platform->platform_v5.context = platform;
    platform->platform_v5.bind_resource = rinos_product_bind_resource;
    platform->platform_v5.unbind_resource = rinos_product_unbind_resource;
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_product_platform_v1_valid(
    const RinVulkanRinOSProcessProductPlatformV1* platform)
{
    return rinos_product_platform_live(platform) &&
           platform->platform_v1.struct_size ==
               sizeof(platform->platform_v1) &&
           platform->platform_v1.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_VERSION &&
           platform->platform_v1.context == platform &&
           platform->platform_v1.get_status == rinos_product_get_status &&
           platform->platform_v1.poll == rinos_product_poll &&
           platform->platform_v1.destroy_allocation ==
               rinos_product_destroy_allocation &&
           platform->platform_v1.prepare_submission == NULL &&
           platform->platform_v1.submit == NULL &&
           platform->platform_v1.allocate == rinos_product_allocate &&
           platform->platform_v1.query_allocation ==
               rinos_product_query_allocation &&
           platform->platform_v2.struct_size ==
               sizeof(platform->platform_v2) &&
           platform->platform_v2.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V2_VERSION &&
           platform->platform_v2.base == &platform->platform_v1 &&
           platform->platform_v2.prepare_submission_v2 == NULL &&
           platform->platform_v2.submit_v2 == NULL &&
           platform->platform_v3.struct_size ==
               sizeof(platform->platform_v3) &&
           platform->platform_v3.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V3_VERSION &&
           platform->platform_v3.base == &platform->platform_v2 &&
           platform->platform_v3.prepare_submission_v3 == NULL &&
           platform->platform_v3.submit_v3 == NULL &&
           platform->platform_v4.struct_size ==
               sizeof(platform->platform_v4) &&
           platform->platform_v4.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V4_VERSION &&
           platform->platform_v4.base == &platform->platform_v3 &&
           platform->platform_v4.context == platform &&
           platform->platform_v4.map_memory == rinos_product_map_memory &&
           platform->platform_v4.unmap_memory == rinos_product_unmap_memory &&
           platform->platform_v4.sync_memory == rinos_product_sync_memory &&
           platform->platform_v5.struct_size ==
               sizeof(platform->platform_v5) &&
           platform->platform_v5.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION &&
           platform->platform_v5.base == &platform->platform_v4 &&
           platform->platform_v5.context == platform &&
           platform->platform_v5.bind_resource ==
               rinos_product_bind_resource &&
           platform->platform_v5.unbind_resource ==
               rinos_product_unbind_resource &&
           platform->platform_v1.reserved[0] == 0u &&
           platform->platform_v1.reserved[1] == 0u &&
           platform->platform_v1.reserved[2] == 0u &&
           platform->platform_v1.reserved[3] == 0u &&
           platform->platform_v2.reserved[0] == 0u &&
           platform->platform_v2.reserved[1] == 0u &&
           platform->platform_v3.reserved[0] == 0u &&
           platform->platform_v3.reserved[1] == 0u &&
           platform->platform_v4.reserved[0] == 0u &&
           platform->platform_v4.reserved[1] == 0u &&
           platform->platform_v5.reserved[0] == 0u &&
           platform->platform_v5.reserved[1] == 0u;
}

#define RINOS_PRODUCT_PLATFORM_GETTER(name, member, type) \
    type* name(RinVulkanRinOSProcessProductPlatformV1* platform) \
    { \
        return rin_vulkan_rinos_process_product_platform_v1_valid(platform) \
                   ? &platform->member \
                   : NULL; \
    }

RINOS_PRODUCT_PLATFORM_GETTER(
    rin_vulkan_rinos_process_product_platform_v1_base, platform_v1,
    RinVulkanProductPlatformV1)
RINOS_PRODUCT_PLATFORM_GETTER(
    rin_vulkan_rinos_process_product_platform_v1_v2, platform_v2,
    RinVulkanProductPlatformV2)
RINOS_PRODUCT_PLATFORM_GETTER(
    rin_vulkan_rinos_process_product_platform_v1_v3, platform_v3,
    RinVulkanProductPlatformV3)
RINOS_PRODUCT_PLATFORM_GETTER(
    rin_vulkan_rinos_process_product_platform_v1_v4, platform_v4,
    RinVulkanProductPlatformV4)
RINOS_PRODUCT_PLATFORM_GETTER(
    rin_vulkan_rinos_process_product_platform_v1_v5, platform_v5,
    RinVulkanProductPlatformV5)

#undef RINOS_PRODUCT_PLATFORM_GETTER

int rin_vulkan_rinos_process_product_platform_v1_destroy(
    RinVulkanRinOSProcessProductPlatformV1* platform)
{
    if (!rin_vulkan_rinos_process_product_platform_v1_valid(platform))
        return RIN_VULKAN_PRODUCT_STATE;
    memset(platform, 0, sizeof(*platform));
    return RIN_VULKAN_PRODUCT_OK;
}
