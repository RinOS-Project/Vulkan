/* SPDX-License-Identifier: MIT */
#include <rinvulkan/graphics.h>

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                #condition); \
        return 1; \
    } \
} while (0)

static void emit(uint32_t* words, size_t* count, uint16_t opcode,
                 uint16_t word_count, const uint32_t* operands)
{
    words[(*count)++] = ((uint32_t)word_count << 16u) | opcode;
    for (uint16_t index = 1u; index < word_count; ++index)
        words[(*count)++] = operands[index - 1u];
}

static size_t make_scalar_module(uint32_t* words, uint32_t stage,
                                 uint32_t input_base_type,
                                 uint32_t output_base_type,
                                 uint32_t input_interpolation,
                                 uint32_t output_interpolation)
{
    size_t count = 5u;
    uint32_t operands[6];

    words[0] = RIN_SPIRV_MAGIC;
    words[1] = 0x00010300u;
    words[2] = 0u;
    words[3] = 12u;
    words[4] = 0u;
    operands[0] = 1u;
    emit(words, &count, 17u, 2u, operands); /* Shader capability */
    operands[0] = 0u; operands[1] = 3u;
    emit(words, &count, 14u, 3u, operands); /* Logical, Vulkan memory model */
    operands[0] = stage; operands[1] = 10u;
    operands[2] = 0x6e69616du; operands[3] = 0u;
    operands[4] = 6u; operands[5] = 7u;
    emit(words, &count, 15u, 7u, operands);
    operands[0] = 1u;
    emit(words, &count, 19u, 2u, operands); /* void */
    operands[0] = 2u; operands[1] = 32u;
    emit(words, &count, 22u, 3u, operands); /* float */
    operands[0] = 3u; operands[1] = 32u; operands[2] = 1u;
    emit(words, &count, 21u, 4u, operands); /* signed int */
    operands[0] = 4u; operands[1] = 1u;
    operands[2] = input_base_type == 1u ? 3u : 2u;
    emit(words, &count, 32u, 4u, operands); /* input pointer */
    operands[0] = 5u; operands[1] = 3u;
    operands[2] = output_base_type == 1u ? 3u : 2u;
    emit(words, &count, 32u, 4u, operands); /* float output pointer */
    operands[0] = 4u; operands[1] = 6u; operands[2] = 1u;
    emit(words, &count, 59u, 4u, operands); /* input */
    operands[0] = 5u; operands[1] = 7u; operands[2] = 3u;
    emit(words, &count, 59u, 4u, operands); /* output */
    operands[0] = 6u; operands[1] = 30u; operands[2] = 0u;
    emit(words, &count, 71u, 4u, operands); /* input Location 0 */
    if (input_interpolation != 0u) {
        operands[0] = 6u; operands[1] = input_interpolation;
        emit(words, &count, 71u, 3u, operands);
    }
    operands[0] = 7u; operands[1] = 30u; operands[2] = 0u;
    emit(words, &count, 71u, 4u, operands); /* output Location 0 */
    if (output_interpolation != 0u) {
        operands[0] = 7u; operands[1] = output_interpolation;
        emit(words, &count, 71u, 3u, operands);
    }
    operands[0] = 8u; operands[1] = 1u;
    emit(words, &count, 33u, 3u, operands); /* function type */
    operands[0] = output_base_type == 1u ? 3u : 2u;
    operands[1] = 9u;
    operands[2] = output_base_type == 1u ? 1u : 0x3f800000u;
    emit(words, &count, 43u, 4u, operands); /* float constant 1 */
    operands[0] = 1u; operands[1] = 10u; operands[2] = 0u; operands[3] = 8u;
    emit(words, &count, 54u, 5u, operands); /* main */
    operands[0] = 11u;
    emit(words, &count, 248u, 2u, operands); /* label */
    operands[0] = 7u; operands[1] = 9u;
    emit(words, &count, 62u, 3u, operands); /* store output */
    emit(words, &count, 253u, 1u, operands); /* return */
    emit(words, &count, 56u, 1u, operands); /* function end */
    return count;
}

static void make_graphics_state(RinGpuVulkanGraphicsStateV1* state)
{
    memset(state, 0, sizeof(*state));
    state->struct_size = sizeof(*state);
    state->version = RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION;
    state->color_format = RIN_GPU_FORMAT_RGBA8_UNORM;
    state->primitive_topology = RIN_GPU_PRIMITIVE_TRIANGLE_LIST;
    state->position_output_location = 0u;
    state->render_pass_model = RIN_GPU_VULKAN_GRAPHICS_RENDER_PASS;
}

static int test_interpolation(uint32_t fragment_decoration,
                              uint32_t vertex_decoration,
                              uint32_t input_type,
                              uint32_t expected_backend_interpolation)
{
    uint32_t vertex_words[128];
    uint32_t fragment_words[128];
    uint8_t shader_bytes[4096];
    RinSpirvTranslationInfoV2 vertex;
    RinSpirvTranslationInfoV2 fragment;
    RinGpuVulkanGraphicsStateV1 state;
    RinGpuVulkanVertexInputAttributeV1 attribute;
    RinGpuVulkanGraphicsPipelinePlanV1 plan;
    int translate_result;
    size_t vertex_word_count = make_scalar_module(
        vertex_words, 0u, 2u, input_type, 0u, vertex_decoration);
    size_t fragment_word_count = make_scalar_module(
        fragment_words, 4u, input_type, 2u, fragment_decoration, 0u);

    translate_result = ringpu_vulkan_graphics_translate_shader_v2(
        vertex_words, vertex_word_count, RIN_SHADER_STAGE_VERTEX,
        NULL, 0u, shader_bytes, sizeof(shader_bytes), &vertex);
    if (translate_result != RIN_GPU_VULKAN_GRAPHICS_OK)
        fprintf(stderr, "vertex translate result %d, frontend %s\n",
                translate_result, vertex.base.diagnostic);
    CHECK(translate_result == RIN_GPU_VULKAN_GRAPHICS_OK);
    CHECK(ringpu_vulkan_graphics_translate_shader_v2(
              fragment_words, fragment_word_count, RIN_SHADER_STAGE_FRAGMENT,
              NULL, 0u, shader_bytes, sizeof(shader_bytes), &fragment) ==
          RIN_GPU_VULKAN_GRAPHICS_OK);
    CHECK(vertex.base.output_count == 1u);
    CHECK(fragment.base.input_count == 1u);
    CHECK(vertex.base.struct_size == sizeof(RinSpirvTranslationInfoV1));
    CHECK(vertex.base.api_version == 1u);
    CHECK(vertex.struct_size == sizeof(vertex) && vertex.api_version == 2u);
    CHECK(fragment.input_interpolation[0] ==
          (fragment_decoration == 14u
               ? RIN_SPIRV_INTERPOLATION_FLAT_V2
               : fragment_decoration == 13u
                   ? RIN_SPIRV_INTERPOLATION_NO_PERSPECTIVE_V2
                   : RIN_SPIRV_INTERPOLATION_UNSPECIFIED_V2));
    CHECK(vertex.output_interpolation[0] ==
          (vertex_decoration == 14u
               ? RIN_SPIRV_INTERPOLATION_FLAT_V2
               : vertex_decoration == 13u
                   ? RIN_SPIRV_INTERPOLATION_NO_PERSPECTIVE_V2
                   : RIN_SPIRV_INTERPOLATION_UNSPECIFIED_V2));
    make_graphics_state(&state);
    memset(&attribute, 0, sizeof(attribute));
    attribute.location = 0u;
    attribute.format = RIN_GPU_VERTEX_FLOAT32;
    attribute.flags = RIN_GPU_VERTEX_ATTRIBUTE_CONSTANT_FLOAT32;
    CHECK(ringpu_vulkan_graphics_build_pipeline_v2(
              &vertex, &fragment, &state, NULL, 0u, &attribute, 1u,
              &plan) == RIN_GPU_VULKAN_GRAPHICS_OK);
    CHECK(plan.backend.varying_count == 1u);
    CHECK(plan.backend.varyings[0].interpolation ==
          expected_backend_interpolation);
    return 0;
}

int main(void)
{
    uint32_t words[128];
    uint8_t shader_bytes[4096];
    RinSpirvTranslationInfoV1 legacy;
    RinSpirvTranslationInfoV2 versioned;
    size_t word_count;

    CHECK(test_interpolation(14u, 13u, 2u,
                             RIN_GPU_INTERPOLATION_FLAT) == 0);
    CHECK(test_interpolation(13u, 14u, 2u,
                             RIN_GPU_INTERPOLATION_NO_PERSPECTIVE) == 0);
    CHECK(test_interpolation(0u, 13u, 2u,
                             RIN_GPU_INTERPOLATION_PERSPECTIVE) == 0);
    CHECK(test_interpolation(14u, 0u, 1u,
                             RIN_GPU_INTERPOLATION_FLAT) == 0);

    word_count = make_scalar_module(words, 4u, 2u, 2u, 14u, 0u);
    CHECK(ringpu_spirv_translate(
              words, word_count, RIN_SHADER_STAGE_FRAGMENT, NULL, 0u,
              shader_bytes, sizeof(shader_bytes), &legacy) ==
          RIN_SPIRV_ERROR_UNSUPPORTED);
    word_count = make_scalar_module(words, 4u, 2u, 2u, 16u, 0u);
    CHECK(ringpu_spirv_translate_v2(
              words, word_count, RIN_SHADER_STAGE_FRAGMENT, NULL, 0u,
              shader_bytes, sizeof(shader_bytes), &versioned) ==
          RIN_SPIRV_ERROR_UNSUPPORTED);
    word_count = make_scalar_module(words, 4u, 2u, 2u, 17u, 0u);
    CHECK(ringpu_spirv_translate_v2(
              words, word_count, RIN_SHADER_STAGE_FRAGMENT, NULL, 0u,
              shader_bytes, sizeof(shader_bytes), &versioned) ==
          RIN_SPIRV_ERROR_UNSUPPORTED);
    word_count = make_scalar_module(words, 4u, 1u, 2u, 0u, 0u);
    CHECK(ringpu_vulkan_graphics_translate_shader_v2(
              words, word_count, RIN_SHADER_STAGE_FRAGMENT, NULL, 0u,
              shader_bytes, sizeof(shader_bytes), &versioned) ==
          RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE);
    return 0;
}
