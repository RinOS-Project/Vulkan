/* SPDX-License-Identifier: MIT */

#include <rinvulkan/software_platform.h>
#include <rinvulkan/icd.h>
#include <ringpu/runtime.h>
#include <ringpu/spirv_frontend.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <stdatomic.h>
#endif

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

static int software_execute_compute(
    RinGpuVulkanSoftwarePlatformV1* platform,
    const RinGpuVulkanComputePacketV1* packet,
    const RinVulkanProductResourceV1* resources, uint32_t resource_count) {
    RinGpuRuntimeDescV1 runtime_desc;
    RinGpuRuntime* runtime = NULL;
    RinShaderInfoV1 shader_info;
    RinGpuBufferBindingV1 bindings[RIN_SHADER_MAX_RESOURCES];
    RinGpuHandle buffers[RIN_SHADER_MAX_RESOURCES];
    RinGpuHandle shader_module = 0u;
    RinGpuHandle pipeline = 0u;
    RinGpuHandle bind_group = 0u;
    RinGpuHandle queue = 0u;
    RinGpuHandle command_list = 0u;
    RinGpuHandle fence = 0u;
    RinGpuComputePipelineDescV1 pipeline_desc;
    RinGpuQueueDescV1 queue_desc;
    RinGpuCommandListDescV1 command_desc;
    RinGpuDispatchV1 dispatch;
    RinGpuSubmitInfoV1 submit;
    const size_t packet_prefix = offsetof(RinGpuVulkanComputePacketV1,
                                          shader_ir);
    uint64_t total_buffer_bytes = 0u;
    uint32_t buffer_count = 0u;
    uint32_t index;
    int shader_result;
    int result = RIN_VULKAN_PRODUCT_PROTOCOL;

    if (!platform || !packet ||
        packet->struct_size < packet_prefix ||
        packet->version != RIN_GPU_VULKAN_COMPUTE_PACKET_VERSION ||
        packet->queue_family_index >= RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES ||
        packet->queue_index >= RIN_VULKAN_PRODUCT_MAX_QUEUES ||
        packet->product_queue_id >= platform->queue_count ||
        packet->group_count_x == 0u || packet->group_count_y == 0u ||
        packet->group_count_z == 0u || packet->group_count_x > 65535u ||
        packet->group_count_y > 65535u || packet->group_count_z > 65535u ||
        packet->binding_count > RIN_SHADER_MAX_RESOURCES ||
        packet->shader_size_bytes == 0u || packet->flags != 0u ||
        packet->reserved != 0u ||
        packet->shader_size_bytes > UINT32_MAX - packet_prefix ||
        packet->struct_size != packet_prefix + packet->shader_size_bytes ||
        packet->binding_count != resource_count ||
        (resource_count != 0u && !resources))
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    shader_result = ringpu_shader_validate(
        packet->shader_ir, packet->shader_size_bytes, &shader_info);
    if (shader_result == RIN_SHADER_ERROR_UNSUPPORTED ||
        shader_result == RIN_SHADER_ERROR_NO_MEMORY)
        return RIN_VULKAN_PRODUCT_BACKEND_FAILED;
    if (shader_result != RIN_SHADER_OK ||
        shader_info.stage != RIN_SHADER_STAGE_COMPUTE ||
        shader_info.resource_count != packet->binding_count)
        return RIN_VULKAN_PRODUCT_PROTOCOL;

    memset(bindings, 0, sizeof(bindings));
    memset(buffers, 0, sizeof(buffers));
    for (index = 0u; index < packet->binding_count; ++index) {
        const RinGpuVulkanComputeBindingV1* source =
            &packet->bindings[index];
        RinGpuVulkanSoftwareAllocationV1* allocation;
        uint32_t prior;
        uint64_t end;
        if (source->allocation_handle == 0u ||
            source->resource_index >= packet->binding_count ||
            source->access == 0u ||
            (source->access & ~(RIN_GPU_RESOURCE_READ |
                                RIN_GPU_RESOURCE_WRITE)) != 0u ||
            source->reserved[0] != 0u || source->reserved[1] != 0u ||
            source->size_bytes == 0u ||
            source->offset > UINT64_MAX - source->size_bytes)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        for (prior = 0u; prior < index; ++prior) {
            const RinGpuVulkanComputeBindingV1* previous =
                &packet->bindings[prior];
            if (previous->resource_index == source->resource_index ||
                previous->allocation_handle == source->allocation_handle)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
        }
        allocation = allocation_by_handle(platform,
                                          source->allocation_handle);
        end = source->offset + source->size_bytes;
        if (!allocation || end > allocation->size_bytes ||
            !resource_has_access(
                resources, resource_count, source->allocation_handle,
                ((source->access & RIN_GPU_RESOURCE_READ) != 0u
                     ? RIN_VULKAN_PRODUCT_MEMORY_GPU_READ : 0u) |
                    ((source->access & RIN_GPU_RESOURCE_WRITE) != 0u
                         ? RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE : 0u)))
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        if (source->size_bytes > UINT32_MAX ||
            total_buffer_bytes > UINT64_MAX - source->size_bytes)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        total_buffer_bytes += source->size_bytes;
        buffers[source->resource_index] = UINT64_MAX;
        ++buffer_count;
    }
    for (index = 0u; index < packet->binding_count; ++index)
        if (buffers[index] != UINT64_MAX)
            return RIN_VULKAN_PRODUCT_PROTOCOL;

    memset(&runtime_desc, 0, sizeof(runtime_desc));
    runtime_desc.struct_size = sizeof(runtime_desc);
    runtime_desc.version = RIN_GPU_RUNTIME_VERSION;
    runtime_desc.device_generation = 1u;
    runtime_desc.handle_secret = UINT64_C(0x564b434f4d505554);
    runtime_desc.max_buffer_size = total_buffer_bytes < UINT64_C(1048576)
                                       ? UINT64_C(1048576)
                                       : total_buffer_bytes;
    runtime_desc.max_image_size = UINT64_C(1048576);
    runtime_desc.max_total_allocation_size =
        total_buffer_bytes < UINT64_C(4194304)
            ? UINT64_C(4194304) : total_buffer_bytes;
    runtime_desc.max_image_dimension = 64u;
    runtime_desc.max_image_layers = 1u;
    runtime_desc.max_image_mip_levels = 1u;
    runtime_desc.max_image_sample_count = 1u;
    runtime_desc.flags = RIN_GPU_RUNTIME_FLAG_HEADLESS;
    runtime_desc.adapter.abi_version = RIN_GPU_ABI_VERSION;
    runtime_desc.adapter.struct_size = sizeof(runtime_desc.adapter);
    runtime_desc.adapter.queue_capabilities = RIN_GPU_QUEUE_COMPUTE |
                                              RIN_GPU_QUEUE_COPY;
    memcpy(runtime_desc.adapter.name, "RinVulkan software compute", 27u);
    result = ringpu_runtime_create(&runtime_desc, &runtime);
    if (result != RIN_GPU_OK || !runtime) goto done;

    memset(buffers, 0, sizeof(buffers));
    for (index = 0u; index < packet->binding_count; ++index) {
        const RinGpuVulkanComputeBindingV1* source =
            &packet->bindings[index];
        RinGpuVulkanSoftwareAllocationV1* allocation =
            allocation_by_handle(platform, source->allocation_handle);
        RinGpuBufferDescV1 buffer_desc;
        uint32_t usage = RIN_GPU_BUFFER_STORAGE;
        if (!allocation) {
            result = RIN_VULKAN_PRODUCT_PROTOCOL;
            goto done;
        }
        if ((source->access & RIN_GPU_RESOURCE_READ) != 0u)
            usage |= RIN_GPU_BUFFER_COPY_SOURCE;
        if ((source->access & RIN_GPU_RESOURCE_WRITE) != 0u)
            usage |= RIN_GPU_BUFFER_COPY_DESTINATION;
        memset(&buffer_desc, 0, sizeof(buffer_desc));
        buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
        buffer_desc.struct_size = sizeof(buffer_desc);
        buffer_desc.size_bytes = source->size_bytes;
        buffer_desc.usage = usage;
        buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
        result = ringpu_runtime_create_buffer(runtime, &buffer_desc,
                                              &buffers[source->resource_index]);
        if (result != RIN_GPU_OK || buffers[source->resource_index] == 0u)
            goto done;
        result = ringpu_runtime_upload_buffer(
            runtime, buffers[source->resource_index], 0u,
            allocation->bytes + source->offset, source->size_bytes);
        if (result != RIN_GPU_OK) goto done;
        bindings[source->resource_index].abi_version = RIN_GPU_ABI_VERSION;
        bindings[source->resource_index].struct_size =
            sizeof(bindings[source->resource_index]);
        bindings[source->resource_index].binding = source->resource_index;
        bindings[source->resource_index].access = source->access;
        bindings[source->resource_index].buffer =
            buffers[source->resource_index];
        bindings[source->resource_index].size_bytes = source->size_bytes;
    }

    result = ringpu_runtime_create_shader_module(
        runtime, packet->shader_ir, packet->shader_size_bytes,
        &shader_module);
    if (result != RIN_GPU_OK || shader_module == 0u) goto done;
    memset(&pipeline_desc, 0, sizeof(pipeline_desc));
    pipeline_desc.abi_version = RIN_GPU_ABI_VERSION;
    pipeline_desc.struct_size = sizeof(pipeline_desc);
    pipeline_desc.shader_module = shader_module;
    result = ringpu_runtime_create_compute_pipeline(
        runtime, &pipeline_desc, &pipeline);
    if (result != RIN_GPU_OK || pipeline == 0u) goto done;
    if (buffer_count != 0u) {
        result = ringpu_runtime_create_compute_bind_group(
            runtime, pipeline, bindings, buffer_count, &bind_group);
        if (result != RIN_GPU_OK || bind_group == 0u) goto done;
    }
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.abi_version = RIN_GPU_ABI_VERSION;
    queue_desc.struct_size = sizeof(queue_desc);
    queue_desc.capabilities = RIN_GPU_QUEUE_COMPUTE | RIN_GPU_QUEUE_COPY;
    result = ringpu_runtime_create_queue(runtime, &queue_desc, &queue);
    if (result != RIN_GPU_OK || queue == 0u) goto done;
    memset(&command_desc, 0, sizeof(command_desc));
    command_desc.abi_version = RIN_GPU_ABI_VERSION;
    command_desc.struct_size = sizeof(command_desc);
    command_desc.capabilities = RIN_GPU_QUEUE_COMPUTE;
    result = ringpu_runtime_create_command_list(runtime, &command_desc,
                                               &command_list);
    if (result != RIN_GPU_OK || command_list == 0u) goto done;
    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.abi_version = RIN_GPU_ABI_VERSION;
    dispatch.struct_size = sizeof(dispatch);
    dispatch.pipeline = pipeline;
    dispatch.bind_group = bind_group;
    dispatch.group_count_x = packet->group_count_x;
    dispatch.group_count_y = packet->group_count_y;
    dispatch.group_count_z = packet->group_count_z;
    result = ringpu_runtime_command_dispatch(runtime, command_list, &dispatch);
    if (result != RIN_GPU_OK) goto done;
    result = ringpu_runtime_command_list_close(runtime, command_list);
    if (result != RIN_GPU_OK) goto done;
    result = ringpu_runtime_create_fence(runtime, 0u, &fence);
    if (result != RIN_GPU_OK || fence == 0u) goto done;
    memset(&submit, 0, sizeof(submit));
    submit.abi_version = RIN_GPU_ABI_VERSION;
    submit.struct_size = sizeof(submit);
    submit.command_list = command_list;
    submit.signal_fence = fence;
    submit.signal_value = 1u;
    result = ringpu_runtime_queue_submit(runtime, queue, &submit);
    if (result != RIN_GPU_OK) goto done;
    result = ringpu_runtime_wait_fence(runtime, fence, 1u, UINT64_MAX);
    if (result != RIN_GPU_OK) goto done;

    for (index = 0u; index < packet->binding_count; ++index) {
        const RinGpuVulkanComputeBindingV1* source =
            &packet->bindings[index];
        RinGpuVulkanSoftwareAllocationV1* allocation =
            allocation_by_handle(platform, source->allocation_handle);
        if ((source->access & RIN_GPU_RESOURCE_WRITE) == 0u) continue;
        if (!allocation || ringpu_runtime_readback_buffer(
                               runtime, buffers[source->resource_index], 0u,
                               allocation->bytes + source->offset,
                               source->size_bytes) != RIN_GPU_OK) {
            result = RIN_GPU_ERROR_BACKEND;
            goto done;
        }
    }
    result = RIN_VULKAN_PRODUCT_OK;

done:
    if (runtime) ringpu_runtime_destroy(runtime);
    return result == RIN_VULKAN_PRODUCT_OK
               ? RIN_VULKAN_PRODUCT_OK
               : (result == RIN_VULKAN_PRODUCT_PROTOCOL
                      ? RIN_VULKAN_PRODUCT_PROTOCOL
                      : RIN_VULKAN_PRODUCT_BACKEND_FAILED);
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
    const RinGpuVulkanTransferPacketV3* packet_v3;
    const RinGpuVulkanComputePacketV1* compute_packet;
    RinGpuVulkanTransferPacketV2 routed_operations;
    RinVulkanProductReportV1* report;
    uint32_t copy_index;
    uint32_t operation_index;
    uint32_t next_tail;
    int compute_result;
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
    if (!packet || packet->struct_size < sizeof(uint32_t) * 2u)
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    if (packet->version == RIN_GPU_VULKAN_COMPUTE_PACKET_VERSION) {
        compute_packet = (const RinGpuVulkanComputePacketV1*)(uintptr_t)
            submission->command_cookie;
        if (compute_packet->struct_size <
                offsetof(RinGpuVulkanComputePacketV1, shader_ir) ||
            compute_packet->product_queue_id != submission->queue_id)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        compute_result = software_execute_compute(
            platform, compute_packet, resources, resource_count);
        if (compute_result != RIN_VULKAN_PRODUCT_OK) return compute_result;
    } else if (packet->struct_size < sizeof(uint32_t) * 4u ||
               packet->reserved != 0u) {
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    } else if (packet->version == RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION) {
        if (packet->struct_size != sizeof(*packet) ||
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
    } else if (packet->version == RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2 ||
               packet->version == RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_3) {
        if (packet->version == RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_3) {
            packet_v3 = (const RinGpuVulkanTransferPacketV3*)(uintptr_t)
                submission->command_cookie;
            if (packet_v3->struct_size != sizeof(*packet_v3) ||
                packet_v3->op_count >
                    RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS ||
                packet_v3->queue_family_index >=
                    RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES ||
                packet_v3->queue_index >= RIN_VULKAN_PRODUCT_MAX_QUEUES ||
                packet_v3->product_queue_id != submission->queue_id ||
                packet_v3->reserved_route != 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            memset(&routed_operations, 0, sizeof(routed_operations));
            routed_operations.struct_size = sizeof(routed_operations);
            routed_operations.version =
                RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2;
            routed_operations.op_count = packet_v3->op_count;
            memcpy(routed_operations.operations, packet_v3->operations,
                   sizeof(routed_operations.operations[0]) *
                       packet_v3->op_count);
            packet_v2 = &routed_operations;
        } else {
            packet_v2 = (const RinGpuVulkanTransferPacketV2*)(uintptr_t)
                submission->command_cookie;
        }
        if (packet_v2->struct_size != sizeof(*packet_v2) ||
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
            if (operation->reserved != 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            if (operation->type ==
                RIN_GPU_VULKAN_TRANSFER_OP_MEMORY_BARRIER) {
                if (operation->source_allocation != 0u ||
                    operation->destination_allocation != 0u ||
                    operation->source_gpu_address != 0u ||
                    operation->destination_gpu_address != 0u ||
                    operation->size_bytes != 0u ||
                    (operation->barrier.src_stage_mask &
                     ~RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) != 0u ||
                    (operation->barrier.src_access_mask &
                     ~RIN_GPU_VULKAN_BARRIER_ACCESS_ALL) != 0u ||
                    (operation->barrier.dst_stage_mask &
                     ~RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) != 0u ||
                    (operation->barrier.dst_access_mask &
                     ~RIN_GPU_VULKAN_BARRIER_ACCESS_ALL) != 0u)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
#if defined(_MSC_VER)
                _mm_mfence();
#else
                atomic_thread_fence(memory_order_seq_cst);
#endif
                continue;
            }
            if (operation->type ==
                    RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_BARRIER ||
                operation->type ==
                    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER) {
                RinGpuVulkanSoftwareAllocationV1* destination;
                uint32_t src_queue_family;
                uint32_t dst_queue_family;
                if (operation->source_allocation == 0u ||
                    operation->source_gpu_address != 0u ||
                    operation->destination_allocation == 0u ||
                    operation->size_bytes == 0u ||
                    !resource_has_access(
                        resources, resource_count,
                        operation->destination_allocation,
                        RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
                            RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) ||
                    (operation->barrier.src_stage_mask &
                     ~RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) != 0u ||
                    (operation->barrier.src_access_mask &
                     ~RIN_GPU_VULKAN_BARRIER_ACCESS_ALL) != 0u ||
                    (operation->barrier.dst_stage_mask &
                     ~RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) != 0u ||
                    (operation->barrier.dst_access_mask &
                     ~RIN_GPU_VULKAN_BARRIER_ACCESS_ALL) != 0u)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                destination = allocation_by_handle(
                    platform, operation->destination_allocation);
                if (!destination ||
                    allocation_for_range(platform,
                                         operation->destination_gpu_address,
                                         operation->size_bytes) != destination)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                if (operation->type ==
                    RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_BARRIER) {
                    src_queue_family = operation->source_width;
                    dst_queue_family = operation->source_height;
                    if (operation->destination_width != 0u ||
                        operation->destination_height != 0u ||
                        operation->filter != 0u ||
                        operation->sample_count != 0u)
                        return RIN_VULKAN_PRODUCT_PROTOCOL;
                } else {
                    src_queue_family = operation->destination_height;
                    dst_queue_family = operation->filter;
                    if ((operation->source_width !=
                             RIN_VK_IMAGE_LAYOUT_UNDEFINED &&
                         operation->source_width != RIN_VK_IMAGE_LAYOUT_GENERAL &&
                         operation->source_width !=
                             RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                         operation->source_width !=
                             RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
                        (operation->source_height !=
                             RIN_VK_IMAGE_LAYOUT_GENERAL &&
                         operation->source_height !=
                             RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                         operation->source_height !=
                             RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
                        (operation->destination_width !=
                             RIN_VK_IMAGE_ASPECT_COLOR_BIT &&
                         operation->destination_width !=
                             RIN_VK_IMAGE_ASPECT_DEPTH_BIT) ||
                        operation->sample_count != 0u)
                        return RIN_VULKAN_PRODUCT_PROTOCOL;
                }
                if (!((src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED &&
                       dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED) ||
                      (src_queue_family != RIN_VK_QUEUE_FAMILY_IGNORED &&
                       dst_queue_family != RIN_VK_QUEUE_FAMILY_IGNORED)))
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
#if defined(_MSC_VER)
                _mm_mfence();
#else
                atomic_thread_fence(memory_order_seq_cst);
#endif
                continue;
            }
            if ((operation->type != RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_COPY &&
                 (operation->type < RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_COPY ||
                  operation->type > RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE)) ||
                operation->size_bytes == 0u ||
                !resource_has_access(resources, resource_count,
                                     operation->destination_allocation,
                                     RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE))
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source = allocation_by_handle(platform, operation->source_allocation);
            destination = allocation_by_handle(platform,
                                               operation->destination_allocation);
            if (!destination ||
                allocation_for_range(platform, operation->destination_gpu_address,
                                     operation->size_bytes) != destination)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            destination_offset = operation->destination_gpu_address -
                                 destination->gpu_virtual_address;
            if (operation->type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR) {
                uint64_t clear_offset;
                if (operation->source_allocation != 0u ||
                    operation->source_gpu_address != 0u ||
                    (operation->size_bytes & 3u) != 0u)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                for (clear_offset = 0u; clear_offset < operation->size_bytes;
                     clear_offset += sizeof(operation->clear_value[0]))
                    memcpy(destination->bytes + destination_offset + clear_offset,
                           &operation->clear_value[0],
                           sizeof(operation->clear_value[0]));
                continue;
            }
            if (operation->type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BLIT) {
                uint64_t source_size;
                uint64_t destination_size;
                uint32_t y;
                if (operation->source_width == 0u ||
                    operation->source_height == 0u ||
                    operation->destination_width == 0u ||
                    operation->destination_height == 0u ||
                    operation->source_width > 4096u ||
                    operation->source_height > 4096u ||
                    operation->destination_width > 4096u ||
                    operation->destination_height > 4096u ||
                    operation->filter > 1u)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                source_size = (uint64_t)operation->source_width *
                              (uint64_t)operation->source_height * 4u;
                destination_size = (uint64_t)operation->destination_width *
                                   (uint64_t)operation->destination_height * 4u;
                if (operation->size_bytes != destination_size ||
                    !resource_has_access(resources, resource_count,
                                         operation->source_allocation,
                                         RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) ||
                    allocation_for_range(platform, operation->source_gpu_address,
                                         source_size) != source ||
                    allocation_for_range(platform,
                                         operation->destination_gpu_address,
                                         destination_size) != destination ||
                    source == destination)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                source_offset = operation->source_gpu_address -
                                source->gpu_virtual_address;
                for (y = 0u; y < operation->destination_height; ++y) {
                    uint32_t x;
                    for (x = 0u; x < operation->destination_width; ++x) {
                        uint32_t sx0;
                        uint32_t sx1;
                        uint32_t sy0;
                        uint32_t sy1;
                        uint32_t fx = 0u;
                        uint32_t fy = 0u;
                        uint32_t channel;
                        uint64_t src_x_fp;
                        uint64_t src_y_fp;
                        uint8_t* out = destination->bytes + destination_offset +
                            ((uint64_t)y * operation->destination_width + x) * 4u;
                        if (operation->filter == 0u) {
                            sx0 = (uint32_t)(((uint64_t)x *
                                              operation->source_width) /
                                             operation->destination_width);
                            sy0 = (uint32_t)(((uint64_t)y *
                                              operation->source_height) /
                                             operation->destination_height);
                            if (sx0 >= operation->source_width)
                                sx0 = operation->source_width - 1u;
                            if (sy0 >= operation->source_height)
                                sy0 = operation->source_height - 1u;
                            memcpy(out, source->bytes + source_offset +
                                         ((uint64_t)sy0 * operation->source_width +
                                          sx0) * 4u, 4u);
                            continue;
                        }
                        src_x_fp = operation->destination_width == 1u
                                       ? 0u
                                       : ((uint64_t)x *
                                          (operation->source_width - 1u) *
                                          65536u) /
                                             (operation->destination_width - 1u);
                        src_y_fp = operation->destination_height == 1u
                                       ? 0u
                                       : ((uint64_t)y *
                                          (operation->source_height - 1u) *
                                          65536u) /
                                             (operation->destination_height - 1u);
                        sx0 = (uint32_t)(src_x_fp / 65536u);
                        sy0 = (uint32_t)(src_y_fp / 65536u);
                        sx1 = sx0 + 1u < operation->source_width ? sx0 + 1u : sx0;
                        sy1 = sy0 + 1u < operation->source_height ? sy0 + 1u : sy0;
                        fx = (uint32_t)(src_x_fp & 65535u);
                        fy = (uint32_t)(src_y_fp & 65535u);
                        for (channel = 0u; channel < 4u; ++channel) {
                            const uint8_t* p00 = source->bytes + source_offset +
                                ((uint64_t)sy0 * operation->source_width + sx0) * 4u;
                            const uint8_t* p10 = source->bytes + source_offset +
                                ((uint64_t)sy0 * operation->source_width + sx1) * 4u;
                            const uint8_t* p01 = source->bytes + source_offset +
                                ((uint64_t)sy1 * operation->source_width + sx0) * 4u;
                            const uint8_t* p11 = source->bytes + source_offset +
                                ((uint64_t)sy1 * operation->source_width + sx1) * 4u;
                            uint32_t top = ((uint32_t)p00[channel] *
                                                (65536u - fx) +
                                            (uint32_t)p10[channel] * fx +
                                            32768u) >> 16;
                            uint32_t bottom = ((uint32_t)p01[channel] *
                                                   (65536u - fx) +
                                               (uint32_t)p11[channel] * fx +
                                               32768u) >> 16;
                            out[channel] = (uint8_t)((top * (65536u - fy) +
                                                     bottom * fy + 32768u) >> 16);
                        }
                    }
                }
                continue;
            }
            if (operation->type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE) {
                uint64_t source_size;
                uint64_t destination_size;
                uint64_t pixel_count;
                uint64_t pixel;
                if (operation->source_width == 0u ||
                    operation->source_height == 0u ||
                    operation->source_width > 4096u ||
                    operation->source_height > 4096u ||
                    operation->destination_width > 4096u ||
                    operation->destination_height > 4096u ||
                    operation->source_width != operation->destination_width ||
                    operation->source_height != operation->destination_height ||
                    (operation->sample_count != 2u &&
                     operation->sample_count != 4u) ||
                    !resource_has_access(resources, resource_count,
                                         operation->source_allocation,
                                         RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) ||
                    source == destination)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                source_size = (uint64_t)operation->source_width *
                              operation->source_height * 4u *
                              operation->sample_count;
                destination_size = (uint64_t)operation->destination_width *
                                   operation->destination_height * 4u;
                pixel_count = (uint64_t)operation->destination_width *
                              operation->destination_height;
                if (operation->size_bytes != destination_size ||
                    allocation_for_range(platform, operation->source_gpu_address,
                                         source_size) != source ||
                    allocation_for_range(platform,
                                         operation->destination_gpu_address,
                                         destination_size) != destination)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                source_offset = operation->source_gpu_address -
                                source->gpu_virtual_address;
                for (pixel = 0u; pixel < pixel_count; ++pixel) {
                    uint32_t channel;
                    for (channel = 0u; channel < 4u; ++channel) {
                        uint32_t sample;
                        uint32_t sum = 0u;
                        for (sample = 0u; sample < operation->sample_count;
                             ++sample)
                            sum += source->bytes[source_offset +
                                ((uint64_t)sample * pixel_count + pixel) * 4u +
                                channel];
                        destination->bytes[destination_offset + pixel * 4u +
                                           channel] = (uint8_t)((sum +
                            operation->sample_count / 2u) /
                            operation->sample_count);
                    }
                }
                continue;
            }
            if (!resource_has_access(resources, resource_count,
                                     operation->source_allocation,
                                     RIN_VULKAN_PRODUCT_MEMORY_GPU_READ))
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source = allocation_by_handle(platform, operation->source_allocation);
            if (!source ||
                allocation_for_range(platform, operation->source_gpu_address,
                                     operation->size_bytes) != source)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source_offset = operation->source_gpu_address -
                            source->gpu_virtual_address;
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
