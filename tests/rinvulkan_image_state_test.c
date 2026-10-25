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
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_SHADER_READ) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_SHADER_READ) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_SHADER_READ) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                        RIN_GPU_IMAGE_STATE_COLOR_TARGET) ||
        !expect_mapping(RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        RIN_GPU_IMAGE_STATE_PRESENT))
        return 1;
    if (rin_vk_image_layout_to_ringpu_state(UINT32_MAX, &state) ||
        state != UINT32_C(0x13579bdf) ||
        rin_vk_image_layout_to_ringpu_state(
            RIN_VK_IMAGE_LAYOUT_GENERAL, NULL))
        return 1;
    if (!rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_GENERAL, RIN_VK_FORMAT_R8_UNORM,
            RIN_VK_IMAGE_USAGE_STORAGE_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_GENERAL, RIN_VK_FORMAT_R8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            RIN_VK_FORMAT_D32_SFLOAT,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT, 0u, 0u) ||
        !rin_vk_image_color_attachment_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ||
        !rin_vk_image_color_attachment_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ||
        rin_vk_image_color_attachment_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ||
        rin_vk_image_color_attachment_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            RIN_VK_FORMAT_D32_SFLOAT,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_UNDEFINED,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 1u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_UNDEFINED,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 0u) ||
        !rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 1u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 0u) ||
        rin_vk_image_layout_valid_for_usage(
            UINT32_MAX, RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, 0u))
        return 1;
    if (!rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT) ||
        !rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R8_UNORM, RIN_VK_IMAGE_USAGE_STORAGE_BIT) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R8_UNORM,
            RIN_VK_IMAGE_USAGE_STORAGE_BIT |
                RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R8_UNORM, RIN_VK_IMAGE_USAGE_SAMPLED_BIT) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_D32_SFLOAT,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R8G8B8A8_UNORM, 0u) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R8G8B8A8_UNORM, UINT32_MAX))
        return 1;
    if (rin_vk_image_format_features(RIN_VK_FORMAT_R8_UNORM) !=
            RIN_VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT ||
        rin_vk_image_format_features(RIN_VK_FORMAT_R8G8B8A8_UNORM) !=
            (RIN_VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
             RIN_VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
             RIN_VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
             RIN_VK_FORMAT_FEATURE_TRANSFER_DST_BIT) ||
        rin_vk_image_format_features(RIN_VK_FORMAT_D32_SFLOAT) !=
            (RIN_VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
             RIN_VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
             RIN_VK_FORMAT_FEATURE_TRANSFER_DST_BIT) ||
        rin_vk_image_format_features(UINT32_MAX) != 0u ||
        rin_vk_buffer_format_features(RIN_VK_FORMAT_R32_SFLOAT) !=
            RIN_VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT ||
        rin_vk_buffer_format_features(RIN_VK_FORMAT_R32G32_SFLOAT) !=
            RIN_VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT ||
        rin_vk_buffer_format_features(RIN_VK_FORMAT_R32G32B32_SFLOAT) !=
            RIN_VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT ||
        rin_vk_buffer_format_features(RIN_VK_FORMAT_R32G32B32A32_SFLOAT) !=
            RIN_VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT ||
        rin_vk_buffer_format_features(INT32_MAX) != 0u ||
        !rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) ||
        !rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) ||
        !rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) ||
        rin_vk_image_descriptor_layout_valid(
            INT32_MAX, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) ||
        rin_vk_image_descriptor_layout_valid(
            INT32_MAX, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        rin_vk_image_descriptor_layout_valid(
            INT32_MAX, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ||
        !rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT,
            RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) ||
        !rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        return 1;
    if (!rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8_UNORM, RIN_VK_IMAGE_USAGE_STORAGE_BIT,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        !rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R16G16B16A16_SFLOAT,
            RIN_VK_IMAGE_USAGE_STORAGE_BIT) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R16G16B16A16_SFLOAT,
            RIN_VK_IMAGE_USAGE_STORAGE_BIT |
                RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        rin_vk_image_usage_valid_for_format(
            RIN_VK_FORMAT_R16G16B16A16_SFLOAT,
            RIN_VK_IMAGE_USAGE_SAMPLED_BIT) ||
        rin_vk_image_format_features(
            RIN_VK_FORMAT_R16G16B16A16_SFLOAT) !=
            RIN_VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT ||
        !rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R16G16B16A16_SFLOAT,
            RIN_VK_IMAGE_USAGE_STORAGE_BIT,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8_UNORM,
            RIN_VK_IMAGE_USAGE_STORAGE_BIT |
                RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8_UNORM, RIN_VK_IMAGE_USAGE_STORAGE_BIT,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM,
            RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_D32_SFLOAT, RIN_VK_IMAGE_USAGE_SAMPLED_BIT,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        rin_vk_image_descriptor_layout_valid(
            RIN_VK_FORMAT_R8G8B8A8_UNORM, 0u,
            RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            RIN_VK_IMAGE_LAYOUT_GENERAL))
        return 1;
    if (!rin_vk_image_layout_transition_valid(
            RIN_VK_IMAGE_LAYOUT_UNDEFINED, RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        !rin_vk_image_layout_transition_valid(
            RIN_VK_IMAGE_LAYOUT_GENERAL,
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ||
        !rin_vk_image_layout_transition_valid(
            RIN_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            RIN_VK_IMAGE_LAYOUT_GENERAL) ||
        rin_vk_image_layout_transition_valid(
            RIN_VK_IMAGE_LAYOUT_GENERAL, RIN_VK_IMAGE_LAYOUT_UNDEFINED) ||
        rin_vk_image_layout_transition_valid(
            UINT32_MAX, RIN_VK_IMAGE_LAYOUT_GENERAL))
        return 1;
    return 0;
}
