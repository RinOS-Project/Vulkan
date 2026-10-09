/* SPDX-License-Identifier: MIT */
/* One-frame RinOS application example for the ordinary Compositor WSI path.
 * The platform must already have admitted and bound its real Vulkan runtime;
 * this example never initializes the software host platform as a fallback. */
#include <rinvulkan/icd.h>
#include <rin/contract_abi.h>
#include <rinruntime/window.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ExampleFrameCompletion {
    RinRuntimeGuiHandle window;
    int completed;
    int32_t status;
    uint64_t frame_sequence;
} ExampleFrameCompletion;

static void example_compositor_completion(
        const RinRuntimeGuiCompletionV1* completion, void* context) {
    ExampleFrameCompletion* state = (ExampleFrameCompletion*)context;
    if (!state || state->completed || !completion ||
        completion->handle != state->window ||
        completion->request_type != RIN_COMPOSITOR_DAMAGE)
        return;
    if (completion->struct_size != sizeof(*completion) ||
        completion->version != 1u || completion->cookie == 0u ||
        completion->payload_size != 0u || completion->reserved != 0u) {
        state->status = RIN_RESULT_CORRUPT_DATA;
    } else {
        state->status = completion->status;
        state->frame_sequence = completion->cookie;
    }
    state->completed = 1;
}

static int example_utc_milliseconds(uint64_t* milliseconds_out) {
    struct timespec now;
    uint64_t seconds;
    uint64_t nanoseconds;
    if (!milliseconds_out || timespec_get(&now, TIME_UTC) != TIME_UTC ||
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
}

static RinVkResult wait_for_compositor_commit(
        ExampleFrameCompletion* state) {
    uint64_t started_ms;
    if (!state || !example_utc_milliseconds(&started_ms))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    while (!state->completed) {
        uint64_t now_ms;
        const int dispatch_result = wnd_dispatch_compositor(50u, 8u);
        if (dispatch_result < 0) {
            fprintf(stderr, "wnd_dispatch_compositor failed: %d\n",
                    dispatch_result);
            return RIN_VK_ERROR_SURFACE_LOST_KHR;
        }
        if (!example_utc_milliseconds(&now_ms) || now_ms < started_ms)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        if (now_ms - started_ms >= UINT64_C(10000)) {
            fprintf(stderr, "timed out waiting for Compositor commit response\n");
            return RIN_VK_TIMEOUT;
        }
    }
    if (state->status != RIN_RESULT_OK) {
        fprintf(stderr, "Compositor rejected frame sequence %llu: %d\n",
                (unsigned long long)state->frame_sequence, state->status);
        return RIN_VK_ERROR_SURFACE_LOST_KHR;
    }
    printf("Compositor accepted damage/commit sequence %llu.\n",
           (unsigned long long)state->frame_sequence);
    return RIN_VK_SUCCESS;
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

static int present_one_clear_frame(RinVkPhysicalDevice physical_device,
                                   uint32_t queue_family,
                                   RinVkSurfaceKHR surface,
                                   RinRuntimeGuiHandle window,
                                   ExampleFrameCompletion* completion) {
    static const char* const device_extensions[] = {
        RIN_VK_KHR_SWAPCHAIN_EXTENSION,
        RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION
    };
    RinVkSurfaceCapabilitiesKHR capabilities;
    RinVkSurfaceFormatKHR* formats = NULL;
    RinVkPresentModeKHR* present_modes = NULL;
    RinVkImage* swapchain_images = NULL;
    RinVkDeviceQueueCreateInfo queue_info;
    RinVkDeviceCreateInfo device_info;
    RinVkPhysicalDeviceFeatures2 physical_features;
    RinVkPhysicalDeviceSynchronization2Features synchronization2;
    RinVkSwapchainCreateInfoKHR swapchain_info;
    RinVkSemaphoreCreateInfo semaphore_info;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandBufferAllocateInfo command_info;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkSubmitInfo submit_info;
    RinVkPresentInfoKHR present_info;
    RinVkClearColorValue clear_color;
    RinVkImageSubresourceRange clear_range;
    RinVkResult present_result = RIN_VK_ERROR_UNKNOWN;
    RinVkResult swapchain_present_result = RIN_VK_ERROR_UNKNOWN;
    RinVkDevice device = NULL;
    RinVkQueue queue = NULL;
    RinVkSwapchainKHR swapchain = 0u;
    RinVkSemaphore image_available = 0u;
    RinVkSemaphore render_complete = 0u;
    RinVkCommandPool command_pool = NULL;
    RinVkCommandBuffer command_buffer = NULL;
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
         RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u ||
        capabilities.currentTransform == 0u ||
        (capabilities.supportedTransforms & capabilities.currentTransform) ==
            0u) {
        fprintf(stderr,
                "surface does not expose a valid one-frame transfer path\n");
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
    memset(&synchronization2, 0, sizeof(synchronization2));
    synchronization2.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    memset(&physical_features, 0, sizeof(physical_features));
    physical_features.sType = RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_features.pNext = &synchronization2;
    vkGetPhysicalDeviceFeatures2(physical_device, &physical_features);
    if (synchronization2.synchronization2 != 1u) {
        fprintf(stderr, "device does not enable the synchronization2 feature\n");
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
        synchronization2.synchronization2 = 1u;
        device_info.pNext = &synchronization2;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount =
            (uint32_t)(sizeof(device_extensions) /
                       sizeof(device_extensions[0]));
        device_info.ppEnabledExtensionNames = device_extensions;
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
    swapchain_info.imageUsage = RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vk_result = vkCreateSemaphore(device, &semaphore_info, NULL,
                                  &image_available);
    if (vk_result != RIN_VK_SUCCESS || image_available == 0u) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateSemaphore(image-available)",
                              vk_result);
        else
            fprintf(stderr, "image-available semaphore is null\n");
        goto cleanup;
    }
    vk_result = vkCreateSemaphore(device, &semaphore_info, NULL,
                                  &render_complete);
    if (vk_result != RIN_VK_SUCCESS || render_complete == 0u) {
        if (vk_result != RIN_VK_SUCCESS)
            report_vk_failure("vkCreateSemaphore(render-complete)",
                              vk_result);
        else
            fprintf(stderr, "render-complete semaphore is null\n");
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
                                      image_available, 0u, &image_index);
    if (vk_result != RIN_VK_SUCCESS &&
        vk_result != RIN_VK_SUBOPTIMAL_KHR) {
        report_vk_failure("vkAcquireNextImageKHR", vk_result);
        goto cleanup;
    }
    if (image_index >= image_count) {
        fprintf(stderr, "acquired swapchain image index is out of range\n");
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
                         RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0u,
                         RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    memset(&clear_color, 0, sizeof(clear_color));
    clear_color.float32[0] = 0.08f;
    clear_color.float32[1] = 0.18f;
    clear_color.float32[2] = 0.42f;
    clear_color.float32[3] = 1.0f;
    memset(&clear_range, 0, sizeof(clear_range));
    clear_range.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.levelCount = 1u;
    clear_range.layerCount = 1u;
    vkCmdClearColorImage(command_buffer, swapchain_images[image_index],
                         RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear_color, 1u, &clear_range);
    record_image_barrier(command_buffer, swapchain_images[image_index],
                         RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         RIN_VK_ACCESS_2_MEMORY_READ_BIT,
                         RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    vk_result = vkEndCommandBuffer(command_buffer);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkEndCommandBuffer", vk_result);
        goto cleanup;
    }

    {
        const uint32_t wait_stage = RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        memset(&submit_info, 0, sizeof(submit_info));
        submit_info.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1u;
        submit_info.pWaitSemaphores = &image_available;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1u;
        submit_info.pCommandBuffers = &command_buffer;
        submit_info.signalSemaphoreCount = 1u;
        submit_info.pSignalSemaphores = &render_complete;
        vk_result = vkQueueSubmit(queue, 1u, &submit_info, 0u);
    }
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkQueueSubmit", vk_result);
        goto cleanup;
    }
    submission_pending = 1;

    memset(&present_info, 0, sizeof(present_info));
    present_info.sType = RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1u;
    present_info.pWaitSemaphores = &render_complete;
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
    vk_result = vkQueueWaitIdle(queue);
    if (vk_result != RIN_VK_SUCCESS) {
        report_vk_failure("vkQueueWaitIdle", vk_result);
        goto cleanup;
    }
    submission_pending = 0;
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
    if (device && render_complete != 0u)
        vkDestroySemaphore(device, render_complete, NULL);
    if (device && image_available != 0u)
        vkDestroySemaphore(device, image_available, NULL);
    free(swapchain_images);
    if (device && swapchain != 0u)
        vkDestroySwapchainKHR(device, swapchain, NULL);
    if (device) vkDestroyDevice(device, NULL);
    free(present_modes);
    free(formats);
    return exit_status;
}

int main(void) {
    static const char* const instance_extensions[] = {
        RIN_VK_KHR_SURFACE_EXTENSION,
        RIN_VK_RINOS_NATIVE_WINDOW_SURFACE_EXTENSION
    };
    RinRuntimeGuiHandle window = RIN_WINDOW_HANDLE_INVALID;
    RinRuntimeCompositorGpuSurfaceOpsV1 surface_ops;
    ExampleFrameCompletion completion;
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

    memset(&completion, 0, sizeof(completion));

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
                completion.window = window;
                runtime_result = wnd_set_compositor_completion_callback(
                    window, example_compositor_completion, &completion);
                if (runtime_result != RIN_RESULT_OK) {
                    free(queue_families);
                    fprintf(stderr,
                            "wnd_set_compositor_completion_callback failed: %d\n",
                            runtime_result);
                    goto cleanup;
                }
                printf("RinOS native-window surface is supported by "
                       "device %u, queue family %u.\n", device_index, family);
                free(queue_families);
                exit_status = present_one_clear_frame(
                    physical_devices[device_index], family, surface, window,
                    &completion);
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
    if (window != RIN_WINDOW_HANDLE_INVALID) {
        (void)wnd_set_compositor_completion_callback(window, NULL, NULL);
        wnd_close(window);
    }
    return exit_status;
}
