/* SPDX-License-Identifier: MIT */

#ifndef RINVULKAN_IMAGE_STATE_H
#define RINVULKAN_IMAGE_STATE_H

#include <stdint.h>

#include <ringpu/ringpu.h>
#include <rinvulkan/icd.h>

static inline int rin_vk_image_usage_valid_for_format(int32_t format,
                                                       uint32_t usage)
{
    if (usage == 0u || (usage & ~RIN_VK_IMAGE_USAGE_KNOWN) != 0u) return 0;
    if ((usage & RIN_VK_IMAGE_USAGE_SAMPLED_BIT) != 0u &&
        format != RIN_VK_FORMAT_R8G8B8A8_UNORM &&
        format != RIN_VK_FORMAT_D32_SFLOAT)
        return 0;
    if ((usage & RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u &&
        format != RIN_VK_FORMAT_R8G8B8A8_UNORM)
        return 0;
    return 1;
}

static inline uint32_t rin_vk_image_format_features(int32_t format)
{
    if (format == RIN_VK_FORMAT_R8G8B8A8_UNORM)
        return RIN_VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
               RIN_VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
               RIN_VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
               RIN_VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if (format == RIN_VK_FORMAT_D32_SFLOAT)
        return RIN_VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
               RIN_VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
               RIN_VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return 0u;
}

static inline uint32_t rin_vk_buffer_format_features(int32_t format)
{
    switch (format) {
    case RIN_VK_FORMAT_R32_SFLOAT:
    case RIN_VK_FORMAT_R32G32_SFLOAT:
    case RIN_VK_FORMAT_R32G32B32_SFLOAT:
    case RIN_VK_FORMAT_R32G32B32A32_SFLOAT:
        return RIN_VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
    default:
        return 0u;
    }
}

static inline int rin_vk_image_descriptor_layout_valid(
    int32_t format, uint32_t image_usage, uint32_t descriptor_type,
    uint32_t layout)
{
    if (descriptor_type == RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        /* This bounded RinVk image profile has no STORAGE usage bit or
         * storage-image format capability. GENERAL alone cannot make an
         * otherwise unsupported image valid for a storage descriptor. */
        return 0;
    }
    if (descriptor_type != RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
        descriptor_type != RIN_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        return 0;
    if ((image_usage & RIN_VK_IMAGE_USAGE_SAMPLED_BIT) == 0u)
        return 0;
    if ((rin_vk_image_format_features(format) &
         RIN_VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0u)
        return 0;
    if (layout == RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
        return format == RIN_VK_FORMAT_R8G8B8A8_UNORM ||
               format == RIN_VK_FORMAT_D32_SFLOAT;
    if (layout == RIN_VK_IMAGE_LAYOUT_GENERAL ||
        layout == RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return 1;
    return format == RIN_VK_FORMAT_D32_SFLOAT &&
           layout == RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
}

static inline int rin_vk_image_color_attachment_layout_valid(
    int32_t format, uint32_t image_usage, uint32_t layout)
{
    return format == RIN_VK_FORMAT_R8G8B8A8_UNORM &&
           (image_usage & RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u &&
           (layout == RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
            layout == RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
}

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
    case RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        state = RIN_GPU_IMAGE_STATE_SHADER_READ;
        break;
    case RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        state = RIN_GPU_IMAGE_STATE_SHADER_READ;
        break;
    case RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        state = RIN_GPU_IMAGE_STATE_SHADER_READ;
        break;
    case RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
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

static inline int rin_vk_image_layout_transition_valid(uint32_t old_layout,
                                                        uint32_t new_layout)
{
    uint32_t old_state;
    uint32_t new_state;

    return rin_vk_image_layout_to_ringpu_state(old_layout, &old_state) &&
           rin_vk_image_layout_to_ringpu_state(new_layout, &new_state) &&
           new_state != RIN_GPU_IMAGE_STATE_UNDEFINED;
}

static inline int rin_vk_image_layout_valid_for_usage(
    uint32_t layout, int32_t format, uint32_t image_usage,
    uint32_t is_swapchain_image, uint32_t is_old_layout)
{
    uint32_t state;

    if (is_swapchain_image > 1u || is_old_layout > 1u || image_usage == 0u ||
        (image_usage & ~RIN_VK_IMAGE_USAGE_KNOWN) != 0u)
        return 0;
    if (layout == RIN_VK_IMAGE_LAYOUT_UNDEFINED) return is_old_layout != 0u;
    if (layout == RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        return is_swapchain_image != 0u;
    if (layout == RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        return format == RIN_VK_FORMAT_D32_SFLOAT &&
               (image_usage & RIN_VK_IMAGE_USAGE_SAMPLED_BIT) != 0u;
    if (layout == RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
        return (format == RIN_VK_FORMAT_R8G8B8A8_UNORM ||
                format == RIN_VK_FORMAT_D32_SFLOAT) &&
               (image_usage & RIN_VK_IMAGE_USAGE_SAMPLED_BIT) != 0u;
    if (layout == RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
        return format == RIN_VK_FORMAT_R8G8B8A8_UNORM &&
               (image_usage & RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u;
    if (!rin_vk_image_layout_to_ringpu_state(layout, &state)) return 0;
    switch (state) {
    case RIN_GPU_IMAGE_STATE_GENERAL:
        return image_usage != 0u;
    case RIN_GPU_IMAGE_STATE_COPY_SOURCE:
        return (image_usage & RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u;
    case RIN_GPU_IMAGE_STATE_COPY_DESTINATION:
        return (image_usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u;
    case RIN_GPU_IMAGE_STATE_COLOR_TARGET:
        return (image_usage & RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u;
    case RIN_GPU_IMAGE_STATE_SHADER_READ:
        return (image_usage & RIN_VK_IMAGE_USAGE_SAMPLED_BIT) != 0u;
    default:
        return 0;
    }
}

#endif /* RINVULKAN_IMAGE_STATE_H */
