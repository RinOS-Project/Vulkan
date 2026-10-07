/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_COMMAND_RUNTIME_H
#define RINVULKAN_PUBLIC_COMMAND_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include <ringpu/compatibility.h>

#define RIN_GPU_VULKAN_COMMAND_MAX_POOLS 64u
#define RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS 128u
#define RIN_GPU_VULKAN_COMMAND_MAX_COPIES 4u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_COPIES 16u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION 1u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2 2u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_3 3u
#define RIN_GPU_VULKAN_COMPUTE_PACKET_VERSION 4u
#define RIN_GPU_VULKAN_GRAPHICS_PACKET_VERSION 5u
#define RIN_GPU_VULKAN_COMPUTE_PACKET_VERSION_2 6u
#define RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS 16u
#define RIN_GPU_VULKAN_COMPUTE_MAX_BINDINGS 64u
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
#define RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS UINT64_C(0x0000000000000004)
#define RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE UINT64_C(0x0000000000000008)
#define RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS \
    (RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER | \
     RIN_GPU_VULKAN_BARRIER_STAGE_HOST | \
     RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS | \
     RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ UINT64_C(0x0000000000000001)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE UINT64_C(0x0000000000000002)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ UINT64_C(0x0000000000000004)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE UINT64_C(0x0000000000000008)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ UINT64_C(0x0000000000000010)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE UINT64_C(0x0000000000000020)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ UINT64_C(0x0000000000000040)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE UINT64_C(0x0000000000000080)
#define RIN_GPU_VULKAN_BARRIER_ACCESS_ALL \
    (RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ | \
     RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE)

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
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE = 7u,
    RIN_GPU_VULKAN_TRANSFER_OP_MEMORY_BARRIER = 8u,
    RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_BARRIER = 9u,
    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER = 10u
};

/* Resource barrier packet encoding: source_allocation carries the opaque
 * Vulkan resource handle; destination_allocation/address/size carry its
 * backing allocation lease and affected byte range. The barrier union carries
 * stage/access scopes. Buffer queue-family indices use source_width/height;
 * image old/new layout, aspect, and queue-family indices use source_width,
 * source_height, destination_width, destination_height, and filter; sample_count
 * remains zero for both barrier operation types. */
typedef struct RinGpuVulkanTransferOpV2 {
    uint32_t type;
    uint32_t reserved;
    uint64_t source_allocation;
    uint64_t destination_allocation;
    uint64_t source_gpu_address;
    uint64_t destination_gpu_address;
    uint64_t size_bytes;
    union {
        uint32_t clear_value[4];
        struct {
            uint32_t src_stage_mask;
            uint32_t src_access_mask;
            uint32_t dst_stage_mask;
            uint32_t dst_access_mask;
        } barrier;
    };
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

/* Submission packet V3 carries the selected Vulkan queue route alongside the
 * same ordered operation stream. queue_id is the product-runtime ordinal;
 * queue_family_index/queue_index preserve Vulkan's requested family/local
 * queue identity for platform lowering. */
typedef struct RinGpuVulkanTransferPacketV3 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t op_count;
    uint32_t reserved;
    uint32_t queue_family_index;
    uint32_t queue_index;
    uint32_t product_queue_id;
    uint32_t reserved_route;
    RinGpuVulkanTransferOpV2
        operations[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS];
} RinGpuVulkanTransferPacketV3;

/* A bounded, API-neutral compute submission. The trailing bytes contain one
 * validated RSH1 module. Buffer binding offsets are relative to the leased
 * product allocation; resource_index matches the RSH1 resource operand.
 * command_cookie points to this immutable packet for the synchronous prepare
 * callback and remains owned by the submission until completion. */
typedef struct RinGpuVulkanComputeBindingV1 {
    uint64_t allocation_handle;
    uint64_t offset;
    uint64_t size_bytes;
    uint32_t resource_index;
    uint32_t access;
    uint32_t reserved[2];
} RinGpuVulkanComputeBindingV1;

typedef struct RinGpuVulkanComputePacketV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_family_index;
    uint32_t queue_index;
    uint32_t product_queue_id;
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
    uint32_t binding_count;
    uint32_t shader_size_bytes;
    uint32_t flags;
    uint32_t reserved;
    RinGpuVulkanComputeBindingV1
        bindings[RIN_GPU_VULKAN_COMPUTE_MAX_BINDINGS];
    uint8_t shader_ir[1];
} RinGpuVulkanComputePacketV1;

/* V2 carries ordered synchronization2 buffer/memory barriers around its
 * single dispatch. barrier_after_dispatch_mask distinguishes the barriers
 * recorded before and after that dispatch. */
typedef struct RinGpuVulkanComputePacketV2 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_family_index;
    uint32_t queue_index;
    uint32_t product_queue_id;
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
    uint32_t binding_count;
    uint32_t shader_size_bytes;
    uint32_t flags;
    uint32_t reserved;
    RinGpuVulkanComputeBindingV1
        bindings[RIN_GPU_VULKAN_COMPUTE_MAX_BINDINGS];
    uint32_t barrier_count;
    uint32_t barrier_after_dispatch_mask;
    RinGpuVulkanTransferOpV2 barriers[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    uint8_t shader_ir[1];
} RinGpuVulkanComputePacketV2;

/* A bounded, API-neutral graphics submission. The trailing bytes contain a
 * validated vertex RSH1 module followed by a validated fragment RSH1 module.
 * Allocation handles and byte ranges identify the vertex source and one
 * RGBA8 color target; product resources separately lease those allocations
 * through completion. V1 carries one full-target clear/store pass and one
 * non-indexed draw, with no shader descriptors or depth attachment. */
typedef struct RinGpuVulkanGraphicsPacketV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t queue_family_index;
    uint32_t queue_index;
    uint32_t product_queue_id;
    uint32_t vertex_shader_size_bytes;
    uint32_t fragment_shader_size_bytes;
    uint32_t vertex_binding;
    uint64_t vertex_allocation_handle;
    uint64_t vertex_offset;
    uint64_t vertex_size_bytes;
    uint64_t color_allocation_handle;
    uint64_t color_offset;
    uint64_t color_size_bytes;
    uint32_t width;
    uint32_t height;
    float viewport_x;
    float viewport_y;
    float viewport_width;
    float viewport_height;
    float viewport_min_depth;
    float viewport_max_depth;
    int32_t scissor_x;
    int32_t scissor_y;
    uint32_t scissor_width;
    uint32_t scissor_height;
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t first_vertex;
    uint32_t first_instance;
    uint32_t flags;
    uint32_t reserved;
    float clear_red;
    float clear_green;
    float clear_blue;
    float clear_alpha;
    RinGpuGraphicsPipelineBackendDescV1 pipeline;
    uint64_t payload_alignment;
    uint8_t shader_ir[1];
} RinGpuVulkanGraphicsPacketV1;

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
    uint32_t stage_mask;
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
    uint32_t transfer_op_compute_phase[
        RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    uint32_t descriptor_bind_recorded;
    uint32_t descriptor_bind_first_set;
    uint32_t descriptor_bind_set_count;
    uint32_t descriptor_dynamic_offset_count;
    uint32_t descriptor_bind_point;
    uint32_t compute_pipeline_bound;
    uint64_t descriptor_sets[RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
    uint64_t descriptor_set_layouts[
        RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
    uint32_t descriptor_dynamic_offsets[RIN_GPU_VULKAN_COMMAND_MAX_DYNAMIC_OFFSETS];
    uint64_t bound_compute_pipeline;
    uint64_t dispatch_compute_pipeline;
    uint32_t compute_dispatch_count;
    uint32_t compute_group_count_x;
    uint32_t compute_group_count_y;
    uint32_t compute_group_count_z;
    uint32_t graphics_rendering_active;
    uint32_t graphics_rendering_begin_count;
    uint32_t graphics_rendering_end_count;
    uint32_t graphics_draw_count;
    uint32_t graphics_pipeline_bound;
    uint32_t graphics_vertex_buffer_bound;
    uint32_t graphics_color_layout;
    uint32_t graphics_width;
    uint32_t graphics_height;
    uint64_t bound_graphics_pipeline;
    uint64_t graphics_color_view;
    uint64_t graphics_vertex_buffer;
    uint64_t graphics_vertex_offset;
    uint32_t graphics_vertex_count;
    uint32_t graphics_instance_count;
    uint32_t graphics_first_vertex;
    uint32_t graphics_first_instance;
    float graphics_clear_red;
    float graphics_clear_green;
    float graphics_clear_blue;
    float graphics_clear_alpha;
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
void rin_gpu_vulkan_command_buffer_invalidate(
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
static_assert(sizeof(RinGpuVulkanTransferPacketV3) == 1440u,
              "RinVulkan routed transfer packet ABI drift");
static_assert(sizeof(RinGpuVulkanComputeBindingV1) == 40u,
              "RinVulkan compute binding ABI drift");
static_assert(offsetof(RinGpuVulkanComputePacketV1, shader_ir) == 2608u,
              "RinVulkan compute packet ABI drift");
static_assert(offsetof(RinGpuVulkanComputePacketV2, shader_ir) == 3320u,
              "RinVulkan compute packet V2 ABI drift");
static_assert(offsetof(RinGpuVulkanGraphicsPacketV1, shader_ir) % 8u == 0u,
              "RinVulkan graphics packet payload alignment drift");
#else
_Static_assert(sizeof(RinGpuVulkanTransferOpV2) == 88u,
               "RinVulkan transfer operation ABI drift");
_Static_assert(sizeof(RinGpuVulkanTransferPacketV3) == 1440u,
               "RinVulkan routed transfer packet ABI drift");
_Static_assert(sizeof(RinGpuVulkanComputeBindingV1) == 40u,
               "RinVulkan compute binding ABI drift");
_Static_assert(offsetof(RinGpuVulkanComputePacketV1, shader_ir) == 2608u,
               "RinVulkan compute packet ABI drift");
_Static_assert(offsetof(RinGpuVulkanComputePacketV2, shader_ir) == 3320u,
               "RinVulkan compute packet V2 ABI drift");
_Static_assert(offsetof(RinGpuVulkanGraphicsPacketV1, shader_ir) % 8u == 0u,
               "RinVulkan graphics packet payload alignment drift");
#endif

#endif
