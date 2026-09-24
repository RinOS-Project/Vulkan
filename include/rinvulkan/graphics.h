/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_GRAPHICS_H
#define RINVULKAN_PUBLIC_GRAPHICS_H

#include <stdint.h>

#include <ringpu/compatibility.h>
#include <ringpu/spirv_frontend.h>

#define RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION 1u

typedef enum RinGpuVulkanGraphicsResult {
    RIN_GPU_VULKAN_GRAPHICS_OK = 0,
    RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT = -1,
    RIN_GPU_VULKAN_GRAPHICS_BOUNDS = -2,
    RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE = -3,
    RIN_GPU_VULKAN_GRAPHICS_LIMIT = -4
} RinGpuVulkanGraphicsResult;

typedef struct RinGpuVulkanVertexInputBindingV1 {
    uint32_t binding;
    uint32_t stride;
    uint32_t divisor;
    uint32_t reserved;
} RinGpuVulkanVertexInputBindingV1;

typedef struct RinGpuVulkanVertexInputAttributeV1 {
    uint32_t location;
    uint32_t binding;
    uint32_t format;
    uint32_t offset;
    uint32_t flags;
    uint32_t reserved;
} RinGpuVulkanVertexInputAttributeV1;

/* Vulkan's render-pass model is selected for v1 because it maps directly to
 * RinGPU's explicit begin/end render-pass ownership. Dynamic rendering can be
 * added as an ABI extension once it has an independent attachment contract. */
#define RIN_GPU_VULKAN_GRAPHICS_RENDER_PASS 1u

typedef struct RinGpuVulkanGraphicsStateV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t color_format;
    uint32_t primitive_topology;
    uint32_t depth_format;
    uint32_t depth_compare;
    uint32_t depth_write_enabled;
    uint32_t blend_enabled;
    uint32_t source_color_factor;
    uint32_t destination_color_factor;
    uint32_t color_operation;
    uint32_t source_alpha_factor;
    uint32_t destination_alpha_factor;
    uint32_t alpha_operation;
    float blend_constant_red;
    float blend_constant_green;
    float blend_constant_blue;
    float blend_constant_alpha;
    uint32_t color_write_mask;
    uint32_t cull_mode;
    uint32_t front_face;
    uint32_t position_output_location;
    uint32_t render_pass_model;
    uint32_t flags;
    uint32_t reserved;
} RinGpuVulkanGraphicsStateV1;

typedef struct RinGpuVulkanGraphicsPipelinePlanV1 {
    uint32_t struct_size;
    uint32_t version;
    RinShaderInfoV1 vertex_shader;
    RinShaderInfoV1 fragment_shader;
    RinGpuGraphicsPipelineBackendDescV1 backend;
} RinGpuVulkanGraphicsPipelinePlanV1;

typedef struct RinGpuVulkanDescriptorSetLayoutBindingV1 {
    uint32_t set;
    uint32_t binding;
    uint32_t descriptor_type;
    uint32_t descriptor_count;
    uint32_t stage_flags;
    uint32_t reserved;
} RinGpuVulkanDescriptorSetLayoutBindingV1;

enum {
    RIN_GPU_VULKAN_DESCRIPTOR_UNIFORM_BUFFER = 1u,
    RIN_GPU_VULKAN_DESCRIPTOR_SAMPLED_IMAGE = 2u,
    RIN_GPU_VULKAN_DESCRIPTOR_SAMPLER = 3u,
    RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_BUFFER = 4u,
    RIN_GPU_VULKAN_DESCRIPTOR_SAMPLED_DEPTH_IMAGE = 5u,
    RIN_GPU_VULKAN_DESCRIPTOR_COMPARISON_SAMPLER = 6u
};

typedef struct RinGpuVulkanDescriptorWriteV1 {
    uint32_t set;
    uint32_t binding;
    uint32_t array_element;
    uint32_t resource_index;
    uint32_t descriptor_type;
    uint32_t access;
    uint32_t flags;
    uint32_t reserved;
    RinGpuHandle resource;
    uint64_t offset;
    uint64_t size_bytes;
    uint32_t mip_level;
    uint32_t array_layer;
} RinGpuVulkanDescriptorWriteV1;

typedef struct RinGpuVulkanDescriptorSetPlanV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t binding_count;
    uint32_t set_count;
    RinGpuGraphicsBindingV1 bindings[RIN_SHADER_MAX_RESOURCES];
} RinGpuVulkanDescriptorSetPlanV1;

/* Build the pointer-free RinGPU graphics pipeline description from the
 * validated stage interfaces produced by ringpu_spirv_translate().  This is
 * the Vulkan-to-RinGPU assembly/raster/IO boundary; the returned descriptor
 * is consumed by ringpu_create_graphics_pipeline_native*(). */
int ringpu_vulkan_graphics_build_pipeline(
    const RinSpirvTranslationInfoV1* vertex,
    const RinSpirvTranslationInfoV1* fragment,
    const RinGpuVulkanGraphicsStateV1* state,
    const RinGpuVulkanVertexInputBindingV1* vertex_bindings,
    uint32_t vertex_binding_count,
    const RinGpuVulkanVertexInputAttributeV1* vertex_attributes,
    uint32_t vertex_attribute_count,
    RinGpuVulkanGraphicsPipelinePlanV1* plan_out);

/* Flatten all active set/binding pairs into RinGPU's resource-indexed typed
 * bind group. Each descriptor write supplies the concrete RinGPU object handle
 * and is validated against both shader reflection and the declared layout. */
int ringpu_vulkan_graphics_build_descriptor_set(
    const RinSpirvTranslationInfoV1* vertex,
    const RinSpirvTranslationInfoV1* fragment,
    const RinGpuVulkanDescriptorSetLayoutBindingV1* layouts,
    uint32_t layout_count,
    const RinGpuVulkanDescriptorWriteV1* writes,
    uint32_t write_count,
    RinGpuVulkanDescriptorSetPlanV1* plan_out);

#endif /* RINVULKAN_PUBLIC_GRAPHICS_H */
