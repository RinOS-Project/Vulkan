/* SPDX-License-Identifier: MIT */
#ifndef RIN_VULKAN_WSI_PRESENT_LAYOUT_H
#define RIN_VULKAN_WSI_PRESENT_LAYOUT_H

#include <stdint.h>

#include <rinvulkan/wsi_platform.h>

static inline int rin_vk_wsi_linear_rgba8_layout_valid(
    const RinVulkanWsiImageLayoutV1* layout, uint32_t width,
    uint32_t height, uint64_t allocation_size) {
    uint64_t minimum_row_bytes;
    uint64_t row_pitch_bytes;
    uint64_t plane_size_bytes;
    uint32_t index;

    if (!layout || width == 0u || height == 0u || allocation_size == 0u ||
        layout->struct_size != sizeof(*layout) ||
        layout->version != RIN_VULKAN_WSI_IMAGE_LAYOUT_V1_VERSION ||
        layout->kind != RIN_VULKAN_WSI_IMAGE_LAYOUT_KIND_LINEAR ||
        layout->plane_count != 1u || layout->modifier != 0u ||
        layout->planes[0].offset_bytes != 0u ||
        layout->reserved[0] != 0u || layout->reserved[1] != 0u)
        return 0;

    for (index = 1u; index < RIN_VULKAN_WSI_IMAGE_LAYOUT_MAX_PLANES;
         ++index) {
        if (layout->planes[index].offset_bytes != 0u ||
            layout->planes[index].row_pitch_bytes != 0u ||
            layout->planes[index].size_bytes != 0u)
            return 0;
    }

    minimum_row_bytes = (uint64_t)width * UINT64_C(4);
    row_pitch_bytes = layout->planes[0].row_pitch_bytes;
    if (row_pitch_bytes < minimum_row_bytes ||
        row_pitch_bytes > UINT64_MAX / (uint64_t)height)
        return 0;
    plane_size_bytes = row_pitch_bytes * (uint64_t)height;
    return layout->planes[0].size_bytes == plane_size_bytes &&
           plane_size_bytes <= allocation_size;
}

#endif
