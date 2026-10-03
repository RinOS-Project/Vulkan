/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "buffer_ownership.h"

static void test_coalesces_equivalent_ranges_before_splitting(void) {
    RinVkBufferOwnershipState state;
    uint32_t index;
    memset(&state, 0, sizeof(state));
    state.range_count = RIN_VK_MAX_BUFFER_OWNERSHIP_RANGES;
    for (index = 0u; index < state.range_count; ++index) {
        state.ranges[index].offset = (uint64_t)index * 8u;
        state.ranges[index].size = 8u;
        state.ranges[index].owner_queue_family = 4u;
    }

    assert(rin_vk_buffer_ownership_ensure_coverage(&state, 3u, 5u, 4u, 0));
    assert(state.range_count == 3u);
    assert(state.ranges[0].offset == 0u && state.ranges[0].size == 3u);
    assert(state.ranges[1].offset == 3u && state.ranges[1].size == 2u);
    assert(state.ranges[2].offset == 5u && state.ranges[2].size == 251u);
}

static void test_preserves_distinct_ownership_metadata(void) {
    RinVkBufferOwnershipState state;
    memset(&state, 0, sizeof(state));
    state.range_count = 2u;
    state.ranges[0].offset = 0u;
    state.ranges[0].size = 8u;
    state.ranges[0].owner_queue_family = 4u;
    state.ranges[1].offset = 8u;
    state.ranges[1].size = 8u;
    state.ranges[1].owner_queue_family = 5u;

    assert(rin_vk_buffer_ownership_ensure_coverage(&state, 2u, 4u, 4u, 0));
    assert(state.range_count == 4u);
    assert(state.ranges[1].offset == 2u && state.ranges[1].size == 2u);
    assert(state.ranges[1].owner_queue_family == 4u);
    assert(state.ranges[3].offset == 8u && state.ranges[3].size == 8u);
    assert(state.ranges[3].owner_queue_family == 5u);
}

static void test_pending_transfers_merge_only_when_identical(void) {
    RinVkBufferOwnershipState state;
    memset(&state, 0, sizeof(state));
    state.range_count = 2u;
    state.ranges[0].offset = 0u;
    state.ranges[0].size = 8u;
    state.ranges[0].owner_queue_family = 4u;
    state.ranges[0].transfer_pending = 1u;
    state.ranges[0].transfer_source_family = 4u;
    state.ranges[0].transfer_destination_family = 5u;
    state.ranges[0].transfer_semaphore_count = 1u;
    state.ranges[0].transfer_semaphores[0] = 17u;
    state.ranges[0].transfer_semaphore_values[0] = 2u;
    state.ranges[1] = state.ranges[0];
    state.ranges[1].offset = 8u;
    state.ranges[1].transfer_semaphores[0] = 18u;

    assert(rin_vk_buffer_ownership_ensure_coverage(&state, 2u, 4u, 4u, 0));
    assert(state.range_count == 4u);
    assert(state.ranges[1].transfer_semaphores[0] == 17u);
    assert(state.ranges[3].transfer_semaphores[0] == 18u);
}

static void test_merges_identical_pending_transfer_metadata(void) {
    RinVkBufferOwnershipState state;
    memset(&state, 0, sizeof(state));
    state.range_count = 2u;
    state.ranges[0].offset = 0u;
    state.ranges[0].size = 8u;
    state.ranges[0].owner_queue_family = 4u;
    state.ranges[0].transfer_pending = 1u;
    state.ranges[0].transfer_source_family = 4u;
    state.ranges[0].transfer_destination_family = 5u;
    state.ranges[0].transfer_semaphore_count = 1u;
    state.ranges[0].transfer_offset = 0u;
    state.ranges[0].transfer_size = 16u;
    state.ranges[0].transfer_semaphores[0] = 17u;
    state.ranges[0].transfer_semaphore_values[0] = 2u;
    state.ranges[1] = state.ranges[0];
    state.ranges[1].offset = 8u;

    assert(rin_vk_buffer_ownership_merge_adjacent(&state));
    assert(state.range_count == 1u);
    assert(state.ranges[0].offset == 0u && state.ranges[0].size == 16u);
    assert(state.ranges[0].transfer_semaphores[0] == 17u);
    assert(state.ranges[0].transfer_semaphore_values[0] == 2u);
}

int main(void) {
    test_coalesces_equivalent_ranges_before_splitting();
    test_preserves_distinct_ownership_metadata();
    test_pending_transfers_merge_only_when_identical();
    test_merges_identical_pending_transfer_metadata();
    return 0;
}
