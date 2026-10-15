/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "sync2_scope.h"

static void test_supported_dependency_flags(void) {
    assert(rin_vk_sync2_dependency_flags_valid(0u));
    assert(rin_vk_sync2_dependency_flags_valid(
        RIN_VK_DEPENDENCY_BY_REGION_BIT));
    assert(!rin_vk_sync2_dependency_flags_valid(UINT32_C(0x2)));
    assert(!rin_vk_sync2_dependency_flags_valid(UINT32_C(0x4)));
    assert(!rin_vk_sync2_dependency_flags_valid(
        RIN_VK_DEPENDENCY_BY_REGION_BIT | UINT32_C(0x2)));
    assert(!rin_vk_sync2_dependency_flags_valid(UINT32_C(0x80000000)));
}

static void test_transfer_stage_aliases(void) {
    const uint64_t transfer_stages =
        RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT |
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
        RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT |
        RIN_VK_PIPELINE_STAGE_2_BLIT_BIT |
        RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT;
    uint64_t runtime_mask = 0u;
    assert(rin_vk_sync2_stage_mask(transfer_stages, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER);
    assert(rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER);
    assert(rin_vk_sync2_recorded_stage_mask_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT));
    assert(rin_vk_sync2_recorded_stage_mask_valid(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT));
    assert(rin_vk_sync2_recorded_stage_mask_valid(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT));
    assert(!rin_vk_sync2_recorded_stage_mask_valid(
        RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT));
    assert(!rin_vk_sync2_recorded_stage_mask_valid(0u));
    assert(!rin_vk_sync2_stage_mask(UINT64_C(0x80000000), &runtime_mask));
}

static void test_graphics_shader_stage_projection(void) {
    const uint64_t graphics_stages =
        RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
        RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        RIN_VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
        RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    uint64_t runtime_mask = 0u;
    uint32_t legacy_mask = 0u;

    assert(RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT == UINT64_C(0x2));
    assert(RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT == UINT64_C(0x8));
    assert(RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT == UINT64_C(0x80));
    assert(RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT == UINT64_C(0x8000));
    assert(rin_vk_sync2_stage_mask(graphics_stages, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);
    assert(rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS |
            RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE));
    assert(!rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
        &runtime_mask));
    assert(!rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
        &runtime_mask));
    assert(!rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT, &runtime_mask));
    assert(!rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT |
            RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
        &runtime_mask));

    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, &legacy_mask));
    assert(legacy_mask == RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    assert(rin_vk_sync2_semaphore_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, &legacy_mask));
    assert(legacy_mask == RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    assert(rin_vk_sync2_semaphore_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, &legacy_mask));
    assert(legacy_mask == (RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
                           RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT));
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, &runtime_mask));
    assert(runtime_mask == RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    assert(!rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
        &runtime_mask));
    assert(!rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
        &runtime_mask));
    assert(!rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, &runtime_mask));
}

static void test_scope_sensitive_legacy_wait_stages(void) {
    uint32_t legacy_mask = UINT32_MAX;

    assert(rin_vk_sync2_semaphore_wait_stage_mask(0u, &legacy_mask));
    assert(legacy_mask == 0u);
    assert(rin_vk_sync2_semaphore_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00010000));
    assert(rin_vk_sync2_semaphore_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, &legacy_mask));
    assert(legacy_mask == 0u);
    assert(rin_vk_sync2_semaphore_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT |
            RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00001000));
    assert(rin_vk_sync2_semaphore_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
            RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00010000));
    assert(!rin_vk_sync2_semaphore_wait_stage_mask(UINT64_C(0x80000000),
                                                   &legacy_mask));
}

static void test_legacy_wait_stage_projection(void) {
    uint32_t legacy_mask = 0u;
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT, &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00001000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, &legacy_mask));
    assert(legacy_mask == RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT |
            RIN_VK_PIPELINE_STAGE_2_BLIT_BIT |
            RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00001000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT, &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00004000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00010000));
    /* Event/reset operations project a first scope: TOP is empty, while
     * BOTTOM includes every preceding command. */
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, &legacy_mask));
    assert(legacy_mask == 0u);
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00010000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
            RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00001000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT |
            RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00010000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
            RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00010000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
            RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00005000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(0u, &legacy_mask));
    assert(legacy_mask == 0u);
    assert(!rin_vk_sync2_legacy_wait_stage_mask(UINT64_C(0x80000000),
                                                &legacy_mask));
    assert(!rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT, NULL));
}

static void test_legacy_stage_conversion(void) {
    uint64_t sync2_mask = 0u;
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_TRANSFER_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_HOST_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_HOST_BIT);
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
    assert(rin_vk_sync2_access_stage_valid(
        sync2_mask, RIN_VK_ACCESS_2_SHADER_WRITE_BIT, 0u));
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    assert(rin_vk_sync2_access_stage_valid(
        sync2_mask, RIN_VK_ACCESS_2_MEMORY_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        sync2_mask, RIN_VK_ACCESS_2_MEMORY_READ_BIT, 1u));
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    assert(rin_vk_sync2_access_stage_valid(
        sync2_mask, RIN_VK_ACCESS_2_MEMORY_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        sync2_mask, RIN_VK_ACCESS_2_MEMORY_READ_BIT, 1u));
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &sync2_mask));
    assert(sync2_mask == RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    assert(rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
            RIN_VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        &sync2_mask));
    assert(sync2_mask ==
           (RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
            RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT));
    assert(!rin_vk_sync2_legacy_stage_mask(0u, &sync2_mask));
    assert(!rin_vk_sync2_legacy_stage_mask(UINT32_C(0x80000000), &sync2_mask));
    assert(!rin_vk_sync2_legacy_stage_mask(
        RIN_VK_PIPELINE_STAGE_TRANSFER_BIT, NULL));
}

static void test_compute_storage_barrier_scope(void) {
    RinGpuVulkanTransferOpV2 operation;
    uint64_t runtime_mask = 0u;
    memset(&operation, 0, sizeof(operation));
    assert(rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE);
    assert(rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS);
    assert((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE) != 0u);

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
            RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0u, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE));
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT |
            RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_ALL);

    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, 0u));
    assert(rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_HOST_READ_BIT, &operation));
    assert(operation.barrier.src_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE);
    assert(operation.barrier.src_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE);
    assert(operation.barrier.dst_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_HOST);
    assert(operation.barrier.dst_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ);
}

static void test_vertex_input_access_bit_values(void) {
    uint64_t runtime_mask = 0u;
    assert(RIN_VK_ACCESS_2_INDEX_READ_BIT == UINT64_C(0x2));
    assert(RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT == UINT64_C(0x4));

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_INDEX_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_INDEX_READ_BIT |
            RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, 0u));
}

static void test_core_shader_and_attachment_access_masks(void) {
    uint64_t runtime_mask = 0u;

    assert(RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT == UINT64_C(0x1));
    assert(RIN_VK_ACCESS_2_UNIFORM_READ_BIT == UINT64_C(0x8));
    assert(RIN_VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT == UINT64_C(0x10));
    assert(RIN_VK_ACCESS_2_SHADER_READ_BIT == UINT64_C(0x20));
    assert(RIN_VK_ACCESS_2_SHADER_WRITE_BIT == UINT64_C(0x40));
    assert(RIN_VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT == UINT64_C(0x80));
    assert(RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT == UINT64_C(0x100));
    assert(RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT ==
           UINT64_C(0x200));
    assert(RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ==
           UINT64_C(0x400));
    assert(RIN_VK_ACCESS_2_SHADER_SAMPLED_READ_BIT ==
           UINT64_C(0x0000000100000000));

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, 0u, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ));
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_UNIFORM_READ_BIT |
            RIN_VK_ACCESS_2_SHADER_READ_BIT |
            RIN_VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_SHADER_READ_BIT | RIN_VK_ACCESS_2_SHADER_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0u, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE));
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
            RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
            RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, 0u,
        &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE);
}

static void test_core_access_stage_compatibility(void) {
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_UNIFORM_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        RIN_VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0u));
    assert(!rin_vk_sync2_access_mask(
        UINT64_C(0x8000000000000000), RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        0u, (uint64_t[1]){0u}));
}

static void test_split_vertex_input_stage_compatibility(void) {
    uint64_t runtime_stages = 0u;
    RinGpuVulkanTransferOpV2 operation;

    assert(rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, &runtime_stages));
    assert(runtime_stages == RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);
    assert(rin_vk_sync2_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        &runtime_stages));
    assert(runtime_stages == RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);

    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
        RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT |
            RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT |
            RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT |
            RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        0u));
    assert(rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
        RIN_VK_ACCESS_2_INDEX_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        RIN_VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, &operation));
    assert(operation.barrier.src_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);
    assert(operation.barrier.src_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
    assert(operation.barrier.dst_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);
    assert(operation.barrier.dst_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);
}

static void test_graphics_shader_storage_barrier_scope(void) {
    RinGpuVulkanTransferOpV2 operation;
    uint64_t runtime_mask = 0u;
    const uint64_t storage_access =
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    memset(&operation, 0, sizeof(operation));

    assert(rin_vk_sync2_access_mask(
        storage_access, RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 0u,
        &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT, 0u));
    assert(rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT, &operation));
    assert(operation.barrier.src_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);
    assert(operation.barrier.src_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE);
    assert(operation.barrier.dst_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS);
    assert(operation.barrier.dst_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ);

    assert(rin_vk_sync2_access_mask(
        storage_access,
        RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
            RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        0u, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE |
            RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_READ_BIT, 0u));
}

static void test_generic_memory_access_follows_stage_scope(void) {
    uint64_t runtime_mask = 0u;
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT | RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE));

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT | RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT, 0u, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE));

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT | RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_ALL);
    assert(!rin_vk_sync2_access_mask(UINT64_C(0x8000000000000000),
                                    RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u,
                                    &runtime_mask));
    assert(rin_vk_sync2_access_mask(RIN_VK_ACCESS_2_MEMORY_READ_BIT, 0u,
                                   0u, &runtime_mask));
    assert(runtime_mask == 0u);
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ);
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT, 0u));
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 1u, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE);
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT, 1u));
}

static void test_generic_memory_access_with_empty_scope(void) {
    const uint64_t generic_access = RIN_VK_ACCESS_2_MEMORY_READ_BIT |
                                    RIN_VK_ACCESS_2_MEMORY_WRITE_BIT;
    uint64_t runtime_access = UINT64_MAX;
    RinGpuVulkanTransferOpV2 operation;

    assert(rin_vk_sync2_access_mask(generic_access, 0u, 0u,
                                    &runtime_access));
    assert(runtime_access == 0u);
    assert(rin_vk_sync2_access_stage_valid(0u, generic_access, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, generic_access, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, generic_access, 1u));

    memset(&operation, 0, sizeof(operation));
    assert(rin_vk_sync2_barrier_scopes(0u, generic_access,
                                       0u, generic_access, &operation));
    assert(operation.barrier.src_stage_mask == 0u);
    assert(operation.barrier.src_access_mask == 0u);
    assert(operation.barrier.dst_stage_mask == 0u);
    assert(operation.barrier.dst_access_mask == 0u);
}

static void test_access_requires_a_compatible_supported_stage(void) {
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT, 0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT |
            RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_READ_BIT | RIN_VK_ACCESS_2_HOST_WRITE_BIT,
        0u));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT, 0u));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT, 1u));
}

static void test_barrier_scope_packet_mapping(void) {
    RinGpuVulkanTransferOpV2 operation;
    memset(&operation, 0, sizeof(operation));
    assert(rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT,
        RIN_VK_ACCESS_2_MEMORY_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_MEMORY_WRITE_BIT, &operation));
    assert(operation.barrier.src_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER);
    assert(operation.barrier.src_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ);
    assert(operation.barrier.dst_stage_mask ==
           RIN_GPU_VULKAN_BARRIER_STAGE_HOST);
    assert(operation.barrier.dst_access_mask ==
           RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE);

    assert(!rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT, 0u, 0u, &operation));
    assert(!rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_READ_BIT, 0u, 0u, &operation));
    assert(!rin_vk_sync2_barrier_scopes(
        0u, 0u, RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT, &operation));
    assert(rin_vk_sync2_barrier_scopes(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT |
            RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_READ_BIT,
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT |
            RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT, &operation));
}

int main(void) {
    test_supported_dependency_flags();
    test_transfer_stage_aliases();
    test_graphics_shader_stage_projection();
    test_scope_sensitive_legacy_wait_stages();
    test_legacy_wait_stage_projection();
    test_legacy_stage_conversion();
    test_compute_storage_barrier_scope();
    test_vertex_input_access_bit_values();
    test_core_shader_and_attachment_access_masks();
    test_core_access_stage_compatibility();
    test_split_vertex_input_stage_compatibility();
    test_graphics_shader_storage_barrier_scope();
    test_generic_memory_access_follows_stage_scope();
    test_generic_memory_access_with_empty_scope();
    test_access_requires_a_compatible_supported_stage();
    test_barrier_scope_packet_mapping();
    return 0;
}
