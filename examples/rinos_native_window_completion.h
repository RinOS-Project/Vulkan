/* SPDX-License-Identifier: MIT */
#ifndef RIN_VULKAN_EXAMPLE_NATIVE_WINDOW_COMPLETION_H
#define RIN_VULKAN_EXAMPLE_NATIVE_WINDOW_COMPLETION_H

#include <rin/contract_abi.h>
#include <rinvulkan/icd.h>
#include <rinruntime/window.h>

typedef struct RinVulkanExampleFrameCompletion {
    RinRuntimeGuiHandle window;
    int completed;
    int32_t status;
    uint64_t frame_sequence;
} RinVulkanExampleFrameCompletion;

typedef int (*RinVulkanExampleReadMonotonicMsFn)(
    void* context, uint64_t* milliseconds_out);
typedef int (*RinVulkanExampleDispatchCompositorFn)(
    void* context, uint32_t timeout_ms, uint32_t max_completions);

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

static inline RinVkResult rin_vulkan_example_wait_for_compositor_commit(
        RinVulkanExampleFrameCompletion* state,
        RinVulkanExampleReadMonotonicMsFn read_monotonic_ms,
        RinVulkanExampleDispatchCompositorFn dispatch_compositor,
        void* context) {
    uint64_t started_ms;
    if (!state || !read_monotonic_ms || !dispatch_compositor ||
        !read_monotonic_ms(context, &started_ms))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    while (!state->completed) {
        uint64_t now_ms;
        if (dispatch_compositor(context, 50u, 8u) < 0)
            return RIN_VK_ERROR_SURFACE_LOST_KHR;
        if (!read_monotonic_ms(context, &now_ms) || now_ms < started_ms)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        if (now_ms - started_ms >= UINT64_C(10000))
            return RIN_VK_TIMEOUT;
    }
    return state->status == RIN_RESULT_OK
               ? RIN_VK_SUCCESS
               : RIN_VK_ERROR_SURFACE_LOST_KHR;
}

#endif
