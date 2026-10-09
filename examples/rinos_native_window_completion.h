/* SPDX-License-Identifier: MIT */
#ifndef RIN_VULKAN_EXAMPLE_NATIVE_WINDOW_COMPLETION_H
#define RIN_VULKAN_EXAMPLE_NATIVE_WINDOW_COMPLETION_H

#include <rin/contract_abi.h>
#include <rinruntime/window.h>

typedef struct RinVulkanExampleFrameCompletion {
    RinRuntimeGuiHandle window;
    int completed;
    int32_t status;
    uint64_t frame_sequence;
} RinVulkanExampleFrameCompletion;

static inline void rin_vulkan_example_compositor_completion(
        const RinRuntimeGuiCompletionV1* completion, void* context) {
    RinVulkanExampleFrameCompletion* state =
        (RinVulkanExampleFrameCompletion*)context;
    if (!state || state->completed || !completion ||
        completion->handle != state->window ||
        completion->request_type != RIN_COMPOSITOR_DAMAGE)
        return;
    if (completion->struct_size != sizeof(*completion) ||
        completion->version != 1u || completion->cookie == 0u ||
        completion->payload_size != 0u || completion->reserved != 0u) {
        state->status = RIN_RESULT_CORRUPT_DATA;
    } else {
        state->status = completion->status;
        state->frame_sequence = completion->cookie;
    }
    state->completed = 1;
}

#endif
