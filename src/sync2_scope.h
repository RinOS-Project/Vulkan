/* SPDX-License-Identifier: MIT */

#ifndef RINVULKAN_SYNC2_SCOPE_H
#define RINVULKAN_SYNC2_SCOPE_H

#include <stdint.h>

#include <rinvulkan/command_runtime.h>
#include <rinvulkan/icd.h>

int rin_vk_sync2_stage_mask(uint64_t public_mask, uint64_t* runtime_mask_out);
int rin_vk_sync2_legacy_wait_stage_mask(uint64_t public_mask,
                                       uint32_t* legacy_mask_out);
int rin_vk_sync2_recorded_stage_mask_valid(uint64_t public_mask);
int rin_vk_sync2_access_mask(uint64_t public_mask, uint64_t stage_mask,
                             uint64_t* runtime_mask_out);
int rin_vk_sync2_access_stage_valid(uint64_t stage_mask,
                                    uint64_t access_mask);
int rin_vk_sync2_barrier_scopes(uint64_t src_stage_public,
                                uint64_t src_access_public,
                                uint64_t dst_stage_public,
                                uint64_t dst_access_public,
                                RinGpuVulkanTransferOpV2* operation);

#endif
