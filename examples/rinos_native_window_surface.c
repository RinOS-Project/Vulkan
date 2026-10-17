/* SPDX-License-Identifier: MIT */
/* One-frame RinOS application example for the ordinary Compositor WSI path.
 * It binds the admitted physical profile, process memory/resource adapter,
 * and canonical process-submit route. Presentation remains a Compositor
 * import and is not the separate VK_KHR_display direct-scanout path. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <rinvulkan/icd.h>
#include <rin/contract_abi.h>
#include <rinruntime/window.h>
#include "rinos_native_window_completion.h"

#if defined(__rinos__)
#include <rin/base.h>
#include <rinvulkan/rinos_catalog_v1.h>
#include <rinvulkan/rinos_process_product_platform_v1.h>
#include "rinos_native_window_runtime_lifecycle_v1.h"
#include "rinos_sdk_target_backend_v1.h"
#include <stdio.h>
#include <string.h>

static RinVulkanRinOSCatalogV1 g_rinos_vulkan_catalog;
static RinGpuVulkanRuntimeV1 g_rinos_vulkan_runtime;
static RinVulkanRinOSProcessProductPlatformV1 g_rinos_product_platform;
static RinVulkanRinOSProcessSubmitBackendV7 g_rinos_process_submit_backend;
static uint32_t g_rinos_product_binding_mask;
static RinVulkanExampleRuntimeLifecycleV1 g_rinos_vulkan_lifecycle = {
    sizeof(RinVulkanExampleRuntimeLifecycleV1),
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_V1_VERSION, 0u, 0u};

enum {
    RINOS_PRODUCT_BIND_V1 = 1u << 0,
    RINOS_PRODUCT_BIND_V2 = 1u << 1,
    RINOS_PRODUCT_BIND_V3 = 1u << 2,
    RINOS_PRODUCT_BIND_V4 = 1u << 3,
    RINOS_PRODUCT_BIND_V5 = 1u << 4,
    RINOS_PRODUCT_BIND_V6 = 1u << 5,
    RINOS_PRODUCT_BIND_V7 = 1u << 6,
    RINOS_PRODUCT_BIND_V8 = 1u << 7,
    RINOS_PRODUCT_BIND_V9 = 1u << 8,
    RINOS_PRODUCT_BIND_V10 = 1u << 9,
    RINOS_PRODUCT_BIND_V11 = 1u << 10
};

static int bind_rinos_product_platform(uint64_t device_id,
                                       uint64_t device_generation,
                                       uint32_t queue_family) {
    RinVulkanRinOSQueueCreateInfoV1 queue_info;
    RinVulkanRinOSProcessQueueMapV4 mappings[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_MAX_QUEUE_MAPPINGS];
    uint32_t mapping_count = 0u;
    int result;

    if (device_id == 0u || device_generation == 0u ||
        g_rinos_product_binding_mask != 0u ||
        g_rinos_product_platform.struct_size != 0u ||
        g_rinos_process_submit_backend.struct_size != 0u) {
        fprintf(stderr, "RinOS product-platform lifecycle is not empty\n");
        return 0;
    }
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.queue_family_index = queue_family;
    queue_info.queue_count = 1u;
    memset(mappings, 0, sizeof(mappings));
    result = rin_vulkan_rinos_catalog_build_queue_maps_v1(
        &g_rinos_vulkan_catalog, device_id, device_generation, &queue_info,
        1u, mappings,
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_MAX_QUEUE_MAPPINGS,
        &mapping_count);
    if (result != RIN_GPU_VULKAN_OK || mapping_count != 1u) {
        fprintf(stderr, "RinOS queue-route admission failed: %d (%u)\n",
                result, mapping_count);
        return 0;
    }
    result = rin_vulkan_rinos_process_product_platform_v1_init(
        &g_rinos_product_platform, device_id, device_generation);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        fprintf(stderr, "RinOS product-platform initialization failed: %d\n",
                result);
        return 0;
    }

#define RINOS_BIND_PRODUCT(stage, expression, label)                        \
    do {                                                                     \
        result = (expression);                                               \
        if (result != RIN_GPU_VULKAN_OK) {                                   \
            fprintf(stderr, "%s failed: %d\n", (label), result);           \
            return 0;                                                        \
        }                                                                    \
        g_rinos_product_binding_mask |= (stage);                             \
    } while (0)

    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V1,
        rin_gpu_vulkan_icd_bind_product_platform(
            &g_rinos_product_platform.platform_v1),
        "bind product platform V1");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V2,
        rin_gpu_vulkan_icd_bind_product_platform_v2(
            &g_rinos_product_platform.platform_v2),
        "bind product platform V2");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V3,
        rin_gpu_vulkan_icd_bind_product_platform_v3(
            &g_rinos_product_platform.platform_v3),
        "bind product platform V3");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V4,
        rin_gpu_vulkan_icd_bind_product_platform_v4(
            &g_rinos_product_platform.platform_v4),
        "bind product platform V4");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V5,
        rin_gpu_vulkan_icd_bind_product_platform_v5(
            &g_rinos_product_platform.platform_v5),
        "bind product platform V5");

    result = rin_vulkan_rinos_process_submit_backend_v7_init(
        &g_rinos_process_submit_backend,
        &g_rinos_product_platform.platform_v5, device_id, device_generation,
        mappings, mapping_count);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        fprintf(stderr, "RinOS process-submit backend init failed: %d\n",
                result);
        return 0;
    }
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V6,
        rin_gpu_vulkan_icd_bind_product_platform_v6(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.submit_backend.platform_v6),
        "bind product platform V6");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V7,
        rin_gpu_vulkan_icd_bind_product_platform_v7(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.submit_backend.platform_v7),
        "bind product platform V7");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V8,
        rin_gpu_vulkan_icd_bind_product_platform_v8(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.submit_backend.platform_v8),
        "bind product platform V8");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V9,
        rin_gpu_vulkan_icd_bind_product_platform_v9(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.platform_v9),
        "bind product platform V9");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V10,
        rin_gpu_vulkan_icd_bind_product_platform_v10(
            rin_vulkan_rinos_process_submit_backend_v6_platform_v10(
                &g_rinos_process_submit_backend.submit_backend)),
        "bind product platform V10");
    RINOS_BIND_PRODUCT(
        RINOS_PRODUCT_BIND_V11,
        rin_gpu_vulkan_icd_bind_product_platform_v11(
            rin_vulkan_rinos_process_submit_backend_v7_platform_v11(
                &g_rinos_process_submit_backend)),
        "bind product platform V11");
#undef RINOS_BIND_PRODUCT
    return 1;
}

static int cleanup_rinos_product_platform(void) {
    int result;

#define RINOS_UNBIND_PRODUCT(stage, expression, label)                      \
    do {                                                                     \
        if ((g_rinos_product_binding_mask & (stage)) != 0u) {                \
            result = (expression);                                           \
            if (result != RIN_GPU_VULKAN_OK) {                               \
                fprintf(stderr, "%s failed: %d; retaining parent lifetimes\n", \
                        (label), result);                                    \
                return 0;                                                    \
            }                                                                \
            g_rinos_product_binding_mask &= ~(stage);                       \
        }                                                                    \
    } while (0)

    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V11,
        rin_gpu_vulkan_icd_unbind_product_platform_v11(
            rin_vulkan_rinos_process_submit_backend_v7_platform_v11(
                &g_rinos_process_submit_backend)),
        "unbind product platform V11");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V10,
        rin_gpu_vulkan_icd_unbind_product_platform_v10(
            rin_vulkan_rinos_process_submit_backend_v6_platform_v10(
                &g_rinos_process_submit_backend.submit_backend)),
        "unbind product platform V10");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V9,
        rin_gpu_vulkan_icd_unbind_product_platform_v9(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.platform_v9),
        "unbind product platform V9");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V8,
        rin_gpu_vulkan_icd_unbind_product_platform_v8(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.submit_backend.platform_v8),
        "unbind product platform V8");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V7,
        rin_gpu_vulkan_icd_unbind_product_platform_v7(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.submit_backend.platform_v7),
        "unbind product platform V7");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V6,
        rin_gpu_vulkan_icd_unbind_product_platform_v6(
            &g_rinos_process_submit_backend.submit_backend
                 .submit_backend.submit_backend.platform_v6),
        "unbind product platform V6");
    if (g_rinos_process_submit_backend.struct_size != 0u) {
        result = rin_vulkan_rinos_process_submit_backend_v7_detach(
            &g_rinos_process_submit_backend);
        if (result != RIN_VULKAN_PRODUCT_OK) {
            fprintf(stderr,
                    "RinOS process-submit backend detach failed: %d; "
                    "retaining parent lifetimes\n", result);
            return 0;
        }
    }
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V5,
        rin_gpu_vulkan_icd_unbind_product_platform_v5(
            &g_rinos_product_platform.platform_v5),
        "unbind product platform V5");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V4,
        rin_gpu_vulkan_icd_unbind_product_platform_v4(
            &g_rinos_product_platform.platform_v4),
        "unbind product platform V4");
    RINOS_UNBIND_PRODUCT(
        RINOS_PRODUCT_BIND_V3,
        rin_gpu_vulkan_icd_unbind_product_platform_v3(
            &g_rinos_product_platform.platform_v3),
        "unbind product platform V3");
    if ((g_rinos_product_binding_mask & RINOS_PRODUCT_BIND_V2) != 0u) {
        result = rin_gpu_vulkan_icd_unbind_product_platform_v2(
            &g_rinos_product_platform.platform_v2);
        if (result != RIN_GPU_VULKAN_OK) {
            fprintf(stderr,
                    "unbind product platform V2 failed: %d; retaining "
                    "parent lifetimes\n", result);
            return 0;
        }
        g_rinos_product_binding_mask &=
            ~(RINOS_PRODUCT_BIND_V2 | RINOS_PRODUCT_BIND_V1);
    } else {
        RINOS_UNBIND_PRODUCT(
            RINOS_PRODUCT_BIND_V1,
            rin_gpu_vulkan_icd_unbind_product_platform(
                &g_rinos_product_platform.platform_v1),
            "unbind product platform V1");
    }
    if (g_rinos_product_platform.struct_size != 0u) {
        result = rin_vulkan_rinos_process_product_platform_v1_destroy(
            &g_rinos_product_platform);
        if (result != RIN_VULKAN_PRODUCT_OK) {
            fprintf(stderr, "RinOS product-platform destroy failed: %d\n",
                    result);
            return 0;
        }
    }
#undef RINOS_UNBIND_PRODUCT
    return g_rinos_product_binding_mask == 0u;
}

static int cleanup_unbind_rinos_runtime(void* context) {
    (void)context;
    return rin_gpu_vulkan_icd_unbind_runtime(&g_rinos_vulkan_runtime);
}

static int cleanup_shutdown_rinos_runtime(void* context) {
    (void)context;
    return rin_gpu_vulkan_runtime_shutdown(&g_rinos_vulkan_runtime);
}

static int cleanup_disconnect_rinos_catalog(void* context) {
    (void)context;
    return rin_vulkan_rinos_catalog_disconnect_v1(&g_rinos_vulkan_catalog);
}

static int cleanup_rinos_product_platform_action(void* context) {
    (void)context;
    return cleanup_rinos_product_platform() ? 0 :
        RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_ACTION_FAILED;
}

static int cleanup_rinos_owned_state(void) {
    RinVulkanExampleRuntimeLifecycleActionsV1 actions;
    uint32_t failed_stage = RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_NONE;
    int action_result = 0;
    int result;

    memset(&actions, 0, sizeof(actions));
    actions.struct_size = sizeof(actions);
    actions.version = RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_V1_VERSION;
    actions.context = &g_rinos_vulkan_lifecycle;
    actions.unbind_runtime = cleanup_unbind_rinos_runtime;
    actions.shutdown_runtime = cleanup_shutdown_rinos_runtime;
    actions.disconnect_catalog = cleanup_disconnect_rinos_catalog;
    result = rin_vulkan_example_runtime_lifecycle_cleanup_entry_v1(
        &g_rinos_vulkan_lifecycle, &actions,
        cleanup_rinos_product_platform_action, &g_rinos_product_platform,
        &failed_stage, &action_result);
    if (result != RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_OK) {
        fprintf(stderr,
                "RinVk owned-state cleanup failed at stage %u: %d (%d)\n",
                failed_stage, result, action_result);
        return 0;
    }
    return 1;
}

static int bind_admitted_vulkan_catalog(void) {
    uint64_t handle_secret = 0u;
    RinSliceV1 random_bytes;
    RinResult sdk_result;
    int result;

    if (g_rinos_vulkan_lifecycle.flags != 0u) {
        fprintf(stderr, "RinVk runtime lifecycle still owns resources\n");
        return 0;
    }
    random_bytes.address = (uint64_t)(uintptr_t)&handle_secret;
    random_bytes.size = sizeof(handle_secret);
    sdk_result = rin_random_fill_v1(random_bytes);
    if (sdk_result != RIN_SUCCESS || handle_secret == 0u) {
        fprintf(stderr, "RinOS secure runtime secret unavailable: %d\n",
                (int)sdk_result);
        return 0;
    }
    result = rin_vulkan_rinos_catalog_connect_v1(&g_rinos_vulkan_catalog);
    if (g_rinos_vulkan_catalog.struct_size != 0u)
        g_rinos_vulkan_lifecycle.flags |=
            RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_CATALOG_CONNECTED;
    if (result != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "admitted Vulkan catalog query failed: %d\n", result);
        memset(&handle_secret, 0, sizeof(handle_secret));
        return 0;
    }
    result = rin_gpu_vulkan_runtime_init_from_catalog(
        &g_rinos_vulkan_runtime, rin_vulkan_rinos_catalog_enumerate_v1,
        &g_rinos_vulkan_catalog, handle_secret);
    memset(&handle_secret, 0, sizeof(handle_secret));
    if (result != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "RinVk runtime catalog initialization failed: %d\n",
                result);
        return 0;
    }
    g_rinos_vulkan_lifecycle.flags |=
        RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_INITIALIZED;
    result = rin_gpu_vulkan_icd_bind_runtime(&g_rinos_vulkan_runtime);
    if (result != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "RinVk ICD runtime binding failed: %d\n", result);
        return 0;
    }
    g_rinos_vulkan_lifecycle.flags |=
        RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_BOUND;
    return 1;
}
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif

/* A window can retain a pending Compositor readback after this one-frame
 * routine returns. Keep the callback target alive if unregister reports
 * BUSY; the caller must not close the window or reuse this state then. */
static RinVulkanExampleFrameCompletion g_native_window_completion;
static uint32_t g_native_window_completion_registered;

static int example_monotonic_milliseconds(uint64_t* milliseconds_out) {
#if defined(_WIN32)
    if (!milliseconds_out) return 0;
    *milliseconds_out = (uint64_t)GetTickCount64();
    return 1;
#else
    struct timespec now;
    uint64_t seconds;
    uint64_t nanoseconds;
    if (!milliseconds_out || clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
        now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
        return 0;
    seconds = (uint64_t)now.tv_sec;
    nanoseconds = (uint64_t)now.tv_nsec;
    if (seconds > (UINT64_MAX - nanoseconds / UINT64_C(1000000)) /
                      UINT64_C(1000))
        return 0;
    *milliseconds_out = seconds * UINT64_C(1000) +
                        nanoseconds / UINT64_C(1000000);
    return 1;
#endif
}

static int example_read_monotonic_ms(void* context,
                                     uint64_t* milliseconds_out) {
    (void)context;
    return example_monotonic_milliseconds(milliseconds_out);
}

static int example_dispatch_compositor(void* context, uint32_t timeout_ms,
                                       uint32_t max_completions) {
    (void)context;
    return wnd_dispatch_compositor(timeout_ms, max_completions);
}

static RinVkResult wait_for_compositor_commit(
        RinVulkanExampleFrameCompletion* state) {
    const RinVkResult result =
        rin_vulkan_example_wait_for_compositor_commit(
            state, example_read_monotonic_ms,
            example_dispatch_compositor, NULL);
    if (result == RIN_VK_TIMEOUT) {
        fprintf(stderr, "timed out waiting for Compositor commit response\n");
    } else if (state && state->completed &&
               state->status != RIN_RESULT_OK) {
        fprintf(stderr, "Compositor rejected frame sequence %llu: %d\n",
                (unsigned long long)state->frame_sequence, state->status);
    } else if (result == RIN_VK_ERROR_SURFACE_LOST_KHR) {
        fprintf(stderr, "wnd_dispatch_compositor failed\n");
    } else if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "monotonic clock failed while waiting for Compositor\n");
    }
    if (result == RIN_VK_SUCCESS)
        printf("Compositor accepted damage/commit sequence %llu.\n",
               (unsigned long long)state->frame_sequence);
    return result;
}

static int report_vk_failure(const char* operation, RinVkResult result) {
    fprintf(stderr, "%s failed with RinVkResult %d\n", operation,
            (int)result);
    return 1;
}

static int array_allocation_size(uint32_t count, size_t element_size,
                                 size_t* size_out) {
    if (!size_out || element_size == 0u ||
        (count != 0u && element_size > SIZE_MAX / (size_t)count))
        return 0;
    *size_out = (size_t)count * element_size;
    return 1;
}

static int device_extension_available(RinVkPhysicalDevice physical_device,
                                     const char* requested_name) {
    RinVkExtensionProperties* properties = NULL;
    uint32_t property_count = 0u;
    uint32_t index;
    size_t allocation_bytes = 0u;
    RinVkResult vk_result;
    int available = 0;
    uint32_t property_capacity;

    if (!physical_device || !requested_name) return 0;
    vk_result = vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &property_count, NULL);
    if (vk_result != RIN_VK_SUCCESS || property_count == 0u ||
        !array_allocation_size(property_count, sizeof(*properties),
                               &allocation_bytes))
        return 0;
    property_capacity = property_count;
    properties = (RinVkExtensionProperties*)calloc(1u, allocation_bytes);
    if (!properties) return 0;
    vk_result = vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &property_count, properties);
    if (vk_result == RIN_VK_SUCCESS || vk_result == RIN_VK_INCOMPLETE) {
        for (index = 0u; index < property_count &&
                         index < property_capacity; ++index) {
            if (strcmp(properties[index].extensionName, requested_name) == 0) {
                available = 1;
                break;
            }
        }
    }
    free(properties);
    return available;
}

static uint32_t clamp_extent(uint32_t value, uint32_t minimum,
                             uint32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static uint32_t choose_composite_alpha(uint32_t supported) {
    supported &= RIN_VK_COMPOSITE_ALPHA_KNOWN_BITS_KHR;
    if ((supported & RIN_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0u)
        return RIN_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    return supported & (0u - supported);
}

static void record_image_barrier(RinVkCommandBuffer command_buffer,
                                 RinVkImage image, uint32_t old_layout,
                                 uint32_t new_layout,
                                 uint64_t source_access,
                                 uint64_t destination_access,
                                 uint64_t source_stage,
                                 uint64_t destination_stage) {
    RinVkImageMemoryBarrier2 barrier;
    RinVkDependencyInfo dependency;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcAccessMask = source_access;
    barrier.dstAccessMask = destination_access;
    barrier.srcStageMask = source_stage;
    barrier.dstStageMask = destination_stage;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1u;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

static int emit_example_spirv_instruction(
        uint32_t* words, size_t capacity, size_t* word_count,
        uint16_t opcode, uint16_t instruction_words,
        const uint32_t* operands) {
    uint16_t index;
    if (!words || !word_count || instruction_words == 0u ||
        *word_count > capacity ||
        (size_t)instruction_words > capacity - *word_count ||
        (instruction_words > 1u && !operands))
        return 0;
    words[(*word_count)++] = ((uint32_t)instruction_words << 16u) | opcode;
    for (index = 1u; index < instruction_words; ++index)
        words[(*word_count)++] = operands[index - 1u];
    return 1;
}

static size_t build_triangle_vertex_spirv(uint32_t* words,
                                          size_t word_capacity) {
    size_t count = 5u;
    uint32_t op[6];
#define EXAMPLE_SPIRV_EMIT(opcode, words_count) \
    do { \
        if (!emit_example_spirv_instruction( \
                words, word_capacity, &count, (opcode), (words_count), op)) \
            return 0u; \
    } while (0)
    if (!words || word_capacity < 5u) return 0u;
    words[0] = UINT32_C(0x07230203);
    words[1] = UINT32_C(0x00010300);
    words[2] = 0u;
    words[3] = 22u;
    words[4] = 0u;
    op[0] = 1u;
    EXAMPLE_SPIRV_EMIT(17u, 2u);                    /* Shader capability */
    op[0] = 0u; op[1] = 3u;
    EXAMPLE_SPIRV_EMIT(14u, 3u);                    /* Vulkan memory model */
    op[0] = 0u; op[1] = 13u; op[2] = UINT32_C(0x6e69616d);
    op[3] = 0u; op[4] = 11u; op[5] = 12u;
    EXAMPLE_SPIRV_EMIT(15u, 7u);                    /* Vertex main */
    op[0] = 11u; op[1] = 11u; op[2] = 0u;
    EXAMPLE_SPIRV_EMIT(71u, 4u);                    /* Position builtin */
    op[0] = 12u; op[1] = 11u; op[2] = 42u;
    EXAMPLE_SPIRV_EMIT(71u, 4u);                    /* VertexIndex builtin */
    op[0] = 1u;
    EXAMPLE_SPIRV_EMIT(19u, 2u);                    /* void */
    op[0] = 2u; op[1] = 32u;
    EXAMPLE_SPIRV_EMIT(22u, 3u);                    /* float */
    op[0] = 3u; op[1] = 32u; op[2] = 1u;
    EXAMPLE_SPIRV_EMIT(21u, 4u);                    /* signed i32 */
    op[0] = 4u; op[1] = 2u; op[2] = 4u;
    EXAMPLE_SPIRV_EMIT(23u, 4u);                    /* vec4 */
    op[0] = 5u; op[1] = 3u; op[2] = 4u;
    EXAMPLE_SPIRV_EMIT(32u, 4u);                    /* output pointer */
    op[0] = 6u; op[1] = 1u; op[2] = 3u;
    EXAMPLE_SPIRV_EMIT(32u, 4u);                    /* input pointer */
    op[0] = 7u; op[1] = 1u;
    EXAMPLE_SPIRV_EMIT(33u, 3u);                    /* function type */
    op[0] = 2u; op[1] = 8u; op[2] = UINT32_C(0xbf800000);
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* -1.0 */
    op[0] = 2u; op[1] = 9u; op[2] = 0u;
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* 0.0 */
    op[0] = 2u; op[1] = 10u; op[2] = UINT32_C(0x3f800000);
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* 1.0 */
    op[0] = 5u; op[1] = 11u; op[2] = 3u;
    EXAMPLE_SPIRV_EMIT(59u, 4u);                    /* output variable */
    op[0] = 6u; op[1] = 12u; op[2] = 1u;
    EXAMPLE_SPIRV_EMIT(59u, 4u);                    /* vertex index */
    op[0] = 1u; op[1] = 13u; op[2] = 0u; op[3] = 7u;
    EXAMPLE_SPIRV_EMIT(54u, 5u);                    /* function */
    op[0] = 14u;
    EXAMPLE_SPIRV_EMIT(248u, 2u);                   /* label */
    op[0] = 3u; op[1] = 15u; op[2] = 12u;
    EXAMPLE_SPIRV_EMIT(61u, 4u);                    /* load VertexIndex */
    op[0] = 2u; op[1] = 16u; op[2] = 15u;
    EXAMPLE_SPIRV_EMIT(111u, 4u);                   /* signed i32 to f32 */
    op[0] = 2u; op[1] = 17u; op[2] = 16u; op[3] = 10u;
    EXAMPLE_SPIRV_EMIT(131u, 5u);                   /* x = index - 1 */
    op[0] = 2u; op[1] = 18u; op[2] = 16u; op[3] = 16u;
    EXAMPLE_SPIRV_EMIT(133u, 5u);                   /* index squared */
    op[0] = 2u; op[1] = 19u; op[2] = 18u; op[3] = 16u;
    EXAMPLE_SPIRV_EMIT(131u, 5u);
    op[0] = 2u; op[1] = 20u; op[2] = 19u; op[3] = 10u;
    EXAMPLE_SPIRV_EMIT(131u, 5u);                   /* y = i*i - i - 1 */
    op[0] = 4u; op[1] = 21u; op[2] = 17u; op[3] = 20u;
    op[4] = 9u; op[5] = 10u;
    EXAMPLE_SPIRV_EMIT(80u, 7u);                    /* position vector */
    op[0] = 11u; op[1] = 21u;
    EXAMPLE_SPIRV_EMIT(62u, 3u);                    /* store position */
    EXAMPLE_SPIRV_EMIT(253u, 1u);                   /* return */
    EXAMPLE_SPIRV_EMIT(56u, 1u);                    /* function end */
#undef EXAMPLE_SPIRV_EMIT
    return count;
}

static size_t build_triangle_fragment_spirv(uint32_t* words,
                                            size_t word_capacity) {
    size_t count = 5u;
    uint32_t op[6];
#define EXAMPLE_SPIRV_EMIT(opcode, words_count) \
    do { \
        if (!emit_example_spirv_instruction( \
                words, word_capacity, &count, (opcode), (words_count), op)) \
            return 0u; \
    } while (0)
    if (!words || word_capacity < 5u) return 0u;
    words[0] = UINT32_C(0x07230203);
    words[1] = UINT32_C(0x00010300);
    words[2] = 0u;
    words[3] = 15u;
    words[4] = 0u;
    op[0] = 1u;
    EXAMPLE_SPIRV_EMIT(17u, 2u);                    /* Shader capability */
    op[0] = 0u; op[1] = 3u;
    EXAMPLE_SPIRV_EMIT(14u, 3u);                    /* Vulkan memory model */
    op[0] = 4u; op[1] = 12u; op[2] = UINT32_C(0x6e69616d);
    op[3] = 0u; op[4] = 10u;
    EXAMPLE_SPIRV_EMIT(15u, 6u);                    /* Fragment main */
    op[0] = 12u; op[1] = 7u;
    EXAMPLE_SPIRV_EMIT(16u, 3u);                    /* OriginUpperLeft */
    op[0] = 10u; op[1] = 30u; op[2] = 0u;
    EXAMPLE_SPIRV_EMIT(71u, 4u);                    /* output location 0 */
    op[0] = 1u;
    EXAMPLE_SPIRV_EMIT(19u, 2u);                    /* void */
    op[0] = 2u; op[1] = 32u;
    EXAMPLE_SPIRV_EMIT(22u, 3u);                    /* float */
    op[0] = 3u; op[1] = 2u; op[2] = 4u;
    EXAMPLE_SPIRV_EMIT(23u, 4u);                    /* vec4 */
    op[0] = 4u; op[1] = 3u; op[2] = 3u;
    EXAMPLE_SPIRV_EMIT(32u, 4u);                    /* output pointer */
    op[0] = 5u; op[1] = 1u;
    EXAMPLE_SPIRV_EMIT(33u, 3u);                    /* function type */
    op[0] = 2u; op[1] = 6u; op[2] = UINT32_C(0x3f000000);
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* 0.5 */
    op[0] = 2u; op[1] = 7u; op[2] = UINT32_C(0x3f800000);
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* 1.0 */
    op[0] = 2u; op[1] = 8u; op[2] = UINT32_C(0x3e800000);
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* 0.25 */
    op[0] = 2u; op[1] = 9u; op[2] = UINT32_C(0x3f800000);
    EXAMPLE_SPIRV_EMIT(43u, 4u);                    /* 1.0 */
    op[0] = 4u; op[1] = 10u; op[2] = 3u;
    EXAMPLE_SPIRV_EMIT(59u, 4u);                    /* output variable */
    op[0] = 1u; op[1] = 12u; op[2] = 0u; op[3] = 5u;
    EXAMPLE_SPIRV_EMIT(54u, 5u);                    /* function */
    op[0] = 13u;
    EXAMPLE_SPIRV_EMIT(248u, 2u);                   /* label */
    op[0] = 3u; op[1] = 14u; op[2] = 6u; op[3] = 8u;
    op[4] = 6u; op[5] = 7u;
    EXAMPLE_SPIRV_EMIT(80u, 7u);                    /* color vector */
    op[0] = 10u; op[1] = 14u;
    EXAMPLE_SPIRV_EMIT(62u, 3u);                    /* store color */
    EXAMPLE_SPIRV_EMIT(253u, 1u);                   /* return */
    EXAMPLE_SPIRV_EMIT(56u, 1u);                    /* function end */
#undef EXAMPLE_SPIRV_EMIT
    return count;
}

static RinVkResult create_example_triangle_pipeline(
        RinVkDevice device, RinVkExtent2D extent,
        RinVkShaderModule* vertex_module_out,
        RinVkShaderModule* fragment_module_out,
        RinVkPipelineLayout* layout_out, RinVkPipeline* pipeline_out) {
    uint32_t vertex_words[128];
    uint32_t fragment_words[96];
    RinVkShaderModuleCreateInfo shader_info;
    RinVkPipelineLayoutCreateInfo layout_info;
    RinVkPipelineShaderStageCreateInfo stages[2];
    RinVkPipelineVertexInputStateCreateInfo vertex_input;
    RinVkPipelineInputAssemblyStateCreateInfo assembly;
    RinVkPipelineViewportStateCreateInfo viewport_state;
    RinVkViewport viewport;
    RinVkRect2D scissor;
    RinVkPipelineRasterizationStateCreateInfo raster;
    RinVkPipelineMultisampleStateCreateInfo multisample;
    RinVkPipelineColorBlendAttachmentState blend_attachment;
    RinVkPipelineColorBlendStateCreateInfo blend;
    RinVkPipelineRenderingCreateInfo rendering;
    RinVkGraphicsPipelineCreateInfo pipeline_info;
    static const int32_t color_format = RIN_VK_FORMAT_R8G8B8A8_UNORM;
    const size_t vertex_word_count = build_triangle_vertex_spirv(
        vertex_words, sizeof(vertex_words) / sizeof(vertex_words[0]));
    const size_t fragment_word_count = build_triangle_fragment_spirv(
        fragment_words, sizeof(fragment_words) / sizeof(fragment_words[0]));
    RinVkResult result;

    if (!device || !vertex_module_out || !fragment_module_out || !layout_out ||
        !pipeline_out || extent.width == 0u || extent.height == 0u ||
        vertex_word_count == 0u || fragment_word_count == 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *vertex_module_out = 0u;
    *fragment_module_out = 0u;
    *layout_out = 0u;
    *pipeline_out = 0u;
    memset(&shader_info, 0, sizeof(shader_info));
    shader_info.sType = RIN_VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = vertex_word_count * sizeof(vertex_words[0]);
    shader_info.pCode = vertex_words;
    result = vkCreateShaderModule(device, &shader_info, NULL,
                                  vertex_module_out);
    if (result != RIN_VK_SUCCESS) return result;
    shader_info.codeSize = fragment_word_count * sizeof(fragment_words[0]);
    shader_info.pCode = fragment_words;
    result = vkCreateShaderModule(device, &shader_info, NULL,
                                  fragment_module_out);
    if (result != RIN_VK_SUCCESS) return result;

    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = RIN_VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    result = vkCreatePipelineLayout(device, &layout_info, NULL, layout_out);
    if (result != RIN_VK_SUCCESS) return result;

    memset(stages, 0, sizeof(stages));
    stages[0].sType = RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = RIN_VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = *vertex_module_out;
    stages[0].pName = "main";
    stages[1].sType = RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = RIN_VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = *fragment_module_out;
    stages[1].pName = "main";
    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    memset(&assembly, 0, sizeof(assembly));
    assembly.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.maxDepth = 1.0f;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent = extent;
    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1u;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1u;
    viewport_state.pScissors = &scissor;
    memset(&raster, 0, sizeof(raster));
    raster.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = RIN_VK_POLYGON_MODE_FILL;
    raster.frontFace = RIN_VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    memset(&multisample, 0, sizeof(multisample));
    multisample.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = RIN_VK_SAMPLE_COUNT_1_BIT;
    memset(&blend_attachment, 0, sizeof(blend_attachment));
    blend_attachment.colorWriteMask = 15u;
    memset(&blend, 0, sizeof(blend));
    blend.sType = RIN_VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1u;
    blend.pAttachments = &blend_attachment;
    memset(&rendering, 0, sizeof(rendering));
    rendering.sType = RIN_VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1u;
    rendering.pColorAttachmentFormats = &color_format;
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType =
        RIN_VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering;
    pipeline_info.stageCount = 2u;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &blend;
    pipeline_info.layout = *layout_out;
    pipeline_info.basePipelineIndex = -1;
    return vkCreateGraphicsPipelines(device, 0u, 1u, &pipeline_info, NULL,
                                    pipeline_out);
}

static int present_one_triangle_frame(RinVkPhysicalDevice physical_device,
                                   uint32_t queue_family,
                                   RinVkSurfaceKHR surface,
                                   RinRuntimeGuiHandle window,
                                   uint64_t product_device_id,
                                   uint64_t product_device_generation,
                                   RinVulkanExampleFrameCompletion* completion) {
    static const char* const device_extensions[] = {
        RIN_VK_KHR_SWAPCHAIN_EXTENSION,
        RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION,
        RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION
    };
    RinVkSurfaceCapabilitiesKHR capabilities;
    RinVkSurfaceFormatKHR* formats = NULL;
    RinVkPresentModeKHR* present_modes = NULL;
    RinVkImage* swapchain_images = NULL;
    RinVkDeviceQueueCreateInfo queue_info;
    RinVkDeviceCreateInfo device_info;
    RinVkPhysicalDeviceFeatures2 physical_features;
    RinVkPhysicalDeviceVulkan13Features vulkan13_features;
    RinVkSwapchainCreateInfoKHR swapchain_info;
    RinVkFenceCreateInfo fence_info;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandBufferAllocateInfo command_info;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkSubmitInfo submit_info;
    RinVkPresentInfoKHR present_info;
    RinVkImageViewCreateInfo image_view_info;
    RinVkRenderingAttachmentInfo rendering_attachment;
    RinVkRenderingInfo rendering_info;
    RinVkResult present_result = RIN_VK_ERROR_UNKNOWN;
    RinVkResult swapchain_present_result = RIN_VK_ERROR_UNKNOWN;
    RinVkDevice device = NULL;
    RinVkQueue queue = NULL;
    RinVkSwapchainKHR swapchain = 0u;
    RinVkFence image_acquired = 0u;
    RinVkCommandPool command_pool = NULL;
    RinVkCommandBuffer command_buffer = NULL;
    RinVkImageView image_view = 0u;
    RinVkShaderModule vertex_module = 0u;
    RinVkShaderModule fragment_module = 0u;
    RinVkPipelineLayout pipeline_layout = 0u;
    RinVkPipeline graphics_pipeline = 0u;
    RinVkSurfaceFormatKHR selected_format = {0};
    RinVkExtent2D extent;
    RinVkResult vk_result;
    uint32_t format_count = 0u;
    uint32_t format_capacity = 0u;
    uint32_t present_mode_count = 0u;
    uint32_t present_mode_capacity = 0u;
    uint32_t image_count = 0u;
    uint32_t image_capacity = 0u;
    uint32_t image_index = 0u;
    uint32_t window_width = 0u;
    uint32_t window_height = 0u;
    uint32_t composite_alpha;
    size_t allocation_bytes = 0u;
    int runtime_result;
    int window_width_px = 0;
    int window_height_px = 0;
    int format_supported = 0;
    int fifo_present_supported = 0;
    int submission_pending = 0;
    int exit_status = 1;

#if !defined(__rinos__)
    (void)product_device_id;
    (void)product_device_generation;
#endif

    memset(&capabilities, 0, sizeof(capabilities));
    vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device, surface, &capabilities);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
                          vk_result);
        goto cleanup;
    }
    if (capabilities.minImageCount == 0u ||
        (capabilities.maxImageCount != 0u &&
         capabilities.minImageCount > capabilities.maxImageCount) ||
        capabilities.maxImageArrayLayers == 0u ||
        (capabilities.supportedUsageFlags &
         (RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
          RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) !=
            (RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
             RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
        capabilities.currentTransform == 0u ||
        (capabilities.supportedTransforms & capabilities.currentTransform) ==
            0u) {
        fprintf(stderr,
                "surface does not expose a valid transfer/readback path\n");
        goto cleanup;
    }
    composite_alpha = choose_composite_alpha(
        capabilities.supportedCompositeAlpha);
    if (composite_alpha == 0u) {
        fprintf(stderr, "surface exposes no composite-alpha mode\n");
        goto cleanup;
    }

    runtime_result = wnd_get_size(window, &window_width_px,
                                  &window_height_px);
    if (runtime_result != RIN_RESULT_OK || window_width_px <= 0 ||
        window_height_px <= 0) {
        fprintf(stderr, "wnd_get_size failed: %d\n", runtime_result);
        goto cleanup;
    }
    window_width = (uint32_t)window_width_px;
    window_height = (uint32_t)window_height_px;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        if (capabilities.minImageExtent.width >
                capabilities.maxImageExtent.width ||
            capabilities.minImageExtent.height >
                capabilities.maxImageExtent.height) {
            fprintf(stderr, "surface extent limits are inconsistent\n");
            goto cleanup;
        }
        extent.width = clamp_extent(window_width,
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
        extent.height = clamp_extent(window_height,
                                     capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    }
    if (extent.width == 0u || extent.height == 0u) {
        fprintf(stderr, "surface extent is empty\n");
        goto cleanup;
    }

    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, NULL);
    if (vk_result != RIN_VK_SUCCESS || format_count == 0u ||
        !array_allocation_size(format_count, sizeof(*formats),
                               &allocation_bytes)) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkGetPhysicalDeviceSurfaceFormatsKHR(count)",
                              vk_result);
        else
            fprintf(stderr, "surface exposes no usable formats\n");
        goto cleanup;
    }
    format_capacity = format_count;
    formats = (RinVkSurfaceFormatKHR*)calloc(1u, allocation_bytes);
    if (!formats) {
        fprintf(stderr, "surface-format allocation failed\n");
        goto cleanup;
    }
    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, formats);
    if (vk_result != RIN_VK_SUCCESS && vk_result != RIN_VK_INCOMPLETE) {
        report_vk_failure("vkGetPhysicalDeviceSurfaceFormatsKHR", vk_result);
        goto cleanup;
    }
    for (uint32_t index = 0u; index < format_count &&
                                 index < format_capacity; ++index) {
        if (formats[index].format == RIN_VK_FORMAT_R8G8B8A8_UNORM &&
            formats[index].colorSpace ==
                RIN_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selected_format = formats[index];
            format_supported = 1;
            break;
        }
    }
    if (!format_supported) {
        fprintf(stderr,
                "surface does not report the RinVk RGBA8/SRGB format\n");
        goto cleanup;
    }

    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, NULL);
    if (vk_result != RIN_VK_SUCCESS || present_mode_count == 0u ||
        !array_allocation_size(present_mode_count, sizeof(*present_modes),
                               &allocation_bytes)) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure(
                "vkGetPhysicalDeviceSurfacePresentModesKHR(count)",
                vk_result);
        else
            fprintf(stderr, "surface exposes no present modes\n");
        goto cleanup;
    }
    present_mode_capacity = present_mode_count;
    present_modes = (RinVkPresentModeKHR*)calloc(1u, allocation_bytes);
    if (!present_modes) {
        fprintf(stderr, "present-mode allocation failed\n");
        goto cleanup;
    }
    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, present_modes);
    if (vk_result != RIN_VK_SUCCESS && vk_result != RIN_VK_INCOMPLETE) {
        report_vk_failure("vkGetPhysicalDeviceSurfacePresentModesKHR",
                          vk_result);
        goto cleanup;
    }
    for (uint32_t index = 0u; index < present_mode_count &&
                                 index < present_mode_capacity; ++index)
        if (present_modes[index] == RIN_VK_PRESENT_MODE_FIFO_KHR)
            fifo_present_supported = 1;
    if (!fifo_present_supported) {
        fprintf(stderr, "surface does not report FIFO presentation\n");
        goto cleanup;
    }

    if (!device_extension_available(
            physical_device, RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION)) {
        fprintf(stderr, "device does not expose VK_KHR_synchronization2\n");
        goto cleanup;
    }
    if (!device_extension_available(
            physical_device, RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION)) {
        fprintf(stderr, "device does not expose VK_KHR_dynamic_rendering\n");
        goto cleanup;
    }
    memset(&vulkan13_features, 0, sizeof(vulkan13_features));
    vulkan13_features.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    memset(&physical_features, 0, sizeof(physical_features));
    physical_features.sType = RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_features.pNext = &vulkan13_features;
    vkGetPhysicalDeviceFeatures2(physical_device, &physical_features);
    if (vulkan13_features.synchronization2 != 1u ||
        vulkan13_features.dynamicRendering != 1u) {
        fprintf(stderr,
                "device lacks synchronization2 or dynamic-rendering support\n");
        goto cleanup;
    }

    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount = 1u;
    {
        const float queue_priority = 1.0f;
        queue_info.pQueuePriorities = &queue_priority;
        memset(&device_info, 0, sizeof(device_info));
        device_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vulkan13_features.synchronization2 = 1u;
        vulkan13_features.dynamicRendering = 1u;
        device_info.pNext = &vulkan13_features;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount =
            (uint32_t)(sizeof(device_extensions) /
                       sizeof(device_extensions[0]));
        device_info.ppEnabledExtensionNames = device_extensions;
#if defined(__rinos__)
        if (!bind_rinos_product_platform(product_device_id,
                                         product_device_generation,
                                         queue_family))
            goto cleanup;
#endif
        vk_result = vkCreateDevice(physical_device, &device_info, NULL,
                                   &device);
    }
    if (vk_result != RIN_VK_SUCCESS || !device) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateDevice", vk_result);
        else
            fprintf(stderr, "vkCreateDevice returned a null device\n");
        goto cleanup;
    }
    vkGetDeviceQueue(device, queue_family, 0u, &queue);
    if (!queue) {
        fprintf(stderr, "vkGetDeviceQueue returned a null queue\n");
        goto cleanup;
    }

    memset(&swapchain_info, 0, sizeof(swapchain_info));
    swapchain_info.sType = RIN_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = capabilities.minImageCount;
    swapchain_info.imageFormat = selected_format.format;
    swapchain_info.imageColorSpace = selected_format.colorSpace;
    swapchain_info.imageExtent = extent;
    swapchain_info.imageArrayLayers = 1u;
    /* Render into the application-owned image; the ordinary Compositor
     * consumer still performs an explicit transfer-src readback afterward. */
    swapchain_info.imageUsage = RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchain_info.imageSharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = composite_alpha;
    swapchain_info.presentMode = RIN_VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = 1u;
    vk_result = vkCreateSwapchainKHR(device, &swapchain_info, NULL,
                                     &swapchain);
    if (vk_result != RIN_VK_SUCCESS || swapchain == 0u) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateSwapchainKHR", vk_result);
        else
            fprintf(stderr, "vkCreateSwapchainKHR returned a null handle\n");
        goto cleanup;
    }

    vk_result = vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
    if (vk_result != RIN_VK_SUCCESS || image_count == 0u ||
        !array_allocation_size(image_count, sizeof(*swapchain_images),
                               &allocation_bytes)) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkGetSwapchainImagesKHR(count)", vk_result);
        else
            fprintf(stderr, "swapchain exposes no images\n");
        goto cleanup;
    }
    image_capacity = image_count;
    swapchain_images = (RinVkImage*)calloc(1u, allocation_bytes);
    if (!swapchain_images) {
        fprintf(stderr, "swapchain image-list allocation failed\n");
        goto cleanup;
    }
    vk_result = vkGetSwapchainImagesKHR(device, swapchain, &image_count,
                                        swapchain_images);
    if (image_count > image_capacity) image_count = image_capacity;
    if ((vk_result != RIN_VK_SUCCESS &&
         vk_result != RIN_VK_INCOMPLETE) || image_count == 0u) {
        if (vk_result != RIN_VK_SUCCESS &&
            vk_result != RIN_VK_INCOMPLETE)
            report_vk_failure("vkGetSwapchainImagesKHR", vk_result);
        else
            fprintf(stderr, "swapchain image enumeration returned none\n");
        goto cleanup;
    }

    vk_result = create_example_triangle_pipeline(
        device, extent, &vertex_module, &fragment_module, &pipeline_layout,
        &graphics_pipeline);
    if (vk_result != RIN_VK_SUCCESS || graphics_pipeline == 0u) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("create_example_triangle_pipeline", vk_result);
        else
            fprintf(stderr, "graphics pipeline creation returned null\n");
        goto cleanup;
    }

    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vk_result = vkCreateFence(device, &fence_info, NULL, &image_acquired);
    if (vk_result != RIN_VK_SUCCESS || image_acquired == 0u) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateFence(image-acquired)", vk_result);
        else
            fprintf(stderr, "image-acquired fence is null\n");
        goto cleanup;
    }

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    vk_result = vkCreateCommandPool(device, &pool_info, NULL, &command_pool);
    if (vk_result != RIN_VK_SUCCESS || command_pool == NULL) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateCommandPool", vk_result);
        else
            fprintf(stderr, "vkCreateCommandPool returned a null handle\n");
        goto cleanup;
    }
    memset(&command_info, 0, sizeof(command_info));
    command_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_info.commandPool = command_pool;
    command_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_info.commandBufferCount = 1u;
    vk_result = vkAllocateCommandBuffers(device, &command_info,
                                        &command_buffer);
    if (vk_result != RIN_VK_SUCCESS || !command_buffer) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkAllocateCommandBuffers", vk_result);
        else
            fprintf(stderr, "vkAllocateCommandBuffers returned null\n");
        goto cleanup;
    }

    vk_result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                      0u, image_acquired, &image_index);
    if (vk_result != RIN_VK_SUCCESS &&
        vk_result != RIN_VK_SUBOPTIMAL_KHR) {
        report_vk_failure("vkAcquireNextImageKHR", vk_result);
        goto cleanup;
    }
    if (image_index >= image_count) {
        fprintf(stderr, "acquired swapchain image index is out of range\n");
        goto cleanup;
    }
    memset(&image_view_info, 0, sizeof(image_view_info));
    image_view_info.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = swapchain_images[image_index];
    image_view_info.viewType = RIN_VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = selected_format.format;
    image_view_info.subresourceRange.aspectMask =
        RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.levelCount = 1u;
    image_view_info.subresourceRange.layerCount = 1u;
    vk_result = vkCreateImageView(device, &image_view_info, NULL, &image_view);
    if (vk_result != RIN_VK_SUCCESS || image_view == 0u) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateImageView(swapchain image)", vk_result);
        else
            fprintf(stderr, "vkCreateImageView returned null\n");
        goto cleanup;
    }
    vk_result = vkWaitForFences(device, 1u, &image_acquired, 1u, UINT64_MAX);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkWaitForFences(image-acquired)", vk_result);
        goto cleanup;
    }

    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = RIN_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkBeginCommandBuffer", vk_result);
        goto cleanup;
    }
    record_image_barrier(command_buffer, swapchain_images[image_index],
                         RIN_VK_IMAGE_LAYOUT_UNDEFINED,
                         RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0u,
                         RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    memset(&rendering_attachment, 0, sizeof(rendering_attachment));
    rendering_attachment.sType =
        RIN_VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    rendering_attachment.imageView = image_view;
    rendering_attachment.imageLayout =
        RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rendering_attachment.loadOp = RIN_VK_ATTACHMENT_LOAD_OP_CLEAR;
    rendering_attachment.storeOp = RIN_VK_ATTACHMENT_STORE_OP_STORE;
    rendering_attachment.clearValue.color.float32[0] = 0.08f;
    rendering_attachment.clearValue.color.float32[1] = 0.18f;
    rendering_attachment.clearValue.color.float32[2] = 0.42f;
    rendering_attachment.clearValue.color.float32[3] = 1.0f;
    memset(&rendering_info, 0, sizeof(rendering_info));
    rendering_info.sType = RIN_VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.extent = extent;
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 1u;
    rendering_info.pColorAttachments = &rendering_attachment;
    vkCmdBeginRendering(command_buffer, &rendering_info);
    vkCmdBindPipeline(command_buffer, RIN_VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphics_pipeline);
    vkCmdDraw(command_buffer, 3u, 1u, 0u, 0u);
    vkCmdEndRendering(command_buffer);
    record_image_barrier(command_buffer, swapchain_images[image_index],
                         RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         RIN_VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         RIN_VK_ACCESS_2_MEMORY_READ_BIT,
                         RIN_VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    vk_result = vkEndCommandBuffer(command_buffer);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkEndCommandBuffer", vk_result);
        goto cleanup;
    }

    {
        memset(&submit_info, 0, sizeof(submit_info));
        submit_info.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1u;
        submit_info.pCommandBuffers = &command_buffer;
        vk_result = vkQueueSubmit(queue, 1u, &submit_info, 0u);
    }
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkQueueSubmit", vk_result);
        goto cleanup;
    }
    submission_pending = 1;

    /* V6 canonical process submission has no native queue-wait callbacks.
     * Complete this queue work before the ordinary surface hands pixels to
     * the Compositor; this is not a direct-display/scanout path. */
    vk_result = vkQueueWaitIdle(queue);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkQueueWaitIdle(before Compositor present)",
                          vk_result);
        goto cleanup;
    }
    submission_pending = 0;

    memset(&present_info, 0, sizeof(present_info));
    present_info.sType = RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.swapchainCount = 1u;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;
    present_info.pResults = &swapchain_present_result;
    present_result = vkQueuePresentKHR(queue, &present_info);
    if ((present_result != RIN_VK_SUCCESS &&
         present_result != RIN_VK_SUBOPTIMAL_KHR) ||
        (swapchain_present_result != RIN_VK_SUCCESS &&
         swapchain_present_result != RIN_VK_SUBOPTIMAL_KHR)) {
        report_vk_failure("vkQueuePresentKHR",
                          present_result != RIN_VK_SUCCESS
                              ? present_result
                              : swapchain_present_result);
        goto cleanup;
    }
    vk_result = wait_for_compositor_commit(completion);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("Compositor damage/commit", vk_result);
        goto cleanup;
    }
    exit_status = 0;

cleanup:
    if (device && submission_pending) {
        vk_result = vkDeviceWaitIdle(device);
        if (vk_result != RIN_VK_SUCCESS) {
            report_vk_failure("vkDeviceWaitIdle(cleanup)", vk_result);
            exit_status = 1;
        }
    }
    if (device && command_pool != NULL)
        vkDestroyCommandPool(device, command_pool, NULL);
    if (device && image_view != 0u)
        vkDestroyImageView(device, image_view, NULL);
    if (device && graphics_pipeline != 0u)
        vkDestroyPipeline(device, graphics_pipeline, NULL);
    if (device && pipeline_layout != 0u)
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    if (device && fragment_module != 0u)
        vkDestroyShaderModule(device, fragment_module, NULL);
    if (device && vertex_module != 0u)
        vkDestroyShaderModule(device, vertex_module, NULL);
    if (device && image_acquired != 0u)
        vkDestroyFence(device, image_acquired, NULL);
    free(swapchain_images);
    if (device && swapchain != 0u)
        vkDestroySwapchainKHR(device, swapchain, NULL);
    if (device) vkDestroyDevice(device, NULL);
    free(present_modes);
    free(formats);
    return exit_status;
}

static int rinos_native_window_surface_main(void) {
    static const char* const instance_extensions[] = {
        RIN_VK_KHR_SURFACE_EXTENSION,
        RIN_VK_RINOS_NATIVE_WINDOW_SURFACE_EXTENSION
    };
    RinRuntimeGuiHandle window = RIN_WINDOW_HANDLE_INVALID;
    RinRuntimeCompositorGpuSurfaceOpsV1 surface_ops;
    RinVkApplicationInfo application;
    RinVkInstanceCreateInfo instance_info;
    RinVkRinOSNativeWindowSurfaceCreateInfoV1 surface_info;
    RinVkInstance instance = NULL;
    RinVkSurfaceKHR surface = 0u;
    RinVkPhysicalDevice* physical_devices = NULL;
    size_t physical_devices_bytes = 0u;
    uint32_t api_version = 0u;
    uint32_t physical_device_count = 0u;
    RinVkResult vk_result;
    int runtime_result;
    int exit_status = 1;

    if (g_native_window_completion_registered != 0u) {
        fprintf(stderr,
                "previous Compositor completion is still registered\n");
        return 1;
    }
#if defined(__rinos__)
    if (!cleanup_rinos_owned_state()) return 1;
#endif
    memset(&g_native_window_completion, 0,
           sizeof(g_native_window_completion));

#if defined(__rinos__)
    {
        const RinResult sdk_result =
            rin_vulkan_example_bind_sdk_target_backend_v1();
        if (sdk_result != RIN_SUCCESS) {
            fprintf(stderr,
                    "rin_sdk_bind_target_backend_v1 failed: %d\n",
                    (int)sdk_result);
            goto cleanup;
        }
    }
    if (!bind_admitted_vulkan_catalog()) goto cleanup;
#endif

    window = wnd_create("RinVulkan native-window surface", 64, 64, 960, 540);
    if (window == RIN_WINDOW_HANDLE_INVALID) {
        fprintf(stderr, "wnd_create failed\n");
        goto cleanup;
    }

    memset(&surface_ops, 0, sizeof(surface_ops));
    runtime_result = wnd_get_gpu_surface_ops_v1(&surface_ops);
    if (runtime_result != RIN_RESULT_OK) {
        fprintf(stderr, "wnd_get_gpu_surface_ops_v1 failed: %d\n",
                runtime_result);
        goto cleanup;
    }

    vk_result = vkEnumerateInstanceVersion(&api_version);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkEnumerateInstanceVersion", vk_result);
        goto cleanup;
    }
    if (api_version == 0u) {
        fprintf(stderr, "Vulkan runtime returned an invalid API version\n");
        goto cleanup;
    }

    memset(&application, 0, sizeof(application));
    application.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "RinVulkan native-window example";
    application.applicationVersion = 1u;
    application.pEngineName = "RinOS";
    application.engineVersion = 1u;
    application.apiVersion = api_version;

    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application;
    instance_info.enabledExtensionCount =
        (uint32_t)(sizeof(instance_extensions) / sizeof(instance_extensions[0]));
    instance_info.ppEnabledExtensionNames = instance_extensions;
    vk_result = vkCreateInstance(&instance_info, NULL, &instance);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkCreateInstance", vk_result);
        goto cleanup;
    }

    memset(&surface_info, 0, sizeof(surface_info));
    surface_info.struct_size = sizeof(surface_info);
    surface_info.version = RIN_VK_RINOS_NATIVE_WINDOW_SURFACE_SPEC_VERSION;
    surface_info.window_handle = (uint64_t)window;
    surface_info.surface_ops = &surface_ops;
    vk_result = vkCreateRinOSNativeWindowSurfaceV1(
        instance, &surface_info, NULL, &surface);
    if (vk_result != RIN_VK_SUCCESS || surface == 0u) {
        if (vk_result == RIN_VK_SUCCESS) {
            fprintf(stderr,
                    "native-window surface creation returned a null handle\n");
        } else {
            report_vk_failure("vkCreateRinOSNativeWindowSurfaceV1", vk_result);
        }
        goto cleanup;
    }

    vk_result = vkEnumeratePhysicalDevices(
        instance, &physical_device_count, NULL);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkEnumeratePhysicalDevices(count)", vk_result);
        goto cleanup;
    }
    if (physical_device_count == 0u) {
        fprintf(stderr, "Vulkan runtime exposed no physical devices\n");
        goto cleanup;
    }
    if (!array_allocation_size(physical_device_count,
                               sizeof(*physical_devices),
                               &physical_devices_bytes)) {
        fprintf(stderr, "physical-device list size overflow\n");
        goto cleanup;
    }
    physical_devices = (RinVkPhysicalDevice*)calloc(
        1u, physical_devices_bytes);
    if (!physical_devices) {
        fprintf(stderr, "physical-device list allocation failed\n");
        goto cleanup;
    }
    vk_result = vkEnumeratePhysicalDevices(
        instance, &physical_device_count, physical_devices);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkEnumeratePhysicalDevices", vk_result);
        goto cleanup;
    }

    for (uint32_t device_index = 0u;
         device_index < physical_device_count; ++device_index) {
        uint32_t queue_family_count = 0u;
        size_t queue_families_bytes = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_devices[device_index], &queue_family_count, NULL);
        if (queue_family_count == 0u) continue;
        if (!array_allocation_size(queue_family_count,
                                   sizeof(RinVkQueueFamilyProperties),
                                   &queue_families_bytes)) {
            fprintf(stderr, "queue-family list size overflow\n");
            goto cleanup;
        }
        RinVkQueueFamilyProperties* queue_families =
            (RinVkQueueFamilyProperties*)calloc(1u, queue_families_bytes);
        if (!queue_families) {
            fprintf(stderr, "queue-family list allocation failed\n");
            goto cleanup;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_devices[device_index], &queue_family_count,
            queue_families);
        for (uint32_t family = 0u; family < queue_family_count; ++family) {
            uint32_t supported = 0u;
            vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_devices[device_index], family, surface, &supported);
            if (vk_result != RIN_VK_SUCCESS) {
                free(queue_families);
                report_vk_failure("vkGetPhysicalDeviceSurfaceSupportKHR",
                                  vk_result);
                goto cleanup;
            }
            if (supported != 0u && queue_families[family].queueCount != 0u &&
                (queue_families[family].queueFlags &
                 (RIN_VK_QUEUE_GRAPHICS_BIT | RIN_VK_QUEUE_TRANSFER_BIT)) !=
                    0u) {
                g_native_window_completion.window = window;
                runtime_result = wnd_set_compositor_completion_callback(
                    window, rin_vulkan_example_compositor_completion,
                    &g_native_window_completion);
                if (runtime_result != RIN_RESULT_OK) {
                    free(queue_families);
                    fprintf(stderr,
                            "wnd_set_compositor_completion_callback failed: %d\n",
                            runtime_result);
                    goto cleanup;
                }
                g_native_window_completion_registered = 1u;
                printf("RinOS native-window surface is supported by "
                       "device %u, queue family %u.\n", device_index, family);
                free(queue_families);
#if defined(__rinos__)
                if (device_index >= g_rinos_vulkan_catalog.device_count) {
                    fprintf(stderr,
                            "runtime/catalog physical-device ordering diverged\n");
                    goto cleanup;
                }
                exit_status = present_one_triangle_frame(
                    physical_devices[device_index], family, surface, window,
                    g_rinos_vulkan_catalog.devices[device_index].device_id,
                    g_rinos_vulkan_catalog.devices[device_index]
                        .device_generation,
                    &g_native_window_completion);
#else
                exit_status = present_one_triangle_frame(
                    physical_devices[device_index], family, surface, window,
                    0u, 0u, &g_native_window_completion);
#endif
                goto cleanup;
            }
        }
        free(queue_families);
    }

    fprintf(stderr, "no physical queue family supports this window surface\n");

cleanup:
    free(physical_devices);
    if (surface != 0u) vkDestroySurfaceKHR(instance, surface, NULL);
    if (instance) vkDestroyInstance(instance, NULL);
#if defined(__rinos__)
    if (!cleanup_rinos_owned_state()) exit_status = 1;
#endif
    if (window != RIN_WINDOW_HANDLE_INVALID) {
        if (g_native_window_completion_registered != 0u) {
            runtime_result = wnd_set_compositor_completion_callback(
                window, NULL, NULL);
            if (runtime_result != RIN_RESULT_OK) {
                fprintf(stderr,
                        "could not unregister pending Compositor completion: %d\n",
                        runtime_result);
                /* Keep the callback target and window alive. Process teardown
                 * owns final reclamation if the pending operation never drains. */
                return 1;
            }
            g_native_window_completion_registered = 0u;
        }
        wnd_close(window);
    }
    return exit_status;
}

#if !defined(RIN_VULKAN_NATIVE_WINDOW_EXAMPLE_NO_MAIN)
int main(void) {
    return rinos_native_window_surface_main();
}

int rinvk_native_window_surface_run(void) {
    return rinos_native_window_surface_main();
}
#endif
