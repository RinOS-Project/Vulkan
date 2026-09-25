/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_GRAPHICS_RUNTIME_H
#define RINVULKAN_PUBLIC_GRAPHICS_RUNTIME_H

#include <stdint.h>

#include <rinvulkan/graphics.h>
#include <ringpu/runtime.h>

#define RIN_GPU_VULKAN_GRAPHICS_RUNTIME_VERSION 1u

/* Bounded dynamic-rendering owner. The attachment contract deliberately
 * reuses RinGPU's explicit MRT/depth-stencil descriptor so load/store,
 * clear, format and lifetime validation remain in one executor. A full
 * Vulkan VkRenderingInfo ABI is a separate loader boundary. */
typedef RinGpuRenderPassMrtDescV1 RinGpuVulkanDynamicRenderingV1;

/* This is the host software execution owner for the bounded Vulkan graphics
 * profile.  It deliberately receives a caller-owned RinGPU software surface
 * descriptor, so presentation and image acquisition are real callbacks rather
 * than an implicit success path.  It does not advertise physical execution. */
typedef struct RinGpuVulkanGraphicsRuntimeV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t initialized;
    uint32_t reserved;
    RinGpuRuntime* runtime;
    RinGpuHandle queue;
} RinGpuVulkanGraphicsRuntimeV1;

int rin_gpu_vulkan_graphics_runtime_init(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    uint32_t queue_capabilities);
int rin_gpu_vulkan_graphics_runtime_shutdown(
    RinGpuVulkanGraphicsRuntimeV1* runtime);

int rin_gpu_vulkan_graphics_runtime_create_buffer(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuBufferDescV1* descriptor, RinGpuHandle* buffer_out);
int rin_gpu_vulkan_graphics_runtime_create_image(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuImageDescV1* descriptor, RinGpuHandle* image_out);
int rin_gpu_vulkan_graphics_runtime_create_memory(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuMemoryDescV1* descriptor, RinGpuHandle* memory_out);
int rin_gpu_vulkan_graphics_runtime_bind_buffer_memory(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle buffer,
    const RinGpuResourceMemoryBindingV1* binding);
int rin_gpu_vulkan_graphics_runtime_bind_image_memory(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle image,
    const RinGpuResourceMemoryBindingV1* binding);
int rin_gpu_vulkan_graphics_runtime_upload_buffer(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle buffer,
    uint64_t destination_offset, const void* source, uint64_t size_bytes);
int rin_gpu_vulkan_graphics_runtime_upload_image(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle image,
    const RinGpuImageUploadV1* upload, const void* source,
    uint64_t source_size);

int rin_gpu_vulkan_graphics_runtime_create_graphics_pipeline(
    RinGpuVulkanGraphicsRuntimeV1* runtime,
    const RinGpuVulkanGraphicsPipelinePlanV1* plan,
    const void* vertex_shader_ir, uint64_t vertex_shader_size,
    const void* fragment_shader_ir, uint64_t fragment_shader_size,
    RinGpuHandle* pipeline_out);
int rin_gpu_vulkan_graphics_runtime_create_compute_pipeline(
    RinGpuVulkanGraphicsRuntimeV1* runtime, const void* shader_ir,
    uint64_t shader_size, RinGpuHandle* pipeline_out);
int rin_gpu_vulkan_graphics_runtime_create_graphics_bind_group(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle pipeline,
    const RinGpuVulkanDescriptorSetPlanV1* plan, RinGpuHandle* bind_group_out);
int rin_gpu_vulkan_graphics_runtime_create_compute_bind_group(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle pipeline,
    const RinGpuBufferBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out);

int rin_gpu_vulkan_graphics_runtime_create_command_list(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle* command_list_out);
int rin_gpu_vulkan_graphics_runtime_reset_command_list(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list);
int rin_gpu_vulkan_graphics_runtime_begin_render_pass(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassDescV1* render_pass);
int rin_gpu_vulkan_graphics_runtime_begin_render_pass_mrt(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassMrtDescV1* render_pass);
int rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassDepthDescV1* render_pass);
int rin_gpu_vulkan_graphics_runtime_begin_render_pass_depth_stencil(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRenderPassDepthStencilDescV1* render_pass);
int rin_gpu_vulkan_graphics_runtime_begin_dynamic_rendering(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuVulkanDynamicRenderingV1* rendering);
int rin_gpu_vulkan_graphics_runtime_bind_graphics_resources(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    RinGpuHandle bind_group);
int rin_gpu_vulkan_graphics_runtime_draw(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawV1* draw);
int rin_gpu_vulkan_graphics_runtime_draw_vertices(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawVerticesV2* draw);
int rin_gpu_vulkan_graphics_runtime_draw_indexed(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawIndexedV2* draw);
int rin_gpu_vulkan_graphics_runtime_draw_indirect(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawIndirectV1* draw);
int rin_gpu_vulkan_graphics_runtime_draw_indexed_indirect(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDrawIndexedIndirectV1* draw);
int rin_gpu_vulkan_graphics_runtime_set_raster_state(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuRasterStateV1* state);
int rin_gpu_vulkan_graphics_runtime_dispatch(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDispatchV1* dispatch);
int rin_gpu_vulkan_graphics_runtime_dispatch_indirect(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    const RinGpuDispatchIndirectV1* dispatch);
int rin_gpu_vulkan_graphics_runtime_end_render_pass(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list);
int rin_gpu_vulkan_graphics_runtime_close_command_list(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list);
int rin_gpu_vulkan_graphics_runtime_submit(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle command_list,
    RinGpuHandle fence, uint64_t signal_value);
int rin_gpu_vulkan_graphics_runtime_wait_fence(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle fence,
    uint64_t signal_value, uint64_t timeout_ns);
int rin_gpu_vulkan_graphics_runtime_readback_image(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle image,
    const RinGpuImageReadbackV1* readback, void* destination,
    uint64_t destination_size);
int rin_gpu_vulkan_graphics_runtime_destroy_object(
    RinGpuVulkanGraphicsRuntimeV1* runtime, RinGpuHandle object);

#endif /* RINVULKAN_PUBLIC_GRAPHICS_RUNTIME_H */
