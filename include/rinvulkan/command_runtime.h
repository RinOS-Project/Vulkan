/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_COMMAND_RUNTIME_H
#define RINVULKAN_PUBLIC_COMMAND_RUNTIME_H

#include <stdint.h>

#define RIN_GPU_VULKAN_COMMAND_MAX_POOLS 64u
#define RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS 128u
#define RIN_GPU_VULKAN_COMMAND_MAX_COPIES 4u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_COPIES 16u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION 1u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2 2u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS 16u
#define RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS 8u
#define RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS 4u
#define RIN_GPU_VULKAN_COMMAND_MAX_DYNAMIC_OFFSETS 32u
#define RIN_GPU_VULKAN_COMMAND_MAX_BARRIERS 8u
#define RIN_GPU_VULKAN_COMMAND_MAX_QUERY_COMMANDS 16u
#define RIN_GPU_VULKAN_COMMAND_MAX_EVENT_COMMANDS 16u

#define RIN_GPU_VULKAN_COMMAND_POOL_TRANSIENT 0x00000001u
#define RIN_GPU_VULKAN_COMMAND_POOL_RESET_BUFFER 0x00000002u
#define RIN_GPU_VULKAN_COMMAND_POOL_FLAGS_KNOWN 0x00000003u

#define RIN_GPU_VULKAN_COMMAND_USAGE_ONE_TIME 0x00000001u
#define RIN_GPU_VULKAN_COMMAND_USAGE_SIMULTANEOUS 0x00000004u
#define RIN_GPU_VULKAN_COMMAND_USAGE_FLAGS_KNOWN 0x00000005u

#define RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER UINT64_C(0x0000000000000001)
#define RIN_GPU_VULKAN_BARRIER_STAGE_HOST UINT64_C(0x0000000000000002)
#define RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS \
    (RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER | \
     RIN_GPU_VULKAN_BARRIER_STAGE_HOST)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ UINT64_C(0x0000000000000001)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE UINT64_C(0x0000000000000002)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ UINT64_C(0x0000000000000004)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE UINT64_C(0x0000000000000008)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_ALL \
    (RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE)

#define RIN_GPU_VULKAN_COMMAND_RESET_RELEASE_RESOURCES 0x00000001u

enum {
    RIN_GPU_VULKAN_QUERY_COMMAND_BEGIN = 1u,
    RIN_GPU_VULKAN_QUERY_COMMAND_END = 2u,
    RIN_GPU_VULKAN_QUERY_COMMAND_RESET = 3u,
    RIN_GPU_VULKAN_QUERY_COMMAND_TIMESTAMP = 4u
};

enum {
    RIN_GPU_VULKAN_EVENT_COMMAND_SET = 1u,
    RIN_GPU_VULKAN_EVENT_COMMAND_RESET = 2u,
    RIN_GPU_VULKAN_EVENT_COMMAND_WAIT = 3u
};

enum {
    RIN_GPU_VULKAN_COMMAND_OK = 0,
    RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT = -1,
    RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE = -2,
    RIN_GPU_VULKAN_COMMAND_LIMIT = -3,
    RIN_GPU_VULKAN_COMMAND_UNSUPPORTED = -4,
    RIN_GPU_VULKAN_COMMAND_INVALID_STATE = -5
};

enum {
    RIN_GPU_VULKAN_COMMAND_BUFFER_INITIAL = 1,
    RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING = 2,
    RIN_GPU_VULKAN_COMMAND_BUFFER_EXECUTABLE = 3
};

typedef uint64_t RinGpuVulkanCommandPoolHandleV1;
typedef struct RinGpuVulkanCommandBufferV1
    RinGpuVulkanCommandBufferV1;

typedef struct RinGpuVulkanCommandPoolSlotV1 {
    uint32_t state;
    uint32_t generation;
    uintptr_t owner;
    uint32_t queue_family_index;
    uint32_t flags;
    uint32_t live_buffer_count;
    uint32_t reserved;
} RinGpuVulkanCommandPoolSlotV1;

/* Source and destination addresses are device virtual addresses.  The
 * corresponding product allocations are carried separately as leases during
 * submission, so a backend may consume only this immutable transfer packet. */
typedef struct RinGpuVulkanBufferCopyCommandV1 {
    uint64_t source_allocation;
    uint64_t destination_allocation;
    uint64_t source_gpu_address;
    uint64_t destination_gpu_address;
    uint64_t size_bytes;
} RinGpuVulkanBufferCopyCommandV1;

typedef struct RinGpuVulkanTransferPacketV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t copy_count;
    uint32_t reserved;
    RinGpuVulkanBufferCopyCommandV1
        copies[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_COPIES];
} RinGpuVulkanTransferPacketV1;

enum {
    RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_COPY = 1u,
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_COPY = 2u,
    RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_TO_IMAGE = 3u,
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_TO_BUFFER = 4u,
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR = 5u,
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BLIT = 6u,
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE = 7u
};

typedef struct RinGpuVulkanTransferOpV2 {
    uint32_t type;
    uint32_t reserved;
    uint64_t source_allocation;
    uint64_t destination_allocation;
    uint64_t source_gpu_address;
    uint64_t destination_gpu_address;
    uint64_t size_bytes;
    uint32_t clear_value[4];
    uint32_t source_width;
    uint32_t source_height;
    uint32_t destination_width;
    uint32_t destination_height;
    uint32_t filter;
    uint32_t sample_count;
} RinGpuVulkanTransferOpV2;

typedef struct RinGpuVulkanTransferPacketV2 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t op_count;
    uint32_t reserved;
    RinGpuVulkanTransferOpV2
        operations[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS];
} RinGpuVulkanTransferPacketV2;

typedef struct RinGpuVulkanQueryCommandV1 {
    uint64_t query_pool;
    uint32_t query;
    uint32_t flags;
    uint32_t operation;
    uint32_t reserved;
} RinGpuVulkanQueryCommandV1;

typedef struct RinGpuVulkanEventCommandV1 {
    uint64_t event;
    uint32_t operation;
    uint32_t reserved;
} RinGpuVulkanEventCommandV1;

struct RinGpuVulkanCommandBufferV1 {
    uintptr_t loader_magic;
    uint32_t state;
    uint32_t lifecycle;
    uintptr_t owner;
    RinGpuVulkanCommandPoolSlotV1* pool;
    uint32_t pool_generation;
    uint32_t level;
    uint32_t usage_flags;
    uint32_t record_error;
    uint32_t in_flight_count;
    uint32_t copy_count;
    RinGpuVulkanBufferCopyCommandV1
        copies[RIN_GPU_VULKAN_COMMAND_MAX_COPIES];
    uint32_t transfer_op_count;
    RinGpuVulkanTransferOpV2
        transfer_ops[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    uint32_t descriptor_bind_recorded;
    uint32_t descriptor_bind_first_set;
    uint32_t descriptor_bind_set_count;
    uint32_t descriptor_dynamic_offset_count;
    uint64_t descriptor_sets[RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
    uint32_t descriptor_dynamic_offsets[RIN_GPU_VULKAN_COMMAND_MAX_DYNAMIC_OFFSETS];
    uint32_t barrier_count;
    uint32_t reserved_barrier;
    struct {
        uint64_t src_stage_mask;
        uint64_t src_access_mask;
        uint64_t dst_stage_mask;
        uint64_t dst_access_mask;
    } barriers[RIN_GPU_VULKAN_COMMAND_MAX_BARRIERS];
    uint32_t query_command_count;
    uint32_t event_command_count;
    RinGpuVulkanQueryCommandV1
        query_commands[RIN_GPU_VULKAN_COMMAND_MAX_QUERY_COMMANDS];
    RinGpuVulkanEventCommandV1
        event_commands[RIN_GPU_VULKAN_COMMAND_MAX_EVENT_COMMANDS];
};

typedef struct RinGpuVulkanCommandRuntimeV1 {
    RinGpuVulkanCommandPoolSlotV1
        pools[RIN_GPU_VULKAN_COMMAND_MAX_POOLS];
    RinGpuVulkanCommandBufferV1
        buffers[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];
} RinGpuVulkanCommandRuntimeV1;

int rin_gpu_vulkan_command_pool_create(
    RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
    uint32_t queue_family_index, uint32_t flags,
    RinGpuVulkanCommandPoolHandleV1* pool_out);
int rin_gpu_vulkan_command_pool_destroy(
    RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
    RinGpuVulkanCommandPoolHandleV1 pool);
int rin_gpu_vulkan_command_pool_reset(
    RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
    RinGpuVulkanCommandPoolHandleV1 pool, uint32_t flags);
int rin_gpu_vulkan_command_buffers_allocate(
    RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
    RinGpuVulkanCommandPoolHandleV1 pool, uint32_t level,
    uint32_t count, uintptr_t loader_magic,
    RinGpuVulkanCommandBufferV1** buffers_out);
uint32_t rin_gpu_vulkan_command_buffers_free(
    RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner,
    RinGpuVulkanCommandPoolHandleV1 pool, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers);
int rin_gpu_vulkan_command_buffer_begin(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uint32_t usage_flags);
int rin_gpu_vulkan_command_buffer_end(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer);
int rin_gpu_vulkan_command_buffer_reset(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uint32_t flags);
int rin_gpu_vulkan_command_buffer_owner(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uintptr_t* owner_out);
int rin_gpu_vulkan_command_buffer_record_copies(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer,
    const RinGpuVulkanBufferCopyCommandV1* copies, uint32_t copy_count);
int rin_gpu_vulkan_command_buffer_record_transfer_ops(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer,
    const RinGpuVulkanTransferOpV2* operations, uint32_t operation_count);
int rin_gpu_vulkan_command_buffer_record_descriptor_bind(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uint32_t first_set,
    const uint64_t* descriptor_sets, uint32_t descriptor_set_count,
    const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
int rin_gpu_vulkan_command_buffer_record_barrier(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uint64_t src_stage_mask,
    uint64_t src_access_mask, uint64_t dst_stage_mask,
        uint64_t dst_access_mask);
int rin_gpu_vulkan_command_buffer_record_query(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uint64_t query_pool,
    uint32_t query, uint32_t flags, uint32_t operation);
int rin_gpu_vulkan_command_buffer_record_event(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer, uint64_t event,
    uint32_t operation);
void rin_gpu_vulkan_command_buffer_record_failure(
    RinGpuVulkanCommandRuntimeV1* runtime,
    RinGpuVulkanCommandBufferV1* buffer);
int rin_gpu_vulkan_command_buffers_validate_submit(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers);
int rin_gpu_vulkan_command_buffers_mark_submitted(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers);
int rin_gpu_vulkan_command_buffers_complete(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers);
void rin_gpu_vulkan_command_buffers_abort(
    RinGpuVulkanCommandRuntimeV1* runtime, uint32_t count,
    RinGpuVulkanCommandBufferV1* const* buffers);
uint32_t rin_gpu_vulkan_command_owner_cleanup(
    RinGpuVulkanCommandRuntimeV1* runtime, uintptr_t owner);

#if defined(__cplusplus)
static_assert(sizeof(RinGpuVulkanTransferOpV2) == 88u,
              "RinVulkan transfer operation ABI drift");
#else
_Static_assert(sizeof(RinGpuVulkanTransferOpV2) == 88u,
               "RinVulkan transfer operation ABI drift");
#endif

#endif
