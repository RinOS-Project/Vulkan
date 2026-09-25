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
    RinShaderInstructionV1 instructions[9];
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

static void make_constant_fragment(ShaderBlob* shader)
{
    static const float color[4] = { 1.0f, 0.25f, 0.0f, 1.0f };
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_FRAGMENT;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                    (uint16_t)index, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                    RIN_SHADER_UNUSED, f32_bits(color[index]));
        instruction(&shader->instructions[4u + index],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)index, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                    index);
    }
    instruction(&shader->instructions[8], RIN_SHADER_OP_RETURN,
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

int main(void)
{
    RinGpuRuntimeSoftwareSurfaceDescV1 surface;
    RinGpuVulkanGraphicsRuntimeV1 runtime;
    ShaderBlob vertex;
    ShaderBlob fragment;
    uint8_t compute[sizeof(RinShaderHeaderV1) +
                    4u * sizeof(RinShaderInstructionV1)];
    RinGpuVulkanGraphicsPipelinePlanV1 plan;
    RinGpuImageDescV1 image_desc;
    RinGpuRenderPassDescV1 render_pass;
    RinGpuImageTransitionV1 transition;
    RinGpuDrawV1 draw;
    RinGpuBufferDescV1 buffer_desc;
    RinGpuBufferBindingV1 buffer_binding;
    RinGpuDispatchV1 dispatch;
    RinGpuImageReadbackV1 readback_desc;
    RinGpuHandle image = 0u;
    RinGpuHandle pipeline = 0u;
    RinGpuHandle buffer = 0u;
    RinGpuHandle compute_pipeline = 0u;
    RinGpuHandle compute_group = 0u;
    RinGpuHandle command_list = 0u;
    RinGpuHandle compute_list = 0u;
    RinGpuHandle fence = 0u;
    uint8_t presented[64u] = {0};
    uint8_t readback[64u] = {0};

    make_surface(&surface, presented);
    CHECK(rin_gpu_vulkan_graphics_runtime_init(
              &runtime, &surface,
              RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                  RIN_GPU_QUEUE_GRAPHICS) == RIN_GPU_OK);
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
    memset(&render_pass, 0, sizeof(render_pass));
    render_pass.abi_version = RIN_GPU_ABI_VERSION;
    render_pass.struct_size = sizeof(render_pass);
    render_pass.color_target = image;
    render_pass.load_op = RIN_GPU_RENDER_CLEAR;
    render_pass.store_op = RIN_GPU_RENDER_STORE;
    render_pass.clear_red = 0.0f;
    render_pass.clear_green = 0.0f;
    render_pass.clear_blue = 0.0f;
    render_pass.clear_alpha = 1.0f;
    CHECK(rin_gpu_vulkan_graphics_runtime_begin_render_pass(
              &runtime, command_list, &render_pass) == RIN_GPU_OK);
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.pipeline = pipeline;
    draw.color_target = image;
    draw.vertex_count = 1u;
    draw.instance_count = 1u;
    CHECK(rin_gpu_vulkan_graphics_runtime_draw(
              &runtime, command_list, &draw) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_end_render_pass(
              &runtime, command_list) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(ringpu_runtime_command_transition_image(runtime.runtime, command_list,
                                                  image, &transition) ==
          RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_close_command_list(
              &runtime, command_list) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_submit(&runtime, command_list, fence,
                                                1u) == RIN_GPU_OK);
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
                                                2u) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_wait_fence(
              &runtime, fence, 2u, RIN_GPU_TIMEOUT_INFINITE) == RIN_GPU_OK);
    CHECK(rin_gpu_vulkan_graphics_runtime_shutdown(&runtime) == RIN_GPU_OK);
    return 0;
}
