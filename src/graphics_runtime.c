/* SPDX-License-Identifier: MIT */

#include <rinvulkan/graphics_runtime.h>

#include <string.h>

static int runtime_valid(const RinGpuVulkanGraphicsRuntimeV1* runtime)
{
    return runtime != NULL && runtime->struct_size == sizeof(*runtime) &&
           runtime->version == RIN_GPU_VULKAN_GRAPHICS_RUNTIME_VERSION &&
           runtime->initialized == 1u && runtime->runtime != NULL &&
           runtime->queue != 0u;
}

static int plan_valid(const RinGpuVulkanGraphicsPipelinePlanV1* plan)
{
    return plan != NULL && plan->struct_size == sizeof(*plan) &&
           plan->version == RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION &&
           plan->backend.vertex_input_count <= RIN_GPU_MAX_VERTEX_ATTRIBUTES &&
           plan->backend.vertex_binding_count <= RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS &&
           plan->backend.varying_count <= RIN_GPU_MAX_VARYINGS &&
           plan->backend.resource_count <= RIN_SHADER_MAX_RESOURCES;
}

int rin_gpu_vulkan_graphics_runtime_init(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    uint32_t queue_capabilities)
{
    RinGpuQueueDescV1 queue_desc;
    int result;

    if (!runtime || !surface || queue_capabilities == 0u ||
        (queue_capabilities & ~RIN_GPU_QUEUE_KNOWN_CAPABILITIES) != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    runtime->struct_size = sizeof(*runtime);
    runtime->version = RIN_GPU_VULKAN_GRAPHICS_RUNTIME_VERSION;
    result = ringpu_runtime_software_surface_create(surface,
                                                     &runtime->runtime);
    if (result != RIN_GPU_OK) {
        memset(runtime, 0, sizeof(*runtime));
        return result;
    }
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.abi_version = RIN_GPU_ABI_VERSION;
    queue_desc.struct_size = sizeof(queue_desc);
    queue_desc.capabilities = queue_capabilities;
    result = ringpu_runtime_create_queue(runtime->runtime, &queue_desc,
                                         &runtime->queue);
    if (result != RIN_GPU_OK) {
        ringpu_runtime_destroy(runtime->runtime);
        memset(runtime, 0, sizeof(*runtime));
        return result;
    }
    runtime->initialized = 1u;
    return RIN_GPU_OK;
}

int rin_gpu_vulkan_graphics_runtime_shutdown(
    RinGpuVulkanGraphicsRuntimeV1* runtime)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    ringpu_runtime_destroy(runtime->runtime);
    memset(runtime, 0, sizeof(*runtime));
    return RIN_GPU_OK;
}

int rin_gpu_vulkan_graphics_runtime_create_buffer(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuBufferDescV1* descriptor, RinGpuHandle* buffer_out)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_buffer(runtime->runtime, descriptor,
                                        buffer_out);
}

int rin_gpu_vulkan_graphics_runtime_create_image(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuImageDescV1* descriptor, RinGpuHandle* image_out)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_image(runtime->runtime, descriptor, image_out);
}

int rin_gpu_vulkan_graphics_runtime_create_memory(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuMemoryDescV1* descriptor, RinGpuHandle* memory_out)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_memory(runtime->runtime, descriptor,
                                        memory_out);
}

int rin_gpu_vulkan_graphics_runtime_bind_buffer_memory(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle buffer,
    const RinGpuResourceMemoryBindingV1* binding)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_bind_buffer_memory(runtime->runtime, buffer, binding);
}

int rin_gpu_vulkan_graphics_runtime_bind_image_memory(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle image,
    const RinGpuResourceMemoryBindingV1* binding)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_bind_image_memory(runtime->runtime, image, binding);
}

int rin_gpu_vulkan_graphics_runtime_upload_buffer(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle buffer,
    uint64_t destination_offset, const void* source, uint64_t size_bytes)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_upload_buffer(runtime->runtime, buffer,
                                        destination_offset, source, size_bytes);
}

int rin_gpu_vulkan_graphics_runtime_upload_image(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle image,
    const RinGpuImageUploadV1* upload, const void* source,
    uint64_t source_size)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_upload_image(runtime->runtime, image, upload, source,
                                       source_size);
}

static int create_shader_pair(
    RinGpuVulkanGraphicsRuntimeV1* runtime, const void* vertex_shader_ir,
    uint64_t vertex_shader_size, const void* fragment_shader_ir,
    uint64_t fragment_shader_size, RinGpuHandle* vertex_shader,
    RinGpuHandle* fragment_shader)
{
    int result;
    if (!runtime_valid(runtime) || !vertex_shader_ir || !fragment_shader_ir ||
        vertex_shader_size == 0u || fragment_shader_size == 0u ||
        !vertex_shader || !fragment_shader)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    *vertex_shader = 0u;
    *fragment_shader = 0u;
    result = ringpu_runtime_create_shader_module(
        runtime->runtime, vertex_shader_ir, vertex_shader_size, vertex_shader);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_create_shader_module(
        runtime->runtime, fragment_shader_ir, fragment_shader_size,
        fragment_shader);
    if (result != RIN_GPU_OK) {
        (void)ringpu_runtime_destroy_object(runtime->runtime, *vertex_shader);
        *vertex_shader = 0u;
    }
    return result;
}

int rin_gpu_vulkan_graphics_runtime_create_graphics_pipeline(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuVulkanGraphicsPipelinePlanV1* plan,
    const void* vertex_shader_ir, uint64_t vertex_shader_size,
    const void* fragment_shader_ir, uint64_t fragment_shader_size,
    RinGpuHandle* pipeline_out)
{
    RinGpuGraphicsPipelineNativeDescV2 descriptor;
    RinGpuVertexAttributeV2 attributes[RIN_GPU_MAX_VERTEX_ATTRIBUTES];
    RinGpuVertexBufferLayoutV1 bindings[RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS];
    RinGpuVaryingV1 varyings[RIN_GPU_MAX_VARYINGS];
    RinGpuHandle vertex_shader = 0u;
    RinGpuHandle fragment_shader = 0u;
    uint32_t index;
    int result;

    if (!runtime_valid(runtime) || !plan_valid(plan) || !pipeline_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    *pipeline_out = 0u;
    result = create_shader_pair(runtime, vertex_shader_ir, vertex_shader_size,
                                fragment_shader_ir, fragment_shader_size,
                                &vertex_shader, &fragment_shader);
    if (result != RIN_GPU_OK) return result;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.base.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.base.struct_size = sizeof(descriptor);
    descriptor.base.vertex_shader = vertex_shader;
    descriptor.base.fragment_shader = fragment_shader;
    descriptor.base.color_format = plan->backend.color_format;
    descriptor.base.primitive_topology = plan->backend.primitive_topology;
    descriptor.base.vertex_stride = 0u;
    descriptor.base.position_output_location =
        plan->backend.position_output_location;
    descriptor.base.depth_format = plan->backend.depth_format;
    descriptor.base.depth_compare = plan->backend.depth_compare;
    descriptor.base.depth_write_enabled = plan->backend.depth_write_enabled;
    descriptor.base.blend_enabled = plan->backend.blend_enabled;
    descriptor.base.source_color_factor = plan->backend.source_color_factor;
    descriptor.base.destination_color_factor =
        plan->backend.destination_color_factor;
    descriptor.base.color_operation = plan->backend.color_operation;
    descriptor.base.source_alpha_factor = plan->backend.source_alpha_factor;
    descriptor.base.destination_alpha_factor =
        plan->backend.destination_alpha_factor;
    descriptor.base.alpha_operation = plan->backend.alpha_operation;
    descriptor.base.color_write_mask = plan->backend.color_write_mask;
    descriptor.base.cull_mode = plan->backend.cull_mode;
    descriptor.base.front_face = plan->backend.front_face;
    descriptor.base.stencil_test_enabled = plan->backend.stencil_test_enabled;
    descriptor.base.stencil_compare = plan->backend.stencil_compare;
    descriptor.base.stencil_reference = plan->backend.stencil_reference;
    descriptor.base.stencil_read_mask = plan->backend.stencil_read_mask;
    descriptor.base.stencil_write_mask = plan->backend.stencil_write_mask;
    descriptor.base.stencil_fail_operation =
        plan->backend.stencil_fail_operation;
    descriptor.base.stencil_depth_fail_operation =
        plan->backend.stencil_depth_fail_operation;
    descriptor.base.stencil_pass_operation =
        plan->backend.stencil_pass_operation;
    descriptor.base.separate_stencil_enabled =
        plan->backend.separate_stencil_enabled;
    descriptor.base.back_stencil_compare = plan->backend.back_stencil_compare;
    descriptor.base.back_stencil_reference =
        plan->backend.back_stencil_reference;
    descriptor.base.back_stencil_read_mask =
        plan->backend.back_stencil_read_mask;
    descriptor.base.back_stencil_write_mask =
        plan->backend.back_stencil_write_mask;
    descriptor.base.back_stencil_fail_operation =
        plan->backend.back_stencil_fail_operation;
    descriptor.base.back_stencil_depth_fail_operation =
        plan->backend.back_stencil_depth_fail_operation;
    descriptor.base.back_stencil_pass_operation =
        plan->backend.back_stencil_pass_operation;
    descriptor.blend_constant_red = plan->backend.blend_constant_red;
    descriptor.blend_constant_green = plan->backend.blend_constant_green;
    descriptor.blend_constant_blue = plan->backend.blend_constant_blue;
    descriptor.blend_constant_alpha = plan->backend.blend_constant_alpha;

    for (index = 0u; index < plan->backend.vertex_input_count; ++index) {
        attributes[index].abi_version = RIN_GPU_ABI_VERSION;
        attributes[index].struct_size = sizeof(attributes[index]);
        attributes[index].location =
            plan->backend.vertex_attributes[index].location;
        attributes[index].format = plan->backend.vertex_attributes[index].format;
        attributes[index].offset = plan->backend.vertex_attributes[index].offset;
        attributes[index].flags = plan->backend.vertex_attributes[index].flags;
        attributes[index].reserved0 = 0u;
        attributes[index].reserved1 = 0u;
        attributes[index].binding =
            plan->backend.vertex_attributes[index].binding;
        attributes[index].reserved2 = 0u;
    }
    for (index = 0u; index < plan->backend.vertex_binding_count; ++index)
        bindings[index] = plan->backend.vertex_bindings[index];
    for (index = 0u; index < plan->backend.varying_count; ++index) {
        varyings[index].abi_version = RIN_GPU_ABI_VERSION;
        varyings[index].struct_size = sizeof(varyings[index]);
        varyings[index].vertex_output_location =
            plan->backend.varyings[index].vertex_output_location;
        varyings[index].fragment_input_location =
            plan->backend.varyings[index].fragment_input_location;
        varyings[index].type = plan->backend.varyings[index].type;
        varyings[index].interpolation = plan->backend.varyings[index].interpolation;
        varyings[index].flags = 0u;
        varyings[index].reserved0 = 0u;
        varyings[index].reserved1 = 0u;
    }
    if (plan->backend.vertex_binding_count == 0u) {
        RinGpuVertexAttributeV1 legacy_attributes[RIN_GPU_MAX_VERTEX_ATTRIBUTES];
        for (index = 0u; index < plan->backend.vertex_input_count; ++index) {
            legacy_attributes[index].abi_version = RIN_GPU_ABI_VERSION;
            legacy_attributes[index].struct_size = sizeof(legacy_attributes[index]);
            legacy_attributes[index].location = attributes[index].location;
            legacy_attributes[index].format = attributes[index].format;
            legacy_attributes[index].offset = attributes[index].offset;
            legacy_attributes[index].flags = attributes[index].flags;
            legacy_attributes[index].reserved0 = 0u;
            legacy_attributes[index].reserved1 = 0u;
        }
        result = ringpu_runtime_create_graphics_pipeline_native_v2(
            runtime->runtime,
            &descriptor,
            plan->backend.vertex_input_count != 0u ? legacy_attributes : NULL,
            plan->backend.vertex_input_count,
            plan->backend.varying_count != 0u ? varyings : NULL,
            plan->backend.varying_count, pipeline_out);
    } else {
        result = ringpu_runtime_create_graphics_pipeline_native_vertex_bindings_v2(
            runtime->runtime, &descriptor, attributes,
            plan->backend.vertex_input_count, bindings,
            plan->backend.vertex_binding_count,
            plan->backend.varying_count != 0u ? varyings : NULL,
            plan->backend.varying_count, pipeline_out);
    }
    if (result != RIN_GPU_OK) {
        (void)ringpu_runtime_destroy_object(runtime->runtime, vertex_shader);
        (void)ringpu_runtime_destroy_object(runtime->runtime, fragment_shader);
    }
    return result;
}

int rin_gpu_vulkan_graphics_runtime_create_compute_pipeline(
    RinGpuVulkanGraphicsRuntimeV1* runtime, const void* shader_ir,
    uint64_t shader_size, RinGpuHandle* pipeline_out)
{
    RinGpuComputePipelineDescV1 descriptor;
    RinGpuHandle shader = 0u;
    int result;
    if (!runtime_valid(runtime) || !shader_ir || shader_size == 0u ||
        !pipeline_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    *pipeline_out = 0u;
    result = ringpu_runtime_create_shader_module(runtime->runtime, shader_ir,
                                                 shader_size, &shader);
    if (result != RIN_GPU_OK) return result;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.shader_module = shader;
    result = ringpu_runtime_create_compute_pipeline(runtime->runtime,
                                                    &descriptor, pipeline_out);
    if (result != RIN_GPU_OK)
        (void)ringpu_runtime_destroy_object(runtime->runtime, shader);
    return result;
}

int rin_gpu_vulkan_graphics_runtime_create_graphics_bind_group(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle pipeline,
    const RinGpuVulkanDescriptorSetPlanV1* plan, RinGpuHandle* bind_group_out)
{
    if (!runtime_valid(runtime) || !plan || plan->struct_size != sizeof(*plan) ||
        plan->version != RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION ||
        plan->binding_count > RIN_SHADER_MAX_RESOURCES || !bind_group_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_create_graphics_bind_group_typed(
        runtime->runtime, pipeline, plan->bindings, plan->binding_count,
        bind_group_out);
}

int rin_gpu_vulkan_graphics_runtime_create_compute_bind_group(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle pipeline,
    const RinGpuBufferBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out)
{
    if (!runtime_valid(runtime) || !bind_group_out) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_compute_bind_group(
        runtime->runtime, pipeline, bindings, binding_count, bind_group_out);
}

int rin_gpu_vulkan_graphics_runtime_create_command_list(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle* command_list_out)
{
    RinGpuCommandListDescV1 descriptor;
    if (!runtime_valid(runtime) || !command_list_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.capabilities = RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                              RIN_GPU_QUEUE_GRAPHICS;
    return ringpu_runtime_create_command_list(runtime->runtime, &descriptor,
                                              command_list_out);
}

int rin_gpu_vulkan_graphics_runtime_reset_command_list(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_list_reset(runtime->runtime, command_list);
}

int rin_gpu_vulkan_graphics_runtime_begin_render_pass(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassDescV1* render_pass)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_begin_render_pass(runtime->runtime,
                                                     command_list, render_pass);
}

int rin_gpu_vulkan_graphics_runtime_begin_render_pass_mrt(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassMrtDescV1* render_pass)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_begin_render_pass_mrt(
        runtime->runtime, command_list, render_pass);
}

int rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassDepthDescV1* render_pass)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_begin_render_pass_depth(
        runtime->runtime, command_list, render_pass);
}

int rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth_stencil(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassDepthStencilDescV1* render_pass)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_begin_render_pass_depth_stencil(
        runtime->runtime, command_list, render_pass);
}

int rin_gpu_vulkan_graphics_runtime_begin_dynamic_rendering(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuVulkanDynamicRenderingV1* rendering)
{
    RinGpuRenderPassDepthDescV1 depth_pass;
    RinGpuRenderPassDepthStencilDescV1 depth_stencil_pass;

    if (!runtime_valid(runtime) || !rendering ||
        rendering->abi_version != RIN_GPU_ABI_VERSION ||
        rendering->struct_size != sizeof(*rendering) ||
        rendering->active_color_mask == 0u ||
        (rendering->active_color_mask & ~((1u << RIN_GPU_MAX_COLOR_TARGETS) - 1u)) != 0u ||
        rendering->reserved0 != 0u || rendering->reserved1 != 0u ||
        rendering->reserved2 != 0u || rendering->clear_region.reserved != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (rendering->stencil_target != 0u && rendering->depth_target == 0u)
        return RIN_GPU_ERROR_UNSUPPORTED;
    if (rendering->depth_target != 0u && rendering->active_color_mask != 1u)
        return RIN_GPU_ERROR_UNSUPPORTED;
    if (rendering->depth_target == 0u && rendering->stencil_target == 0u)
        return rin_gpu_vulkan_graphics_runtime_begin_render_pass_mrt(
            runtime, command_list, rendering);
    if (rendering->stencil_target == 0u) {
        memset(&depth_pass, 0, sizeof(depth_pass));
        depth_pass.abi_version = RIN_GPU_ABI_VERSION;
        depth_pass.struct_size = sizeof(depth_pass);
        depth_pass.color_target = rendering->color_attachments[0].target;
        depth_pass.depth_target = rendering->depth_target;
        depth_pass.color_mip_level = rendering->color_attachments[0].mip_level;
        depth_pass.color_array_layer = rendering->color_attachments[0].array_layer;
        depth_pass.depth_mip_level = rendering->depth_mip_level;
        depth_pass.depth_array_layer = rendering->depth_array_layer;
        depth_pass.color_load_op = rendering->color_load_op;
        depth_pass.color_store_op = rendering->color_store_op;
        depth_pass.depth_load_op = rendering->depth_load_op;
        depth_pass.depth_store_op = rendering->depth_store_op;
        depth_pass.clear_red = rendering->clear_red;
        depth_pass.clear_green = rendering->clear_green;
        depth_pass.clear_blue = rendering->clear_blue;
        depth_pass.clear_alpha = rendering->clear_alpha;
        depth_pass.clear_depth = rendering->clear_depth;
        depth_pass.color_write_mask = rendering->color_write_mask;
        depth_pass.clear_region = rendering->clear_region;
        return rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth(
            runtime, command_list, &depth_pass);
    }
    memset(&depth_stencil_pass, 0, sizeof(depth_stencil_pass));
    depth_stencil_pass.abi_version = RIN_GPU_ABI_VERSION;
    depth_stencil_pass.struct_size = sizeof(depth_stencil_pass);
    depth_stencil_pass.color_target = rendering->color_attachments[0].target;
    depth_stencil_pass.depth_target = rendering->depth_target;
    depth_stencil_pass.stencil_target = rendering->stencil_target;
    depth_stencil_pass.color_mip_level = rendering->color_attachments[0].mip_level;
    depth_stencil_pass.color_array_layer = rendering->color_attachments[0].array_layer;
    depth_stencil_pass.depth_mip_level = rendering->depth_mip_level;
    depth_stencil_pass.depth_array_layer = rendering->depth_array_layer;
    depth_stencil_pass.stencil_mip_level = rendering->stencil_mip_level;
    depth_stencil_pass.stencil_array_layer = rendering->stencil_array_layer;
    depth_stencil_pass.color_load_op = rendering->color_load_op;
    depth_stencil_pass.color_store_op = rendering->color_store_op;
    depth_stencil_pass.depth_load_op = rendering->depth_load_op;
    depth_stencil_pass.depth_store_op = rendering->depth_store_op;
    depth_stencil_pass.stencil_load_op = rendering->stencil_load_op;
    depth_stencil_pass.stencil_store_op = rendering->stencil_store_op;
    depth_stencil_pass.clear_red = rendering->clear_red;
    depth_stencil_pass.clear_green = rendering->clear_green;
    depth_stencil_pass.clear_blue = rendering->clear_blue;
    depth_stencil_pass.clear_alpha = rendering->clear_alpha;
    depth_stencil_pass.clear_depth = rendering->clear_depth;
    depth_stencil_pass.clear_stencil = rendering->clear_stencil;
    depth_stencil_pass.stencil_write_mask = rendering->stencil_write_mask;
    depth_stencil_pass.color_write_mask = rendering->color_write_mask;
    depth_stencil_pass.clear_region = rendering->clear_region;
    return rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth_stencil(
        runtime, command_list, &depth_stencil_pass);
}

int rin_gpu_vulkan_graphics_runtime_bind_graphics_resources(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    RinGpuHandle bind_group)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_bind_graphics_resources(
        runtime->runtime, command_list, bind_group);
}

int rin_gpu_vulkan_graphics_runtime_draw(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawV1* draw)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_draw(runtime->runtime, command_list, draw);
}

int rin_gpu_vulkan_graphics_runtime_draw_vertices(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawVerticesV2* draw)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_draw_vertices_v2(runtime->runtime,
                                                   command_list, draw);
}

int rin_gpu_vulkan_graphics_runtime_draw_indexed(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawIndexedV2* draw)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_draw_indexed_v2(runtime->runtime,
                                                  command_list, draw);
}

int rin_gpu_vulkan_graphics_runtime_set_raster_state(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRasterStateV1* state)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_set_raster_state(runtime->runtime,
                                                   command_list, state);
}

int rin_gpu_vulkan_graphics_runtime_dispatch(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDispatchV1* dispatch)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_dispatch(runtime->runtime, command_list,
                                            dispatch);
}

int rin_gpu_vulkan_graphics_runtime_end_render_pass(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_end_render_pass(runtime->runtime,
                                                  command_list);
}

int rin_gpu_vulkan_graphics_runtime_close_command_list(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_list_close(runtime->runtime, command_list);
}

int rin_gpu_vulkan_graphics_runtime_submit(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    RinGpuHandle fence, uint64_t signal_value)
{
    RinGpuSubmitInfoV1 submit;
    if (!runtime_valid(runtime) || command_list == 0u || fence == 0u ||
        signal_value == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&submit, 0, sizeof(submit));
    submit.abi_version = RIN_GPU_ABI_VERSION;
    submit.struct_size = sizeof(submit);
    submit.command_list = command_list;
    submit.signal_fence = fence;
    submit.signal_value = signal_value;
    return ringpu_runtime_queue_submit(runtime->runtime, runtime->queue,
                                       &submit);
}

int rin_gpu_vulkan_graphics_runtime_wait_fence(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle fence,
    uint64_t signal_value, uint64_t timeout_ns)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_wait_fence(runtime->runtime, fence, signal_value,
                                     timeout_ns);
}

int rin_gpu_vulkan_graphics_runtime_readback_image(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle image,
    const RinGpuImageReadbackV1* readback, void* destination,
    uint64_t destination_size)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_readback_image(runtime->runtime, image, readback,
                                         destination, destination_size);
}

int rin_gpu_vulkan_graphics_runtime_destroy_object(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle object)
{
    if (!runtime_valid(runtime)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_destroy_object(runtime->runtime, object);
}
