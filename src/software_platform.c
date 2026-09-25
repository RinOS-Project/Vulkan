/* SPDX-License-Identifier: MIT */

#include <rinvulkan/software_platform.h>

#include <stdlib.h>
#include <string.h>

#define RIN_GPU_VULKAN_SOFTWARE_ADDRESS_BASE UINT64_C(0x1000000000)
#define RIN_GPU_VULKAN_SOFTWARE_ADDRESS_ALIGNMENT UINT64_C(2097152)
#define RIN_GPU_VULKAN_SOFTWARE_MEMORY_FLAGS \
    (RIN_VULKAN_PRODUCT_MEMORY_GPU_READ | \
     RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE | \
     RIN_VULKAN_PRODUCT_MEMORY_CPU_VISIBLE | \
     RIN_VULKAN_PRODUCT_MEMORY_ZEROED)

static RinGpuVulkanSoftwarePlatformV1* software_context(void* context) {
    RinGpuVulkanSoftwarePlatformV1* platform =
        (RinGpuVulkanSoftwarePlatformV1*)context;
    return platform && platform->initialized == 1u ? platform : NULL;
}

static int descriptor_valid(const RinVulkanProductAllocationDescV1* descriptor) {
    return descriptor && descriptor->struct_size == sizeof(*descriptor) &&
           descriptor->version == RIN_VULKAN_PRODUCT_MEMORY_VERSION &&
           (descriptor->heap == RIN_VULKAN_PRODUCT_MEMORY_HEAP_LOCAL ||
            descriptor->heap == RIN_VULKAN_PRODUCT_MEMORY_HEAP_SYSTEM) &&
           descriptor->flags != 0u &&
           (descriptor->flags & ~RIN_GPU_VULKAN_SOFTWARE_MEMORY_FLAGS) == 0u &&
           descriptor->size_bytes != 0u && descriptor->alignment != 0u &&
           (descriptor->alignment & (descriptor->alignment - 1u)) == 0u &&
           descriptor->alignment <= RIN_GPU_VULKAN_SOFTWARE_ADDRESS_ALIGNMENT &&
           descriptor->reserved[0] == 0u && descriptor->reserved[1] == 0u &&
           descriptor->reserved[2] == 0u && descriptor->reserved[3] == 0u;
}

static int next_aligned(uint64_t value, uint64_t alignment,
                        uint64_t* result_out) {
    uint64_t remainder;
    if (!result_out || alignment == 0u) return 0;
    remainder = value % alignment;
    if (remainder != 0u && value > UINT64_MAX - (alignment - remainder))
        return 0;
    *result_out = remainder == 0u ? value : value + alignment - remainder;
    return 1;
}

static RinGpuVulkanSoftwareAllocationV1* allocation_by_handle(
        RinGpuVulkanSoftwarePlatformV1* platform, uint64_t handle) {
    uint32_t index;
    if (!platform || handle == 0u) return NULL;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        RinGpuVulkanSoftwareAllocationV1* allocation =
            &platform->allocations[index];
        if (allocation->handle == handle) return allocation;
    }
    return NULL;
}

static RinGpuVulkanSoftwareAllocationV1* allocation_for_range(
        RinGpuVulkanSoftwarePlatformV1* platform, uint64_t address,
        uint64_t size) {
    uint32_t index;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        RinGpuVulkanSoftwareAllocationV1* allocation =
            &platform->allocations[index];
        if (allocation->handle == 0u || address < allocation->gpu_virtual_address)
            continue;
        if (address - allocation->gpu_virtual_address > allocation->size_bytes)
            continue;
        if (size > allocation->size_bytes -
                       (address - allocation->gpu_virtual_address))
            continue;
        return allocation;
    }
    return NULL;
}

static int resource_has_access(const RinVulkanProductResourceV1* resources,
                               uint32_t resource_count, uint64_t allocation,
                               uint32_t access) {
    uint32_t index;
    for (index = 0u; index < resource_count; ++index) {
        if (resources[index].allocation_handle == allocation &&
            resources[index].reserved == 0u &&
            (resources[index].required_gpu_access & access) == access)
            return 1;
    }
    return 0;
}

static int software_get_status(void* context,
                               RinVulkanProductStatusV1* status_out) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    if (!platform || !status_out) return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(status_out, 0, sizeof(*status_out));
    status_out->struct_size = sizeof(*status_out);
    status_out->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    status_out->flags = RIN_VULKAN_PRODUCT_STATUS_READY;
    status_out->queue_count = platform->queue_count;
    status_out->iommu_domain_cookie = platform->iommu_domain_cookie;
    status_out->iommu_map_generation = platform->iommu_map_generation;
    status_out->device_epoch = platform->device_epoch;
    status_out->max_resources_per_submission =
        RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION;
    memcpy(status_out->completed_values, platform->completed_values,
           sizeof(status_out->completed_values));
    status_out->pending_submission_count =
        (platform->report_tail + RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS -
         platform->report_head) % RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS;
    status_out->active_allocation_count = 0u;
    {
        uint32_t index;
        for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
             ++index)
            if (platform->allocations[index].handle != 0u)
                ++status_out->active_allocation_count;
    }
    return RIN_VULKAN_PRODUCT_OK;
}

static int software_poll(void* context, RinVulkanProductReportV1* report_out) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    if (!platform || !report_out) return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (platform->report_head != platform->report_tail) {
        *report_out = platform->reports[platform->report_head];
        platform->report_head =
            (platform->report_head + 1u) % RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS;
        return RIN_VULKAN_PRODUCT_OK;
    }
    memset(report_out, 0, sizeof(*report_out));
    report_out->struct_size = sizeof(*report_out);
    report_out->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    report_out->iommu_domain_cookie = platform->iommu_domain_cookie;
    report_out->iommu_map_generation = platform->iommu_map_generation;
    report_out->device_epoch = platform->device_epoch;
    memcpy(report_out->completed_values, platform->completed_values,
           sizeof(report_out->completed_values));
    return RIN_VULKAN_PRODUCT_OK;
}

static int software_destroy_allocation(void* context, uint64_t handle) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    RinGpuVulkanSoftwareAllocationV1* allocation;
    if (!platform || handle == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    allocation = allocation_by_handle(platform, handle);
    if (!allocation) return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    free(allocation->bytes);
    if (platform->total_bytes >= allocation->size_bytes)
        platform->total_bytes -= allocation->size_bytes;
    else
        platform->total_bytes = 0u;
    memset(allocation, 0, sizeof(*allocation));
    return RIN_VULKAN_PRODUCT_OK;
}

static int software_prepare_submission(
        void* context, uint32_t queue_id, uint64_t command_cookie,
        RinVulkanProductSubmissionV1* submission_out) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    if (!platform || !submission_out || command_cookie == 0u ||
        queue_id >= platform->queue_count)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (platform->next_sequence == UINT64_MAX) return RIN_VULKAN_PRODUCT_LIMIT;
    memset(submission_out, 0, sizeof(*submission_out));
    submission_out->struct_size = sizeof(*submission_out);
    submission_out->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    submission_out->queue_id = queue_id;
    submission_out->sequence = ++platform->next_sequence;
    submission_out->command_cookie = command_cookie;
    submission_out->completion_value = submission_out->sequence;
    submission_out->iommu_domain_cookie = platform->iommu_domain_cookie;
    submission_out->iommu_map_generation = platform->iommu_map_generation;
    submission_out->device_epoch = platform->device_epoch;
    return RIN_VULKAN_PRODUCT_OK;
}

static int software_submit(void* context,
                           const RinVulkanProductSubmissionV1* submission,
                           const RinVulkanProductResourceV1* resources,
                           uint32_t resource_count) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    const RinGpuVulkanTransferPacketV1* packet;
    const RinGpuVulkanTransferPacketV2* packet_v2;
    RinVulkanProductReportV1* report;
    uint32_t copy_index;
    uint32_t operation_index;
    uint32_t next_tail;
    if (!platform || !submission || submission->struct_size != sizeof(*submission) ||
        submission->version != RIN_VULKAN_PRODUCT_PLATFORM_VERSION ||
        submission->flags != 0u || submission->queue_id >= platform->queue_count ||
        submission->sequence == 0u ||
        submission->completion_value != submission->sequence ||
        submission->iommu_domain_cookie != platform->iommu_domain_cookie ||
        submission->iommu_map_generation != platform->iommu_map_generation ||
        submission->device_epoch != platform->device_epoch ||
        resource_count > RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION ||
        (resource_count != 0u && !resources))
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    next_tail = (platform->report_tail + 1u) %
                RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS;
    if (next_tail == platform->report_head) return RIN_VULKAN_PRODUCT_BUSY;
    packet = (const RinGpuVulkanTransferPacketV1*)(uintptr_t)
        submission->command_cookie;
    if (!packet || packet->struct_size < sizeof(uint32_t) * 4u ||
        packet->reserved != 0u)
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    if (packet->version == RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION) {
        if (packet->struct_size != sizeof(*packet) || packet->copy_count == 0u ||
            packet->copy_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_COPIES)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        for (copy_index = 0u; copy_index < packet->copy_count; ++copy_index) {
            const RinGpuVulkanBufferCopyCommandV1* copy =
                &packet->copies[copy_index];
            RinGpuVulkanSoftwareAllocationV1* source;
            RinGpuVulkanSoftwareAllocationV1* destination;
            uint64_t source_offset;
            uint64_t destination_offset;
            if (!resource_has_access(resources, resource_count,
                                     copy->source_allocation,
                                     RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) ||
                !resource_has_access(resources, resource_count,
                                     copy->destination_allocation,
                                     RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) ||
                copy->size_bytes == 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source = allocation_by_handle(platform, copy->source_allocation);
            destination = allocation_by_handle(platform,
                                               copy->destination_allocation);
            if (!source || !destination ||
                allocation_for_range(platform, copy->source_gpu_address,
                                     copy->size_bytes) != source ||
                allocation_for_range(platform, copy->destination_gpu_address,
                                     copy->size_bytes) != destination)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source_offset = copy->source_gpu_address -
                            source->gpu_virtual_address;
            destination_offset = copy->destination_gpu_address -
                                 destination->gpu_virtual_address;
            memmove(destination->bytes + destination_offset,
                    source->bytes + source_offset, (size_t)copy->size_bytes);
        }
    } else if (packet->version == RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2) {
        packet_v2 = (const RinGpuVulkanTransferPacketV2*)(uintptr_t)
            submission->command_cookie;
        if (packet_v2->struct_size != sizeof(*packet_v2) ||
            packet_v2->op_count == 0u ||
            packet_v2->op_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        for (operation_index = 0u; operation_index < packet_v2->op_count;
             ++operation_index) {
            const RinGpuVulkanTransferOpV2* operation =
                &packet_v2->operations[operation_index];
            RinGpuVulkanSoftwareAllocationV1* source;
            RinGpuVulkanSoftwareAllocationV1* destination;
            uint64_t source_offset;
            uint64_t destination_offset;
            if ((operation->type != RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_COPY &&
                 (operation->type < RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_COPY ||
                  operation->type > RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_TO_BUFFER)) ||
                operation->reserved != 0u || operation->size_bytes == 0u ||
                !resource_has_access(resources, resource_count,
                                     operation->source_allocation,
                                     RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) ||
                !resource_has_access(resources, resource_count,
                                     operation->destination_allocation,
                                     RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE))
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source = allocation_by_handle(platform, operation->source_allocation);
            destination = allocation_by_handle(platform,
                                               operation->destination_allocation);
            if (!source || !destination ||
                allocation_for_range(platform, operation->source_gpu_address,
                                     operation->size_bytes) != source ||
                allocation_for_range(platform, operation->destination_gpu_address,
                                     operation->size_bytes) != destination)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source_offset = operation->source_gpu_address -
                            source->gpu_virtual_address;
            destination_offset = operation->destination_gpu_address -
                                 destination->gpu_virtual_address;
            memmove(destination->bytes + destination_offset,
                    source->bytes + source_offset,
                    (size_t)operation->size_bytes);
        }
    } else {
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    }
    report = &platform->reports[platform->report_tail];
    memset(report, 0, sizeof(*report));
    report->struct_size = sizeof(*report);
    report->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    report->completed_count = 1u;
    report->in_flight_count = 0u;
    report->device_epoch = platform->device_epoch;
    report->iommu_domain_cookie = platform->iommu_domain_cookie;
    report->iommu_map_generation = platform->iommu_map_generation;
    report->completed_values[submission->queue_id] =
        submission->completion_value;
    platform->completed_values[submission->queue_id] =
        submission->completion_value;
    platform->report_tail = next_tail;
    return RIN_VULKAN_PRODUCT_OK;
}

static int software_allocate(void* context,
                            const RinVulkanProductAllocationDescV1* descriptor,
                            uint64_t* handle_out) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    RinGpuVulkanSoftwareAllocationV1* allocation = NULL;
    uint64_t address;
    uint32_t index;
    if (!platform || !handle_out || !descriptor_valid(descriptor))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    *handle_out = 0u;
    if (descriptor->size_bytes > platform->max_total_bytes -
                                   platform->total_bytes ||
        descriptor->size_bytes > (uint64_t)SIZE_MAX)
        return RIN_VULKAN_PRODUCT_NO_SPACE;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        if (platform->allocations[index].handle == 0u) {
            allocation = &platform->allocations[index];
            break;
        }
    }
    if (!allocation) return RIN_VULKAN_PRODUCT_LIMIT;
    if (platform->next_allocation_handle == UINT64_MAX ||
        !next_aligned(platform->next_gpu_virtual_address,
                      RIN_GPU_VULKAN_SOFTWARE_ADDRESS_ALIGNMENT, &address) ||
        descriptor->size_bytes > UINT64_MAX - address)
        return RIN_VULKAN_PRODUCT_LIMIT;
    allocation->bytes = (uint8_t*)calloc(1u, (size_t)descriptor->size_bytes);
    if (!allocation->bytes) return RIN_VULKAN_PRODUCT_NO_SPACE;
    allocation->handle = ++platform->next_allocation_handle;
    allocation->gpu_virtual_address = address;
    allocation->size_bytes = descriptor->size_bytes;
    allocation->heap = descriptor->heap;
    allocation->flags = descriptor->flags;
    platform->next_gpu_virtual_address = address + descriptor->size_bytes;
    platform->total_bytes += descriptor->size_bytes;
    *handle_out = allocation->handle;
    return RIN_VULKAN_PRODUCT_OK;
}

static int software_query_allocation(
        void* context, uint64_t handle, RinVulkanProductAllocationInfoV1* info_out) {
    RinGpuVulkanSoftwarePlatformV1* platform = software_context(context);
    RinGpuVulkanSoftwareAllocationV1* allocation;
    if (!platform || !info_out || handle == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    allocation = allocation_by_handle(platform, handle);
    if (!allocation) return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(info_out, 0, sizeof(*info_out));
    info_out->struct_size = sizeof(*info_out);
    info_out->version = RIN_VULKAN_PRODUCT_MEMORY_VERSION;
    info_out->heap = allocation->heap;
    info_out->flags = allocation->flags;
    info_out->allocation_handle = allocation->handle;
    info_out->gpu_virtual_address = allocation->gpu_virtual_address;
    info_out->heap_offset = allocation->gpu_virtual_address -
                            RIN_GPU_VULKAN_SOFTWARE_ADDRESS_BASE;
    info_out->requested_size_bytes = allocation->size_bytes;
    info_out->allocation_size_bytes = allocation->size_bytes;
    info_out->alignment = RIN_GPU_VULKAN_SOFTWARE_ADDRESS_ALIGNMENT;
    info_out->iommu_map_generation = platform->iommu_map_generation;
    info_out->device_epoch = platform->device_epoch;
    info_out->state = RIN_VULKAN_PRODUCT_MEMORY_ALLOCATION_ACTIVE;
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_gpu_vulkan_software_platform_init(
        RinGpuVulkanSoftwarePlatformV1* platform, uint64_t domain_cookie,
        uint64_t device_epoch, uint32_t queue_count, uint64_t max_total_bytes) {
    if (!platform || platform->initialized != 0u || domain_cookie == 0u ||
        device_epoch == 0u || queue_count == 0u ||
        queue_count > RIN_VULKAN_PRODUCT_MAX_QUEUES || max_total_bytes == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(platform, 0, sizeof(*platform));
    platform->iommu_domain_cookie = domain_cookie;
    platform->device_epoch = device_epoch;
    platform->iommu_map_generation = 1u;
    platform->queue_count = queue_count;
    platform->max_total_bytes = max_total_bytes;
    platform->next_gpu_virtual_address = RIN_GPU_VULKAN_SOFTWARE_ADDRESS_BASE;
    platform->platform.struct_size = sizeof(platform->platform);
    platform->platform.version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    platform->platform.context = platform;
    platform->platform.get_status = software_get_status;
    platform->platform.poll = software_poll;
    platform->platform.destroy_allocation = software_destroy_allocation;
    platform->platform.prepare_submission = software_prepare_submission;
    platform->platform.submit = software_submit;
    platform->platform.allocate = software_allocate;
    platform->platform.query_allocation = software_query_allocation;
    platform->initialized = 1u;
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_gpu_vulkan_software_platform_shutdown(
        RinGpuVulkanSoftwarePlatformV1* platform) {
    uint32_t index;
    if (!platform || platform->initialized != 1u) {
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    if (platform->report_head != platform->report_tail) {
        return RIN_VULKAN_PRODUCT_BUSY;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        if (platform->allocations[index].handle != 0u)
            return RIN_VULKAN_PRODUCT_BUSY;
    }
    memset(platform, 0, sizeof(*platform));
    return RIN_VULKAN_PRODUCT_OK;
}
