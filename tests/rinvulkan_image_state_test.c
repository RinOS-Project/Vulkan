/* SPDX-License-Identifier: MIT */
#include <stdint.h>

#include "image_state.h"

static int expect_mapping(uint32_t layout, uint32_t expected_state)
{
    uint32_t state = UINT32_MAX;
    return rin_vk_image_layout_to_ringpu_state(layout, &state) &&
           state == expected_state;
}

int main(void)
{
    uint32_t state = UINT32_C(0x13579bdf);

    if (!expect_mapping(RIN_VK_IMAGE_LAYOUT_UNDEFINED,
                        RIN_GPU_IMAGE_STATE_UNDEFINED) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_GENERAL,
                        RIN_GPU_IMAGE_STATE_GENERAL) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_COPY_SOURCE) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_COPY_DESTINATION) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_COLOR_TARGET) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        RIN_GPU_IMAGE_STATE_PRESENT))
        return 1;
    if (rin_vk_image_layout_to_ringpu_state(UINT32_MAX, &state) ||
        state != UINT32_C(0x13579bdf) ||
        rin_vk_image_layout_to_ringpu_state(
            RIN_VK_IMAGE_LAYOUT_GENERAL, NULL))
        return 1;
    return 0;
}
