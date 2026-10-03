/* SPDX-License-Identifier: MIT */

#include "buffer_ownership.h"

#include <string.h>

static int buffer_ownership_metadata_equal(
        const RinVkBufferOwnershipRange* left,
        const RinVkBufferOwnershipRange* right) {
    return left->owner_queue_family == right->owner_queue_family &&
           left->transfer_pending == right->transfer_pending &&
           left->transfer_source_family == right->transfer_source_family &&
           left->transfer_destination_family ==
               right->transfer_destination_family &&
           left->transfer_semaphore_count ==
               right->transfer_semaphore_count &&
           left->reserved == right->reserved &&
           left->transfer_offset == right->transfer_offset &&
           left->transfer_size == right->transfer_size &&
           memcmp(left->transfer_semaphores, right->transfer_semaphores,
                  sizeof(left->transfer_semaphores)) == 0 &&
           memcmp(left->transfer_semaphore_values,
                  right->transfer_semaphore_values,
                  sizeof(left->transfer_semaphore_values)) == 0;
}

int rin_vk_buffer_ownership_merge_adjacent(RinVkBufferOwnershipState* state) {
    uint32_t index;
    if (!state || state->range_count > RIN_VK_MAX_BUFFER_OWNERSHIP_RANGES)
        return 0;

    for (index = 0u; index < state->range_count; ++index) {
        const RinVkBufferOwnershipRange* range = &state->ranges[index];
        if (range->size == 0u ||
            range->offset > UINT64_MAX - range->size ||
            range->transfer_semaphore_count > RIN_VK_MAX_SUBMIT_SEMAPHORES)
            return 0;
        if (index != 0u && state->ranges[index - 1u].offset +
                                   state->ranges[index - 1u].size >
                               range->offset)
            return 0;
    }

    index = 0u;
    while (index + 1u < state->range_count) {
        RinVkBufferOwnershipRange* left = &state->ranges[index];
        RinVkBufferOwnershipRange* right = &state->ranges[index + 1u];
        if (left->offset + left->size != right->offset ||
            !buffer_ownership_metadata_equal(left, right)) {
            ++index;
            continue;
        }
        left->size += right->size;
        --state->range_count;
        memmove(right, right + 1u,
                sizeof(state->ranges[0]) * (state->range_count - index - 1u));
        memset(&state->ranges[state->range_count], 0,
               sizeof(state->ranges[0]));
    }
    return 1;
}

static int buffer_ownership_split_at(RinVkBufferOwnershipState* state,
                                     uint64_t position) {
    uint32_t index;
    for (index = 0u; index < state->range_count; ++index) {
        RinVkBufferOwnershipRange* range = &state->ranges[index];
        const uint64_t range_end = range->offset + range->size;
        if (position <= range->offset || position >= range_end) continue;
        if (state->range_count >= RIN_VK_MAX_BUFFER_OWNERSHIP_RANGES)
            return 0;
        memmove(&state->ranges[index + 2u], &state->ranges[index + 1u],
                sizeof(state->ranges[0]) *
                    (state->range_count - index - 1u));
        state->ranges[index + 1u] = *range;
        state->ranges[index].size = position - range->offset;
        state->ranges[index + 1u].offset = position;
        state->ranges[index + 1u].size = range_end - position;
        ++state->range_count;
        return 1;
    }
    return 1;
}

static int buffer_ownership_insert(RinVkBufferOwnershipState* state,
                                   uint32_t index, uint64_t offset,
                                   uint64_t size, uint32_t owner_family) {
    if (state->range_count >= RIN_VK_MAX_BUFFER_OWNERSHIP_RANGES ||
        index > state->range_count)
        return 0;
    memmove(&state->ranges[index + 1u], &state->ranges[index],
            sizeof(state->ranges[0]) * (state->range_count - index));
    memset(&state->ranges[index], 0, sizeof(state->ranges[index]));
    state->ranges[index].offset = offset;
    state->ranges[index].size = size;
    state->ranges[index].owner_queue_family = owner_family;
    ++state->range_count;
    return 1;
}

int rin_vk_buffer_ownership_ensure_coverage(
        RinVkBufferOwnershipState* state, uint64_t start, uint64_t end,
        uint32_t default_owner_family, int create_gaps) {
    uint64_t position = start;
    uint32_t index = 0u;
    if (!state || start >= end ||
        !rin_vk_buffer_ownership_merge_adjacent(state) ||
        !buffer_ownership_split_at(state, start) ||
        !buffer_ownership_split_at(state, end))
        return 0;
    while (position < end) {
        while (index < state->range_count &&
               state->ranges[index].offset + state->ranges[index].size <=
                   position)
            ++index;
        if (index < state->range_count &&
            state->ranges[index].offset <= position) {
            const uint64_t range_end = state->ranges[index].offset +
                                       state->ranges[index].size;
            if (range_end <= position) return 0;
            position = range_end < end ? range_end : end;
            ++index;
            continue;
        }
        if (!create_gaps) return 0;
        {
            const uint64_t gap_end =
                index < state->range_count &&
                        state->ranges[index].offset < end
                    ? state->ranges[index].offset
                    : end;
            if (gap_end <= position ||
                !buffer_ownership_insert(state, index, position,
                                         gap_end - position,
                                         default_owner_family))
                return 0;
            position = gap_end;
            ++index;
        }
    }
    return 1;
}
