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
    assert(!rin_vk_sync2_recorded_stage_mask_valid(0u));
    assert(!rin_vk_sync2_stage_mask(UINT64_C(0x8), &runtime_mask));
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
    assert(rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
            RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        &legacy_mask));
    assert(legacy_mask == UINT32_C(0x00005000));
    assert(rin_vk_sync2_legacy_wait_stage_mask(0u, &legacy_mask));
    assert(legacy_mask == 0u);
    assert(!rin_vk_sync2_legacy_wait_stage_mask(UINT64_C(0x8), &legacy_mask));
    assert(!rin_vk_sync2_legacy_wait_stage_mask(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT, NULL));
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
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE));
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT |
            RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_ALL);

    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT));
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

static void test_generic_memory_access_follows_stage_scope(void) {
    uint64_t runtime_mask = 0u;
    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT | RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE));

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT | RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT, &runtime_mask));
    assert(runtime_mask ==
           (RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ |
            RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE));

    assert(rin_vk_sync2_access_mask(
        RIN_VK_ACCESS_2_MEMORY_READ_BIT | RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, &runtime_mask));
    assert(runtime_mask == RIN_GPU_VULKAN_BARRIER_ACCESS_ALL);
    assert(!rin_vk_sync2_access_mask(UINT64_C(0x20),
                                    RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
                                    &runtime_mask));
    assert(!rin_vk_sync2_access_mask(RIN_VK_ACCESS_2_MEMORY_READ_BIT, 0u,
                                    &runtime_mask));
}

static void test_access_requires_a_compatible_supported_stage(void) {
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT));
    assert(rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_HOST_BIT,
        RIN_VK_ACCESS_2_TRANSFER_READ_BIT));
    assert(!rin_vk_sync2_access_stage_valid(
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
        RIN_VK_ACCESS_2_HOST_WRITE_BIT));
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
}

int main(void) {
    test_supported_dependency_flags();
    test_transfer_stage_aliases();
    test_legacy_wait_stage_projection();
    test_compute_storage_barrier_scope();
    test_generic_memory_access_follows_stage_scope();
    test_access_requires_a_compatible_supported_stage();
    test_barrier_scope_packet_mapping();
    return 0;
}
