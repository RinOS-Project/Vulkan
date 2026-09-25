/* SPDX-License-Identifier: MIT */

#include <rinvulkan/command_runtime.h>

#include <stddef.h>
#include <string.h>

#define RIN_GPU_VULKAN_COMMAND_POOL_TAG UINT64_C(0x5243)

static int overlaps_runtime(const RinGpuVulkanCommandRuntimeV1* runtime,
                            const void* data, size_t size) {
    uintptr_t runtime_first;
    uintptr_t runtime_end;
    uintptr_t data_first;
    uintptr_t data_end;
    if (!runtime || !data || size == 0u) return 0;
    runtime_first = (uintptr_t)runtime;
    data_first = (uintptr_t)data;
    if (sizeof(*runtime) > UINTPTR_MAX - runtime_first ||
        size > UINTPTR_MAX - data_first)
        return 1;
    runtime_end = runtime_first + sizeof(*runtime);
    data_end = data_first + size;
    return data_first < runtime_end && runtime_first < data_end;
}

static RinGpuVulkanCommandPoolHandleV1 make_pool_handle(
        uint32_t index, uint32_t generation) {
    return (RIN_GPU_VULKAN_COMMAND_POOL_TAG << 48u) |
           ((uint64_t)generation << 16u) | (uint64_t)(index + 1u);
}

static RinGpuVulkanCommandPoolSlotV1* pool_slot(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
        RinGpuVulkanCommandPoolHandleV1 handle) {
    uint32_t index_field;
    uint32_t index;
    uint32_t generation;
    RinGpuVulkanCommandPoolSlotV1* pool;
    if (!runtime || owner == 0u ||
        (handle >> 48u) != RIN_GPU_VULKAN_COMMAND_POOL_TAG)
        return NULL;
    index_field = (uint32_t)(handle & UINT64_C(0xffff));
    generation = (uint32_t)(handle >> 16u);
    if (index_field == 0u ||
        index_field > RIN_GPU_VULKAN_COMMAND_MAX_POOLS || generation == 0u)
        return NULL;
    index = index_field - 1u;
    pool = &runtime->pools[index];
    if (__atomic_load_n(&pool->state, __ATOMIC_ACQUIRE) != 1u ||
        pool->generation != generation ||
        __atomic_load_n(&pool->owner, __ATOMIC_ACQUIRE) != owner)
        return NULL;
    return pool;
}

static RinGpuVulkanCommandBufferV1* buffer_slot(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* buffer) {
    uintptr_t address;
    uintptr_t first;
    uintptr_t end;
    uintptr_t offset;
    uint32_t index;
    if (!runtime || !buffer) return NULL;
    address = (uintptr_t)buffer;
    first = (uintptr_t)&runtime->buffers[0];
    end = (uintptr_t)&runtime->buffers[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];
    if (address < first || address >= end) return NULL;
    offset = address - first;
    if ((offset % sizeof(runtime->buffers[0])) != 0u) return NULL;
    index = (uint32_t)(offset / sizeof(runtime->buffers[0]));
    if (__atomic_load_n(&runtime->buffers[index].state,
                        __ATOMIC_ACQUIRE) != 1u)
        return NULL;
    return &runtime->buffers[index];
}

static void reset_recording(RinGpuVulkanCommandBufferV1* buffer) {
    buffer->usage_flags = 0u;
    buffer->record_error = 0u;
    buffer->copy_count = 0u;
    memset(buffer->copies, 0, sizeof(buffer->copies));
    buffer->transfer_op_count = 0u;
    memset(buffer->transfer_ops, 0, sizeof(buffer->transfer_ops));
    buffer->descriptor_bind_recorded = 0u;
    buffer->descriptor_bind_first_set = 0u;
    buffer->descriptor_bind_set_count = 0u;
    buffer->descriptor_dynamic_offset_count = 0u;
    memset(buffer->descriptor_sets, 0, sizeof(buffer->descriptor_sets));
    memset(buffer->descriptor_dynamic_offsets, 0,
           sizeof(buffer->descriptor_dynamic_offsets));
    buffer->barrier_count = 0u;
    buffer->reserved_barrier = 0u;
    memset(buffer->barriers, 0, sizeof(buffer->barriers));
    buffer->query_command_count = 0u;
    buffer->event_command_count = 0u;
    memset(buffer->query_commands, 0, sizeof(buffer->query_commands));
    memset(buffer->event_commands, 0, sizeof(buffer->event_commands));
}

static int pool_has_in_flight(const RinGpuVulkanCommandRuntimeV1* runtime,
                              const RinGpuVulkanCommandPoolSlotV1* pool) {
    uint32_t index;

    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS; ++index) {
        const RinGpuVulkanCommandBufferV1* buffer = &runtime->buffers[index];
        if (__atomic_load_n(&buffer->state, __ATOMIC_ACQUIRE) == 1u &&
            __atomic_load_n(&buffer->pool, __ATOMIC_ACQUIRE) == pool &&
            __atomic_load_n(&buffer->pool_generation, __ATOMIC_ACQUIRE) ==
                pool->generation && buffer->in_flight_count != 0u) {
            return 1;
        }
    }
    return 0;
}

static int submission_buffers_valid(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers) {
    uint32_t index;

    if (!runtime || !buffers || count == 0u ||
        count > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS ||
        overlaps_runtime(runtime, buffers, sizeof(*buffers) * (size_t)count)) {
        return 0;
    }
    for (index = 0u; index < count; ++index) {
        RinGpuVulkanCommandBufferV1* buffer =
            buffer_slot(runtime, buffers[index]);
        uint32_t prior;

        if (!buffer || !buffer->pool ||
            __atomic_load_n(&buffer->pool->state, __ATOMIC_ACQUIRE) != 1u ||
            buffer->pool_generation != buffer->pool->generation ||
            buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_EXECUTABLE ||
            buffer->record_error != 0u ||
            (buffer->in_flight_count != 0u &&
             (buffer->usage_flags &
              RIN_GPU_VULKAN_COMMAND_USAGE_SIMULTANEOUS) == 0u)) {
            return 0;
        }
        for (prior = 0u; prior < index; ++prior) {
            if (buffers[prior] == buffers[index]) return 0;
        }
    }
    return 1;
}

static void clear_buffer(RinGpuVulkanCommandBufferV1* buffer) {
    buffer->loader_magic = 0u;
    buffer->lifecycle = 0u;
    __atomic_store_n(&buffer->owner, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&buffer->pool, NULL, __ATOMIC_RELAXED);
    __atomic_store_n(&buffer->pool_generation, 0u, __ATOMIC_RELAXED);
    buffer->level = 0u;
    reset_recording(buffer);
    buffer->in_flight_count = 0u;
    __atomic_store_n(&buffer->state, 0u, __ATOMIC_RELEASE);
}

static uint32_t clear_pool_buffers(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandPoolSlotV1* pool) {
    uint32_t index;
    uint32_t cleared = 0u;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS; ++index) {
        RinGpuVulkanCommandBufferV1* buffer = &runtime->buffers[index];
        uint32_t expected = 1u;
        if (__atomic_load_n(&buffer->state, __ATOMIC_ACQUIRE) != 1u ||
            __atomic_load_n(&buffer->pool, __ATOMIC_ACQUIRE) != pool ||
            __atomic_load_n(&buffer->pool_generation, __ATOMIC_ACQUIRE) !=
                pool->generation ||
            !__atomic_compare_exchange_n(&buffer->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        clear_buffer(buffer);
        ++cleared;
    }
    return cleared;
}

int rin_gpu_vulkan_command_pool_create(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
        uint32_t queue_family_index, uint32_t flags,
        RinGpuVulkanCommandPoolHandleV1* pool_out) {
    uint32_t index;
    if (!pool_out) return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    if (runtime &&
        overlaps_runtime(runtime, pool_out, sizeof(*pool_out)))
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    *pool_out = 0u;
    if (!runtime || owner == 0u ||
        (flags & ~RIN_GPU_VULKAN_COMMAND_POOL_FLAGS_KNOWN) != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_POOLS; ++index) {
        RinGpuVulkanCommandPoolSlotV1* pool = &runtime->pools[index];
        uint32_t expected = 0u;
        if (!__atomic_compare_exchange_n(&pool->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        if (pool->generation == UINT32_MAX) {
            __atomic_store_n(&pool->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        ++pool->generation;
        __atomic_store_n(&pool->owner, owner, __ATOMIC_RELAXED);
        pool->queue_family_index = queue_family_index;
        pool->flags = flags;
        pool->live_buffer_count = 0u;
        pool->reserved = 0u;
        *pool_out = make_pool_handle(index, pool->generation);
        __atomic_store_n(&pool->state, 1u, __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_COMMAND_OK;
    }
    return RIN_GPU_VULKAN_COMMAND_LIMIT;
}

int rin_gpu_vulkan_command_pool_destroy(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
        RinGpuVulkanCommandPoolHandleV1 handle) {
    RinGpuVulkanCommandPoolSlotV1* pool = pool_slot(runtime, owner, handle);
    uint32_t expected = 1u;
    if (!pool) return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    if (pool_has_in_flight(runtime, pool))
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    if (!__atomic_compare_exchange_n(&pool->state, &expected, 2u, 0,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    (void)clear_pool_buffers(runtime, pool);
    __atomic_store_n(&pool->owner, 0u, __ATOMIC_RELAXED);
    pool->queue_family_index = 0u;
    pool->flags = 0u;
    pool->live_buffer_count = 0u;
    pool->reserved = 0u;
    __atomic_store_n(&pool->state, 0u, __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_pool_reset(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
        RinGpuVulkanCommandPoolHandleV1 handle, uint32_t flags) {
    RinGpuVulkanCommandPoolSlotV1* pool;
    uint32_t index;
    if ((flags & ~RIN_GPU_VULKAN_COMMAND_RESET_RELEASE_RESOURCES) != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    pool = pool_slot(runtime, owner, handle);
    if (!pool) return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    if (pool_has_in_flight(runtime, pool))
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS; ++index) {
        RinGpuVulkanCommandBufferV1* buffer = &runtime->buffers[index];
        if (__atomic_load_n(&buffer->state, __ATOMIC_ACQUIRE) == 1u &&
            __atomic_load_n(&buffer->pool, __ATOMIC_ACQUIRE) == pool &&
            __atomic_load_n(&buffer->pool_generation, __ATOMIC_ACQUIRE) ==
                pool->generation) {
            buffer->lifecycle = RIN_GPU_VULKAN_COMMAND_BUFFER_INITIAL;
            reset_recording(buffer);
        }
    }
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffers_allocate(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
        RinGpuVulkanCommandPoolHandleV1 handle, uint32_t level,
        uint32_t count, uintptr_t loader_magic,
        RinGpuVulkanCommandBufferV1** buffers_out) {
    RinGpuVulkanCommandPoolSlotV1* pool;
    RinGpuVulkanCommandBufferV1*
        reserved[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];
    uint32_t reserved_count = 0u;
    uint32_t index;
    if (!runtime || !buffers_out || count == 0u ||
        count > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS || loader_magic == 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    if (overlaps_runtime(runtime, buffers_out,
                         sizeof(*buffers_out) * (size_t)count))
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    if (level != 0u) return RIN_GPU_VULKAN_COMMAND_UNSUPPORTED;
    pool = pool_slot(runtime, owner, handle);
    if (!pool) return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    for (index = 0u;
         index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS &&
         reserved_count < count;
         ++index) {
        uint32_t expected = 0u;
        if (__atomic_compare_exchange_n(&runtime->buffers[index].state,
                                        &expected, 2u, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED))
            reserved[reserved_count++] = &runtime->buffers[index];
    }
    if (reserved_count != count) {
        while (reserved_count != 0u) {
            --reserved_count;
            __atomic_store_n(&reserved[reserved_count]->state, 0u,
                             __ATOMIC_RELEASE);
        }
        return RIN_GPU_VULKAN_COMMAND_LIMIT;
    }
    for (index = 0u; index < count; ++index) {
        RinGpuVulkanCommandBufferV1* buffer = reserved[index];
        buffer->loader_magic = loader_magic;
        buffer->lifecycle = RIN_GPU_VULKAN_COMMAND_BUFFER_INITIAL;
        __atomic_store_n(&buffer->owner, owner, __ATOMIC_RELAXED);
        __atomic_store_n(&buffer->pool, pool, __ATOMIC_RELAXED);
        __atomic_store_n(&buffer->pool_generation, pool->generation,
                         __ATOMIC_RELAXED);
        buffer->level = level;
        reset_recording(buffer);
        buffer->in_flight_count = 0u;
        __atomic_store_n(&buffer->state, 1u, __ATOMIC_RELEASE);
        buffers_out[index] = buffer;
    }
    pool->live_buffer_count += count;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

uint32_t rin_gpu_vulkan_command_buffers_free(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
        RinGpuVulkanCommandPoolHandleV1 handle, uint32_t count,
        RinGpuVulkanCommandBufferV1* const* buffers) {
    RinGpuVulkanCommandPoolSlotV1* pool;
    uint32_t index;
    uint32_t freed = 0u;
    if (!runtime || !buffers || count == 0u ||
        count > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS ||
        overlaps_runtime(runtime, buffers,
                         sizeof(*buffers) * (size_t)count))
        return 0u;
    pool = pool_slot(runtime, owner, handle);
    if (!pool) return 0u;
    for (index = 0u; index < count; ++index) {
        RinGpuVulkanCommandBufferV1* buffer =
            buffer_slot(runtime, buffers[index]);
        uint32_t expected = 1u;
        if (!buffer || buffer->owner != owner || buffer->pool != pool ||
            buffer->pool_generation != pool->generation ||
            buffer->in_flight_count != 0u ||
            !__atomic_compare_exchange_n(&buffer->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        clear_buffer(buffer);
        ++freed;
    }
    if (freed <= pool->live_buffer_count)
        pool->live_buffer_count -= freed;
    else
        pool->live_buffer_count = 0u;
    return freed;
}

int rin_gpu_vulkan_command_buffer_begin(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle, uint32_t usage_flags) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer || !buffer->pool ||
        __atomic_load_n(&buffer->pool->state, __ATOMIC_ACQUIRE) != 1u ||
        buffer->pool_generation != buffer->pool->generation)
        return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    if ((usage_flags & ~RIN_GPU_VULKAN_COMMAND_USAGE_FLAGS_KNOWN) != 0u)
        return RIN_GPU_VULKAN_COMMAND_UNSUPPORTED;
    if (buffer->in_flight_count != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    if (buffer->lifecycle == RIN_GPU_VULKAN_COMMAND_BUFFER_EXECUTABLE &&
        (buffer->pool->flags &
         RIN_GPU_VULKAN_COMMAND_POOL_RESET_BUFFER) == 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    if (buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_INITIAL &&
        buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_EXECUTABLE)
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    reset_recording(buffer);
    buffer->usage_flags = usage_flags;
    buffer->lifecycle = RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_end(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer || !buffer->pool ||
        __atomic_load_n(&buffer->pool->state, __ATOMIC_ACQUIRE) != 1u ||
        buffer->pool_generation != buffer->pool->generation)
        return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    if (buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING)
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    if (buffer->record_error != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    buffer->lifecycle = RIN_GPU_VULKAN_COMMAND_BUFFER_EXECUTABLE;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_reset(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle, uint32_t flags) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer) return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    if ((flags & ~RIN_GPU_VULKAN_COMMAND_RESET_RELEASE_RESOURCES) != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    if (!buffer->pool ||
        __atomic_load_n(&buffer->pool->state, __ATOMIC_ACQUIRE) != 1u ||
        buffer->pool_generation != buffer->pool->generation)
        return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    if ((buffer->pool->flags &
         RIN_GPU_VULKAN_COMMAND_POOL_RESET_BUFFER) == 0u)
        return RIN_GPU_VULKAN_COMMAND_UNSUPPORTED;
    if (buffer->in_flight_count != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    reset_recording(buffer);
    buffer->lifecycle = RIN_GPU_VULKAN_COMMAND_BUFFER_INITIAL;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_owner(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* handle, uintptr_t* owner_out) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);

    if (!owner_out) return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    *owner_out = 0u;
    if (!buffer || !buffer->pool ||
        __atomic_load_n(&buffer->pool->state, __ATOMIC_ACQUIRE) != 1u ||
        buffer->pool_generation != buffer->pool->generation ||
        buffer->owner == 0u) {
        return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    }
    *owner_out = buffer->owner;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_record_copies(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* handle,
    const RinGpuVulkanBufferCopyCommandV1* copies, uint32_t copy_count) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    uint32_t index;

    if (!copies || copy_count == 0u ||
        copy_count > RIN_GPU_VULKAN_COMMAND_MAX_COPIES ||
        (runtime && overlaps_runtime(runtime, copies,
                                     sizeof(*copies) * (size_t)copy_count))) {
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    }
    if (!buffer || !buffer->pool ||
        __atomic_load_n(&buffer->pool->state, __ATOMIC_ACQUIRE) != 1u ||
        buffer->pool_generation != buffer->pool->generation) {
        return RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE;
    }
    if (buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        buffer->record_error != 0u || buffer->in_flight_count != 0u) {
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    }
    if (copy_count > RIN_GPU_VULKAN_COMMAND_MAX_COPIES -
                         buffer->copy_count) {
        return RIN_GPU_VULKAN_COMMAND_LIMIT;
    }
    for (index = 0u; index < copy_count; ++index) {
        if (copies[index].source_allocation == 0u ||
            copies[index].destination_allocation == 0u ||
            copies[index].source_gpu_address == 0u ||
            copies[index].destination_gpu_address == 0u ||
            copies[index].size_bytes == 0u) {
            return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
        }
    }
    memcpy(&buffer->copies[buffer->copy_count], copies,
           sizeof(*copies) * (size_t)copy_count);
    buffer->copy_count += copy_count;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_record_transfer_ops(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle,
        const RinGpuVulkanTransferOpV2* operations,
        uint32_t operation_count) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    uint32_t index;
    if (!buffer || !operations || operation_count == 0u ||
        operation_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS ||
        buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        buffer->transfer_op_count >
            RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS - operation_count ||
        overlaps_runtime(runtime, operations,
                         sizeof(*operations) * (size_t)operation_count))
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    for (index = 0u; index < operation_count; ++index) {
        if (operations[index].type < RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_COPY ||
            operations[index].type > RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE ||
            operations[index].reserved != 0u ||
            operations[index].destination_allocation == 0u ||
            operations[index].destination_gpu_address == 0u ||
            operations[index].size_bytes == 0u)
            return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
        if (operations[index].type != RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR &&
            (operations[index].source_allocation == 0u ||
             operations[index].source_gpu_address == 0u))
            return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
        if (operations[index].type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR &&
            (operations[index].source_allocation != 0u ||
             operations[index].source_gpu_address != 0u ||
             (operations[index].size_bytes & 3u) != 0u))
            return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
        if (operations[index].type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BLIT &&
            (operations[index].source_width == 0u ||
             operations[index].source_height == 0u ||
             operations[index].destination_width == 0u ||
             operations[index].destination_height == 0u ||
             operations[index].filter > 1u))
            return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
        if (operations[index].type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE &&
            (operations[index].source_width == 0u ||
             operations[index].source_height == 0u ||
             operations[index].destination_width == 0u ||
             operations[index].destination_height == 0u ||
             (operations[index].sample_count != 2u &&
              operations[index].sample_count != 4u)))
            return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    }
    memcpy(&buffer->transfer_ops[buffer->transfer_op_count], operations,
           sizeof(*operations) * (size_t)operation_count);
    buffer->transfer_op_count += operation_count;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_record_descriptor_bind(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle, uint32_t first_set,
        const uint64_t* descriptor_sets, uint32_t descriptor_set_count,
        const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer || buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        !descriptor_sets || descriptor_set_count == 0u ||
        descriptor_set_count > RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS ||
        dynamic_offset_count > RIN_GPU_VULKAN_COMMAND_MAX_DYNAMIC_OFFSETS ||
        (dynamic_offset_count != 0u && !dynamic_offsets) ||
        buffer->descriptor_bind_recorded != 0u)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    buffer->descriptor_bind_recorded = 1u;
    buffer->descriptor_bind_first_set = first_set;
    buffer->descriptor_bind_set_count = descriptor_set_count;
    buffer->descriptor_dynamic_offset_count = dynamic_offset_count;
    memcpy(buffer->descriptor_sets, descriptor_sets,
           sizeof(*descriptor_sets) * descriptor_set_count);
    if (dynamic_offset_count != 0u)
        memcpy(buffer->descriptor_dynamic_offsets, dynamic_offsets,
               sizeof(*dynamic_offsets) * dynamic_offset_count);
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_record_barrier(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle, uint64_t src_stage_mask,
        uint64_t src_access_mask, uint64_t dst_stage_mask,
        uint64_t dst_access_mask) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer || buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        src_stage_mask == 0u ||
        (src_stage_mask & ~RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) != 0u ||
        src_access_mask == 0u ||
        (src_access_mask & ~RIN_GPU_VULKAN_BARRIER_ACCESS_ALL) != 0u ||
        dst_stage_mask == 0u ||
        (dst_stage_mask & ~RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) != 0u ||
        dst_access_mask == 0u ||
        (dst_access_mask & ~RIN_GPU_VULKAN_BARRIER_ACCESS_ALL) != 0u ||
        buffer->barrier_count >= RIN_GPU_VULKAN_COMMAND_MAX_BARRIERS)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    buffer->barriers[buffer->barrier_count].src_stage_mask = src_stage_mask;
    buffer->barriers[buffer->barrier_count].src_access_mask = src_access_mask;
    buffer->barriers[buffer->barrier_count].dst_stage_mask = dst_stage_mask;
    buffer->barriers[buffer->barrier_count].dst_access_mask = dst_access_mask;
    ++buffer->barrier_count;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_record_query(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle, uint64_t query_pool,
        uint32_t query, uint32_t flags, uint32_t operation) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer || buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        query_pool == 0u || operation < RIN_GPU_VULKAN_QUERY_COMMAND_BEGIN ||
        operation > RIN_GPU_VULKAN_QUERY_COMMAND_TIMESTAMP ||
        buffer->query_command_count >=
            RIN_GPU_VULKAN_COMMAND_MAX_QUERY_COMMANDS)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    buffer->query_commands[buffer->query_command_count].query_pool = query_pool;
    buffer->query_commands[buffer->query_command_count].query = query;
    buffer->query_commands[buffer->query_command_count].flags = flags;
    buffer->query_commands[buffer->query_command_count].operation = operation;
    buffer->query_commands[buffer->query_command_count].reserved = 0u;
    ++buffer->query_command_count;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffer_record_event(
        RinGpuVulkanCommandRuntimeV1* runtime,
        RinGpuVulkanCommandBufferV1* handle, uint64_t event,
        uint32_t operation) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);
    if (!buffer || buffer->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        event == 0u || operation < RIN_GPU_VULKAN_EVENT_COMMAND_SET ||
        operation > RIN_GPU_VULKAN_EVENT_COMMAND_WAIT ||
        buffer->event_command_count >=
            RIN_GPU_VULKAN_COMMAND_MAX_EVENT_COMMANDS)
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    buffer->event_commands[buffer->event_command_count].event = event;
    buffer->event_commands[buffer->event_command_count].operation = operation;
    buffer->event_commands[buffer->event_command_count].reserved = 0u;
    ++buffer->event_command_count;
    return RIN_GPU_VULKAN_COMMAND_OK;
}

void rin_gpu_vulkan_command_buffer_record_failure(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* handle) {
    RinGpuVulkanCommandBufferV1* buffer = buffer_slot(runtime, handle);

    if (buffer && buffer->lifecycle ==
                      RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING) {
        buffer->record_error = 1u;
    }
}

int rin_gpu_vulkan_command_buffers_validate_submit(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers) {
    return submission_buffers_valid(runtime, count, buffers)
               ? RIN_GPU_VULKAN_COMMAND_OK
               : RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
}

int rin_gpu_vulkan_command_buffers_mark_submitted(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers) {
    uint32_t index;

    if (!submission_buffers_valid(runtime, count, buffers)) {
        return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
    }
    for (index = 0u; index < count; ++index) {
        if (buffers[index]->in_flight_count == UINT32_MAX) {
            return RIN_GPU_VULKAN_COMMAND_LIMIT;
        }
    }
    for (index = 0u; index < count; ++index) {
        buffers[index]->in_flight_count++;
    }
    return RIN_GPU_VULKAN_COMMAND_OK;
}

int rin_gpu_vulkan_command_buffers_complete(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers) {
    uint32_t index;

    if (!runtime || !buffers || count == 0u ||
        count > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS ||
        overlaps_runtime(runtime, buffers, sizeof(*buffers) * (size_t)count)) {
        return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
    }
    for (index = 0u; index < count; ++index) {
        RinGpuVulkanCommandBufferV1* buffer =
            buffer_slot(runtime, buffers[index]);
        uint32_t prior;

        if (!buffer || buffer->in_flight_count == 0u) {
            return RIN_GPU_VULKAN_COMMAND_INVALID_STATE;
        }
        for (prior = 0u; prior < index; ++prior) {
            if (buffers[prior] == buffers[index]) {
                return RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT;
            }
        }
    }
    for (index = 0u; index < count; ++index) {
        buffers[index]->in_flight_count--;
    }
    return RIN_GPU_VULKAN_COMMAND_OK;
}

void rin_gpu_vulkan_command_buffers_abort(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers) {
    uint32_t index;

    if (!runtime || !buffers || count == 0u ||
        count > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS ||
        overlaps_runtime(runtime, buffers, sizeof(*buffers) * (size_t)count)) {
        return;
    }
    for (index = 0u; index < count; ++index) {
        RinGpuVulkanCommandBufferV1* buffer =
            buffer_slot(runtime, buffers[index]);
        if (buffer && buffer->in_flight_count != 0u) {
            buffer->in_flight_count--;
        }
    }
}

uint32_t rin_gpu_vulkan_command_owner_cleanup(
        RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner) {
    uint32_t index;
    uint32_t pools = 0u;
    if (!runtime || owner == 0u) return 0u;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_POOLS; ++index) {
        RinGpuVulkanCommandPoolSlotV1* pool = &runtime->pools[index];
        uint32_t expected = 1u;
        if (__atomic_load_n(&pool->state, __ATOMIC_ACQUIRE) != 1u ||
            __atomic_load_n(&pool->owner, __ATOMIC_ACQUIRE) != owner ||
            !__atomic_compare_exchange_n(&pool->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        (void)clear_pool_buffers(runtime, pool);
        __atomic_store_n(&pool->owner, 0u, __ATOMIC_RELAXED);
        pool->queue_family_index = 0u;
        pool->flags = 0u;
        pool->live_buffer_count = 0u;
        pool->reserved = 0u;
        __atomic_store_n(&pool->state, 0u, __ATOMIC_RELEASE);
        ++pools;
    }
    return pools;
}
