/* SPDX-License-Identifier: MIT */

#include <rinvulkan/graphics_runtime.h>

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", \
                                __FILE__, __LINE__, #condition); return 1; } \
} while (0)

typedef struct ShaderBlob {
    RinShaderHeaderV1 header;
    RinShaderInstructionV1 instructions[17];
} ShaderBlob;

static void instruction(RinShaderInstructionV1* out, uint16_t opcode,
                        uint16_t destination, uint16_t source0,
                        uint16_t source1, uint16_t resource,
                        uint32_t immediate)
{
    memset(out, 0, sizeof(*out));
    out->opcode = opcode;
    out->destination = destination;
    out->source0 = source0;
    out->source1 = source1;
    out->resource = resource;
    out->immediate = immediate;
}

static uint32_t f32_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void make_constant_vertex(ShaderBlob* shader)
{
    static const float position[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_VERTEX;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                    (uint16_t)index, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                    RIN_SHADER_UNUSED, f32_bits(position[index]));
        instruction(&shader->instructions[4u + index],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)index, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                    index);
    }
    instruction(&shader->instructions[8], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                RIN_SHADER_UNUSED, 0u);
}

static void make_streamed_vertex(ShaderBlob* shader)
{
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_VERTEX;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.input_count = 1u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    instruction(&shader->instructions[0], RIN_SHADER_OP_LOAD_INPUT_F32,
                0u, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                0u);
    instruction(&shader->instructions[1], RIN_SHADER_OP_CONST_F32, 1u,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                f32_bits(0.0f));
    instruction(&shader->instructions[2], RIN_SHADER_OP_CONST_F32, 2u,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                f32_bits(0.0f));
    instruction(&shader->instructions[3], RIN_SHADER_OP_CONST_F32, 3u,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                f32_bits(1.0f));
    instruction(&shader->instructions[4], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 0u, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                0u);
    instruction(&shader->instructions[5], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 1u, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                1u);
    instruction(&shader->instructions[6], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 2u, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                2u);
    instruction(&shader->instructions[7], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 3u, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                3u);
    instruction(&shader->instructions[8], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                RIN_SHADER_UNUSED, 0u);
}

static void make_constant_fragment(ShaderBlob* shader)
{
    static const float color[8] = {
        1.0f, 0.25f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.25f, 1.0f
    };
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_FRAGMENT;
    shader->header.instruction_count = 17u;
    shader->header.register_count = 8u;
    shader->header.output_count = 8u;
    shader->header.total_size = sizeof(shader->header) +
                                17u * sizeof(shader->instructions[0]);
    for (index = 0u; index < 8u; ++index) {
        instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                    (uint16_t)index, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                    RIN_SHADER_UNUSED, f32_bits(color[index]));
        instruction(&shader->instructions[8u + index],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)index, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                    index);
    }
    instruction(&shader->instructions[16], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                RIN_SHADER_UNUSED, 0u);
}

static void make_constant_compute(uint8_t* storage)
{
    RinShaderHeaderV1* header = (RinShaderHeaderV1*)storage;
    RinShaderInstructionV1* code =
        (RinShaderInstructionV1*)(storage + sizeof(*header));
    memset(storage, 0, sizeof(*header) + 4u * sizeof(*code));
    header->magic = RIN_SHADER_MAGIC;
    header->version = RIN_SHADER_IR_VERSION;
    header->header_size = sizeof(*header);
    header->stage = RIN_SHADER_STAGE_COMPUTE;
    header->instruction_count = 4u;
    header->register_count = 2u;
    header->resource_count = 1u;
    header->workgroup_x = 1u;
    header->workgroup_y = 1u;
    header->workgroup_z = 1u;
    header->total_size = sizeof(*header) + 4u * sizeof(*code);
    instruction(&code[0], RIN_SHADER_OP_CONST_I32, 0u, RIN_SHADER_UNUSED,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
    instruction(&code[1], RIN_SHADER_OP_CONST_I32, 1u, RIN_SHADER_UNUSED,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 42u);
    instruction(&code[2], RIN_SHADER_OP_STORE_RESOURCE_I32,
                RIN_SHADER_UNUSED, 0u, 1u, 0u, 0u);
    instruction(&code[3], RIN_SHADER_OP_RETURN, RIN_SHADER_UNUSED,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
}

static int present(void* context, const RinGpuSoftwarePresentedImageV1* image)
{
    uint8_t* destination = (uint8_t*)context;
    if (!destination || !image || !image->pixels || image->size_bytes != 64u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memcpy(destination, image->pixels, 64u);
    return RIN_GPU_OK;
}

static int acquire(void* context, const RinGpuImageDescV1* descriptor,
                   uint64_t allocation_bytes,
                   RinGpuSoftwareExternalImageV1* storage)
{
    (void)context;
    (void)descriptor;
    (void)allocation_bytes;
    if (!storage) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(storage, 0, sizeof(*storage));
    return RIN_GPU_OK;
}

static void make_surface(RinGpuRuntimeSoftwareSurfaceDescV1* surface,
                         uint8_t* presented)
{
    memset(surface, 0, sizeof(*surface));
    surface->struct_size = sizeof(*surface);
    surface->version = RIN_GPU_RUNTIME_VERSION;
    surface->device_generation = 1u;
    surface->handle_secret = UINT64_C(0x564b475241504831);
    surface->max_buffer_size = 1024u * 1024u;
    surface->max_image_size = 1024u * 1024u;
    surface->max_total_allocation_size = 4u * 1024u * 1024u;
    surface->max_image_dimension = 64u;
    surface->max_image_layers = 1u;
    surface->max_image_mip_levels = 1u;
    surface->max_image_sample_count = 1u;
    surface->adapter.abi_version = RIN_GPU_ABI_VERSION;
    surface->adapter.struct_size = sizeof(surface->adapter);
    surface->adapter.queue_capabilities = RIN_GPU_QUEUE_COPY |
                                          RIN_GPU_QUEUE_COMPUTE |
                                          RIN_GPU_QUEUE_GRAPHICS;
    memcpy(surface->adapter.name, "vulkan-software", 16u);
    surface->display.abi_version = RIN_GPU_ABI_VERSION;
    surface->display.struct_size = sizeof(surface->display);
    surface->display.display_id = RIN_GPU_PRIMARY_DISPLAY;
    surface->display.flags = RIN_GPU_DISPLAY_CONNECTED |
                             RIN_GPU_DISPLAY_PRIMARY;
    surface->display.width = 4u;
    surface->display.height = 4u;
    surface->display.refresh_millihertz = 60000u;
    surface->display.format = RIN_GPU_FORMAT_RGBA8_UNORM;
    surface->display.scale_milli = 1000u;
    memcpy(surface->display.name, "vulkan-software", 16u);
    surface->present_callback = present;
    surface->present_context = presented;
    surface->acquire_image = acquire;
}

static int storage_image_descriptor_profile(void)
{
    RinSpirvTranslationInfoV1 vertex = {0};
    RinSpirvTranslationInfoV1 fragment = {0};
    RinGpuVulkanDescriptorSetLayoutBindingV1 layout = {0};
    RinGpuVulkanDescriptorWriteV1 write = {0};
    RinGpuVulkanDescriptorSetPlanV1 plan = {0};

    vertex.struct_size = sizeof(vertex);
    vertex.stage = RIN_SHADER_STAGE_VERTEX;
    vertex.shader.abi_version = RIN_GPU_ABI_VERSION;
    vertex.shader.struct_size = sizeof(vertex.shader);
    vertex.shader.stage = RIN_SHADER_STAGE_VERTEX;
    fragment.struct_size = sizeof(fragment);
    fragment.stage = RIN_SHADER_STAGE_FRAGMENT;
    fragment.shader.abi_version = RIN_GPU_ABI_VERSION;
    fragment.shader.struct_size = sizeof(fragment.shader);
    fragment.shader.stage = RIN_SHADER_STAGE_FRAGMENT;
    fragment.shader.resource_count = 1u;
    fragment.descriptor_count = 1u;
    fragment.descriptors[0].set = 0u;
    fragment.descriptors[0].binding = 3u;
    fragment.descriptors[0].resource_index = 0u;
    fragment.descriptors[0].resource_kind = RIN_SHADER_RESOURCE_STORAGE_IMAGE;
    layout.set = 0u;
    layout.binding = 3u;
    layout.descriptor_type = RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_IMAGE;
    layout.descriptor_count = 1u;
    layout.stage_flags = 1u;
    write.set = 0u;
    write.binding = 3u;
    write.descriptor_type = RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_IMAGE;
    write.access = RIN_GPU_RESOURCE_READ | RIN_GPU_RESOURCE_WRITE;
    write.resource = 1u;
    CHECK(ringpu_vulkan_graphics_build_descriptor_set(
              &vertex, &fragment, &layout, 1u, &write, 1u, &plan) ==
          RIN_GPU_VULKAN_GRAPHICS_OK);
    CHECK(plan.binding_count == 1u);
    CHECK(plan.bindings[0].kind == RIN_SHADER_RESOURCE_STORAGE_IMAGE);
    CHECK(plan.bindings[0].access ==
          (RIN_GPU_RESOURCE_READ | RIN_GPU_RESOURCE_WRITE));
    return 0;
}

static int combined_image_sampler_descriptor_profile(void)
{
    RinSpirvTranslationInfoV1 vertex = {0};
    RinSpirvTranslationInfoV1 fragment = {0};
    RinGpuVulkanDescriptorSetLayoutBindingV1 layout = {0};
    RinGpuVulkanCombinedImageSamplerWriteV1 write = {0};
    RinGpuVulkanDescriptorSetPlanV1 plan = {0};

    vertex.struct_size = sizeof(vertex);
    vertex.stage = RIN_SHADER_STAGE_VERTEX;
    vertex.shader.abi_version = RIN_GPU_ABI_VERSION;
    vertex.shader.struct_size = sizeof(vertex.shader);
    vertex.shader.stage = RIN_SHADER_STAGE_VERTEX;
    fragment.struct_size = sizeof(fragment);
    fragment.stage = RIN_SHADER_STAGE_FRAGMENT;
    fragment.shader.abi_version = RIN_GPU_ABI_VERSION;
    fragment.shader.struct_size = sizeof(fragment.shader);
    fragment.shader.stage = RIN_SHADER_STAGE_FRAGMENT;
    fragment.shader.resource_count = 2u;
    fragment.descriptor_count = 2u;
    fragment.descriptors[0].set = 1u;
    fragment.descriptors[0].binding = 5u;
    fragment.descriptors[0].resource_index = 0u;
    fragment.descriptors[0].resource_kind = RIN_SHADER_RESOURCE_SAMPLED_IMAGE;
    fragment.descriptors[1].set = 1u;
    fragment.descriptors[1].binding = 5u;
    fragment.descriptors[1].resource_index = 1u;
    fragment.descriptors[1].resource_kind = RIN_SHADER_RESOURCE_SAMPLER;
    layout.set = 1u;
    layout.binding = 5u;
    layout.descriptor_type = RIN_GPU_VULKAN_DESCRIPTOR_COMBINED_IMAGE_SAMPLER;
    layout.descriptor_count = 1u;
    layout.stage_flags = 1u;
    write.struct_size = sizeof(write);
    write.version = 1u;
    write.set = 1u;
    write.binding = 5u;
    write.image_resource_index = 0u;
    write.sampler_resource_index = 1u;
    write.image_resource = 17u;
    write.sampler_resource = 23u;
    CHECK(ringpu_vulkan_graphics_build_combined_descriptor_set(
              &vertex, &fragment, &layout, 1u, &write, 1u, &plan) ==
          RIN_GPU_VULKAN_GRAPHICS_OK);
    CHECK(plan.binding_count == 2u);
    CHECK(plan.bindings[0].kind == RIN_SHADER_RESOURCE_SAMPLED_IMAGE);
    CHECK(plan.bindings[0].resource == 17u);
    CHECK(plan.bindings[1].kind == RIN_SHADER_RESOURCE_SAMPLER);
    CHECK(plan.bindings[1].resource == 23u);
    write.sampler_resource = 0u;
    CHECK(ringpu_vulkan_graphics_build_combined_descriptor_set(
              &vertex, &fragment, &layout, 1u, &write, 1u, &plan) ==
          RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT);
    return 0;
}

int main(void)
{
    RinGpuRuntimeSoftwareSurfaceDescV1 surface;
    RinGpuVulkanGraphicsRuntimeV1 runtime;
    ShaderBlob vertex;
    ShaderBlob streamed_vertex;
    ShaderBlob fragment;
    uint8_t compute[sizeof(RinShaderHeaderV1) +
                    4u * sizeof(RinShaderInstructionV1)];
    RinGpuVulkanGraphicsPipelinePlanV1 plan;
    RinGpuVulkanGraphicsPipelinePlanV1 indexed_plan;
    RinGpuVulkanGraphicsPipelinePlanV1 depth_plan;
    RinGpuImageDescV1 image_desc;
    RinGpuImageDescV1 depth_image_desc;
    RinGpuRenderPassMrtDescV1 render_pass_mrt;
    RinGpuRenderPassDepthDescV1 depth_pass;
    RinGpuRasterStateV1 raster_state;
    RinGpuImageTransitionV1 transition;
    RinGpuDrawIndexedV2 indexed_draw;
    RinGpuBufferDescV1 buffer_desc;
    RinGpuBufferDescV1 vertex_buffer_desc;
    RinGpuBufferDescV1 index_buffer_desc;
    RinGpuBufferDescV1 indirect_buffer_desc;
    RinGpuBufferBindingV1 buffer_binding;
    RinGpuDispatchV1 dispatch;
    RinGpuImageReadbackV1 readback_desc;
    RinGpuHandle image = 0u;
    RinGpuHandle image2 = 0u;
    RinGpuHandle depth_color_image = 0u;
    RinGpuHandle depth_image = 0u;
    RinGpuHandle pipeline = 0u;
    RinGpuHandle indexed_pipeline = 0u;
    RinGpuHandle depth_pipeline = 0u;
    RinGpuHandle buffer = 0u;
    RinGpuHandle vertex_buffer = 0u;
    RinGpuHandle index_buffer = 0u;
    RinGpuHandle indirect_buffer = 0u;
    RinGpuHandle compute_pipeline = 0u;
    RinGpuHandle compute_group = 0u;
    RinGpuHandle command_list = 0u;
    RinGpuHandle depth_command_list = 0u;
    RinGpuHandle compute_list = 0u;
    RinGpuHandle fence = 0u;
    uint8_t presented[64u] = {0};
    uint8_t readback[64u] = {0};

    make_surface(&surface, presented);
    CHECK(rin_gpu_vulkan_graphics_runtime_init(
              &runtime, &surface,
              RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                  RIN_GPU_QUEUE_GRAPHICS) == RIN_GPU_OK);
    CHECK(storage_image_descriptor_profile() == 0);
    CHECK(combined_image_sampler_descriptor_profile() == 0);
    make_constant_vertex(&vertex);
    make_constant_fragment(&fragment);
    memset(&plan, 0, sizeof(plan));
    plan.struct_size = sizeof(plan);
    plan.version = RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION;
    plan.backend.color_format = RIN_GPU_FORMAT_RGBA8_UNORM;
    plan.backend.primitive_topology = RIN_GPU_PRIMITIVE_POINT_LIST;
    plan.backend.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
    plan.backend.cull_mode = RIN_GPU_CULL_NONE;
    plan.backend.front_face = RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE;
    plan.backend.position_output_location = 0u;
    {
        int pipeline_result = rin_gpu_vulkan_graphics_runtime_create_graphics_pipeline(
            &runtime, &plan, &vertex, vertex.header.total_size, &fragment,
            fragment.header.total_size, &pipeline);
        CHECK(pipeline_result == RIN_GPU_OK);
    }

    make_streamed_vertex(&streamed_vertex);
    indexed_plan = plan;
    indexed_plan.backend.vertex_input_count = 1u;
    indexed_plan.backend.vertex_binding_count = 1u;
    indexed_plan.backend.vertex_stride = sizeof(float);
    indexed_plan.backend.vertex_bindings[0].binding = 0u;
    indexed_plan.backend.vertex_bindings[0].stride = sizeof(float);
    indexed_plan.backend.vertex_bindings[0].flags = 0u;
    indexed_plan.backend.vertex_bindings[0].reserved = 0u;
    indexed_plan.backend.vertex_attributes[0].location = 0u;
    indexed_plan.backend.vertex_attributes[0].binding = 0u;
    indexed_plan.backend.vertex_attributes[0].format = RIN_GPU_VERTEX_FLOAT32;
    indexed_plan.backend.vertex_attributes[0].offset = 0u;
    indexed_plan.backend.vertex_attributes[0].flags = 0u;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_graphics_pipeline(
              &runtime, &indexed_plan, &streamed_vertex,
              streamed_vertex.header.total_size, &fragment,
              fragment.header.total_size, &indexed_pipeline) == RIN_GPU_OK);

    memset(&vertex_buffer_desc, 0, sizeof(vertex_buffer_desc));
    vertex_buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    vertex_buffer_desc.struct_size = sizeof(vertex_buffer_desc);
    vertex_buffer_desc.size_bytes = sizeof(float);
    vertex_buffer_desc.usage = RIN_GPU_BUFFER_VERTEX |
                               RIN_GPU_BUFFER_COPY_DESTINATION;
    vertex_buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_buffer(
              &runtime, &vertex_buffer_desc, &vertex_buffer) == RIN_GPU_OK);
    {
        const float position_x = 0.0f;
        CHECK(rin_gpu_vulkan_graphics_runtime_upload_buffer(
                  &runtime, vertex_buffer, 0u, &position_x,
                  sizeof(position_x)) == RIN_GPU_OK);
    }
    memset(&index_buffer_desc, 0, sizeof(index_buffer_desc));
    index_buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    index_buffer_desc.struct_size = sizeof(index_buffer_desc);
    index_buffer_desc.size_bytes = 1u;
    index_buffer_desc.usage = RIN_GPU_BUFFER_INDEX |
                              RIN_GPU_BUFFER_COPY_DESTINATION;
    index_buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_buffer(
              &runtime, &index_buffer_desc, &index_buffer) == RIN_GPU_OK);
    {
        const uint8_t vertex_index = 0u;
        CHECK(rin_gpu_vulkan_graphics_runtime_upload_buffer(
                  &runtime, index_buffer, 0u, &vertex_index,
                  sizeof(vertex_index)) == RIN_GPU_OK);
    }
    memset(&indirect_buffer_desc, 0, sizeof(indirect_buffer_desc));
    indirect_buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    indirect_buffer_desc.struct_size = sizeof(indirect_buffer_desc);
    indirect_buffer_desc.size_bytes = 20u;
    indirect_buffer_desc.usage = RIN_GPU_BUFFER_INDIRECT |
                                 RIN_GPU_BUFFER_COPY_DESTINATION;
    indirect_buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_buffer(
              &runtime, &indirect_buffer_desc, &indirect_buffer) == RIN_GPU_OK);
    {
        const uint32_t indirect_draw[5] = {1u, 1u, 0u, 0u, 0u};
        CHECK(rin_gpu_vulkan_graphics_runtime_upload_buffer(
                  &runtime, indirect_buffer, 0u, indirect_draw,
                  sizeof(indirect_draw)) == RIN_GPU_OK);
    }

    memset(&image_desc, 0, sizeof(image_desc));
    image_desc.abi_version = RIN_GPU_ABI_VERSION;
    image_desc.struct_size = sizeof(image_desc);
    image_desc.dimension = RIN_GPU_IMAGE_DIMENSION_2D;
    image_desc.format = RIN_GPU_FORMAT_RGBA8_UNORM;
    image_desc.width = 4u;
    image_desc.height = 4u;
    image_desc.depth = 1u;
    image_desc.array_layers = 1u;
    image_desc.mip_levels = 1u;
    image_desc.sample_count = 1u;
    image_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                       RIN_GPU_IMAGE_COLOR_TARGET;
    image_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_image(
              &runtime, &image_desc, &image) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_create_image(
              &runtime, &image_desc, &image2) == RIN_GPU_OK);
    CHECK(ringpu_runtime_create_fence(runtime.runtime, 0u, &fence) ==
          RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_create_command_list(
              &runtime, &command_list) == RIN_GPU_OK);
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.mip_level_count = 1u;
    transition.array_layer_count = 1u;
    transition.before_state = RIN_GPU_IMAGE_STATE_UNDEFINED;
    transition.after_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    CHECK(ringpu_runtime_command_transition_image(runtime.runtime, command_list,
                                                  image, &transition) ==
          RIN_GPU_OK);
    CHECK(ringpu_runtime_command_transition_image(runtime.runtime, command_list,
                                                  image2, &transition) ==
          RIN_GPU_OK);
    memset(&render_pass_mrt, 0, sizeof(render_pass_mrt));
    render_pass_mrt.abi_version = RIN_GPU_ABI_VERSION;
    render_pass_mrt.struct_size = sizeof(render_pass_mrt);
    render_pass_mrt.active_color_mask = 3u;
    render_pass_mrt.color_attachments[0].target = image;
    render_pass_mrt.color_attachments[1].target = image2;
    render_pass_mrt.color_load_op = RIN_GPU_RENDER_CLEAR;
    render_pass_mrt.color_store_op = RIN_GPU_RENDER_STORE;
    render_pass_mrt.clear_alpha = 1.0f;
    render_pass_mrt.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
    CHECK(rin_gpu_vulkan_graphics_runtime_begin_dynamic_rendering(
              &runtime, command_list, &render_pass_mrt) == RIN_GPU_OK);
    memset(&raster_state, 0, sizeof(raster_state));
    raster_state.abi_version = RIN_GPU_ABI_VERSION;
    raster_state.struct_size = sizeof(raster_state);
    raster_state.viewport.abi_version = RIN_GPU_ABI_VERSION;
    raster_state.viewport.struct_size = sizeof(raster_state.viewport);
    raster_state.viewport.width = 4.0f;
    raster_state.viewport.height = 4.0f;
    raster_state.viewport.max_depth = 1.0f;
    raster_state.scissor.abi_version = RIN_GPU_ABI_VERSION;
    raster_state.scissor.struct_size = sizeof(raster_state.scissor);
    raster_state.scissor.width = 4u;
    raster_state.scissor.height = 4u;
    raster_state.scissor.enabled = 1u;
    CHECK(rin_gpu_vulkan_graphics_runtime_set_raster_state(
              &runtime, command_list, &raster_state) == RIN_GPU_OK);
    memset(&indexed_draw, 0, sizeof(indexed_draw));
    indexed_draw.abi_version = RIN_GPU_ABI_VERSION;
    indexed_draw.struct_size = sizeof(indexed_draw);
    indexed_draw.pipeline = indexed_pipeline;
    indexed_draw.color_target = image;
    indexed_draw.index_buffer = index_buffer;
    indexed_draw.index_format = RIN_GPU_INDEX_UINT8;
    indexed_draw.index_count = 1u;
    indexed_draw.instance_count = 2u;
    indexed_draw.vertex_count = 1u;
    indexed_draw.binding_count = 1u;
    indexed_draw.vertex_buffers[0].binding = 0u;
    indexed_draw.vertex_buffers[0].buffer = vertex_buffer;
    indexed_draw.vertex_buffers[0].offset = 0u;
    CHECK(rin_gpu_vulkan_graphics_runtime_draw_indexed(
              &runtime, command_list, &indexed_draw) == RIN_GPU_OK);
    {
        RinGpuDrawIndirectV1 indirect_draw = {0};
        indirect_draw.abi_version = RIN_GPU_ABI_VERSION;
        indirect_draw.struct_size = sizeof(indirect_draw);
        indirect_draw.pipeline = indexed_pipeline;
        indirect_draw.color_target = image;
        indirect_draw.indirect_buffer = indirect_buffer;
        indirect_draw.draw_count = 1u;
        indirect_draw.stride = 20u;
        indirect_draw.binding_count = 1u;
        indirect_draw.vertex_buffers[0].binding = 0u;
        indirect_draw.vertex_buffers[0].buffer = vertex_buffer;
        CHECK(rin_gpu_vulkan_graphics_runtime_draw_indirect(
                  &runtime, command_list, &indirect_draw) == RIN_GPU_OK);
    }
    {
        RinGpuDrawIndexedIndirectV1 indirect_draw = {0};
        indirect_draw.abi_version = RIN_GPU_ABI_VERSION;
        indirect_draw.struct_size = sizeof(indirect_draw);
        indirect_draw.pipeline = indexed_pipeline;
        indirect_draw.color_target = image;
        indirect_draw.index_buffer = index_buffer;
        indirect_draw.indirect_buffer = indirect_buffer;
        indirect_draw.index_format = RIN_GPU_INDEX_UINT8;
        indirect_draw.draw_count = 1u;
        indirect_draw.stride = 20u;
        indirect_draw.vertex_count = 1u;
        indirect_draw.binding_count = 1u;
        indirect_draw.vertex_buffers[0].binding = 0u;
        indirect_draw.vertex_buffers[0].buffer = vertex_buffer;
        CHECK(rin_gpu_vulkan_graphics_runtime_draw_indexed_indirect(
                  &runtime, command_list, &indirect_draw) == RIN_GPU_OK);
    }
    CHECK(rin_gpu_vulkan_graphics_runtime_end_render_pass(
              &runtime, command_list) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(ringpu_runtime_command_transition_image(runtime.runtime, command_list,
                                                  image, &transition) ==
          RIN_GPU_OK);
    CHECK(ringpu_runtime_command_transition_image(runtime.runtime, command_list,
                                                  image2, &transition) ==
          RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_close_command_list(
              &runtime, command_list) == RIN_GPU_OK);
    {
        int submit_result = rin_gpu_vulkan_graphics_runtime_submit(
            &runtime, command_list, fence, 1u);
        if (submit_result != RIN_GPU_OK) {
            fprintf(stderr, "MRT submit error %d\n", submit_result);
            return 1;
        }
    }
    CHECK(rin_gpu_vulkan_graphics_runtime_wait_fence(
              &runtime, fence, 1u, RIN_GPU_TIMEOUT_INFINITE) == RIN_GPU_OK);
    memset(&readback_desc, 0, sizeof(readback_desc));
    readback_desc.abi_version = RIN_GPU_ABI_VERSION;
    readback_desc.struct_size = sizeof(readback_desc);
    readback_desc.width = 4u;
    readback_desc.height = 4u;
    readback_desc.depth = 1u;
    CHECK(rin_gpu_vulkan_graphics_runtime_readback_image(
              &runtime, image, &readback_desc, readback, sizeof(readback)) ==
          RIN_GPU_OK);
    {
        uint32_t index;
        int nonzero = 0;
        for (index = 0u; index < sizeof(readback); ++index)
            if (readback[index] != 0u) nonzero = 1;
        CHECK(nonzero);
    }
    memset(readback, 0, sizeof(readback));
    CHECK(rin_gpu_vulkan_graphics_runtime_readback_image(
              &runtime, image2, &readback_desc, readback, sizeof(readback)) ==
          RIN_GPU_OK);
    {
        uint32_t index;
        int nonzero = 0;
        for (index = 0u; index < sizeof(readback); ++index)
            if (readback[index] != 0u) nonzero = 1;
        CHECK(nonzero);
    }

    depth_plan = plan;
    depth_plan.backend.depth_format = RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    depth_plan.backend.depth_compare = RIN_GPU_COMPARE_ALWAYS;
    depth_plan.backend.depth_write_enabled = 1u;
    depth_plan.backend.stencil_test_enabled = 1u;
    depth_plan.backend.stencil_compare = RIN_GPU_COMPARE_EQUAL;
    depth_plan.backend.stencil_reference = 0x2au;
    depth_plan.backend.stencil_read_mask = 0xffu;
    depth_plan.backend.stencil_write_mask = 0xffu;
    depth_plan.backend.stencil_fail_operation = RIN_GPU_STENCIL_KEEP;
    depth_plan.backend.stencil_depth_fail_operation = RIN_GPU_STENCIL_KEEP;
    depth_plan.backend.stencil_pass_operation = RIN_GPU_STENCIL_KEEP;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_graphics_pipeline(
              &runtime, &depth_plan, &vertex, vertex.header.total_size,
              &fragment, fragment.header.total_size, &depth_pipeline) ==
          RIN_GPU_OK);
    depth_image_desc = image_desc;
    depth_image_desc.format = RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    depth_image_desc.usage = RIN_GPU_IMAGE_DEPTH_STENCIL |
                             RIN_GPU_IMAGE_COPY_SOURCE;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_image(
              &runtime, &depth_image_desc, &depth_image) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_create_image(
              &runtime, &image_desc, &depth_color_image) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_create_command_list(
              &runtime, &depth_command_list) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_UNDEFINED;
    transition.after_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    CHECK(ringpu_runtime_command_transition_image(
              runtime.runtime, depth_command_list, depth_color_image,
              &transition) == RIN_GPU_OK);
    transition.after_state = RIN_GPU_IMAGE_STATE_DEPTH_TARGET;
    CHECK(ringpu_runtime_command_transition_image(
              runtime.runtime, depth_command_list, depth_image, &transition) ==
          RIN_GPU_OK);
    memset(&depth_pass, 0, sizeof(depth_pass));
    depth_pass.abi_version = RIN_GPU_ABI_VERSION;
    depth_pass.struct_size = sizeof(depth_pass);
    depth_pass.color_target = depth_color_image;
    depth_pass.depth_target = depth_image;
    depth_pass.color_load_op = RIN_GPU_RENDER_CLEAR;
    depth_pass.color_store_op = RIN_GPU_RENDER_STORE;
    depth_pass.depth_load_op = RIN_GPU_RENDER_CLEAR;
    depth_pass.depth_store_op = RIN_GPU_RENDER_STORE;
    depth_pass.stencil_load_op = RIN_GPU_RENDER_CLEAR;
    depth_pass.stencil_store_op = RIN_GPU_RENDER_STORE;
    depth_pass.clear_alpha = 1.0f;
    depth_pass.clear_depth = 1.0f;
    depth_pass.clear_stencil = 0x2au;
    depth_pass.stencil_write_mask = 0xffu;
    depth_pass.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
    CHECK(rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth(
              &runtime, depth_command_list, &depth_pass) == RIN_GPU_OK);
    {
        RinGpuDrawV1 depth_draw = {0};
        depth_draw.abi_version = RIN_GPU_ABI_VERSION;
        depth_draw.struct_size = sizeof(depth_draw);
        depth_draw.pipeline = depth_pipeline;
        depth_draw.color_target = depth_color_image;
        depth_draw.vertex_count = 1u;
        depth_draw.instance_count = 1u;
        CHECK(rin_gpu_vulkan_graphics_runtime_draw(
                  &runtime, depth_command_list, &depth_draw) == RIN_GPU_OK);
    }
    CHECK(rin_gpu_vulkan_graphics_runtime_end_render_pass(
              &runtime, depth_command_list) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(ringpu_runtime_command_transition_image(
              runtime.runtime, depth_command_list, depth_color_image,
              &transition) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_DEPTH_TARGET;
    CHECK(ringpu_runtime_command_transition_image(
              runtime.runtime, depth_command_list, depth_image, &transition) ==
          RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_close_command_list(
              &runtime, depth_command_list) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_submit(
              &runtime, depth_command_list, fence, 2u) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_wait_fence(
              &runtime, fence, 2u, RIN_GPU_TIMEOUT_INFINITE) == RIN_GPU_OK);
    memset(readback, 0, sizeof(readback));
    CHECK(rin_gpu_vulkan_graphics_runtime_readback_image(
              &runtime, depth_color_image, &readback_desc, readback,
              sizeof(readback)) == RIN_GPU_OK);
    {
        uint32_t index;
        int nonzero = 0;
        for (index = 0u; index < sizeof(readback); ++index)
            if (readback[index] != 0u) nonzero = 1;
        CHECK(nonzero);
    }

    make_constant_compute(compute);
    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    buffer_desc.struct_size = sizeof(buffer_desc);
    buffer_desc.size_bytes = 4u;
    buffer_desc.usage = RIN_GPU_BUFFER_STORAGE | RIN_GPU_BUFFER_COPY_DESTINATION;
    buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_buffer(
              &runtime, &buffer_desc, &buffer) == RIN_GPU_OK);
    {
        uint32_t initial = 0u;
        CHECK(rin_gpu_vulkan_graphics_runtime_upload_buffer(
                  &runtime, buffer, 0u, &initial, sizeof(initial)) == RIN_GPU_OK);
    }
    CHECK(rin_gpu_vulkan_graphics_runtime_create_compute_pipeline(
              &runtime, compute, sizeof(compute), &compute_pipeline) ==
          RIN_GPU_OK);
    memset(&buffer_binding, 0, sizeof(buffer_binding));
    buffer_binding.abi_version = RIN_GPU_ABI_VERSION;
    buffer_binding.struct_size = sizeof(buffer_binding);
    buffer_binding.binding = 0u;
    buffer_binding.access = RIN_GPU_RESOURCE_WRITE;
    buffer_binding.buffer = buffer;
    buffer_binding.size_bytes = 4u;
    CHECK(rin_gpu_vulkan_graphics_runtime_create_compute_bind_group(
              &runtime, compute_pipeline, &buffer_binding, 1u,
              &compute_group) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_create_command_list(
              &runtime, &compute_list) == RIN_GPU_OK);
    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.abi_version = RIN_GPU_ABI_VERSION;
    dispatch.struct_size = sizeof(dispatch);
    dispatch.pipeline = compute_pipeline;
    dispatch.bind_group = compute_group;
    dispatch.group_count_x = 1u;
    dispatch.group_count_y = 1u;
    dispatch.group_count_z = 1u;
    CHECK(rin_gpu_vulkan_graphics_runtime_dispatch(
              &runtime, compute_list, &dispatch) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_close_command_list(
              &runtime, compute_list) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_submit(&runtime, compute_list, fence,
                                                3u) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_wait_fence(
              &runtime, fence, 3u, RIN_GPU_TIMEOUT_INFINITE) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_shutdown(&runtime) == RIN_GPU_OK);
    return 0;
}
