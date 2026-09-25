/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_GRAPHICS_H
#define RINVULKAN_PUBLIC_GRAPHICS_H

#include <stddef.h>
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
    RIN_GPU_VULKAN_DESCRIPTOR_COMPARISON_SAMPLER = 6u,
    RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_IMAGE = 7u,
    RIN_GPU_VULKAN_DESCRIPTOR_COMBINED_IMAGE_SAMPLER = 8u
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

/* Additive combined-image-sampler write. A single logical Vulkan descriptor
 * is lowered to the two explicit RinGPU resource bindings required by RSH1.
 * The two resource indices are carried explicitly; no handle is packed into
 * offset/size fields and array descriptors are intentionally bounded to one
 * element until the full descriptor-array ABI is available. */
typedef struct RinGpuVulkanCombinedImageSamplerWriteV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t set;
    uint32_t binding;
    uint32_t array_element;
    uint32_t image_resource_index;
    uint32_t sampler_resource_index;
    uint32_t flags;
    uint32_t reserved;
    RinGpuHandle image_resource;
    RinGpuHandle sampler_resource;
    uint32_t mip_level;
    uint32_t array_layer;
    uint32_t reserved2;
    uint32_t reserved3;
} RinGpuVulkanCombinedImageSamplerWriteV1;

typedef struct RinGpuVulkanDescriptorSetPlanV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t binding_count;
    uint32_t set_count;
    RinGpuGraphicsBindingV1 bindings[RIN_SHADER_MAX_RESOURCES];
} RinGpuVulkanDescriptorSetPlanV1;

/* Translate a caller-owned SPIR-V module through the RinGPU frontend before
 * it is consumed by the Vulkan graphics plan builder.  Specialization
 * overrides are applied while the module is translated; the returned RSH1
 * bytes and metadata are published only after structural and shader
 * validation succeeds. */
int ringpu_vulkan_graphics_translate_shader(
    const uint32_t* words, size_t word_count, uint32_t expected_stage,
    const RinSpirvSpecializationValueV1* overrides, uint32_t override_count,
    void* rin_shader_out, size_t rin_shader_capacity,
    RinSpirvTranslationInfoV1* info_out);

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

/* Lower the bounded combined-image-sampler profile. Reflection must contain
 * exactly one sampled-image and one sampler resource at the same set/binding;
 * the explicit pair write supplies both concrete RinGPU handles. */
int ringpu_vulkan_graphics_build_combined_descriptor_set(
    const RinSpirvTranslationInfoV1* vertex,
    const RinSpirvTranslationInfoV1* fragment,
    const RinGpuVulkanDescriptorSetLayoutBindingV1* layouts,
    uint32_t layout_count,
    const RinGpuVulkanCombinedImageSamplerWriteV1* writes,
    uint32_t write_count,
    RinGpuVulkanDescriptorSetPlanV1* plan_out);

#endif /* RINVULKAN_PUBLIC_GRAPHICS_H */
