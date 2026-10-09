/* SPDX-License-Identifier: MIT */
/* Minimal RinOS application bootstrap for the ordinary Compositor WSI path.
 * The platform must already have admitted and bound its real Vulkan runtime;
 * this example never initializes the software host platform as a fallback. */
#include <rinvulkan/icd.h>
#include <rin/contract_abi.h>
#include <rinruntime/window.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void) {
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
            if (supported != 0u) {
                printf("RinOS native-window surface is supported by "
                       "device %u, queue family %u.\n", device_index, family);
                free(queue_families);
                exit_status = 0;
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
    if (window != RIN_WINDOW_HANDLE_INVALID) wnd_close(window);
    return exit_status;
}
