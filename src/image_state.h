/* SPDX-License-Identifier: MIT */

#ifndef RINVULKAN_IMAGE_STATE_H
#define RINVULKAN_IMAGE_STATE_H

#include <stdint.h>

#include <ringpu/ringpu.h>
#include <rinvulkan/icd.h>

static inline int rin_vk_image_layout_to_ringpu_state(uint32_t layout,
                                                       uint32_t* state_out)
{
    uint32_t state;

    if (!state_out) return 0;
    switch (layout) {
    case RIN_VK_IMAGE_LAYOUT_UNDEFINED:
        state = RIN_GPU_IMAGE_STATE_UNDEFINED;
        break;
    case RIN_VK_IMAGE_LAYOUT_GENERAL:
        state = RIN_GPU_IMAGE_STATE_GENERAL;
        break;
    case RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
        break;
    case RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
        break;
    case RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
        break;
    case RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        state = RIN_GPU_IMAGE_STATE_PRESENT;
        break;
    default:
        return 0;
    }
    *state_out = state;
    return 1;
}

#endif /* RINVULKAN_IMAGE_STATE_H */
