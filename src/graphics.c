/* SPDX-License-Identifier: MIT */

#include <rinvulkan/graphics.h>

#include <string.h>

static int translation_info_valid(const RinSpirvTranslationInfoV1* info,
                                  uint32_t stage) {
    if (!info || info->struct_size != sizeof(*info) ||
        info->shader.abi_version != RIN_GPU_ABI_VERSION ||
        info->shader.struct_size != sizeof(info->shader) ||
        info->shader.stage != stage || info->stage != stage ||
        info->input_count > RIN_SPIRV_MAX_IO ||
        info->output_count > RIN_SPIRV_MAX_IO ||
        info->descriptor_count > RIN_SPIRV_MAX_RESOURCES ||
        info->shader.resource_count > RIN_SHADER_MAX_RESOURCES)
        return 0;
    return 1;
}

static int finite_float(float value) {
    union {
        float value;
        uint32_t bits;
    } cast;
    uint32_t exponent;
    cast.value = value;
    exponent = (cast.bits >> 23u) & 0xffu;
    return exponent != 0xffu;
}

static int descriptor_kind_valid(uint32_t kind) {
    return kind == RIN_SHADER_RESOURCE_STORAGE_BUFFER ||
           kind == RIN_SHADER_RESOURCE_SAMPLED_IMAGE ||
           kind == RIN_SHADER_RESOURCE_SAMPLER ||
           kind == RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE ||
           kind == RIN_SHADER_RESOURCE_COMPARISON_SAMPLER ||
           kind == RIN_SHADER_RESOURCE_STORAGE_IMAGE;
}

int ringpu_vulkan_graphics_translate_shader(
    const uint32_t* words, size_t word_count, uint32_t expected_stage,
    const RinSpirvSpecializationValueV1* overrides, uint32_t override_count,
    void* rin_shader_out, size_t rin_shader_capacity,
    RinSpirvTranslationInfoV1* info_out) {
    int result;
    if (!words || word_count == 0u || !rin_shader_out ||
        rin_shader_capacity == 0u || !info_out ||
        (expected_stage != RIN_SHADER_STAGE_VERTEX &&
         expected_stage != RIN_SHADER_STAGE_FRAGMENT &&
         expected_stage != RIN_SHADER_STAGE_COMPUTE) ||
        (override_count != 0u && !overrides))
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    result = ringpu_spirv_translate(
        words, word_count, expected_stage, overrides, override_count,
        rin_shader_out, rin_shader_capacity, info_out);
    if (result != RIN_SPIRV_OK) {
        memset(info_out, 0, sizeof(*info_out));
        return result == RIN_SPIRV_ERROR_NO_MEMORY
                   ? RIN_GPU_VULKAN_GRAPHICS_LIMIT
                   : RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    }
    if (!translation_info_valid(info_out, expected_stage)) {
        memset(info_out, 0, sizeof(*info_out));
        return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    }
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

static uint32_t descriptor_resource_kind(uint32_t descriptor_type) {
    switch (descriptor_type) {
        case RIN_GPU_VULKAN_DESCRIPTOR_UNIFORM_BUFFER:
        case RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_BUFFER:
            return RIN_SHADER_RESOURCE_STORAGE_BUFFER;
        case RIN_GPU_VULKAN_DESCRIPTOR_SAMPLED_IMAGE:
            return RIN_SHADER_RESOURCE_SAMPLED_IMAGE;
        case RIN_GPU_VULKAN_DESCRIPTOR_SAMPLER:
            return RIN_SHADER_RESOURCE_SAMPLER;
        case RIN_GPU_VULKAN_DESCRIPTOR_SAMPLED_DEPTH_IMAGE:
            return RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE;
        case RIN_GPU_VULKAN_DESCRIPTOR_COMPARISON_SAMPLER:
            return RIN_SHADER_RESOURCE_COMPARISON_SAMPLER;
        case RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_IMAGE:
            return RIN_SHADER_RESOURCE_STORAGE_IMAGE;
        default:
            return RIN_SHADER_RESOURCE_NONE;
    }
}

static int graphics_state_valid(const RinGpuVulkanGraphicsStateV1* state) {
    return state && state->struct_size == sizeof(*state) &&
           state->version == RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION &&
           state->render_pass_model == RIN_GPU_VULKAN_GRAPHICS_RENDER_PASS &&
           state->flags == 0u && state->reserved == 0u &&
           state->color_format != 0u && state->primitive_topology != 0u &&
           state->position_output_location < RIN_SHADER_MAX_IO &&
           (!state->blend_enabled ||
            (finite_float(state->blend_constant_red) &&
             finite_float(state->blend_constant_green) &&
             finite_float(state->blend_constant_blue) &&
             finite_float(state->blend_constant_alpha)));
}

static const RinSpirvIoV1* find_io(const RinSpirvIoV1* io, uint32_t count,
                                   uint32_t location) {
    uint32_t index;
    if (!io) return NULL;
    for (index = 0u; index < count; ++index)
        if (io[index].location == location) return &io[index];
    return NULL;
}

static int descriptor_kind_merge(uint32_t* kinds, uint32_t* sets,
                                 uint32_t* bindings, uint32_t* count,
                                 const RinSpirvTranslationInfoV1* info) {
    uint32_t index;
    if (!kinds || !sets || !bindings || !count || !info) return 0;
    for (index = 0u; index < info->descriptor_count; ++index) {
        const RinSpirvDescriptorV1* descriptor = &info->descriptors[index];
        uint32_t resource_index = descriptor->resource_index;
        if (resource_index >= RIN_SHADER_MAX_RESOURCES ||
            descriptor->set == UINT32_MAX ||
            descriptor->binding == UINT32_MAX ||
            !descriptor_kind_valid(descriptor->resource_kind))
            return 0;
        for (uint32_t prior = 0u; prior < index; ++prior)
            if (info->descriptors[prior].resource_index == resource_index)
                return 0;
        if (kinds[resource_index] != RIN_SHADER_RESOURCE_NONE &&
            (kinds[resource_index] != descriptor->resource_kind ||
             sets[resource_index] != descriptor->set ||
             bindings[resource_index] != descriptor->binding))
            return 0;
        if (kinds[resource_index] == RIN_SHADER_RESOURCE_NONE) {
            kinds[resource_index] = descriptor->resource_kind;
            sets[resource_index] = descriptor->set;
            bindings[resource_index] = descriptor->binding;
            if (resource_index + 1u > *count) *count = resource_index + 1u;
        }
    }
    return 1;
}

static int validate_vertex_inputs(
    const RinSpirvTranslationInfoV1* vertex,
    const RinGpuVulkanVertexInputBindingV1* bindings,
    uint32_t binding_count,
    const RinGpuVulkanVertexInputAttributeV1* attributes,
    uint32_t attribute_count) {
    uint32_t index;
    if (binding_count > RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS ||
        attribute_count > RIN_GPU_MAX_VERTEX_ATTRIBUTES ||
        (binding_count != 0u && !bindings) ||
        (attribute_count != 0u && !attributes))
        return 0;
    for (index = 0u; index < binding_count; ++index) {
        if (bindings[index].binding >= RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS ||
            bindings[index].stride == 0u ||
            bindings[index].stride > RIN_GPU_MAX_VERTEX_STRIDE ||
            bindings[index].divisor > 1u || bindings[index].reserved != 0u)
            return 0;
        for (uint32_t prior = 0u; prior < index; ++prior)
            if (bindings[prior].binding == bindings[index].binding) return 0;
    }
    for (index = 0u; index < vertex->input_count; ++index) {
        const RinSpirvIoV1* input = &vertex->inputs[index];
        const RinGpuVulkanVertexInputAttributeV1* attribute = NULL;
        uint32_t attribute_index;
        if (input->width == 0u || input->width > 4u ||
            input->base_type == 0u || input->location >= RIN_SHADER_MAX_IO)
            return 0;
        for (attribute_index = 0u; attribute_index < attribute_count;
             ++attribute_index)
            if (attributes[attribute_index].location == input->location) {
                attribute = &attributes[attribute_index];
                break;
            }
        if (!attribute || attribute->binding >= RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS ||
            attribute->reserved != 0u ||
            (attribute->offset > UINT32_MAX - (input->width - 1u) * 4u))
            return 0;
        if (attribute->flags & ~RIN_GPU_VERTEX_ATTRIBUTE_CONSTANT_FLOAT32)
            return 0;
        if ((attribute->flags & RIN_GPU_VERTEX_ATTRIBUTE_CONSTANT_FLOAT32) != 0u &&
            attribute->format != RIN_GPU_VERTEX_FLOAT32)
            return 0;
        for (attribute_index = 0u; attribute_index < binding_count;
             ++attribute_index)
            if (bindings[attribute_index].binding == attribute->binding) break;
        if (attribute_index == binding_count &&
            (attribute->flags & RIN_GPU_VERTEX_ATTRIBUTE_CONSTANT_FLOAT32) == 0u)
            return 0;
    }
    return 1;
}

int ringpu_vulkan_graphics_build_pipeline(
    const RinSpirvTranslationInfoV1* vertex,
    const RinSpirvTranslationInfoV1* fragment,
    const RinGpuVulkanGraphicsStateV1* state,
    const RinGpuVulkanVertexInputBindingV1* vertex_bindings,
    uint32_t vertex_binding_count,
    const RinGpuVulkanVertexInputAttributeV1* vertex_attributes,
    uint32_t vertex_attribute_count,
    RinGpuVulkanGraphicsPipelinePlanV1* plan_out) {
    RinGpuVulkanGraphicsPipelinePlanV1 plan;
    uint32_t resource_kinds[RIN_SHADER_MAX_RESOURCES];
    uint32_t resource_sets[RIN_SHADER_MAX_RESOURCES];
    uint32_t resource_bindings[RIN_SHADER_MAX_RESOURCES];
    uint32_t resource_count = 0u;
    uint32_t output_index;
    uint32_t attribute_count = 0u;
    uint32_t varying_count = 0u;

    if (!plan_out || (vertex &&
                      (uintptr_t)plan_out >= (uintptr_t)vertex &&
                      (uintptr_t)plan_out - (uintptr_t)vertex < sizeof(*vertex)) ||
        (fragment && (uintptr_t)plan_out >= (uintptr_t)fragment &&
         (uintptr_t)plan_out - (uintptr_t)fragment < sizeof(*fragment)) ||
        !translation_info_valid(vertex, RIN_SHADER_STAGE_VERTEX) ||
        !translation_info_valid(fragment, RIN_SHADER_STAGE_FRAGMENT) ||
        !graphics_state_valid(state) ||
        !validate_vertex_inputs(vertex, vertex_bindings, vertex_binding_count,
                                vertex_attributes, vertex_attribute_count))
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;

    memset(&plan, 0, sizeof(plan));
    plan.struct_size = sizeof(plan);
    plan.version = RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION;
    plan.vertex_shader = vertex->shader;
    plan.fragment_shader = fragment->shader;
    plan.backend.color_format = state->color_format;
    plan.backend.primitive_topology = state->primitive_topology;
    plan.backend.vertex_binding_count = vertex_binding_count;
    plan.backend.depth_format = state->depth_format;
    plan.backend.depth_compare = state->depth_compare;
    plan.backend.depth_write_enabled = state->depth_write_enabled;
    plan.backend.blend_enabled = state->blend_enabled;
    plan.backend.source_color_factor = state->source_color_factor;
    plan.backend.destination_color_factor = state->destination_color_factor;
    plan.backend.color_operation = state->color_operation;
    plan.backend.source_alpha_factor = state->source_alpha_factor;
    plan.backend.destination_alpha_factor = state->destination_alpha_factor;
    plan.backend.alpha_operation = state->alpha_operation;
    plan.backend.blend_constant_red = state->blend_constant_red;
    plan.backend.blend_constant_green = state->blend_constant_green;
    plan.backend.blend_constant_blue = state->blend_constant_blue;
    plan.backend.blend_constant_alpha = state->blend_constant_alpha;
    plan.backend.color_write_mask = state->color_write_mask;
    plan.backend.cull_mode = state->cull_mode;
    plan.backend.front_face = state->front_face;
    plan.backend.position_output_location = state->position_output_location;

    for (uint32_t index = 0u; index < vertex_binding_count; ++index) {
        plan.backend.vertex_bindings[index].binding =
            vertex_bindings[index].binding;
        plan.backend.vertex_bindings[index].stride = vertex_bindings[index].stride;
        plan.backend.vertex_bindings[index].flags = vertex_bindings[index].divisor;
        plan.backend.vertex_bindings[index].reserved = 0u;
        if (index == 0u) plan.backend.vertex_stride = vertex_bindings[index].stride;
    }
    for (uint32_t index = 0u; index < vertex->input_count; ++index) {
        const RinSpirvIoV1* input = &vertex->inputs[index];
        const RinGpuVulkanVertexInputAttributeV1* attribute = NULL;
        uint32_t component;
        for (uint32_t candidate = 0u; candidate < vertex_attribute_count;
             ++candidate)
            if (vertex_attributes[candidate].location == input->location) {
                attribute = &vertex_attributes[candidate];
                break;
            }
        if (!attribute) return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        if (attribute_count > RIN_GPU_MAX_VERTEX_ATTRIBUTES - input->width)
            return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
        if (input->location > RIN_SHADER_MAX_IO - input->width)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        for (component = 0u; component < input->width; ++component) {
            RinGpuGraphicsPipelineBackendVertexAttributeV1* output =
                &plan.backend.vertex_attributes[attribute_count++];
            output->location = input->location + component;
            output->format = attribute->format;
            output->offset = attribute->offset + component * 4u;
            output->flags = attribute->flags;
            output->binding = attribute->binding;
        }
    }
    plan.backend.vertex_input_count = attribute_count;

    for (output_index = 0u; output_index < fragment->input_count;
         ++output_index) {
        const RinSpirvIoV1* input = &fragment->inputs[output_index];
        const RinSpirvIoV1* output =
            find_io(vertex->outputs, vertex->output_count, input->location);
        if (!output || output->width != input->width ||
            output->base_type != input->base_type || input->width == 0u ||
            input->width > 4u)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        if (input->location == state->position_output_location) continue;
        if (varying_count > RIN_GPU_MAX_VARYINGS - input->width)
            return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
        if (input->location > RIN_SHADER_MAX_IO - input->width)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        for (uint32_t component = 0u; component < input->width; ++component) {
            RinGpuGraphicsPipelineBackendVaryingV1* varying =
                &plan.backend.varyings[varying_count++];
            varying->vertex_output_location = output->location + component;
            varying->fragment_input_location = input->location + component;
            varying->type = input->base_type == 2u
                                ? RIN_GPU_VARYING_FLOAT32
                                : RIN_GPU_VARYING_SINT32;
            varying->interpolation = RIN_GPU_INTERPOLATION_PERSPECTIVE;
        }
    }
    plan.backend.varying_count = varying_count;
    memset(resource_kinds, 0, sizeof(resource_kinds));
    memset(resource_sets, 0, sizeof(resource_sets));
    memset(resource_bindings, 0, sizeof(resource_bindings));
    if (!descriptor_kind_merge(resource_kinds, resource_sets,
                               resource_bindings, &resource_count, vertex) ||
        !descriptor_kind_merge(resource_kinds, resource_sets,
                               resource_bindings, &resource_count, fragment))
        return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    plan.backend.resource_count = resource_count;
    for (uint32_t index = 0u; index < resource_count; ++index) {
        if (resource_kinds[index] == RIN_SHADER_RESOURCE_NONE)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        plan.backend.resource_kinds[index] = resource_kinds[index];
    }
    *plan_out = plan;
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

static const RinGpuVulkanDescriptorSetLayoutBindingV1* find_layout(
    const RinGpuVulkanDescriptorSetLayoutBindingV1* layouts,
    uint32_t layout_count, uint32_t set, uint32_t binding) {
    uint32_t index;
    for (index = 0u; index < layout_count; ++index)
        if (layouts[index].set == set && layouts[index].binding == binding)
            return &layouts[index];
    return NULL;
}

static const RinGpuVulkanDescriptorWriteV1* find_write(
    const RinGpuVulkanDescriptorWriteV1* writes, uint32_t write_count,
    uint32_t resource_index) {
    uint32_t index;
    for (index = 0u; index < write_count; ++index)
        if (writes[index].resource_index == resource_index) return &writes[index];
    return NULL;
}

static int descriptor_writes_unique(
    const RinGpuVulkanDescriptorWriteV1* writes, uint32_t write_count,
    uint32_t resource_count) {
    uint32_t index;

    for (index = 0u; index < write_count; ++index) {
        if (writes[index].resource_index >= resource_count ||
            writes[index].set == UINT32_MAX ||
            writes[index].binding == UINT32_MAX)
            return 0;
        for (uint32_t prior = 0u; prior < index; ++prior) {
            if (writes[prior].resource_index == writes[index].resource_index ||
                (writes[prior].set == writes[index].set &&
                 writes[prior].binding == writes[index].binding &&
                 writes[prior].array_element == writes[index].array_element))
                return 0;
        }
    }
    return 1;
}

static int descriptor_metadata_collect(
    uint32_t* kinds, uint32_t* sets, uint32_t* bindings, uint32_t* count,
    const RinSpirvTranslationInfoV1* info) {
    return descriptor_kind_merge(kinds, sets, bindings, count, info);
}

int ringpu_vulkan_graphics_build_descriptor_set(
    const RinSpirvTranslationInfoV1* vertex,
    const RinSpirvTranslationInfoV1* fragment,
    const RinGpuVulkanDescriptorSetLayoutBindingV1* layouts,
    uint32_t layout_count,
    const RinGpuVulkanDescriptorWriteV1* writes,
    uint32_t write_count,
    RinGpuVulkanDescriptorSetPlanV1* plan_out) {
    uint32_t kinds[RIN_SHADER_MAX_RESOURCES];
    uint32_t sets[RIN_SHADER_MAX_RESOURCES];
    uint32_t bindings[RIN_SHADER_MAX_RESOURCES];
    uint32_t resource_count = 0u;
    uint32_t index;
    RinGpuVulkanDescriptorSetPlanV1 plan;

    if (!plan_out || (layout_count != 0u && !layouts) ||
        (write_count != 0u && !writes) ||
        layout_count > RIN_SHADER_MAX_RESOURCES ||
        write_count > RIN_SHADER_MAX_RESOURCES ||
        !translation_info_valid(vertex, RIN_SHADER_STAGE_VERTEX) ||
        !translation_info_valid(fragment, RIN_SHADER_STAGE_FRAGMENT))
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    memset(kinds, 0, sizeof(kinds));
    memset(sets, 0, sizeof(sets));
    memset(bindings, 0, sizeof(bindings));
    if (!descriptor_metadata_collect(kinds, sets, bindings, &resource_count,
                                     vertex) ||
        !descriptor_metadata_collect(kinds, sets, bindings, &resource_count,
                                     fragment))
        return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    if (write_count != resource_count ||
        !descriptor_writes_unique(writes, write_count, resource_count))
        return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    for (index = 0u; index < layout_count; ++index) {
        if (descriptor_resource_kind(layouts[index].descriptor_type) ==
                RIN_SHADER_RESOURCE_NONE ||
            layouts[index].set == UINT32_MAX ||
            layouts[index].binding == UINT32_MAX ||
            layouts[index].descriptor_count == 0u ||
            layouts[index].descriptor_count > RIN_SHADER_MAX_RESOURCES ||
            layouts[index].stage_flags == 0u || layouts[index].reserved != 0u ||
            (find_layout(layouts, index, layouts[index].set,
                         layouts[index].binding) != NULL))
            return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    }
    memset(&plan, 0, sizeof(plan));
    plan.struct_size = sizeof(plan);
    plan.version = RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION;
    plan.binding_count = resource_count;
    for (index = 0u; index < resource_count; ++index) {
        const RinGpuVulkanDescriptorSetLayoutBindingV1* layout;
        const RinGpuVulkanDescriptorWriteV1* write;
        RinGpuGraphicsBindingV1* binding;
        if (kinds[index] == RIN_SHADER_RESOURCE_NONE)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        layout = find_layout(layouts, layout_count, sets[index], bindings[index]);
        write = find_write(writes, write_count, index);
        if (!layout || descriptor_resource_kind(layout->descriptor_type) !=
                          kinds[index] || !write ||
            write->set != sets[index] || write->binding != bindings[index] ||
            write->array_element >= layout->descriptor_count ||
            write->descriptor_type != layout->descriptor_type ||
            descriptor_resource_kind(write->descriptor_type) != kinds[index] ||
            write->resource == 0u ||
            write->reserved != 0u ||
            (write->flags & ~RIN_GPU_GRAPHICS_BINDING_KNOWN_FLAGS) != 0u)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        if (kinds[index] == RIN_SHADER_RESOURCE_STORAGE_BUFFER) {
            if (write->access == 0u ||
                (write->access & ~RIN_GPU_RESOURCE_KNOWN_ACCESS) != 0u ||
                write->mip_level != 0u || write->array_layer != 0u)
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
            if (write->descriptor_type ==
                    RIN_GPU_VULKAN_DESCRIPTOR_UNIFORM_BUFFER &&
                write->access != RIN_GPU_RESOURCE_READ)
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        } else if (kinds[index] == RIN_SHADER_RESOURCE_STORAGE_IMAGE) {
            if (write->access == 0u ||
                (write->access & ~RIN_GPU_RESOURCE_KNOWN_ACCESS) != 0u ||
                write->offset != 0u || write->size_bytes != 0u ||
                write->flags != 0u)
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        } else if (kinds[index] == RIN_SHADER_RESOURCE_SAMPLED_IMAGE ||
                   kinds[index] == RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE) {
            if (write->access != RIN_GPU_RESOURCE_READ || write->offset != 0u ||
                write->size_bytes != 0u ||
                (kinds[index] == RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE &&
                 write->flags != 0u))
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        } else if (write->access != 0u || write->offset != 0u ||
                   write->size_bytes != 0u || write->mip_level != 0u ||
                   write->array_layer != 0u || write->flags != 0u) {
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        }
        binding = &plan.bindings[index];
        binding->abi_version = RIN_GPU_ABI_VERSION;
        binding->struct_size = sizeof(*binding);
        binding->binding = index;
        binding->kind = kinds[index];
        binding->access = write->access;
        binding->flags = write->flags;
        binding->resource = write->resource;
        binding->offset = write->offset;
        binding->size_bytes = write->size_bytes;
        binding->mip_level = write->mip_level;
        binding->array_layer = write->array_layer;
        plan.set_count = sets[index] + 1u > plan.set_count
                             ? sets[index] + 1u
                             : plan.set_count;
    }
    *plan_out = plan;
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}
