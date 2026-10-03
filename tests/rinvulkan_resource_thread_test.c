/* SPDX-License-Identifier: MIT */

#include "../src/atomic_compat.h"

#include <rinvulkan/icd.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

#define RESOURCE_THREAD_COUNT 4u
#define RESOURCE_THREAD_ITERATIONS 2000u

typedef struct ResourceThreadState {
    RinVkDevice device;
    const RinVkBufferCreateInfo* create_info;
    uint32_t* ready_count;
    uint32_t* start_flag;
    uint32_t* failure_flag;
    RinVkBuffer* live_handles;
    uint32_t thread_index;
    int failed;
} ResourceThreadState;

typedef struct ResourceStressState {
    uint32_t ready_count;
    uint32_t start_flag;
    uint32_t failure_flag;
    RinVkBuffer live_handles[RESOURCE_THREAD_COUNT];
} ResourceStressState;

static void yield_thread(void) {
#if defined(_WIN32)
    (void)SwitchToThread();
#else
    (void)sched_yield();
#endif
}

static void resource_worker(void* opaque) {
    ResourceThreadState* state = (ResourceThreadState*)opaque;
    uint32_t iteration;

    (void)__atomic_add_fetch(state->ready_count, 1u, __ATOMIC_RELEASE);
    while (__atomic_load_n(state->start_flag, __ATOMIC_ACQUIRE) == 0u)
        yield_thread();

    for (iteration = 0u; iteration < RESOURCE_THREAD_ITERATIONS;
         ++iteration) {
        RinVkBuffer buffer = 0u;
        RinVkMemoryRequirements requirements;
        uint32_t other_index;

        if (__atomic_load_n(state->failure_flag, __ATOMIC_ACQUIRE) != 0u)
            break;
        if (vkCreateBuffer(state->device, state->create_info, NULL, &buffer) !=
                RIN_VK_SUCCESS ||
            buffer == 0u) {
            state->failed = 1;
            (void)__atomic_store_n(state->failure_flag, 1u, __ATOMIC_RELEASE);
            break;
        }

        __atomic_store_n(&state->live_handles[state->thread_index], buffer,
                         __ATOMIC_RELEASE);
        for (other_index = 0u; other_index < RESOURCE_THREAD_COUNT;
             ++other_index) {
            if (other_index != state->thread_index &&
                __atomic_load_n(&state->live_handles[other_index],
                                __ATOMIC_ACQUIRE) == buffer) {
                state->failed = 1;
                (void)__atomic_store_n(state->failure_flag, 1u,
                                       __ATOMIC_RELEASE);
            }
        }

        memset(&requirements, 0, sizeof(requirements));
        vkGetBufferMemoryRequirements(state->device, buffer, &requirements);
        if (requirements.size < state->create_info->size ||
            requirements.alignment == 0u || requirements.memoryTypeBits == 0u)
            state->failed = 1;

        vkDestroyBuffer(state->device, buffer, NULL);
        __atomic_store_n(&state->live_handles[state->thread_index], 0u,
                         __ATOMIC_RELEASE);
        if (state->failed) {
            (void)__atomic_store_n(state->failure_flag, 1u, __ATOMIC_RELEASE);
            break;
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI resource_worker_entry(LPVOID opaque) {
    resource_worker(opaque);
    return 0u;
}
#else
static void* resource_worker_entry(void* opaque) {
    resource_worker(opaque);
    return NULL;
}
#endif

static int run_resource_stress(RinVkDevice device,
                               const RinVkBufferCreateInfo* create_info) {
    ResourceStressState stress;
    ResourceThreadState states[RESOURCE_THREAD_COUNT];
    uint32_t created = 0u;
    uint32_t index;
    int success = 1;

    memset(&stress, 0, sizeof(stress));
    memset(states, 0, sizeof(states));
    for (index = 0u; index < RESOURCE_THREAD_COUNT; ++index) {
        states[index].device = device;
        states[index].create_info = create_info;
        states[index].ready_count = &stress.ready_count;
        states[index].start_flag = &stress.start_flag;
        states[index].failure_flag = &stress.failure_flag;
        states[index].live_handles = stress.live_handles;
        states[index].thread_index = index;
    }

#if defined(_WIN32)
    {
        HANDLE threads[RESOURCE_THREAD_COUNT] = {NULL};
        for (index = 0u; index < RESOURCE_THREAD_COUNT; ++index) {
            threads[index] = CreateThread(NULL, 0u, resource_worker_entry,
                                          &states[index], 0u, NULL);
            if (!threads[index]) {
                success = 0;
                break;
            }
            ++created;
        }
        while (__atomic_load_n(&stress.ready_count, __ATOMIC_ACQUIRE) < created)
            yield_thread();
        __atomic_store_n(&stress.start_flag, 1u, __ATOMIC_RELEASE);
        for (index = 0u; index < created; ++index) {
            if (WaitForSingleObject(threads[index], INFINITE) != WAIT_OBJECT_0)
                success = 0;
            CloseHandle(threads[index]);
        }
    }
#else
    {
        pthread_t threads[RESOURCE_THREAD_COUNT];
        for (index = 0u; index < RESOURCE_THREAD_COUNT; ++index) {
            if (pthread_create(&threads[index], NULL, resource_worker_entry,
                               &states[index]) != 0) {
                success = 0;
                break;
            }
            ++created;
        }
        while (__atomic_load_n(&stress.ready_count, __ATOMIC_ACQUIRE) < created)
            yield_thread();
        __atomic_store_n(&stress.start_flag, 1u, __ATOMIC_RELEASE);
        for (index = 0u; index < created; ++index)
            if (pthread_join(threads[index], NULL) != 0) success = 0;
    }
#endif

    if (created != RESOURCE_THREAD_COUNT ||
        __atomic_load_n(&stress.failure_flag, __ATOMIC_ACQUIRE) != 0u)
        success = 0;
    for (index = 0u; index < created; ++index)
        if (states[index].failed) success = 0;
    return success;
}

static void make_profile(RinGpuVulkanPhysicalDeviceV2* profile) {
    memset(profile, 0, sizeof(*profile));
    profile->struct_size = sizeof(*profile);
    profile->version = RIN_GPU_VULKAN_PHYSICAL_VERSION;
    profile->api_version = RIN_GPU_VK_MAKE_VERSION(1u, 3u, 0u);
    profile->flags = RIN_GPU_VK_PHYSICAL_DMA_ISOLATED |
                     RIN_GPU_VK_PHYSICAL_RESET_CAPABLE;
    profile->features = RIN_GPU_VK_FEATURE_KNOWN;
    profile->device_uuid[0] = 0x11u;
    profile->driver_digest[0] = 0x22u;
    profile->iommu_domain_cookie = UINT64_C(0x3001);
    profile->device_epoch = 1u;
    profile->queue_family_count = 1u;
    profile->queue_families[0].flags = RIN_GPU_VK_QUEUE_GRAPHICS |
                                       RIN_GPU_VK_QUEUE_COMPUTE |
                                       RIN_GPU_VK_QUEUE_TRANSFER;
    profile->queue_families[0].queue_count = 1u;
    profile->queue_families[0].timestamp_valid_bits = 64u;
    profile->memory_heap_count = 2u;
    profile->memory_heaps[0].size_bytes = UINT64_C(512) * 1024u * 1024u;
    profile->memory_heaps[0].flags = RIN_GPU_VK_HEAP_DEVICE_LOCAL;
    profile->memory_heaps[1].size_bytes = UINT64_C(128) * 1024u * 1024u;
    profile->memory_type_count = 2u;
    profile->memory_types[0].heap_index = 0u;
    profile->memory_types[0].property_flags = RIN_GPU_VK_MEMORY_DEVICE_LOCAL;
    profile->memory_types[1].heap_index = 1u;
    profile->memory_types[1].property_flags =
        RIN_GPU_VK_MEMORY_HOST_VISIBLE | RIN_GPU_VK_MEMORY_HOST_COHERENT;
    profile->max_sampler_anisotropy = 16.0f;
    profile->max_image_dimension_2d = 8192u;
    profile->max_bound_descriptor_sets = 8u;
    profile->max_per_stage_resources = 256u;
    profile->max_push_constants_size = 256u;
    profile->max_memory_allocation_count = 4096u;
    profile->timestamp_period_ns_x1000 = 1000u;
    profile->max_buffer_size = UINT64_C(1) << 30u;
    profile->driver_version = RIN_GPU_VK_MAKE_VERSION(3u, 1u, 4u);
    profile->vendor_id = 0x1af4u;
    profile->device_id = 0x1050u;
    profile->device_type = RIN_GPU_VK_PHYSICAL_TYPE_VIRTUAL_GPU;
    memcpy(profile->device_name, "RinGPU Test", sizeof("RinGPU Test"));
    profile->pipeline_cache_uuid[0] = 0x33u;
}

int main(void) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanRuntimeV1 runtime;
    RinVkApplicationInfo application;
    RinVkInstanceCreateInfo instance_create;
    RinVkDeviceQueueCreateInfo queue_create;
    RinVkDeviceCreateInfo device_create;
    RinVkBufferCreateInfo buffer_create;
    RinVkInstance instance = NULL;
    RinVkPhysicalDevice physical = NULL;
    RinVkDevice device = NULL;
    float priority = 1.0f;
    uint32_t physical_count = 1u;
    int runtime_initialized = 0;
    int runtime_bound = 0;
    int result = 1;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

    make_profile(&profile);
    CHECK(rin_gpu_vulkan_runtime_init(&runtime, &profile, 1u,
                                     UINT64_C(0x1122334455667788)) ==
          RIN_GPU_VULKAN_OK);
    runtime_initialized = 1;
    CHECK(rin_gpu_vulkan_icd_bind_runtime(&runtime) == RIN_GPU_VULKAN_OK);
    runtime_bound = 1;

    memset(&application, 0, sizeof(application));
    application.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "RinVulkan resource thread test";
    application.apiVersion = RIN_GPU_VK_ICD_API_VERSION;
    memset(&instance_create, 0, sizeof(instance_create));
    instance_create.sType = RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create.pApplicationInfo = &application;
    CHECK(vkCreateInstance(&instance_create, NULL, &instance) == RIN_VK_SUCCESS);
    CHECK(vkEnumeratePhysicalDevices(instance, &physical_count, &physical) ==
          RIN_VK_SUCCESS);
    CHECK(physical_count == 1u && physical != NULL);

    memset(&queue_create, 0, sizeof(queue_create));
    queue_create.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueCount = 1u;
    queue_create.pQueuePriorities = &priority;
    memset(&device_create, 0, sizeof(device_create));
    device_create.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 1u;
    device_create.pQueueCreateInfos = &queue_create;
    CHECK(vkCreateDevice(physical, &device_create, NULL, &device) ==
          RIN_VK_SUCCESS);

    memset(&buffer_create, 0, sizeof(buffer_create));
    buffer_create.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create.size = 4096u;
    buffer_create.usage = RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                          RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_create.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    CHECK(run_resource_stress(device, &buffer_create));
    result = 0;

cleanup:
    if (device) vkDestroyDevice(device, NULL);
    if (instance) vkDestroyInstance(instance, NULL);
    if (runtime_bound)
        (void)rin_gpu_vulkan_icd_unbind_runtime(&runtime);
    if (runtime_initialized)
        (void)rin_gpu_vulkan_runtime_shutdown(&runtime);
#undef CHECK
    return result;
}
