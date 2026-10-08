/* SPDX-License-Identifier: MIT */

#include "../src/atomic_compat.h"

#include <rinvulkan/icd.h>
#include <rinvulkan/software_platform.h>

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

static RinVulkanProductSubmitFn g_software_submit;
static RinVulkanProductPollFn g_software_poll;
static RinGpuVulkanSoftwarePlatformV1* g_software_platform;
static RinVulkanProductPlatformV1* g_v2_product_base;
static uint32_t g_busy_submit_responses;
static uint32_t g_submit_call_count;
static uint32_t g_hold_software_completions;
static uint32_t g_v2_submit_call_count;
static uint32_t g_v2_wait_count;
static RinVulkanProductSubmissionWaitV1
    g_v2_waits[RIN_VULKAN_PRODUCT_MAX_SUBMISSION_WAITS];

typedef struct DebugCapture {
    uint32_t callback_count;
    uint32_t object_name_count;
    uint32_t object_tag_count;
    uint32_t command_label_count;
    uint32_t queue_label_count;
    uint32_t submitted_message_count;
    uint32_t instance_failed_count;
    int malformed;
} DebugCapture;

static uint32_t debug_capture_callback(
        uint32_t severity, uint32_t types,
        const RinVkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* opaque) {
    DebugCapture* capture = (DebugCapture*)opaque;
    if (!capture || !callback_data || !callback_data->pMessageIdName ||
        !callback_data->pMessage || severity == 0u || types == 0u) {
        if (capture) capture->malformed = 1;
        return 0u;
    }
    ++capture->callback_count;
    if (strcmp(callback_data->pMessageIdName,
               "RinVulkan.DebugUtils.ObjectName") == 0) {
        if (callback_data->objectCount != 1u || !callback_data->pObjects ||
            !callback_data->pObjects[0].pObjectName ||
            strcmp(callback_data->pObjects[0].pObjectName,
                   "command buffer") != 0)
            capture->malformed = 1;
        else
            ++capture->object_name_count;
    } else if (strcmp(callback_data->pMessageIdName,
                      "RinVulkan.DebugUtils.ObjectTag") == 0) {
        if (callback_data->objectCount != 1u || !callback_data->pObjects ||
            !callback_data->pObjects[0].pObjectName ||
            strcmp(callback_data->pMessage,
                   "tag=0x0072696e74657374 data=0x476e6952") != 0) {
            fprintf(stderr, "object tag callback message: %s\n",
                    callback_data->pMessage);
            capture->malformed = 1;
        } else
            ++capture->object_tag_count;
    } else if (strcmp(callback_data->pMessageIdName,
                      "RinVulkan.DebugUtils.CommandLabelBegin") == 0 ||
               strcmp(callback_data->pMessageIdName,
                      "RinVulkan.DebugUtils.CommandLabelInsert") == 0) {
        if (callback_data->cmdBufLabelCount != 1u ||
            !callback_data->pCmdBufLabels ||
            !callback_data->pCmdBufLabels[0].pLabelName ||
            strcmp(callback_data->pCmdBufLabels[0].pLabelName,
                   "record transfer") != 0 ||
            callback_data->pCmdBufLabels[0].color[1] != 0.5f)
            capture->malformed = 1;
        else
            ++capture->command_label_count;
    } else if (strcmp(callback_data->pMessageIdName,
                      "RinVulkan.DebugUtils.QueueLabelBegin") == 0) {
        if (callback_data->queueLabelCount != 1u ||
            !callback_data->pQueueLabels ||
            !callback_data->pQueueLabels[0].pLabelName ||
            strcmp(callback_data->pQueueLabels[0].pLabelName,
                   "main queue") != 0)
            capture->malformed = 1;
        else
            ++capture->queue_label_count;
    } else if (strcmp(callback_data->pMessageIdName, "app.message") == 0) {
        if (strcmp(callback_data->pMessage, "validation probe") != 0)
            capture->malformed = 1;
        else
            ++capture->submitted_message_count;
    } else if (strcmp(callback_data->pMessageIdName,
                      "RinVulkan.InstanceCreateFailed") == 0) {
        if (severity != RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            capture->malformed = 1;
        else
            ++capture->instance_failed_count;
    }
    return 0u;
}

static int submit_busy_once(void* context,
                            const RinVulkanProductSubmissionV1* submission,
                            const RinVulkanProductResourceV1* resources,
                            uint32_t resource_count) {
    ++g_submit_call_count;
    if (g_busy_submit_responses != 0u) {
        --g_busy_submit_responses;
        return RIN_VULKAN_PRODUCT_BUSY;
    }
    return g_software_submit(context, submission, resources, resource_count);
}

static int prepare_submission_v2(
        void* context, uint32_t queue_id, uint64_t command_cookie,
        uint32_t wait_count, const RinVulkanProductSubmissionWaitV1* waits,
        RinVulkanProductSubmissionV2* submission_out) {
    int result;
    if (!g_v2_product_base || !submission_out ||
        wait_count > RIN_VULKAN_PRODUCT_MAX_SUBMISSION_WAITS ||
        (wait_count != 0u && !waits))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(submission_out, 0, sizeof(*submission_out));
    submission_out->struct_size = sizeof(*submission_out);
    submission_out->version = RIN_VULKAN_PRODUCT_SUBMISSION_V2_VERSION;
    submission_out->wait_count = wait_count;
    if (wait_count != 0u)
        memcpy(submission_out->waits, waits,
               sizeof(*waits) * wait_count);
    result = g_v2_product_base->prepare_submission(
        context, queue_id, command_cookie, &submission_out->base);
    if (result == RIN_VULKAN_PRODUCT_OK)
        submission_out->base.deadline_ns = UINT64_C(1);
    return result;
}

static int submit_v2(
        void* context, const RinVulkanProductSubmissionV2* submission,
        const RinVulkanProductResourceV1* resources,
        uint32_t resource_count) {
    if (!g_v2_product_base || !submission ||
        submission->struct_size != sizeof(*submission) ||
        submission->version != RIN_VULKAN_PRODUCT_SUBMISSION_V2_VERSION ||
        submission->wait_count > RIN_VULKAN_PRODUCT_MAX_SUBMISSION_WAITS ||
        submission->reserved0 != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    g_v2_wait_count = submission->wait_count;
    memset(g_v2_waits, 0, sizeof(g_v2_waits));
    if (g_v2_wait_count != 0u)
        memcpy(g_v2_waits, submission->waits,
               sizeof(g_v2_waits[0]) * g_v2_wait_count);
    ++g_v2_submit_call_count;
    return g_v2_product_base->submit(
        context, &submission->base, resources, resource_count);
}

static int poll_software_withhold_completions(
        void* context, RinVulkanProductReportV1* report_out) {
    if (g_hold_software_completions == 0u)
        return g_software_poll(context, report_out);
    if (!g_software_platform || context != g_software_platform || !report_out)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(report_out, 0, sizeof(*report_out));
    report_out->struct_size = sizeof(*report_out);
    report_out->version = RIN_VULKAN_PRODUCT_PLATFORM_VERSION;
    report_out->iommu_domain_cookie =
        g_software_platform->iommu_domain_cookie;
    report_out->iommu_map_generation =
        g_software_platform->iommu_map_generation;
    report_out->device_epoch = g_software_platform->device_epoch;
    return RIN_VULKAN_PRODUCT_OK;
}

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

static int sync_objects_round_trip(RinVkDevice device) {
    RinVkFenceCreateInfo fence_create;
    RinVkSemaphoreCreateInfo semaphore_create;
    RinVkSemaphoreTypeCreateInfo timeline_type;
    RinVkFence fence = 0u;
    RinVkSemaphore semaphore = 0u;
    uint64_t counter = 0u;
    int success = 0;

    memset(&fence_create, 0, sizeof(fence_create));
    fence_create.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    memset(&timeline_type, 0, sizeof(timeline_type));
    timeline_type.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_type.semaphoreType = RIN_VK_SEMAPHORE_TYPE_TIMELINE;
    memset(&semaphore_create, 0, sizeof(semaphore_create));
    semaphore_create.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create.pNext = &timeline_type;

    if (vkCreateFence(device, &fence_create, NULL, &fence) != RIN_VK_SUCCESS ||
        fence == 0u || vkResetFences(device, 1u, &fence) != RIN_VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphore_create, NULL, &semaphore) !=
            RIN_VK_SUCCESS ||
        semaphore == 0u || vkSignalSemaphore(device, semaphore, 1u) !=
            RIN_VK_SUCCESS ||
        vkGetSemaphoreCounterValue(device, semaphore, &counter) !=
            RIN_VK_SUCCESS ||
        counter != 1u)
        goto cleanup;
    success = 1;

cleanup:
    if (semaphore != 0u) vkDestroySemaphore(device, semaphore, NULL);
    if (fence != 0u) vkDestroyFence(device, fence, NULL);
    return success;
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
        if (!sync_objects_round_trip(state->device)) state->failed = 1;

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
    profile->queue_families[0].queue_count = 2u;
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
    RinGpuVulkanSoftwarePlatformV1 software_platform;
    RinVulkanProductPlatformV2 product_platform_v2;
    RinVkApplicationInfo application;
    RinVkInstanceCreateInfo instance_create;
    RinVkInstanceCreateInfo no_debug_instance_create;
    RinVkDebugUtilsMessengerCreateInfoEXT debug_messenger_create;
    RinVkDebugUtilsMessengerCallbackDataEXT debug_message;
    RinVkExtensionProperties instance_extension;
    RinVkDebugUtilsObjectNameInfoEXT debug_object_name;
    RinVkDebugUtilsObjectTagInfoEXT debug_object_tag;
    RinVkDebugUtilsLabelEXT debug_label;
    RinVkDeviceQueueCreateInfo queue_create;
    RinVkDeviceCreateInfo device_create;
    RinVkPhysicalDeviceVulkan12Features vulkan12_features;
    RinVkPhysicalDeviceSynchronization2Features synchronization2_features;
    RinVkBufferCreateInfo buffer_create;
    RinVkInstance instance = NULL;
    RinVkInstance no_debug_instance = NULL;
    RinVkDebugUtilsMessengerEXT debug_messenger = 0u;
    RinVkPhysicalDevice physical = NULL;
    RinVkDevice device = NULL;
    RinVkQueue queue = NULL;
    RinVkQueue second_queue = NULL;
    RinVkFence fence = 0u;
    RinVkFence second_fence = 0u;
    RinVkFence queue_order_fence = 0u;
    RinVkSemaphore semaphore = 0u;
    RinVkSemaphore deferred_semaphore = 0u;
    RinVkSemaphore timeline_gate = 0u;
    RinVkSemaphore timeline_output = 0u;
    RinVkSemaphore native_wait_semaphore = 0u;
    RinVkFenceCreateInfo fence_create;
    RinVkSemaphoreCreateInfo semaphore_create;
    RinVkSemaphoreTypeCreateInfo timeline_type;
    RinVkTimelineSemaphoreSubmitInfo timeline_values;
    RinVkSemaphoreWaitInfo timeline_wait_info;
    RinVkSubmitInfo empty_submit;
    RinVkSubmitInfo deferred_wait;
    RinVkSubmitInfo deferred_signal;
    RinVkSubmitInfo timeline_submit;
    RinVkSubmitInfo timeline_wait_submit;
    RinVkSubmitInfo timeline_signal_submit;
    RinVkSubmitInfo2 empty_submit2;
    RinVkSubmitInfo busy_product_submit;
    RinVkSubmitInfo native_wait_producer_submit;
    RinVkSubmitInfo native_wait_consumer_submit;
    RinVkSemaphoreSubmitInfo semaphore_submit_info;
    RinVkCommandBufferSubmitInfo ignored_command_info;
    RinVkCommandBuffer ignored_command_buffer = NULL;
    RinVkCommandPool command_pool = 0u;
    RinVkCommandPoolCreateInfo command_pool_create;
    RinVkCommandBufferAllocateInfo command_buffer_allocate;
    RinVkCommandBuffer command_buffer = NULL;
    RinVkCommandBuffer native_wait_command_buffer = NULL;
    RinVkCommandBufferBeginInfo command_buffer_begin;
    const char* synchronization2_extension =
        RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION;
    const char* timeline_extension = RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION;
    const char* enabled_extensions[2];
    RinVkSemaphore timeline_wait_semaphore[1];
    RinVkSemaphore timeline_signal_semaphore[1];
    uint64_t timeline_wait_value[1];
    uint64_t timeline_signal_value[1];
    uint64_t timeline_counter = 0u;
    uint32_t wait_stage = UINT32_C(0x00001000);
    uint32_t native_wait_stage = RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    float priorities[2] = {1.0f, 1.0f};
    uint32_t physical_count = 1u;
    uint32_t instance_extension_count = 0u;
    uint32_t debug_tag_payload = UINT32_C(0x52696e47);
    int runtime_initialized = 0;
    int runtime_bound = 0;
    int software_platform_initialized = 0;
    int product_platform_bound = 0;
    int result = 1;
    DebugCapture debug_capture = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0};
    const char* debug_utils_extension = RIN_VK_EXT_DEBUG_UTILS_EXTENSION;
    const char* enabled_instance_extensions[1];
    const char* unknown_instance_extension = "VK_EXT_rin_unknown";

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
    memset(&software_platform, 0, sizeof(software_platform));
    CHECK(rin_gpu_vulkan_software_platform_init(
              &software_platform, profile.iommu_domain_cookie,
              profile.device_epoch, profile.queue_families[0].queue_count,
              UINT64_C(1024) * 1024u) == RIN_VULKAN_PRODUCT_OK);
    software_platform_initialized = 1;
    g_software_submit = software_platform.platform.submit;
    g_software_poll = software_platform.platform.poll;
    g_software_platform = &software_platform;
    software_platform.platform.submit = submit_busy_once;
    software_platform.platform.poll = poll_software_withhold_completions;
    memset(&product_platform_v2, 0, sizeof(product_platform_v2));
    product_platform_v2.struct_size = sizeof(product_platform_v2);
    product_platform_v2.version = RIN_VULKAN_PRODUCT_PLATFORM_V2_VERSION;
    product_platform_v2.base = &software_platform.platform;
    product_platform_v2.prepare_submission_v2 = prepare_submission_v2;
    product_platform_v2.submit_v2 = submit_v2;
    g_v2_product_base = &software_platform.platform;
    g_v2_submit_call_count = 0u;
    g_v2_wait_count = 0u;
    memset(g_v2_waits, 0, sizeof(g_v2_waits));
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v2(
              &product_platform_v2) == RIN_GPU_VULKAN_OK);
    product_platform_bound = 1;
    CHECK(rin_gpu_vulkan_icd_unbind_product_platform(
              &software_platform.platform) == RIN_GPU_VULKAN_BUSY);

    memset(&application, 0, sizeof(application));
    application.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "RinVulkan resource thread test";
    application.apiVersion = RIN_GPU_VK_ICD_API_VERSION;
    CHECK(vkEnumerateInstanceExtensionProperties(
              NULL, &instance_extension_count, NULL) == RIN_VK_SUCCESS);
    CHECK(instance_extension_count == 1u);
    instance_extension_count = 1u;
    CHECK(vkEnumerateInstanceExtensionProperties(
              NULL, &instance_extension_count, &instance_extension) ==
          RIN_VK_SUCCESS);
    CHECK(strcmp(instance_extension.extensionName,
                 RIN_VK_EXT_DEBUG_UTILS_EXTENSION) == 0 &&
          instance_extension.specVersion == RIN_VK_DEBUG_UTILS_SPEC_VERSION);
    memset(&instance_create, 0, sizeof(instance_create));
    instance_create.sType = RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create.pApplicationInfo = &application;
    enabled_instance_extensions[0] = debug_utils_extension;
    instance_create.enabledExtensionCount = 1u;
    instance_create.ppEnabledExtensionNames = enabled_instance_extensions;
    memset(&debug_messenger_create, 0, sizeof(debug_messenger_create));
    debug_messenger_create.sType =
        RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_create.messageSeverity =
        RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_create.messageType =
        RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_messenger_create.pfnUserCallback = debug_capture_callback;
    debug_messenger_create.pUserData = &debug_capture;
    instance_create.pNext = &debug_messenger_create;
    no_debug_instance_create = instance_create;
    no_debug_instance_create.enabledExtensionCount = 1u;
    no_debug_instance_create.ppEnabledExtensionNames =
        &unknown_instance_extension;
    no_debug_instance_create.pNext = NULL;
    CHECK(vkCreateInstance(&no_debug_instance_create, NULL,
                           &no_debug_instance) ==
          RIN_VK_ERROR_EXTENSION_NOT_PRESENT);
    CHECK(no_debug_instance == NULL);
    no_debug_instance_create.enabledExtensionCount = 0u;
    no_debug_instance_create.ppEnabledExtensionNames = NULL;
    CHECK(vkCreateInstance(&no_debug_instance_create, NULL,
                           &no_debug_instance) == RIN_VK_SUCCESS);
    CHECK(vkGetInstanceProcAddr(no_debug_instance,
              "vkCreateDebugUtilsMessengerEXT") == NULL);
    vkDestroyInstance(no_debug_instance, NULL);
    no_debug_instance = NULL;
    CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v2(
              &product_platform_v2) == RIN_GPU_VULKAN_OK);
    product_platform_bound = 0;
    CHECK(rin_gpu_vulkan_icd_unbind_runtime(&runtime) == RIN_GPU_VULKAN_OK);
    runtime_bound = 0;
    CHECK(vkCreateInstance(&instance_create, NULL, &instance) ==
          RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(instance == NULL && debug_capture.instance_failed_count == 1u &&
          debug_capture.malformed == 0);
    CHECK(rin_gpu_vulkan_icd_bind_runtime(&runtime) == RIN_GPU_VULKAN_OK);
    runtime_bound = 1;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v2(
              &product_platform_v2) == RIN_GPU_VULKAN_OK);
    product_platform_bound = 1;
    CHECK(vkCreateInstance(&instance_create, NULL, &instance) == RIN_VK_SUCCESS);
    CHECK(debug_capture.callback_count == 2u &&
          debug_capture.instance_failed_count == 1u &&
          debug_capture.malformed == 0);
    CHECK(vkGetInstanceProcAddr(instance,
              "vkCreateDebugUtilsMessengerEXT") != NULL);
    CHECK(vkGetInstanceProcAddr(instance,
              "vkSubmitDebugUtilsMessageEXT") != NULL);
    CHECK(vkCreateDebugUtilsMessengerEXT(
              instance, &debug_messenger_create, NULL, &debug_messenger) ==
          RIN_VK_SUCCESS);
    CHECK(debug_messenger != 0u);
    memset(&debug_message, 0, sizeof(debug_message));
    debug_message.sType =
        RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    debug_message.pMessageIdName = "app.message";
    debug_message.pMessage = "validation probe";
    vkSubmitDebugUtilsMessageEXT(
        instance, RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &debug_message);
    CHECK(debug_capture.submitted_message_count == 1u &&
          debug_capture.malformed == 0);
    CHECK(vkEnumeratePhysicalDevices(instance, &physical_count, &physical) ==
          RIN_VK_SUCCESS);
    CHECK(physical_count == 1u && physical != NULL);

    memset(&queue_create, 0, sizeof(queue_create));
    queue_create.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueCount = 2u;
    queue_create.pQueuePriorities = priorities;
    memset(&device_create, 0, sizeof(device_create));
    device_create.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    memset(&vulkan12_features, 0, sizeof(vulkan12_features));
    vulkan12_features.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12_features.timelineSemaphore = 1u;
    memset(&synchronization2_features, 0, sizeof(synchronization2_features));
    synchronization2_features.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2_features.synchronization2 = 1u;
    vulkan12_features.pNext = &synchronization2_features;
    device_create.pNext = &vulkan12_features;
    device_create.queueCreateInfoCount = 1u;
    device_create.pQueueCreateInfos = &queue_create;
    enabled_extensions[0] = synchronization2_extension;
    enabled_extensions[1] = timeline_extension;
    device_create.enabledExtensionCount = 2u;
    device_create.ppEnabledExtensionNames = enabled_extensions;
    CHECK(vkCreateDevice(physical, &device_create, NULL, &device) ==
          RIN_VK_SUCCESS);
    vkGetDeviceQueue(device, 0u, 0u, &queue);
    vkGetDeviceQueue(device, 0u, 1u, &second_queue);
    CHECK(queue != NULL && second_queue != NULL);
    CHECK(vkGetDeviceProcAddr(device,
              "vkCmdBeginDebugUtilsLabelEXT") != NULL);
    CHECK(vkGetDeviceProcAddr(device,
              "vkSetDebugUtilsObjectNameEXT") != NULL);
    memset(&debug_label, 0, sizeof(debug_label));
    debug_label.sType = RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    debug_label.pLabelName = "main queue";
    debug_label.color[0] = 0.25f;
    vkQueueBeginDebugUtilsLabelEXT(queue, &debug_label);
    vkQueueInsertDebugUtilsLabelEXT(queue, &debug_label);
    vkQueueEndDebugUtilsLabelEXT(queue);
    CHECK(debug_capture.queue_label_count == 1u &&
          debug_capture.malformed == 0);

    memset(&fence_create, 0, sizeof(fence_create));
    fence_create.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    memset(&semaphore_create, 0, sizeof(semaphore_create));
    semaphore_create.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CHECK(vkCreateFence(device, &fence_create, NULL, &fence) ==
          RIN_VK_SUCCESS);
    memset(&command_pool_create, 0, sizeof(command_pool_create));
    command_pool_create.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create.queueFamilyIndex = 0u;
    CHECK(vkCreateCommandPool(device, &command_pool_create, NULL,
                              &command_pool) == RIN_VK_SUCCESS);
    memset(&command_buffer_allocate, 0, sizeof(command_buffer_allocate));
    command_buffer_allocate.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate.commandPool = command_pool;
    command_buffer_allocate.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate.commandBufferCount = 1u;
    CHECK(vkAllocateCommandBuffers(device, &command_buffer_allocate,
                                   &command_buffer) == RIN_VK_SUCCESS);
    memset(&command_buffer_begin, 0, sizeof(command_buffer_begin));
    command_buffer_begin.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin) ==
          RIN_VK_SUCCESS);
    memset(&debug_object_name, 0, sizeof(debug_object_name));
    debug_object_name.sType =
        RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    debug_object_name.objectType = RIN_VK_OBJECT_TYPE_COMMAND_BUFFER;
    debug_object_name.objectHandle = (uint64_t)(uintptr_t)command_buffer;
    debug_object_name.pObjectName = "command buffer";
    CHECK(vkSetDebugUtilsObjectNameEXT(device, &debug_object_name) ==
          RIN_VK_SUCCESS);
    memset(&debug_object_tag, 0, sizeof(debug_object_tag));
    debug_object_tag.sType =
        RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT;
    debug_object_tag.objectType = debug_object_name.objectType;
    debug_object_tag.objectHandle = debug_object_name.objectHandle;
    debug_object_tag.tagName = UINT64_C(0x72696e74657374);
    debug_object_tag.tagSize = sizeof(debug_capture.callback_count);
    debug_object_tag.pTag = &debug_tag_payload;
    CHECK(vkSetDebugUtilsObjectTagEXT(device, &debug_object_tag) ==
          RIN_VK_SUCCESS);
    debug_label.pLabelName = "record transfer";
    debug_label.color[1] = 0.5f;
    vkCmdBeginDebugUtilsLabelEXT(command_buffer, &debug_label);
    vkCmdInsertDebugUtilsLabelEXT(command_buffer, &debug_label);
    vkCmdEndDebugUtilsLabelEXT(command_buffer);
    if (debug_capture.object_name_count != 1u ||
        debug_capture.object_tag_count != 1u ||
        debug_capture.command_label_count != 2u ||
        debug_capture.malformed != 0)
        fprintf(stderr,
                "debug captures name=%u tag=%u command-label=%u queue-label=%u submitted=%u malformed=%d callbacks=%u\n",
                debug_capture.object_name_count,
                debug_capture.object_tag_count,
                debug_capture.command_label_count,
                debug_capture.queue_label_count,
                debug_capture.submitted_message_count,
                debug_capture.malformed, debug_capture.callback_count);
    CHECK(debug_capture.object_name_count == 1u &&
          debug_capture.object_tag_count == 1u &&
          debug_capture.command_label_count == 2u &&
          debug_capture.callback_count == 11u &&
          debug_capture.malformed == 0);
    vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, NULL);
    debug_messenger = 0u;
    vkSubmitDebugUtilsMessageEXT(
        instance, RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &debug_message);
    CHECK(debug_capture.callback_count == 11u);
    CHECK(vkEndCommandBuffer(command_buffer) == RIN_VK_SUCCESS);
    memset(&busy_product_submit, 0, sizeof(busy_product_submit));
    busy_product_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    busy_product_submit.commandBufferCount = 1u;
    busy_product_submit.pCommandBuffers = &command_buffer;
    g_busy_submit_responses = 2u;
    g_submit_call_count = 0u;
    CHECK(vkQueueSubmit(queue, 1u, &busy_product_submit, fence) ==
          RIN_VK_SUCCESS);
    CHECK(g_busy_submit_responses == 1u && g_submit_call_count == 1u);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_NOT_READY);
    CHECK(g_busy_submit_responses == 0u && g_submit_call_count == 2u);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(g_submit_call_count == 3u);
    CHECK(software_platform.completed_values[0] != 0u);

    CHECK(vkAllocateCommandBuffers(device, &command_buffer_allocate,
                                   &native_wait_command_buffer) ==
          RIN_VK_SUCCESS);
    CHECK(vkBeginCommandBuffer(native_wait_command_buffer,
                               &command_buffer_begin) == RIN_VK_SUCCESS);
    CHECK(vkEndCommandBuffer(native_wait_command_buffer) == RIN_VK_SUCCESS);
    CHECK(vkCreateSemaphore(device, &semaphore_create, NULL,
                            &native_wait_semaphore) == RIN_VK_SUCCESS);
    g_hold_software_completions = 1u;
    memset(&native_wait_producer_submit, 0,
           sizeof(native_wait_producer_submit));
    native_wait_producer_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    native_wait_producer_submit.commandBufferCount = 1u;
    native_wait_producer_submit.pCommandBuffers = &command_buffer;
    native_wait_producer_submit.signalSemaphoreCount = 1u;
    native_wait_producer_submit.pSignalSemaphores = &native_wait_semaphore;
    CHECK(vkQueueSubmit(queue, 1u, &native_wait_producer_submit, 0u) ==
          RIN_VK_SUCCESS);
    memset(&native_wait_consumer_submit, 0,
           sizeof(native_wait_consumer_submit));
    native_wait_consumer_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    native_wait_consumer_submit.waitSemaphoreCount = 1u;
    native_wait_consumer_submit.pWaitSemaphores = &native_wait_semaphore;
    native_wait_consumer_submit.pWaitDstStageMask = &native_wait_stage;
    native_wait_consumer_submit.commandBufferCount = 1u;
    native_wait_consumer_submit.pCommandBuffers =
        &native_wait_command_buffer;
    {
        const RinVkResult native_submit_result = vkQueueSubmit(
            second_queue, 1u, &native_wait_consumer_submit, 0u);
        if (native_submit_result != RIN_VK_SUCCESS)
            fprintf(stderr,
                    "native wait submit result=%d calls=%u count=%u queue=%u value=%llu\n",
                    native_submit_result, g_v2_submit_call_count,
                    g_v2_wait_count, g_v2_waits[0].queue_id,
                    (unsigned long long)g_v2_waits[0].completion_value);
        CHECK(native_submit_result == RIN_VK_SUCCESS);
    }
    if (!(g_v2_submit_call_count == 1u && g_v2_wait_count == 1u &&
          g_v2_waits[0].struct_size == sizeof(g_v2_waits[0]) &&
          g_v2_waits[0].version ==
              RIN_VULKAN_PRODUCT_SUBMISSION_WAIT_VERSION &&
          g_v2_waits[0].queue_id == 0u &&
          g_v2_waits[0].completion_value != 0u))
        fprintf(stderr,
                "native waits: calls=%u count=%u size=%u version=%u queue=%u value=%llu\n",
                g_v2_submit_call_count, g_v2_wait_count,
                g_v2_waits[0].struct_size, g_v2_waits[0].version,
                g_v2_waits[0].queue_id,
                (unsigned long long)g_v2_waits[0].completion_value);
    CHECK(g_v2_submit_call_count == 1u && g_v2_wait_count == 1u &&
          g_v2_waits[0].struct_size == sizeof(g_v2_waits[0]) &&
          g_v2_waits[0].version ==
              RIN_VULKAN_PRODUCT_SUBMISSION_WAIT_VERSION &&
          g_v2_waits[0].queue_id == 0u &&
          g_v2_waits[0].completion_value != 0u);
    CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v2(
              &product_platform_v2) == RIN_GPU_VULKAN_BUSY);
    g_hold_software_completions = 0u;
    CHECK(vkDeviceWaitIdle(device) == RIN_VK_SUCCESS);
    memset(&native_wait_producer_submit, 0,
           sizeof(native_wait_producer_submit));
    native_wait_producer_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    native_wait_producer_submit.signalSemaphoreCount = 1u;
    native_wait_producer_submit.pSignalSemaphores = &native_wait_semaphore;
    memset(&native_wait_consumer_submit, 0,
           sizeof(native_wait_consumer_submit));
    native_wait_consumer_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    native_wait_consumer_submit.waitSemaphoreCount = 1u;
    native_wait_consumer_submit.pWaitSemaphores = &native_wait_semaphore;
    native_wait_consumer_submit.pWaitDstStageMask = &wait_stage;
    CHECK(vkQueueSubmit(queue, 1u, &native_wait_producer_submit, 0u) ==
          RIN_VK_SUCCESS);
    CHECK(vkQueueSubmit(second_queue, 1u, &native_wait_consumer_submit, 0u) ==
          RIN_VK_SUCCESS);
    CHECK(vkQueueSubmit(queue, 1u, &native_wait_producer_submit, 0u) ==
          RIN_VK_SUCCESS);
    CHECK(vkQueueSubmit(second_queue, 1u, &native_wait_consumer_submit, 0u) ==
          RIN_VK_SUCCESS);
    CHECK(g_v2_submit_call_count == 1u);
    vkDestroyCommandPool(device, command_pool, NULL);
    command_pool = 0u;
    command_buffer = NULL;
    software_platform.platform.submit = g_software_submit;
    g_software_submit = NULL;
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);
    CHECK(vkQueueSubmit(queue, 0u, NULL, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);
    CHECK(vkQueueSubmit2(queue, 0u, NULL, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);

    CHECK(vkCreateFence(device, &fence_create, NULL, &second_fence) ==
          RIN_VK_SUCCESS);
    CHECK(vkCreateFence(device, &fence_create, NULL, &queue_order_fence) ==
          RIN_VK_SUCCESS);
    CHECK(vkCreateSemaphore(device, &semaphore_create, NULL,
                            &deferred_semaphore) == RIN_VK_SUCCESS);
    memset(&deferred_wait, 0, sizeof(deferred_wait));
    deferred_wait.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    deferred_wait.waitSemaphoreCount = 1u;
    deferred_wait.pWaitSemaphores = &deferred_semaphore;
    deferred_wait.pWaitDstStageMask = &wait_stage;
    CHECK(vkQueueSubmit(queue, 1u, &deferred_wait, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_NOT_READY);
    CHECK(vkQueueSubmit2(queue, 0u, NULL, queue_order_fence) ==
          RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, queue_order_fence) == RIN_VK_NOT_READY);
    memset(&deferred_signal, 0, sizeof(deferred_signal));
    deferred_signal.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    deferred_signal.signalSemaphoreCount = 1u;
    deferred_signal.pSignalSemaphores = &deferred_semaphore;
    CHECK(vkQueueSubmit(second_queue, 1u, &deferred_signal, second_fence) ==
          RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, second_fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, queue_order_fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);

    CHECK(vkCreateSemaphore(device, &semaphore_create, NULL, &semaphore) ==
          RIN_VK_SUCCESS);
    memset(&empty_submit, 0, sizeof(empty_submit));
    empty_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    empty_submit.signalSemaphoreCount = 1u;
    empty_submit.pSignalSemaphores = &semaphore;
    CHECK(vkQueueSubmit(queue, 1u, &empty_submit, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);

    memset(&empty_submit, 0, sizeof(empty_submit));
    empty_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    empty_submit.waitSemaphoreCount = 1u;
    empty_submit.pWaitSemaphores = &semaphore;
    empty_submit.pWaitDstStageMask = &wait_stage;
    CHECK(vkQueueSubmit(queue, 1u, &empty_submit, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);

    memset(&semaphore_submit_info, 0, sizeof(semaphore_submit_info));
    semaphore_submit_info.sType =
        RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphore_submit_info.semaphore = semaphore;
    semaphore_submit_info.stageMask =
        RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memset(&empty_submit2, 0, sizeof(empty_submit2));
    empty_submit2.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    empty_submit2.signalSemaphoreInfoCount = 1u;
    empty_submit2.pSignalSemaphoreInfos = &semaphore_submit_info;
    CHECK(vkQueueSubmit2(queue, 1u, &empty_submit2, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);

    empty_submit2.signalSemaphoreInfoCount = 0u;
    empty_submit2.pSignalSemaphoreInfos = NULL;
    empty_submit2.waitSemaphoreInfoCount = 1u;
    empty_submit2.pWaitSemaphoreInfos = &semaphore_submit_info;
    CHECK(vkQueueSubmit2(queue, 1u, &empty_submit2, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);

    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);
    memset(&ignored_command_info, 0, sizeof(ignored_command_info));
    ignored_command_info.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    memset(&empty_submit, 0, sizeof(empty_submit));
    empty_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    empty_submit.pWaitSemaphores = &semaphore;
    empty_submit.pWaitDstStageMask = &wait_stage;
    empty_submit.pCommandBuffers = &ignored_command_buffer;
    empty_submit.pSignalSemaphores = &semaphore;
    CHECK(vkQueueSubmit(queue, 1u, &empty_submit, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);
    CHECK(vkResetFences(device, 1u, &fence) == RIN_VK_SUCCESS);

    memset(&empty_submit2, 0, sizeof(empty_submit2));
    empty_submit2.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    empty_submit2.pWaitSemaphoreInfos = &semaphore_submit_info;
    empty_submit2.pCommandBufferInfos = &ignored_command_info;
    empty_submit2.pSignalSemaphoreInfos = &semaphore_submit_info;
    CHECK(vkQueueSubmit2(queue, 1u, &empty_submit2, fence) == RIN_VK_SUCCESS);
    CHECK(vkGetFenceStatus(device, fence) == RIN_VK_SUCCESS);

    memset(&timeline_type, 0, sizeof(timeline_type));
    timeline_type.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_type.semaphoreType = RIN_VK_SEMAPHORE_TYPE_TIMELINE;
    memset(&semaphore_create, 0, sizeof(semaphore_create));
    semaphore_create.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create.pNext = &timeline_type;
    CHECK(vkCreateSemaphore(device, &semaphore_create, NULL, &timeline_gate) ==
          RIN_VK_SUCCESS);
    CHECK(vkCreateSemaphore(device, &semaphore_create, NULL,
                            &timeline_output) == RIN_VK_SUCCESS);
    semaphore_create.pNext = NULL;
    CHECK(vkSignalSemaphore(device, timeline_gate, 0u) ==
          RIN_VK_ERROR_INITIALIZATION_FAILED);
    memset(&timeline_values, 0, sizeof(timeline_values));
    timeline_values.sType =
        RIN_VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_values.waitSemaphoreValueCount = 1u;
    timeline_values.pWaitSemaphoreValues = timeline_wait_value;
    timeline_values.signalSemaphoreValueCount = 1u;
    timeline_values.pSignalSemaphoreValues = timeline_signal_value;
    timeline_wait_semaphore[0] = timeline_gate;
    timeline_signal_semaphore[0] = timeline_output;
    timeline_wait_value[0] = 1u;
    timeline_signal_value[0] = 2u;
    memset(&timeline_submit, 0, sizeof(timeline_submit));
    timeline_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    timeline_submit.pNext = &timeline_values;
    timeline_submit.waitSemaphoreCount = 1u;
    timeline_submit.pWaitSemaphores = timeline_wait_semaphore;
    timeline_submit.pWaitDstStageMask = &wait_stage;
    timeline_submit.signalSemaphoreCount = 1u;
    timeline_submit.pSignalSemaphores = timeline_signal_semaphore;
    CHECK(vkQueueSubmit(queue, 1u, &timeline_submit, 0u) == RIN_VK_SUCCESS);
    timeline_signal_value[0] = 3u;
    CHECK(vkQueueSubmit(queue, 1u, &timeline_submit, 0u) == RIN_VK_SUCCESS);
    CHECK(vkGetSemaphoreCounterValue(device, timeline_output,
                                     &timeline_counter) == RIN_VK_SUCCESS);
    CHECK(timeline_counter == 0u);
    memset(&timeline_wait_info, 0, sizeof(timeline_wait_info));
    timeline_wait_info.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    timeline_wait_info.semaphoreCount = 1u;
    timeline_wait_info.pSemaphores = timeline_signal_semaphore;
    timeline_wait_info.pValues = timeline_signal_value;
    timeline_wait_value[0] = 3u;
    CHECK(vkWaitSemaphores(device, &timeline_wait_info, 0u) ==
          RIN_VK_NOT_READY);
    CHECK(vkWaitSemaphores(device, &timeline_wait_info, UINT64_C(1000000)) ==
          RIN_VK_TIMEOUT);
    CHECK(vkSignalSemaphore(device, timeline_output, 2u) ==
          RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(vkSignalSemaphore(device, timeline_output, 1u) == RIN_VK_SUCCESS);
    CHECK(vkGetSemaphoreCounterValue(device, timeline_output,
                                     &timeline_counter) == RIN_VK_SUCCESS);
    CHECK(timeline_counter == 1u);

    timeline_wait_semaphore[0] = timeline_output;
    timeline_wait_value[0] = 3u;
    timeline_values.signalSemaphoreValueCount = 0u;
    timeline_values.pSignalSemaphoreValues = NULL;
    memset(&timeline_wait_submit, 0, sizeof(timeline_wait_submit));
    timeline_wait_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    timeline_wait_submit.pNext = &timeline_values;
    timeline_wait_submit.waitSemaphoreCount = 1u;
    timeline_wait_submit.pWaitSemaphores = timeline_wait_semaphore;
    timeline_wait_submit.pWaitDstStageMask = &wait_stage;
    CHECK(vkQueueSubmit(second_queue, 1u, &timeline_wait_submit, 0u) ==
          RIN_VK_SUCCESS);

    timeline_values.waitSemaphoreValueCount = 0u;
    timeline_values.pWaitSemaphoreValues = NULL;
    timeline_values.signalSemaphoreValueCount = 1u;
    timeline_values.pSignalSemaphoreValues = timeline_signal_value;
    timeline_signal_value[0] = 4u;
    memset(&timeline_signal_submit, 0, sizeof(timeline_signal_submit));
    timeline_signal_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    timeline_signal_submit.pNext = &timeline_values;
    timeline_signal_submit.signalSemaphoreCount = 1u;
    timeline_signal_submit.pSignalSemaphores = timeline_signal_semaphore;
    CHECK(vkQueueSubmit(second_queue, 1u, &timeline_signal_submit, 0u) ==
          RIN_VK_SUCCESS);

    CHECK(vkSignalSemaphore(device, timeline_gate, 1u) == RIN_VK_SUCCESS);
    CHECK(vkGetSemaphoreCounterValue(device, timeline_output,
                                     &timeline_counter) == RIN_VK_SUCCESS);
    CHECK(timeline_counter == 4u);
    CHECK(vkSignalSemaphore(device, timeline_output, 4u) ==
          RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(vkSignalSemaphore(device, timeline_output, 5u) == RIN_VK_SUCCESS);

    memset(&buffer_create, 0, sizeof(buffer_create));
    buffer_create.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create.size = 4096u;
    buffer_create.usage = RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                          RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_create.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    CHECK(run_resource_stress(device, &buffer_create));
    CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v2(
              &product_platform_v2) == RIN_GPU_VULKAN_OK);
    product_platform_bound = 0;
    g_v2_product_base = NULL;
    CHECK(rin_gpu_vulkan_software_platform_shutdown(&software_platform) ==
          RIN_VULKAN_PRODUCT_OK);
    software_platform_initialized = 0;
    result = 0;

cleanup:
    g_hold_software_completions = 0u;
    if (no_debug_instance) vkDestroyInstance(no_debug_instance, NULL);
    if (deferred_semaphore)
        vkDestroySemaphore(device, deferred_semaphore, NULL);
    if (timeline_gate) vkDestroySemaphore(device, timeline_gate, NULL);
    if (timeline_output) vkDestroySemaphore(device, timeline_output, NULL);
    if (native_wait_semaphore)
        vkDestroySemaphore(device, native_wait_semaphore, NULL);
    if (queue_order_fence) vkDestroyFence(device, queue_order_fence, NULL);
    if (second_fence) vkDestroyFence(device, second_fence, NULL);
    if (semaphore) vkDestroySemaphore(device, semaphore, NULL);
    if (fence) vkDestroyFence(device, fence, NULL);
    if (device) vkDestroyDevice(device, NULL);
    if (debug_messenger && instance)
        vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, NULL);
    if (instance) vkDestroyInstance(instance, NULL);
    if (product_platform_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform_v2(
            &product_platform_v2);
    if (software_platform_initialized)
        (void)rin_gpu_vulkan_software_platform_shutdown(&software_platform);
    if (runtime_bound)
        (void)rin_gpu_vulkan_icd_unbind_runtime(&runtime);
    if (runtime_initialized)
        (void)rin_gpu_vulkan_runtime_shutdown(&runtime);
#undef CHECK
    return result;
}
