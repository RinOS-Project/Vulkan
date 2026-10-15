/* SPDX-License-Identifier: MIT */

#include "sync2_scope.h"

#define RIN_VK_SYNC2_FIRST_SCOPE UINT32_C(0)
#define RIN_VK_SYNC2_SECOND_SCOPE UINT32_C(1)

static int sync2_effective_stage_mask_for_scope(uint64_t public_mask,
                                                uint32_t second_scope,
                                                uint64_t* effective_out) {
    uint64_t ordinary_mask;
    uint64_t ignored_runtime_mask;

    if (!effective_out || second_scope > RIN_VK_SYNC2_SECOND_SCOPE)
        return 0;
    ordinary_mask = public_mask &
        ~(RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
          RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    if (!rin_vk_sync2_stage_mask(ordinary_mask, &ignored_runtime_mask))
        return 0;
    /* TOP_OF_PIPE is NONE in the first scope and ALL_COMMANDS in the
     * second. BOTTOM_OF_PIPE has the opposite meaning. */
    if ((second_scope == RIN_VK_SYNC2_SECOND_SCOPE &&
         (public_mask & RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT) != 0u) ||
        (second_scope == RIN_VK_SYNC2_FIRST_SCOPE &&
         (public_mask & RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT) != 0u))
        ordinary_mask |= RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    *effective_out = ordinary_mask;
    return 1;
}

static int sync2_stage_mask_for_scope(uint64_t public_mask,
                                      uint32_t second_scope,
                                      uint64_t* runtime_mask_out) {
    uint64_t effective_mask;
    uint64_t runtime_mask;

    if (!runtime_mask_out ||
        !sync2_effective_stage_mask_for_scope(public_mask, second_scope,
                                              &effective_mask))
        return 0;
    if (!rin_vk_sync2_stage_mask(effective_mask, &runtime_mask)) return 0;
    *runtime_mask_out = runtime_mask;
    return 1;
}

int rin_vk_sync2_dependency_flags_valid(uint32_t public_flags) {
    return (public_flags & ~RIN_VK_DEPENDENCY_BY_REGION_BIT) == 0u;
}

int rin_vk_sync2_stage_mask(uint64_t public_mask, uint64_t* runtime_mask_out) {
    const uint64_t transfer_stages =
        RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT |
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
        RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT |
        RIN_VK_PIPELINE_STAGE_2_BLIT_BIT |
        RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT;
    const uint64_t graphics_shader_stages =
        RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        RIN_VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    const uint64_t graphics_stages =
        RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
        RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
        RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT |
        graphics_shader_stages |
        RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
        RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    const uint64_t indirect_stage =
        RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    const uint64_t known = transfer_stages |
                           graphics_stages |
                           indirect_stage |
                           RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                           RIN_VK_PIPELINE_STAGE_2_HOST_BIT |
                           RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    /* The ICD reports geometryShader and tessellationShader as unsupported
     * and rejects enabling them at device creation. Keep their explicit
     * synchronization stages out of every API-to-runtime projection. */
    const uint64_t disabled_feature_stages =
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    uint64_t runtime_mask = 0u;
    if (!runtime_mask_out || (public_mask & ~known) != 0u ||
        (public_mask & disabled_feature_stages) != 0u)
        return 0;
    if ((public_mask & transfer_stages) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER;
    if ((public_mask & graphics_stages) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS;
    /* The packet ABI has no indirect-command stage. Cover both graphics
     * draws and compute dispatches that consume indirect parameters. */
    if ((public_mask & indirect_stage) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS |
                        RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_HOST_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_HOST;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) != 0u)
        runtime_mask = RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS;
    *runtime_mask_out = runtime_mask;
    return 1;
}

int rin_vk_sync2_semaphore_wait_stage_mask(uint64_t public_mask,
                                           uint32_t* legacy_mask_out) {
    const uint32_t legacy_transfer = UINT32_C(0x00001000);
    const uint32_t legacy_host = UINT32_C(0x00004000);
    const uint32_t legacy_all_commands = UINT32_C(0x00010000);
    const uint32_t legacy_graphics =
        RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    const uint32_t legacy_compute = RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    uint64_t runtime_mask;
    uint32_t legacy_mask = 0u;

    if (!legacy_mask_out ||
        !sync2_stage_mask_for_scope(public_mask, RIN_VK_SYNC2_SECOND_SCOPE,
                                    &runtime_mask))
        return 0;
    if (runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS) {
        legacy_mask = legacy_all_commands;
    } else {
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER) != 0u)
            legacy_mask |= legacy_transfer;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_HOST) != 0u)
            legacy_mask |= legacy_host;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            legacy_mask |= legacy_graphics;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            legacy_mask |= legacy_compute;
    }
    /* The scoped conversion maps BOTTOM (and NONE) to an empty second scope;
     * broad graphics scopes project to legacy ALL_GRAPHICS. */
    *legacy_mask_out = legacy_mask;
    return 1;
}

int rin_vk_sync2_legacy_stage_mask(uint32_t public_mask,
                                   uint64_t* sync2_mask_out) {
    const uint32_t graphics_stages =
        RIN_VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
        RIN_VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
        RIN_VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        RIN_VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
        RIN_VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    const uint32_t known = RIN_VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT |
                           graphics_stages |
                           RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           RIN_VK_PIPELINE_STAGE_TRANSFER_BIT |
                           RIN_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT |
                           RIN_VK_PIPELINE_STAGE_HOST_BIT |
                           RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    /* These core optional features are never exposed or enabled by this ICD. */
    const uint32_t disabled_feature_stages =
        RIN_VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    uint64_t sync2_mask = 0u;
    /* The packet contract has coarser stage groups; widen masks rather than
     * dropping a legacy stage when mapping into those groups. */
    if (!sync2_mask_out || public_mask == 0u ||
        (public_mask & ~known) != 0u ||
        (public_mask & disabled_feature_stages) != 0u)
        return 0;
    /* Preserve the exact legacy stage identity here. The runtime projection
     * widens to RinGPU's coarser groups later; retaining the Vulkan bits lets
     * access validation apply shader-stage rules and lets barrier callers
     * interpret TOP/BOTTOM in the correct first/second-scope direction. */
    if ((public_mask & RIN_VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_TRANSFER_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_HOST_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_HOST_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) != 0u)
        sync2_mask |= RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    if (sync2_mask == 0u) return 0;
    *sync2_mask_out = sync2_mask;
    return 1;
}

int rin_vk_sync2_legacy_wait_stage_mask(uint64_t public_mask,
                                        uint32_t* legacy_mask_out) {
    const uint32_t legacy_transfer = UINT32_C(0x00001000);
    const uint32_t legacy_host = UINT32_C(0x00004000);
    const uint32_t legacy_all_commands = UINT32_C(0x00010000);
    const uint32_t legacy_graphics =
        RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    const uint32_t legacy_compute =
        RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    uint64_t source_scope_mask;
    uint64_t runtime_mask;
    uint32_t legacy_mask = 0u;

    if (!legacy_mask_out)
        return 0;
    /* This projection is used for event operations' first synchronization
     * scope. TOP_OF_PIPE contributes no execution stage there, while
     * BOTTOM_OF_PIPE denotes ALL_COMMANDS. Semaphore waits have the opposite
     * direction and use rin_vk_sync2_semaphore_wait_stage_mask(). */
    source_scope_mask = public_mask &
        ~(RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
          RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    if (!rin_vk_sync2_stage_mask(source_scope_mask, &runtime_mask))
        return 0;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT) != 0u)
        runtime_mask = RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS;
    if (runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS ||
        (public_mask & RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) != 0u) {
        legacy_mask = legacy_all_commands;
    } else {
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER) != 0u)
            legacy_mask |= legacy_transfer;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_HOST) != 0u)
            legacy_mask |= legacy_host;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            legacy_mask |= legacy_graphics;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            legacy_mask |= legacy_compute;
    }
    *legacy_mask_out = legacy_mask;
    return 1;
}

int rin_vk_sync2_recorded_stage_mask_valid(uint64_t public_mask) {
    const uint64_t endpoint_stages =
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    uint64_t runtime_mask;
    if (public_mask == 0u) return 0;
    /* Endpoint meaning depends on whether the caller uses a first or second
     * scope. Here only validate the bit set; direction is handled by the
     * operation-specific scope converter. */
    return rin_vk_sync2_stage_mask(public_mask & ~endpoint_stages,
                                   &runtime_mask);
}

int rin_vk_sync2_access_mask(uint64_t public_mask, uint64_t stage_mask,
                             uint32_t second_scope,
                             uint64_t* runtime_mask_out) {
    const uint64_t memory_read = RIN_VK_ACCESS_2_MEMORY_READ_BIT;
    const uint64_t memory_write = RIN_VK_ACCESS_2_MEMORY_WRITE_BIT;
    const uint64_t shader_read = RIN_VK_ACCESS_2_UNIFORM_READ_BIT |
                                 RIN_VK_ACCESS_2_SHADER_READ_BIT |
                                 RIN_VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
                                 RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    const uint64_t shader_write = RIN_VK_ACCESS_2_SHADER_WRITE_BIT |
                                  RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    const uint64_t graphics_read = RIN_VK_ACCESS_2_INDEX_READ_BIT |
                                   RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |
                                   RIN_VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT |
                                   RIN_VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                                   RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    const uint64_t graphics_write =
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    const uint64_t known = RIN_VK_ACCESS_2_TRANSFER_READ_BIT |
                           RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT |
                           RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                           shader_read | shader_write | graphics_read |
                           graphics_write |
                           RIN_VK_ACCESS_2_HOST_READ_BIT |
                           RIN_VK_ACCESS_2_HOST_WRITE_BIT | memory_read |
                           memory_write;
    uint64_t runtime_mask = 0u;
    uint64_t runtime_stages;
    if (!runtime_mask_out || (public_mask & ~known) != 0u) return 0;
    if (!sync2_stage_mask_for_scope(stage_mask, second_scope,
                                    &runtime_stages))
        return 0;
    if ((public_mask & RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT) != 0u) {
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ;
    }
    if ((public_mask & graphics_read) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ;
    if ((public_mask & graphics_write) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE;
    if ((public_mask & shader_read) != 0u) {
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ;
    }
    if ((public_mask & shader_write) != 0u) {
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE;
    }
    /* MEMORY_READ/WRITE are valid with any stage mask. A NONE stage scope
     * selects no accesses, so the projected runtime access mask stays empty. */
    if ((public_mask & RIN_VK_ACCESS_2_TRANSFER_READ_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ;
    if ((public_mask & RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE;
    if ((public_mask & RIN_VK_ACCESS_2_HOST_READ_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ;
    if ((public_mask & RIN_VK_ACCESS_2_HOST_WRITE_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE;
    if ((public_mask & memory_read) != 0u) {
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_HOST) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ;
    }
    if ((public_mask & memory_write) != 0u) {
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_HOST) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE;
    }
    *runtime_mask_out = runtime_mask;
    return 1;
}

int rin_vk_sync2_access_stage_valid(uint64_t stage_mask,
                                    uint64_t access_mask,
                                    uint32_t second_scope) {
    const uint64_t indirect_access =
        RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    const uint64_t transfer_read_access =
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT;
    const uint64_t transfer_write_access =
        RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    const uint64_t index_read = RIN_VK_ACCESS_2_INDEX_READ_BIT;
    const uint64_t vertex_attribute_read =
        RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    const uint64_t input_attachment_access =
        RIN_VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
    const uint64_t color_attachment_read =
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    const uint64_t color_attachment_write =
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    const uint64_t depth_stencil_read =
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    const uint64_t depth_stencil_write =
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    const uint64_t shader_access =
        RIN_VK_ACCESS_2_UNIFORM_READ_BIT |
        RIN_VK_ACCESS_2_SHADER_READ_BIT |
        RIN_VK_ACCESS_2_SHADER_WRITE_BIT |
        RIN_VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    const uint64_t graphics_shader_stages =
        RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    const uint64_t host_access = RIN_VK_ACCESS_2_HOST_READ_BIT |
                                 RIN_VK_ACCESS_2_HOST_WRITE_BIT;
    const uint64_t transfer_read_stages =
        RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT |
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
        RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT |
        RIN_VK_PIPELINE_STAGE_2_BLIT_BIT;
    const uint64_t transfer_write_stages =
        transfer_read_stages | RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT;
    uint64_t effective_stages;
    if (!sync2_effective_stage_mask_for_scope(stage_mask, second_scope,
                                              &effective_stages))
        return 0;
    if ((access_mask & indirect_access) != 0u &&
        (effective_stages & (RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                             RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
                             RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & index_read) != 0u &&
        (effective_stages & (RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
                             RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
                             RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
                             RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & vertex_attribute_read) != 0u &&
        (effective_stages &
         (RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT |
          RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & input_attachment_access) != 0u &&
        (effective_stages &
         (RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & color_attachment_read) != 0u &&
        (effective_stages &
         (RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & color_attachment_write) != 0u &&
        (effective_stages &
         (RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & depth_stencil_read) != 0u &&
        (effective_stages &
         (RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          RIN_VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & depth_stencil_write) != 0u &&
        (effective_stages &
         (RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          RIN_VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & shader_access) != 0u &&
        (effective_stages &
         (graphics_shader_stages |
          RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & transfer_read_access) != 0u &&
        (effective_stages &
         (transfer_read_stages |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & transfer_write_access) != 0u &&
        (effective_stages &
         (transfer_write_stages |
          RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)) == 0u)
        return 0;
    if ((access_mask & host_access) != 0u &&
        (effective_stages & RIN_VK_PIPELINE_STAGE_2_HOST_BIT) == 0u)
        return 0;
    /* Generic memory access flags have no stage-compatibility restriction;
     * an empty stage scope consequently selects an empty access scope. */
    return 1;
}

int rin_vk_sync2_barrier_scopes(uint64_t src_stage_public,
                                uint64_t src_access_public,
                                uint64_t dst_stage_public,
                                uint64_t dst_access_public,
                                RinGpuVulkanTransferOpV2* operation) {
    uint64_t src_stage;
    uint64_t src_access;
    uint64_t dst_stage;
    uint64_t dst_access;
    if (!operation ||
        !sync2_stage_mask_for_scope(src_stage_public,
                                    RIN_VK_SYNC2_FIRST_SCOPE, &src_stage) ||
        !rin_vk_sync2_access_mask(src_access_public, src_stage_public,
                                  RIN_VK_SYNC2_FIRST_SCOPE, &src_access) ||
        !sync2_stage_mask_for_scope(dst_stage_public,
                                    RIN_VK_SYNC2_SECOND_SCOPE, &dst_stage) ||
        !rin_vk_sync2_access_mask(dst_access_public, dst_stage_public,
                                  RIN_VK_SYNC2_SECOND_SCOPE, &dst_access) ||
        !rin_vk_sync2_access_stage_valid(src_stage_public,
                                         src_access_public,
                                         RIN_VK_SYNC2_FIRST_SCOPE) ||
        !rin_vk_sync2_access_stage_valid(dst_stage_public,
                                         dst_access_public,
                                         RIN_VK_SYNC2_SECOND_SCOPE))
        return 0;
    operation->barrier.src_stage_mask = (uint32_t)src_stage;
    operation->barrier.src_access_mask = (uint32_t)src_access;
    operation->barrier.dst_stage_mask = (uint32_t)dst_stage;
    operation->barrier.dst_access_mask = (uint32_t)dst_access;
    return 1;
}
