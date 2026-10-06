/* SPDX-License-Identifier: MIT */
#include <rinvulkan/icd.h>

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int (*RinVulkanRuntimeInitFn)(
    RinGpuVulkanRuntimeV1*, const RinGpuVulkanPhysicalDeviceV2*, uint32_t,
    uint64_t);
typedef int (*RinVulkanRuntimeShutdownFn)(RinGpuVulkanRuntimeV1*);
typedef int (*RinVulkanIcdBindRuntimeFn)(RinGpuVulkanRuntimeV1*);
typedef int (*RinVulkanIcdUnbindRuntimeFn)(RinGpuVulkanRuntimeV1*);
typedef RinVkResult (RIN_VKAPI_CALL *RinVkCreateDebugMessengerFn)(
    RinVkInstance, const RinVkDebugUtilsMessengerCreateInfoEXT*, const void*,
    RinVkDebugUtilsMessengerEXT*);
typedef void (RIN_VKAPI_CALL *RinVkDestroyDebugMessengerFn)(
    RinVkInstance, RinVkDebugUtilsMessengerEXT, const void*);

static int resolve_symbol(void* module, const char* name, void* output,
                          size_t output_size) {
    void* address = dlsym(module, name);
    if (!address || output_size != sizeof(address)) return 0;
    memcpy(output, &address, sizeof(address));
    return 1;
}

static void initialize_profile(RinGpuVulkanPhysicalDeviceV2* profile) {
    memset(profile, 0, sizeof(*profile));
    profile->struct_size = sizeof(*profile);
    profile->version = RIN_GPU_VULKAN_PHYSICAL_VERSION;
    profile->api_version = RIN_GPU_VK_ICD_API_VERSION;
    profile->flags = RIN_GPU_VK_PHYSICAL_DMA_ISOLATED |
                     RIN_GPU_VK_PHYSICAL_RESET_CAPABLE;
    profile->features = RIN_GPU_VK_FEATURE_KNOWN;
    profile->device_uuid[0] = 0x31u;
    profile->driver_digest[0] = 0x42u;
    profile->iommu_domain_cookie = UINT64_C(0x3001);
    profile->device_epoch = 7u;
    profile->queue_family_count = 1u;
    profile->queue_families[0].flags = RIN_GPU_VK_QUEUE_GRAPHICS |
                                       RIN_GPU_VK_QUEUE_COMPUTE |
                                       RIN_GPU_VK_QUEUE_TRANSFER;
    profile->queue_families[0].queue_count = 1u;
    profile->queue_families[0].timestamp_valid_bits = 64u;
    profile->memory_heap_count = 1u;
    profile->memory_heaps[0].size_bytes = UINT64_C(512) * 1024u * 1024u;
    profile->memory_heaps[0].flags = RIN_GPU_VK_HEAP_DEVICE_LOCAL;
    profile->memory_type_count = 1u;
    profile->memory_types[0].heap_index = 0u;
    profile->memory_types[0].property_flags = RIN_GPU_VK_MEMORY_DEVICE_LOCAL;
    profile->max_sampler_anisotropy = 1.0f;
    profile->max_image_dimension_2d = 4096u;
    profile->max_bound_descriptor_sets = 4u;
    profile->max_per_stage_resources = 128u;
    profile->max_push_constants_size = 128u;
    profile->max_memory_allocation_count = 4096u;
    profile->timestamp_period_ns_x1000 = 1000u;
    profile->max_buffer_size = UINT64_C(1) << 30u;
    profile->driver_version = RIN_GPU_VK_MAKE_VERSION(1u, 0u, 0u);
    profile->vendor_id = 0x1af4u;
    profile->device_id = 0x1050u;
    profile->device_type = RIN_GPU_VK_PHYSICAL_TYPE_VIRTUAL_GPU;
    memcpy(profile->device_name, "RinVulkan validation",
           sizeof("RinVulkan validation"));
    profile->pipeline_cache_uuid[0] = 0x53u;
}

static uint32_t RIN_VKAPI_CALL validation_callback(
        uint32_t severity, uint32_t types,
        const RinVkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data) {
    uint32_t* error_count = (uint32_t*)user_data;
    (void)types;
    if ((severity & RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
        ++*error_count;
        fprintf(stderr, "Vulkan validation error: %s\n",
                callback_data && callback_data->pMessage
                    ? callback_data->pMessage
                    : "(no message)");
    }
    return 0u;
}

int main(int argc, char** argv) {
    static const char* const enabled_layers[] = {
        "VK_LAYER_KHRONOS_validation"};
    static const char* const enabled_extensions[] = {
        RIN_VK_EXT_DEBUG_UTILS_EXTENSION};
    void* module = NULL;
    RinVulkanRuntimeInitFn runtime_init = NULL;
    RinVulkanRuntimeShutdownFn runtime_shutdown = NULL;
    RinVulkanIcdBindRuntimeFn bind_runtime = NULL;
    RinVulkanIcdUnbindRuntimeFn unbind_runtime = NULL;
    RinVkCreateDebugMessengerFn create_messenger = NULL;
    RinVkDestroyDebugMessengerFn destroy_messenger = NULL;
    RinGpuVulkanRuntimeV1 runtime;
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinVkApplicationInfo application;
    RinVkDebugUtilsMessengerCreateInfoEXT debug_info;
    RinVkInstanceCreateInfo instance_info;
    RinVkInstance instance = NULL;
    RinVkDebugUtilsMessengerEXT messenger = 0u;
    RinVkPhysicalDevice physical_devices[4];
    RinVkPhysicalDeviceProperties physical_properties;
    RinVkDeviceQueueCreateInfo queue_info;
    RinVkDeviceCreateInfo device_info;
    RinVkDevice device = NULL;
    RinVkQueue queue = NULL;
    float queue_priority = 1.0f;
    uint32_t physical_device_count =
        (uint32_t)(sizeof(physical_devices) / sizeof(physical_devices[0]));
    uint32_t validation_errors = 0u;
    int runtime_initialized = 0;
    int runtime_bound = 0;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <RinVulkan ICD shared library>\n", argv[0]);
        return 2;
    }
    module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        fprintf(stderr, "cannot load RinVulkan ICD: %s\n", dlerror());
        goto cleanup;
    }
    if (!resolve_symbol(module, "rin_gpu_vulkan_runtime_init",
                        &runtime_init, sizeof(runtime_init)) ||
        !resolve_symbol(module, "rin_gpu_vulkan_runtime_shutdown",
                        &runtime_shutdown, sizeof(runtime_shutdown)) ||
        !resolve_symbol(module, "rin_gpu_vulkan_icd_bind_runtime",
                        &bind_runtime, sizeof(bind_runtime)) ||
        !resolve_symbol(module, "rin_gpu_vulkan_icd_unbind_runtime",
                        &unbind_runtime, sizeof(unbind_runtime))) {
        fprintf(stderr, "RinVulkan ICD product binding exports are missing\n");
        goto cleanup;
    }

    initialize_profile(&profile);
    if (runtime_init(&runtime, &profile, 1u,
                     UINT64_C(0x123456789abcdef0)) != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "RinVulkan runtime initialization failed\n");
        goto cleanup;
    }
    runtime_initialized = 1;
    if (bind_runtime(&runtime) != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "RinVulkan ICD runtime bind failed\n");
        goto cleanup;
    }
    runtime_bound = 1;

    memset(&application, 0, sizeof(application));
    application.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "RinVulkan loader validation smoke";
    application.pEngineName = "RinOS host validation";
    application.apiVersion = RIN_GPU_VK_ICD_API_VERSION;

    memset(&debug_info, 0, sizeof(debug_info));
    debug_info.sType =
        RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.messageSeverity =
        RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType = RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = validation_callback;
    debug_info.pUserData = &validation_errors;

    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = &debug_info;
    instance_info.pApplicationInfo = &application;
    instance_info.enabledLayerCount =
        (uint32_t)(sizeof(enabled_layers) / sizeof(enabled_layers[0]));
    instance_info.ppEnabledLayerNames = enabled_layers;
    instance_info.enabledExtensionCount =
        (uint32_t)(sizeof(enabled_extensions) / sizeof(enabled_extensions[0]));
    instance_info.ppEnabledExtensionNames = enabled_extensions;

    {
        RinVkResult api_result = vkCreateInstance(&instance_info, NULL,
                                                   &instance);
        if (api_result != RIN_VK_SUCCESS || !instance) {
            fprintf(stderr, "loader vkCreateInstance failed: %d\n",
                    api_result);
            goto cleanup;
        }
    }
    {
        RinVkVoidFunction create_entry = vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        RinVkVoidFunction destroy_entry = vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
        if (!create_entry || !destroy_entry ||
            sizeof(create_messenger) != sizeof(create_entry) ||
            sizeof(destroy_messenger) != sizeof(destroy_entry)) {
            fprintf(stderr, "loader debug-utils dispatch is unavailable\n");
            goto cleanup;
        }
        memcpy(&create_messenger, &create_entry, sizeof(create_messenger));
        memcpy(&destroy_messenger, &destroy_entry, sizeof(destroy_messenger));
    }
    if (create_messenger(instance, &debug_info, NULL, &messenger) !=
            RIN_VK_SUCCESS ||
        messenger == 0u) {
        fprintf(stderr, "loader vkCreateDebugUtilsMessengerEXT failed\n");
        goto cleanup;
    }

    if (vkEnumeratePhysicalDevices(instance, &physical_device_count,
                                   physical_devices) != RIN_VK_SUCCESS ||
        physical_device_count == 0u) {
        fprintf(stderr, "loader did not expose the RinVulkan fixture device\n");
        goto cleanup;
    }
    memset(&physical_properties, 0, sizeof(physical_properties));
    vkGetPhysicalDeviceProperties(physical_devices[0], &physical_properties);
    if (strcmp(physical_properties.deviceName, "RinVulkan validation") != 0) {
        fprintf(stderr, "loader selected an unexpected Vulkan ICD: %s\n",
                physical_properties.deviceName);
        goto cleanup;
    }
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = 0u;
    queue_info.queueCount = 1u;
    queue_info.pQueuePriorities = &queue_priority;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1u;
    device_info.pQueueCreateInfos = &queue_info;
    if (vkCreateDevice(physical_devices[0], &device_info, NULL, &device) !=
            RIN_VK_SUCCESS ||
        !device) {
        fprintf(stderr, "loader vkCreateDevice failed\n");
        goto cleanup;
    }
    vkGetDeviceQueue(device, 0u, 0u, &queue);
    if (!queue) {
        fprintf(stderr, "loader logical-device dispatch failed\n");
        goto cleanup;
    }
    vkDestroyDevice(device, NULL);
    device = NULL;

    destroy_messenger(instance, messenger, NULL);
    messenger = 0u;
    vkDestroyInstance(instance, NULL);
    instance = NULL;
    if (validation_errors != 0u) {
        fprintf(stderr, "validation reported %u error(s)\n",
                validation_errors);
        goto cleanup;
    }
    puts("rinvulkan-loader-validation PASS (0 validation errors)");
    result = 0;

cleanup:
    if (device) vkDestroyDevice(device, NULL);
    if (instance) {
        if (messenger && destroy_messenger)
            destroy_messenger(instance, messenger, NULL);
        vkDestroyInstance(instance, NULL);
    }
    if (runtime_bound && unbind_runtime(&runtime) != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "RinVulkan ICD runtime unbind failed\n");
        result = 1;
    }
    if (runtime_initialized &&
        runtime_shutdown(&runtime) != RIN_GPU_VULKAN_OK) {
        fprintf(stderr, "RinVulkan runtime shutdown failed\n");
        result = 1;
    }
    if (module && dlclose(module) != 0) {
        fprintf(stderr, "RinVulkan ICD unload failed\n");
        result = 1;
    }
    return result;
}
