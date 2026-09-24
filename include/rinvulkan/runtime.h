/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_RUNTIME_H
#define RINVULKAN_PUBLIC_RUNTIME_H

#include <stdint.h>

#include <rinvulkan/profile.h>

#define RIN_GPU_VULKAN_RUNTIME_VERSION 1u
#define RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES 8u
#define RIN_GPU_VULKAN_MAX_INSTANCES 16u
#define RIN_GPU_VULKAN_MAX_DEVICES 32u
#define RIN_GPU_VULKAN_NAME_MAX 64u

typedef uint64_t RinGpuVulkanHandle;

/* The catalog owns physical-device discovery. The Vulkan runtime copies the
 * bounded snapshot returned by this callback and validates every profile
 * before publishing it to an instance. The callback must not retain the
 * output pointer. */
typedef int (*RinGpuVulkanPhysicalCatalogEnumerateFn)(
    void* context, RinGpuVulkanPhysicalDeviceV2* devices, uint32_t capacity,
    uint32_t* count_out);

typedef struct RinGpuVulkanInstanceRequestV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t api_version;
    uint32_t flags;
    uint32_t application_version;
    uint32_t engine_version;
    uint32_t enabled_extension_count;
    uint32_t enabled_layer_count;
    char application_name[RIN_GPU_VULKAN_NAME_MAX];
    char engine_name[RIN_GPU_VULKAN_NAME_MAX];
    uint64_t reserved[4];
} RinGpuVulkanInstanceRequestV1;

typedef struct RinGpuVulkanInstanceSlotV1 {
    uint32_t generation;
    uint32_t occupied;
    RinGpuVulkanInstanceRequestV1 request;
} RinGpuVulkanInstanceSlotV1;

typedef struct RinGpuVulkanDeviceSlotV1 {
    uint32_t generation;
    uint32_t occupied;
    uint32_t owner_instance_index;
    uint32_t owner_instance_generation;
    uint32_t physical_device_index;
    uint32_t reserved0;
    RinGpuVulkanDevicePlanV1 plan;
} RinGpuVulkanDeviceSlotV1;

typedef struct RinGpuVulkanRuntimeV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t lock;
    uint32_t initialized;
    uint64_t handle_secret;
    uint32_t physical_device_count;
    uint32_t reserved0;
    RinGpuVulkanPhysicalDeviceV2
        physical_devices[RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
    RinGpuVulkanInstanceSlotV1 instances[RIN_GPU_VULKAN_MAX_INSTANCES];
    RinGpuVulkanDeviceSlotV1 devices[RIN_GPU_VULKAN_MAX_DEVICES];
} RinGpuVulkanRuntimeV1;

int rin_gpu_vulkan_runtime_init(
    RinGpuVulkanRuntimeV1* runtime,
    const RinGpuVulkanPhysicalDeviceV2* physical_devices,
    uint32_t physical_device_count,
    uint64_t handle_secret);
int rin_gpu_vulkan_runtime_init_from_catalog(
    RinGpuVulkanRuntimeV1* runtime,
    RinGpuVulkanPhysicalCatalogEnumerateFn enumerate,
    void* context, uint64_t handle_secret);
int rin_gpu_vulkan_runtime_shutdown(RinGpuVulkanRuntimeV1* runtime);

int rin_gpu_vulkan_create_instance(
    RinGpuVulkanRuntimeV1* runtime,
    const RinGpuVulkanInstanceRequestV1* request,
    RinGpuVulkanHandle* instance_out);
int rin_gpu_vulkan_destroy_instance(RinGpuVulkanRuntimeV1* runtime,
                                    RinGpuVulkanHandle instance);
int rin_gpu_vulkan_query_instance(
    RinGpuVulkanRuntimeV1* runtime,
    RinGpuVulkanHandle instance,
    RinGpuVulkanInstanceRequestV1* request_out);

int rin_gpu_vulkan_enumerate_physical_devices(
    RinGpuVulkanRuntimeV1* runtime,
    RinGpuVulkanHandle instance,
    uint32_t* physical_device_count,
    RinGpuVulkanHandle* physical_devices);
int rin_gpu_vulkan_query_physical_device(
    RinGpuVulkanRuntimeV1* runtime,
    RinGpuVulkanHandle instance,
    RinGpuVulkanHandle physical_device,
    RinGpuVulkanPhysicalDeviceV2* profile_out);

int rin_gpu_vulkan_create_device(
    RinGpuVulkanRuntimeV1* runtime,
    RinGpuVulkanHandle instance,
    RinGpuVulkanHandle physical_device,
    const RinGpuVulkanCreateRequestV1* request,
    RinGpuVulkanHandle* device_out,
    RinGpuVulkanDevicePlanV1* plan_out);
int rin_gpu_vulkan_destroy_device(RinGpuVulkanRuntimeV1* runtime,
                                  RinGpuVulkanHandle instance,
                                  RinGpuVulkanHandle device);
int rin_gpu_vulkan_query_device(RinGpuVulkanRuntimeV1* runtime,
                                RinGpuVulkanHandle instance,
                                RinGpuVulkanHandle device,
                                RinGpuVulkanDevicePlanV1* plan_out);

#if defined(__cplusplus)
static_assert(sizeof(RinGpuVulkanInstanceRequestV1) == 192u,
              "RinGPU Vulkan instance request drift");
#else
_Static_assert(sizeof(RinGpuVulkanInstanceRequestV1) == 192u,
               "RinGPU Vulkan instance request drift");
#endif

#endif
