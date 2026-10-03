/* SPDX-License-Identifier: MIT */

#ifndef RINVULKAN_BUFFER_OWNERSHIP_H
#define RINVULKAN_BUFFER_OWNERSHIP_H

#include <stdint.h>

#include <rinvulkan/icd.h>

#define RIN_VK_MAX_SUBMIT_SEMAPHORES 8u
#define RIN_VK_MAX_BUFFER_OWNERSHIP_RANGES 32u

typedef struct RinVkBufferOwnershipRange {
    uint64_t offset;
    uint64_t size;
    uint32_t owner_queue_family;
    uint32_t transfer_pending;
    uint32_t transfer_source_family;
    uint32_t transfer_destination_family;
    uint32_t transfer_semaphore_count;
    uint32_t reserved;
    uint64_t transfer_offset;
    uint64_t transfer_size;
    RinVkSemaphore transfer_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t transfer_semaphore_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
} RinVkBufferOwnershipRange;

typedef struct RinVkBufferOwnershipState {
    uint32_t range_count;
    uint32_t reserved;
    RinVkBufferOwnershipRange ranges[RIN_VK_MAX_BUFFER_OWNERSHIP_RANGES];
} RinVkBufferOwnershipState;

int rin_vk_buffer_ownership_merge_adjacent(RinVkBufferOwnershipState* state);
int rin_vk_buffer_ownership_ensure_coverage(
        RinVkBufferOwnershipState* state, uint64_t start, uint64_t end,
        uint32_t default_owner_family, int create_gaps);

#endif
