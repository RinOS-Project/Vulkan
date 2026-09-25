/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_DESCRIPTOR_RUNTIME_H
#define RINVULKAN_PUBLIC_DESCRIPTOR_RUNTIME_H

#include <stdint.h>

#include <rinvulkan/graphics.h>

#define RIN_GPU_VULKAN_DESCRIPTOR_RUNTIME_VERSION 1u
#define RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS 64u
#define RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS 32u
#define RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS 128u

typedef uint64_t RinGpuVulkanDescriptorHandleV1;

typedef struct RinGpuVulkanDescriptorRuntimeV1 {
    uint32_t initialized;
    uint32_t reserved;
    uint64_t handle_secret;
    uint32_t layout_generation;
    uint32_t pool_generation;
    uint32_t set_generation;
    uint32_t reserved0;
    struct {
        uint32_t state;
        uint32_t generation;
        uint32_t binding_count;
        uint32_t reserved;
        RinGpuVulkanDescriptorSetLayoutBindingV1
            bindings[RIN_SHADER_MAX_RESOURCES];
    } layouts[RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS];
    struct {
        uint32_t state;
        uint32_t generation;
        uint32_t max_sets;
        uint32_t live_sets;
    } pools[RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS];
    struct {
        uint32_t state;
        uint32_t generation;
        uint32_t pool_generation;
        uint32_t layout_generation;
        uint32_t write_count;
        uint32_t reserved;
        RinGpuVulkanDescriptorHandleV1 pool;
        RinGpuVulkanDescriptorHandleV1 layout;
        RinGpuVulkanDescriptorWriteV1
            writes[RIN_SHADER_MAX_RESOURCES];
    } sets[RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS];
} RinGpuVulkanDescriptorRuntimeV1;

int rin_gpu_vulkan_descriptor_runtime_init(
    RinGpuVulkanDescriptorRuntimeV1* runtime, uint64_t handle_secret);
int rin_gpu_vulkan_descriptor_runtime_shutdown(
    RinGpuVulkanDescriptorRuntimeV1* runtime);
int rin_gpu_vulkan_descriptor_layout_create(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    const RinGpuVulkanDescriptorSetLayoutBindingV1* bindings,
    uint32_t binding_count, RinGpuVulkanDescriptorHandleV1* layout_out);
int rin_gpu_vulkan_descriptor_layout_destroy(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    RinGpuVulkanDescriptorHandleV1 layout);
int rin_gpu_vulkan_descriptor_pool_create(
    RinGpuVulkanDescriptorRuntimeV1* runtime, uint32_t max_sets,
    RinGpuVulkanDescriptorHandleV1* pool_out);
int rin_gpu_vulkan_descriptor_pool_destroy(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    RinGpuVulkanDescriptorHandleV1 pool);
int rin_gpu_vulkan_descriptor_set_allocate(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    RinGpuVulkanDescriptorHandleV1 pool,
    RinGpuVulkanDescriptorHandleV1 layout,
    RinGpuVulkanDescriptorHandleV1* set_out);
int rin_gpu_vulkan_descriptor_set_free(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    RinGpuVulkanDescriptorHandleV1 set);
int rin_gpu_vulkan_descriptor_set_update(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    RinGpuVulkanDescriptorHandleV1 set,
    const RinGpuVulkanDescriptorWriteV1* writes, uint32_t write_count);
int rin_gpu_vulkan_descriptor_set_build_plan(
    RinGpuVulkanDescriptorRuntimeV1* runtime,
    RinGpuVulkanDescriptorHandleV1 set,
    const RinSpirvTranslationInfoV1* vertex,
    const RinSpirvTranslationInfoV1* fragment,
    RinGpuVulkanDescriptorSetPlanV1* plan_out);

#endif /* RINVULKAN_PUBLIC_DESCRIPTOR_RUNTIME_H */
