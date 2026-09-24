/* SPDX-License-Identifier: MIT */

#include <rinvulkan/runtime.h>

#include <stddef.h>
#include <string.h>

#define RIN_GPU_VULKAN_HANDLE_INSTANCE UINT64_C(1)
#define RIN_GPU_VULKAN_HANDLE_PHYSICAL UINT64_C(2)
#define RIN_GPU_VULKAN_HANDLE_DEVICE UINT64_C(3)

static int all_zero(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t index;
    for (index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    return 1;
}

static int ranges_overlap(const void* left, size_t left_size,
                          const void* right, size_t right_size) {
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    if (left_size == 0u || right_size == 0u) return 0;
    if (left_start > UINTPTR_MAX - left_size ||
        right_start > UINTPTR_MAX - right_size)
        return 1;
    return left_start < right_start + right_size &&
           right_start < left_start + left_size;
}

static int instance_version_supported(uint32_t version) {
    uint32_t variant = version >> 29u;
    uint32_t major = (version >> 22u) & 0x7fu;
    uint32_t minor = (version >> 12u) & 0x3ffu;
    return variant == 0u && major == 1u && minor <= 3u;
}

static int fixed_utf8_name_valid(const char name[RIN_GPU_VULKAN_NAME_MAX]) {
    const uint8_t* bytes = (const uint8_t*)name;
    size_t length = 0u;
    size_t index;

    while (length < RIN_GPU_VULKAN_NAME_MAX && bytes[length] != 0u)
        length++;
    if (length == RIN_GPU_VULKAN_NAME_MAX) return 0;
    for (index = length; index < RIN_GPU_VULKAN_NAME_MAX; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    for (index = 0u; index < length;) {
        uint8_t first = bytes[index];
        if (first >= 0x20u && first <= 0x7eu) {
            index++;
        } else if (first >= 0xc2u && first <= 0xdfu &&
                   index + 1u < length &&
                   bytes[index + 1u] >= 0x80u &&
                   bytes[index + 1u] <= 0xbfu) {
            index += 2u;
        } else if (first >= 0xe0u && first <= 0xefu &&
                   index + 2u < length &&
                   bytes[index + 1u] >=
                       (first == 0xe0u ? 0xa0u : 0x80u) &&
                   bytes[index + 1u] <=
                       (first == 0xedu ? 0x9fu : 0xbfu) &&
                   bytes[index + 2u] >= 0x80u &&
                   bytes[index + 2u] <= 0xbfu) {
            index += 3u;
        } else if (first >= 0xf0u && first <= 0xf4u &&
                   index + 3u < length &&
                   bytes[index + 1u] >=
                       (first == 0xf0u ? 0x90u : 0x80u) &&
                   bytes[index + 1u] <=
                       (first == 0xf4u ? 0x8fu : 0xbfu) &&
                   bytes[index + 2u] >= 0x80u &&
                   bytes[index + 2u] <= 0xbfu &&
                   bytes[index + 3u] >= 0x80u &&
                   bytes[index + 3u] <= 0xbfu) {
            index += 4u;
        } else {
            return 0;
        }
    }
    return 1;
}

static int instance_request_valid(
        const RinGpuVulkanInstanceRequestV1* request) {
    return request && request->struct_size == sizeof(*request) &&
           request->version == RIN_GPU_VULKAN_RUNTIME_VERSION &&
           instance_version_supported(request->api_version) &&
           request->flags == 0u &&
           request->enabled_extension_count == 0u &&
           request->enabled_layer_count == 0u &&
           fixed_utf8_name_valid(request->application_name) &&
           fixed_utf8_name_valid(request->engine_name) &&
           all_zero(request->reserved, sizeof(request->reserved));
}

static int runtime_enter(RinGpuVulkanRuntimeV1* runtime) {
    uint32_t expected = 0u;
    if (!runtime ||
        __atomic_load_n(&runtime->initialized, __ATOMIC_ACQUIRE) != 1u)
        return RIN_GPU_VULKAN_NOT_INITIALIZED;
    if (!__atomic_compare_exchange_n(&runtime->lock, &expected, 1u, 0,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    if (__atomic_load_n(&runtime->initialized, __ATOMIC_RELAXED) != 1u ||
        runtime->struct_size != sizeof(*runtime) ||
        runtime->version != RIN_GPU_VULKAN_RUNTIME_VERSION ||
        runtime->handle_secret == 0u ||
        runtime->physical_device_count >
            RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES ||
        runtime->reserved0 != 0u) {
        __atomic_store_n(&runtime->lock, 0u, __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_NOT_INITIALIZED;
    }
    return RIN_GPU_VULKAN_OK;
}

static void runtime_leave(RinGpuVulkanRuntimeV1* runtime) {
    __atomic_store_n(&runtime->lock, 0u, __ATOMIC_RELEASE);
}

static uint32_t next_generation(uint32_t generation) {
    generation++;
    return generation == 0u ? 1u : generation;
}

static RinGpuVulkanHandle encode_handle(uint64_t raw, uint64_t secret) {
    RinGpuVulkanHandle encoded = raw ^ secret;
    return encoded == 0u ? raw : encoded;
}

static RinGpuVulkanHandle instance_handle(
        const RinGpuVulkanRuntimeV1* runtime, uint32_t index) {
    uint64_t raw =
        ((uint64_t)runtime->instances[index].generation << 32u) |
        (RIN_GPU_VULKAN_HANDLE_INSTANCE << 16u) | (uint64_t)(index + 1u);
    return encode_handle(raw, runtime->handle_secret);
}

static RinGpuVulkanHandle physical_handle(
        const RinGpuVulkanRuntimeV1* runtime, uint32_t instance_index,
        uint32_t physical_index) {
    uint64_t raw =
        ((uint64_t)runtime->instances[instance_index].generation << 32u) |
        (RIN_GPU_VULKAN_HANDLE_PHYSICAL << 16u) |
        ((uint64_t)(instance_index + 1u) << 8u) |
        (uint64_t)(physical_index + 1u);
    return encode_handle(raw, runtime->handle_secret);
}

static RinGpuVulkanHandle device_handle(
        const RinGpuVulkanRuntimeV1* runtime, uint32_t index) {
    uint64_t raw = ((uint64_t)runtime->devices[index].generation << 32u) |
                   (RIN_GPU_VULKAN_HANDLE_DEVICE << 16u) |
                   (uint64_t)(index + 1u);
    return encode_handle(raw, runtime->handle_secret);
}

static int find_instance(const RinGpuVulkanRuntimeV1* runtime,
                         RinGpuVulkanHandle handle, uint32_t* index_out) {
    uint32_t index;
    if (handle == 0u) return RIN_GPU_VULKAN_INVALID_HANDLE;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        if (runtime->instances[index].occupied != 0u &&
            instance_handle(runtime, index) == handle) {
            *index_out = index;
            return RIN_GPU_VULKAN_OK;
        }
    }
    return RIN_GPU_VULKAN_INVALID_HANDLE;
}

static int find_physical(const RinGpuVulkanRuntimeV1* runtime,
                         uint32_t instance_index,
                         RinGpuVulkanHandle handle,
                         uint32_t* index_out) {
    uint32_t index;
    if (handle == 0u) return RIN_GPU_VULKAN_INVALID_HANDLE;
    for (index = 0u; index < runtime->physical_device_count; ++index) {
        if (physical_handle(runtime, instance_index, index) == handle) {
            *index_out = index;
            return RIN_GPU_VULKAN_OK;
        }
    }
    return RIN_GPU_VULKAN_INVALID_HANDLE;
}

static int find_device(const RinGpuVulkanRuntimeV1* runtime,
                       uint32_t instance_index,
                       RinGpuVulkanHandle handle, uint32_t* index_out) {
    uint32_t index;
    if (handle == 0u) return RIN_GPU_VULKAN_INVALID_HANDLE;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        const RinGpuVulkanDeviceSlotV1* slot = &runtime->devices[index];
        if (slot->occupied != 0u &&
            slot->owner_instance_index == instance_index &&
            slot->owner_instance_generation ==
                runtime->instances[instance_index].generation &&
            device_handle(runtime, index) == handle) {
            *index_out = index;
            return RIN_GPU_VULKAN_OK;
        }
    }
    return RIN_GPU_VULKAN_INVALID_HANDLE;
}

static void validation_request(RinGpuVulkanCreateRequestV1* request) {
    memset(request, 0, sizeof(*request));
    request->struct_size = sizeof(*request);
    request->version = RIN_GPU_VULKAN_PROFILE_VERSION;
    request->api_version = RIN_GPU_VK_API_1_3;
    request->max_in_flight = 1u;
}

int rin_gpu_vulkan_runtime_init(
        RinGpuVulkanRuntimeV1* runtime,
        const RinGpuVulkanPhysicalDeviceV2* physical_devices,
        uint32_t physical_device_count,
        uint64_t handle_secret) {
    RinGpuVulkanCreateRequestV1 request;
    RinGpuVulkanDevicePlanV1 plan;
    uint32_t index;
    uint32_t previous;

    if (!runtime || physical_device_count >
                        RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES ||
        (physical_device_count != 0u && !physical_devices) ||
        handle_secret == 0u)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (physical_device_count != 0u &&
        ranges_overlap(runtime, sizeof(*runtime), physical_devices,
                       (size_t)physical_device_count *
                           sizeof(*physical_devices)))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;

    memset(runtime, 0, sizeof(*runtime));
    validation_request(&request);
    for (index = 0u; index < physical_device_count; ++index) {
        RinGpuVulkanPhysicalDeviceV2 profile = physical_devices[index];
        int result = rin_gpu_vulkan_plan_device(&profile, &request, &plan);
        if (result != RIN_GPU_VULKAN_OK) {
            memset(runtime, 0, sizeof(*runtime));
            return result;
        }
        for (previous = 0u; previous < index; ++previous) {
            const RinGpuVulkanPhysicalDeviceV2* candidate =
                &runtime->physical_devices[previous];
            if (memcmp(profile.device_uuid, candidate->device_uuid,
                       sizeof(profile.device_uuid)) == 0 ||
                profile.iommu_domain_cookie ==
                    candidate->iommu_domain_cookie) {
                memset(runtime, 0, sizeof(*runtime));
                return RIN_GPU_VULKAN_SECURITY;
            }
        }
        runtime->physical_devices[index] = profile;
    }
    runtime->struct_size = sizeof(*runtime);
    runtime->version = RIN_GPU_VULKAN_RUNTIME_VERSION;
    runtime->handle_secret = handle_secret;
    runtime->physical_device_count = physical_device_count;
    __atomic_store_n(&runtime->initialized, 1u, __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_runtime_init_from_catalog(
        RinGpuVulkanRuntimeV1* runtime,
        RinGpuVulkanPhysicalCatalogEnumerateFn enumerate,
        void* context, uint64_t handle_secret) {
    RinGpuVulkanPhysicalDeviceV2 physical_devices[
        RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
    uint32_t physical_device_count = 0u;
    int result;

    if (!runtime || !enumerate || handle_secret == 0u)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    memset(physical_devices, 0, sizeof(physical_devices));
    result = enumerate(context, physical_devices,
                       RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES,
                       &physical_device_count);
    if (result != RIN_GPU_VULKAN_OK) {
        memset(physical_devices, 0, sizeof(physical_devices));
        return result;
    }
    if (physical_device_count > RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES) {
        memset(physical_devices, 0, sizeof(physical_devices));
        return RIN_GPU_VULKAN_LIMIT;
    }
    result = rin_gpu_vulkan_runtime_init(runtime, physical_devices,
                                         physical_device_count, handle_secret);
    memset(physical_devices, 0, sizeof(physical_devices));
    return result;
}

int rin_gpu_vulkan_runtime_shutdown(RinGpuVulkanRuntimeV1* runtime) {
    uint32_t index;
    int result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        if (runtime->instances[index].occupied != 0u) {
            runtime_leave(runtime);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        if (runtime->devices[index].occupied != 0u) {
            runtime_leave(runtime);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    memset(runtime->physical_devices, 0,
           sizeof(runtime->physical_devices));
    runtime->physical_device_count = 0u;
    runtime->handle_secret = 0u;
    runtime->struct_size = 0u;
    runtime->version = 0u;
    __atomic_store_n(&runtime->initialized, 0u, __ATOMIC_RELEASE);
    runtime_leave(runtime);
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_create_instance(
        RinGpuVulkanRuntimeV1* runtime,
        const RinGpuVulkanInstanceRequestV1* request,
        RinGpuVulkanHandle* instance_out) {
    RinGpuVulkanInstanceRequestV1 wanted;
    uint32_t index;
    int result;

    if (!instance_out ||
        (runtime && ranges_overlap(runtime, sizeof(*runtime), instance_out,
                                   sizeof(*instance_out))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!request) {
        *instance_out = 0u;
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    }
    wanted = *request;
    *instance_out = 0u;
    if (!instance_request_valid(&wanted))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        RinGpuVulkanInstanceSlotV1* slot = &runtime->instances[index];
        if (slot->occupied != 0u) continue;
        if (slot->generation == 0u) slot->generation = 1u;
        slot->request = wanted;
        slot->occupied = 1u;
        *instance_out = instance_handle(runtime, index);
        runtime_leave(runtime);
        return RIN_GPU_VULKAN_OK;
    }
    runtime_leave(runtime);
    return RIN_GPU_VULKAN_LIMIT;
}

int rin_gpu_vulkan_destroy_instance(RinGpuVulkanRuntimeV1* runtime,
                                    RinGpuVulkanHandle instance) {
    uint32_t instance_index;
    uint32_t index;
    int result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &instance_index);
    if (result != RIN_GPU_VULKAN_OK) {
        runtime_leave(runtime);
        return result;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        if (runtime->devices[index].occupied != 0u &&
            runtime->devices[index].owner_instance_index == instance_index &&
            runtime->devices[index].owner_instance_generation ==
                runtime->instances[instance_index].generation) {
            runtime_leave(runtime);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    memset(&runtime->instances[instance_index].request, 0,
           sizeof(runtime->instances[instance_index].request));
    runtime->instances[instance_index].occupied = 0u;
    runtime->instances[instance_index].generation = next_generation(
        runtime->instances[instance_index].generation);
    runtime_leave(runtime);
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_query_instance(
        RinGpuVulkanRuntimeV1* runtime,
        RinGpuVulkanHandle instance,
        RinGpuVulkanInstanceRequestV1* request_out) {
    uint32_t index;
    int result;
    if (!request_out ||
        (runtime && ranges_overlap(runtime, sizeof(*runtime), request_out,
                                   sizeof(*request_out))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    memset(request_out, 0, sizeof(*request_out));
    result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &index);
    if (result == RIN_GPU_VULKAN_OK)
        *request_out = runtime->instances[index].request;
    runtime_leave(runtime);
    return result;
}

int rin_gpu_vulkan_enumerate_physical_devices(
        RinGpuVulkanRuntimeV1* runtime,
        RinGpuVulkanHandle instance,
        uint32_t* physical_device_count,
        RinGpuVulkanHandle* physical_devices) {
    uint32_t capacity;
    uint32_t instance_index;
    uint32_t written;
    uint32_t index;
    int result;

    if (!physical_device_count ||
        (runtime && ranges_overlap(runtime, sizeof(*runtime),
                                   physical_device_count,
                                   sizeof(*physical_device_count))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    capacity = *physical_device_count;
    *physical_device_count = 0u;
    written = capacity < RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES
                  ? capacity
                  : RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES;
    if (physical_devices &&
        (ranges_overlap(physical_device_count,
                        sizeof(*physical_device_count), physical_devices,
                        (size_t)written * sizeof(*physical_devices)) ||
         (runtime && ranges_overlap(runtime, sizeof(*runtime),
                                    physical_devices,
                                    (size_t)written *
                                        sizeof(*physical_devices)))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &instance_index);
    if (result != RIN_GPU_VULKAN_OK) {
        runtime_leave(runtime);
        return result;
    }
    if (!physical_devices) {
        *physical_device_count = runtime->physical_device_count;
        runtime_leave(runtime);
        return RIN_GPU_VULKAN_OK;
    }
    written = capacity < runtime->physical_device_count
                  ? capacity
                  : runtime->physical_device_count;
    for (index = 0u; index < written; ++index)
        physical_devices[index] =
            physical_handle(runtime, instance_index, index);
    *physical_device_count = written;
    result = written < runtime->physical_device_count
                 ? RIN_GPU_VULKAN_INCOMPLETE
                 : RIN_GPU_VULKAN_OK;
    runtime_leave(runtime);
    return result;
}

int rin_gpu_vulkan_query_physical_device(
        RinGpuVulkanRuntimeV1* runtime,
        RinGpuVulkanHandle instance,
        RinGpuVulkanHandle physical_device,
        RinGpuVulkanPhysicalDeviceV2* profile_out) {
    uint32_t instance_index;
    uint32_t physical_index;
    int result;
    if (!profile_out ||
        (runtime && ranges_overlap(runtime, sizeof(*runtime), profile_out,
                                   sizeof(*profile_out))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    memset(profile_out, 0, sizeof(*profile_out));
    result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &instance_index);
    if (result == RIN_GPU_VULKAN_OK)
        result = find_physical(runtime, instance_index, physical_device,
                               &physical_index);
    if (result == RIN_GPU_VULKAN_OK)
        *profile_out = runtime->physical_devices[physical_index];
    runtime_leave(runtime);
    return result;
}

int rin_gpu_vulkan_create_device(
        RinGpuVulkanRuntimeV1* runtime,
        RinGpuVulkanHandle instance,
        RinGpuVulkanHandle physical_device,
        const RinGpuVulkanCreateRequestV1* request,
        RinGpuVulkanHandle* device_out,
        RinGpuVulkanDevicePlanV1* plan_out) {
    RinGpuVulkanCreateRequestV1 wanted;
    RinGpuVulkanDevicePlanV1 plan;
    uint32_t instance_index;
    uint32_t physical_index;
    uint32_t index;
    int result;

    if (!device_out || !plan_out ||
        ranges_overlap(device_out, sizeof(*device_out), plan_out,
                       sizeof(*plan_out)) ||
        (runtime &&
         (ranges_overlap(runtime, sizeof(*runtime), device_out,
                         sizeof(*device_out)) ||
          ranges_overlap(runtime, sizeof(*runtime), plan_out,
                         sizeof(*plan_out)))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!request) {
        *device_out = 0u;
        memset(plan_out, 0, sizeof(*plan_out));
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    }
    wanted = *request;
    *device_out = 0u;
    memset(plan_out, 0, sizeof(*plan_out));
    result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &instance_index);
    if (result == RIN_GPU_VULKAN_OK)
        result = find_physical(runtime, instance_index, physical_device,
                               &physical_index);
    if (result != RIN_GPU_VULKAN_OK) {
        runtime_leave(runtime);
        return result;
    }
    result = rin_gpu_vulkan_plan_device(
        &runtime->physical_devices[physical_index], &wanted, &plan);
    if (result != RIN_GPU_VULKAN_OK) {
        runtime_leave(runtime);
        return result;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        RinGpuVulkanDeviceSlotV1* slot = &runtime->devices[index];
        if (slot->occupied != 0u) continue;
        if (slot->generation == 0u) slot->generation = 1u;
        slot->owner_instance_index = instance_index;
        slot->owner_instance_generation =
            runtime->instances[instance_index].generation;
        slot->physical_device_index = physical_index;
        slot->reserved0 = 0u;
        slot->plan = plan;
        slot->occupied = 1u;
        *device_out = device_handle(runtime, index);
        *plan_out = plan;
        runtime_leave(runtime);
        return RIN_GPU_VULKAN_OK;
    }
    runtime_leave(runtime);
    return RIN_GPU_VULKAN_LIMIT;
}

int rin_gpu_vulkan_destroy_device(RinGpuVulkanRuntimeV1* runtime,
                                  RinGpuVulkanHandle instance,
                                  RinGpuVulkanHandle device) {
    uint32_t instance_index;
    uint32_t device_index;
    int result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &instance_index);
    if (result == RIN_GPU_VULKAN_OK)
        result = find_device(runtime, instance_index, device, &device_index);
    if (result == RIN_GPU_VULKAN_OK) {
        RinGpuVulkanDeviceSlotV1* slot = &runtime->devices[device_index];
        memset(&slot->plan, 0, sizeof(slot->plan));
        slot->occupied = 0u;
        slot->owner_instance_index = 0u;
        slot->owner_instance_generation = 0u;
        slot->physical_device_index = 0u;
        slot->reserved0 = 0u;
        slot->generation = next_generation(slot->generation);
    }
    runtime_leave(runtime);
    return result;
}

int rin_gpu_vulkan_query_device(RinGpuVulkanRuntimeV1* runtime,
                                RinGpuVulkanHandle instance,
                                RinGpuVulkanHandle device,
                                RinGpuVulkanDevicePlanV1* plan_out) {
    uint32_t instance_index;
    uint32_t device_index;
    int result;
    if (!plan_out ||
        (runtime && ranges_overlap(runtime, sizeof(*runtime), plan_out,
                                   sizeof(*plan_out))))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    memset(plan_out, 0, sizeof(*plan_out));
    result = runtime_enter(runtime);
    if (result != RIN_GPU_VULKAN_OK) return result;
    result = find_instance(runtime, instance, &instance_index);
    if (result == RIN_GPU_VULKAN_OK)
        result = find_device(runtime, instance_index, device, &device_index);
    if (result == RIN_GPU_VULKAN_OK)
        *plan_out = runtime->devices[device_index].plan;
    runtime_leave(runtime);
    return result;
}
