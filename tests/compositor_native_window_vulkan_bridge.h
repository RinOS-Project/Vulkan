/* SPDX-License-Identifier: MIT */
#ifndef RINOS_TEST_COMPOSITOR_NATIVE_WINDOW_VULKAN_BRIDGE_H
#define RINOS_TEST_COMPOSITOR_NATIVE_WINDOW_VULKAN_BRIDGE_H

#include <rin/gui/compositor_protocol.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int compositor_native_window_vulkan_reset(
    uint32_t surface_id, uint32_t owner_pid, uint32_t width, uint32_t height,
    void* slot0, uint64_t slot0_bytes, void* slot1, uint64_t slot1_bytes,
    uint32_t pitch);
int compositor_native_window_vulkan_dispatch_frame(
    const RinCompositorDamage* damage, uint32_t damage_size,
    const RinCompositorCommitV2* commit, uint32_t commit_size,
    uint32_t owner_pid, const void* expected_front_pixels,
    uint64_t expected_front_bytes);

#ifdef __cplusplus
}
#endif

#endif /* RINOS_TEST_COMPOSITOR_NATIVE_WINDOW_VULKAN_BRIDGE_H */
