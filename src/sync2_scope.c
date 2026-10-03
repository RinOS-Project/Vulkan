/* SPDX-License-Identifier: MIT */

#include "sync2_scope.h"

int rin_vk_sync2_stage_mask(uint64_t public_mask, uint64_t* runtime_mask_out) {
    const uint64_t transfer_stages =
        RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT |
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
        RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT |
        RIN_VK_PIPELINE_STAGE_2_BLIT_BIT |
        RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT;
    const uint64_t known = transfer_stages |
                           RIN_VK_PIPELINE_STAGE_2_HOST_BIT |
                           RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    uint64_t runtime_mask = 0u;
    if (!runtime_mask_out || (public_mask & ~known) != 0u) return 0;
    if ((public_mask & transfer_stages) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_HOST_BIT) != 0u)
        runtime_mask |= RIN_GPU_VULKAN_BARRIER_STAGE_HOST;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) != 0u)
        runtime_mask = RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS;
    *runtime_mask_out = runtime_mask;
    return 1;
}

int rin_vk_sync2_legacy_wait_stage_mask(uint64_t public_mask,
                                        uint32_t* legacy_mask_out) {
    const uint32_t legacy_transfer = UINT32_C(0x00001000);
    const uint32_t legacy_host = UINT32_C(0x00004000);
    const uint32_t legacy_all_commands = UINT32_C(0x00010000);
    uint64_t runtime_mask;
    uint32_t legacy_mask = 0u;

    if (!legacy_mask_out || public_mask == 0u ||
        !rin_vk_sync2_stage_mask(public_mask, &runtime_mask))
        return 0;
    if ((public_mask & RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) != 0u) {
        legacy_mask = legacy_all_commands;
    } else {
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER) != 0u)
            legacy_mask |= legacy_transfer;
        if ((runtime_mask & RIN_GPU_VULKAN_BARRIER_STAGE_HOST) != 0u)
            legacy_mask |= legacy_host;
    }
    *legacy_mask_out = legacy_mask;
    return 1;
}

int rin_vk_sync2_recorded_stage_mask_valid(uint64_t public_mask) {
    uint64_t runtime_mask;
    return public_mask != 0u &&
           rin_vk_sync2_stage_mask(public_mask, &runtime_mask);
}

int rin_vk_sync2_access_mask(uint64_t public_mask, uint64_t stage_mask,
                             uint64_t* runtime_mask_out) {
    const uint64_t memory_read = RIN_VK_ACCESS_2_MEMORY_READ_BIT;
    const uint64_t memory_write = RIN_VK_ACCESS_2_MEMORY_WRITE_BIT;
    const uint64_t known = RIN_VK_ACCESS_2_TRANSFER_READ_BIT |
                           RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT |
                           RIN_VK_ACCESS_2_HOST_READ_BIT |
                           RIN_VK_ACCESS_2_HOST_WRITE_BIT | memory_read |
                           memory_write;
    uint64_t runtime_mask = 0u;
    uint64_t runtime_stages;
    if (!runtime_mask_out || (public_mask & ~known) != 0u) return 0;
    if (!rin_vk_sync2_stage_mask(stage_mask, &runtime_stages)) return 0;
    if ((public_mask & (memory_read | memory_write)) != 0u &&
        runtime_stages == 0u)
        return 0;
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
    }
    if ((public_mask & memory_write) != 0u) {
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE;
        if ((runtime_stages & RIN_GPU_VULKAN_BARRIER_STAGE_HOST) != 0u)
            runtime_mask |= RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE;
    }
    *runtime_mask_out = runtime_mask;
    return 1;
}

int rin_vk_sync2_access_stage_valid(uint64_t stage_mask,
                                    uint64_t access_mask) {
    const uint64_t transfer_access = RIN_VK_ACCESS_2_TRANSFER_READ_BIT |
                                     RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    const uint64_t host_access = RIN_VK_ACCESS_2_HOST_READ_BIT |
                                 RIN_VK_ACCESS_2_HOST_WRITE_BIT;
    const uint64_t generic_access = RIN_VK_ACCESS_2_MEMORY_READ_BIT |
                                    RIN_VK_ACCESS_2_MEMORY_WRITE_BIT;
    const uint64_t transfer_stages =
        RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT |
        RIN_VK_PIPELINE_STAGE_2_COPY_BIT |
        RIN_VK_PIPELINE_STAGE_2_RESOLVE_BIT |
        RIN_VK_PIPELINE_STAGE_2_BLIT_BIT |
        RIN_VK_PIPELINE_STAGE_2_CLEAR_BIT;
    const uint64_t all_commands = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    if ((access_mask & transfer_access) != 0u &&
        (stage_mask & (transfer_stages | all_commands)) == 0u)
        return 0;
    if ((access_mask & host_access) != 0u &&
        (stage_mask & (RIN_VK_PIPELINE_STAGE_2_HOST_BIT | all_commands)) == 0u)
        return 0;
    if ((access_mask & generic_access) != 0u && stage_mask == 0u) return 0;
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
        !rin_vk_sync2_stage_mask(src_stage_public, &src_stage) ||
        !rin_vk_sync2_access_mask(src_access_public, src_stage_public,
                                  &src_access) ||
        !rin_vk_sync2_stage_mask(dst_stage_public, &dst_stage) ||
        !rin_vk_sync2_access_mask(dst_access_public, dst_stage_public,
                                  &dst_access) ||
        !rin_vk_sync2_access_stage_valid(src_stage_public,
                                         src_access_public) ||
        !rin_vk_sync2_access_stage_valid(dst_stage_public,
                                         dst_access_public))
        return 0;
    operation->barrier.src_stage_mask = (uint32_t)src_stage;
    operation->barrier.src_access_mask = (uint32_t)src_access;
    operation->barrier.dst_stage_mask = (uint32_t)dst_stage;
    operation->barrier.dst_access_mask = (uint32_t)dst_access;
    return 1;
}
