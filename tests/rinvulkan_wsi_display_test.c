/* SPDX-License-Identifier: MIT */
#include <rinvulkan/icd.h>
#include <rinvulkan/software_platform.h>

#include <stdio.h>
#include <string.h>

typedef struct TestWsiProvider {
    uint64_t output_generation;
    uint64_t device_generation;
    int query_displays_result;
    int query_modes_result;
    uint32_t display_count;
    uint32_t mode_count;
    RinVulkanWsiDisplayV1 display;
    RinVulkanWsiModeV1 modes[RIN_VULKAN_WSI_MAX_MODES];
} TestWsiProvider;

static void make_profile(RinGpuVulkanPhysicalDeviceV2* profile) {
    memset(profile, 0, sizeof(*profile));
    profile->struct_size = sizeof(*profile);
    profile->version = RIN_GPU_VULKAN_PHYSICAL_VERSION;
    profile->api_version = RIN_GPU_VK_MAKE_VERSION(1u, 3u, 0u);
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
    profile->memory_heap_count = 2u;
    profile->memory_heaps[0].size_bytes = UINT64_C(512) * 1024u * 1024u;
    profile->memory_heaps[0].flags = RIN_GPU_VK_HEAP_DEVICE_LOCAL;
    profile->memory_heaps[1].size_bytes = UINT64_C(128) * 1024u * 1024u;
    profile->memory_type_count = 2u;
    profile->memory_types[0].heap_index = 0u;
    profile->memory_types[0].property_flags =
        RIN_GPU_VK_MEMORY_DEVICE_LOCAL;
    profile->memory_types[1].heap_index = 1u;
    profile->memory_types[1].property_flags =
        RIN_GPU_VK_MEMORY_HOST_VISIBLE | RIN_GPU_VK_MEMORY_HOST_COHERENT;
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
    memcpy(profile->device_name, "RinGPU WSI test", sizeof("RinGPU WSI test"));
    profile->pipeline_cache_uuid[0] = 0x53u;
}

static void make_display_provider(TestWsiProvider* provider) {
    RinVulkanWsiDisplayV1* display;
    RinVulkanWsiModeV1* mode;
    memset(provider, 0, sizeof(*provider));
    provider->display_count = 1u;
    provider->mode_count = 2u;
    provider->output_generation = 11u;
    provider->device_generation = 7u;
    display = &provider->display;
    display->struct_size = sizeof(*display);
    display->version = RIN_VULKAN_WSI_PLATFORM_VERSION;
    display->display_cookie = UINT64_C(0xabc1);
    display->output_generation = provider->output_generation;
    display->device_generation = provider->device_generation;
    display->current_mode_cookie = UINT64_C(0xabc2);
    display->width = 1920u;
    display->height = 1080u;
    display->refresh_millihertz = 60000u;
    display->format = RIN_VK_FORMAT_R8G8B8A8_UNORM;
    display->physical_width_mm = 600u;
    display->physical_height_mm = 340u;
    display->flags = RIN_VULKAN_WSI_DISPLAY_TRANSFORM_IDENTITY |
                     RIN_VULKAN_WSI_DISPLAY_PERSISTENT_CONTENT;
    display->plane_count = 1u;
    display->mode_count = provider->mode_count;
    memcpy(display->display_name, "RinOS primary", sizeof("RinOS primary"));

    mode = &provider->modes[0];
    mode->struct_size = sizeof(*mode);
    mode->version = RIN_VULKAN_WSI_PLATFORM_VERSION;
    mode->display_cookie = display->display_cookie;
    mode->mode_cookie = display->current_mode_cookie;
    mode->output_generation = provider->output_generation;
    mode->width = display->width;
    mode->height = display->height;
    mode->refresh_millihertz = display->refresh_millihertz;
    mode->format = display->format;

    mode = &provider->modes[1];
    mode->struct_size = sizeof(*mode);
    mode->version = RIN_VULKAN_WSI_PLATFORM_VERSION;
    mode->display_cookie = display->display_cookie;
    mode->mode_cookie = UINT64_C(0xabc3);
    mode->output_generation = provider->output_generation;
    mode->width = 1280u;
    mode->height = 720u;
    mode->refresh_millihertz = 60000u;
    mode->format = display->format;
}

static int query_displays(void* context, uint64_t device_generation,
                          uint32_t capacity, uint32_t* count_out,
                          RinVulkanWsiDisplayV1* displays_out) {
    TestWsiProvider* provider = (TestWsiProvider*)context;
    uint32_t written;
    if (!provider || !count_out ||
        (capacity != 0u && !displays_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    *count_out = 0u;
    if (provider->query_displays_result != RIN_VULKAN_WSI_PLATFORM_OK)
        return provider->query_displays_result;
    if (device_generation != provider->device_generation)
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    written = capacity < provider->display_count
                  ? capacity
                  : provider->display_count;
    if (written != 0u) displays_out[0] = provider->display;
    *count_out = written;
    return written < provider->display_count
               ? RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
               : RIN_VULKAN_WSI_PLATFORM_OK;
}

static int query_modes(void* context, uint64_t device_generation,
                       uint64_t display_cookie, uint64_t output_generation,
                       uint32_t capacity, uint32_t* count_out,
                       RinVulkanWsiModeV1* modes_out) {
    TestWsiProvider* provider = (TestWsiProvider*)context;
    uint32_t written;
    if (!provider || !count_out || (capacity != 0u && !modes_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    *count_out = 0u;
    if (provider->query_modes_result != RIN_VULKAN_WSI_PLATFORM_OK)
        return provider->query_modes_result;
    if (device_generation != provider->device_generation)
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    if (display_cookie != provider->display.display_cookie ||
        output_generation != provider->output_generation)
        return RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
    written = capacity < provider->mode_count ? capacity : provider->mode_count;
    if (written != 0u)
        memcpy(modes_out, provider->modes,
               sizeof(provider->modes[0]) * written);
    *count_out = written;
    return written < provider->mode_count
               ? RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
               : RIN_VULKAN_WSI_PLATFORM_OK;
}

static int present_unsupported(
    void* context, const RinVulkanWsiPresentRequestV1* request,
    uint64_t* present_token_out) {
    (void)context;
    (void)request;
    if (present_token_out) *present_token_out = 0u;
    return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
}

static int poll_present_unsupported(
    void* context, uint64_t display_cookie, uint64_t present_token,
    RinVulkanWsiPresentStatusV1* status_out) {
    (void)context;
    (void)display_cookie;
    (void)present_token;
    (void)status_out;
    return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
}

static int cancel_present_unsupported(void* context, uint64_t display_cookie,
                                      uint64_t present_token) {
    (void)context;
    (void)display_cookie;
    (void)present_token;
    return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
}

int main(void) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanRuntimeV1 runtime;
    RinGpuVulkanSoftwarePlatformV1 software;
    RinVulkanWsiPlatformV1 wsi;
    TestWsiProvider provider;
    RinVkApplicationInfo application;
    RinVkInstanceCreateInfo instance_create;
    RinVkInstance instance = NULL;
    RinVkPhysicalDevice physical_devices[1];
    RinVkDisplayPropertiesKHR display_properties[1];
    RinVkDisplayModePropertiesKHR mode_properties[2];
    RinVkExtensionProperties extensions[2];
    RinVkPhysicalDevice physical = NULL;
    uint32_t count;
    uint32_t instance_initialized = 0u;
    uint32_t runtime_bound = 0u;
    uint32_t software_initialized = 0u;
    uint32_t product_bound = 0u;
    uint32_t wsi_bound = 0u;
    int exit_code = 1;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

    make_profile(&profile);
    CHECK(rin_gpu_vulkan_runtime_init(&runtime, &profile, 1u,
                                     UINT64_C(0x123456789abcdef0)) ==
          RIN_GPU_VULKAN_OK);
    instance_initialized = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_runtime(&runtime) == RIN_GPU_VULKAN_OK);
    runtime_bound = 1u;
    CHECK(rin_gpu_vulkan_software_platform_init(
              &software, profile.iommu_domain_cookie, profile.device_epoch,
              profile.queue_families[0].queue_count,
              UINT64_C(16) * 1024u * 1024u) == RIN_VULKAN_PRODUCT_OK);
    software_initialized = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform(&software.platform) ==
          RIN_GPU_VULKAN_OK);
    product_bound = 1u;

    make_display_provider(&provider);
    memset(&wsi, 0, sizeof(wsi));
    wsi.struct_size = sizeof(wsi);
    wsi.version = RIN_VULKAN_WSI_PLATFORM_VERSION;
    wsi.context = &provider;
    wsi.query_displays = query_displays;
    wsi.query_modes = query_modes;
    wsi.present = present_unsupported;
    wsi.poll_present = poll_present_unsupported;
    wsi.cancel_present = cancel_present_unsupported;
    CHECK(rin_gpu_vulkan_icd_bind_wsi_platform(&wsi) == RIN_GPU_VULKAN_OK);
    wsi_bound = 1u;

    count = 2u;
    CHECK(vkEnumerateInstanceExtensionProperties(NULL, &count, extensions) ==
          RIN_VK_SUCCESS);
    CHECK(count == 1u &&
          strcmp(extensions[0].extensionName,
                 RIN_VK_EXT_DEBUG_UTILS_EXTENSION) == 0);
    memset(&application, 0, sizeof(application));
    application.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "RinVulkan WSI display test";
    application.apiVersion = RIN_GPU_VK_ICD_API_VERSION;
    memset(&instance_create, 0, sizeof(instance_create));
    instance_create.sType = RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create.pApplicationInfo = &application;
    CHECK(vkCreateInstance(&instance_create, NULL, &instance) ==
          RIN_VK_SUCCESS);
    CHECK(rin_gpu_vulkan_icd_unbind_wsi_platform(&wsi) ==
          RIN_GPU_VULKAN_BUSY);
    CHECK(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR")
          == NULL);
    CHECK(vk_icdGetPhysicalDeviceProcAddr(
              instance, "vkGetPhysicalDeviceDisplayPropertiesKHR") == NULL);

    count = 0u;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, NULL) == RIN_VK_SUCCESS);
    CHECK(count == 1u);
    count = 1u;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, physical_devices) ==
          RIN_VK_SUCCESS);
    CHECK(count == 1u);
    physical = physical_devices[0];

    count = 0u;
    CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(physical, &count, NULL) ==
          RIN_VK_SUCCESS);
    CHECK(count == 1u);
    count = 0u;
    CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
              physical, &count, display_properties) == RIN_VK_INCOMPLETE);
    CHECK(count == 0u);
    count = 1u;
    CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
              physical, &count, display_properties) == RIN_VK_SUCCESS);
    CHECK(count == 1u && display_properties[0].display != 0u &&
          strcmp(display_properties[0].displayName, "RinOS primary") == 0 &&
          display_properties[0].physicalResolution.width == 1920u &&
          display_properties[0].physicalResolution.height == 1080u &&
          display_properties[0].supportedTransforms == 1u &&
          display_properties[0].persistentContent == 1u);

    count = 0u;
    CHECK(vkGetDisplayModePropertiesKHR(physical,
              display_properties[0].display, &count, NULL) == RIN_VK_SUCCESS);
    CHECK(count == 2u);
    count = 1u;
    CHECK(vkGetDisplayModePropertiesKHR(physical,
              display_properties[0].display, &count, mode_properties) ==
          RIN_VK_INCOMPLETE);
    CHECK(count == 1u && mode_properties[0].parameters.visibleRegion.width ==
          1920u && mode_properties[0].parameters.visibleRegion.height == 1080u &&
          mode_properties[0].parameters.refreshRate == 60000u);
    count = 2u;
    CHECK(vkGetDisplayModePropertiesKHR(physical,
              display_properties[0].display, &count, mode_properties) ==
          RIN_VK_SUCCESS);
    CHECK(count == 2u && mode_properties[1].parameters.visibleRegion.width ==
          1280u && mode_properties[1].parameters.visibleRegion.height == 720u);

    provider.modes[0].width = 1919u;
    count = 2u;
    CHECK(vkGetDisplayModePropertiesKHR(physical,
              display_properties[0].display, &count, mode_properties) ==
          RIN_VK_ERROR_DEVICE_LOST);
    provider.modes[0].width = 1920u;
    provider.query_modes_result = RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    count = 2u;
    CHECK(vkGetDisplayModePropertiesKHR(physical,
              display_properties[0].display, &count, mode_properties) ==
          RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    provider.query_modes_result = RIN_VULKAN_WSI_PLATFORM_OK;

    {
        const RinVkDisplayKHR old_display = display_properties[0].display;
        const char* const old_display_name = display_properties[0].displayName;
        provider.output_generation += 1u;
        provider.display.output_generation = provider.output_generation;
        provider.display.width = 1600u;
        provider.display.height = 900u;
        provider.modes[0].output_generation = provider.output_generation;
        provider.modes[0].width = provider.display.width;
        provider.modes[0].height = provider.display.height;
        provider.modes[1].output_generation = provider.output_generation;
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
                  physical, &count, display_properties) == RIN_VK_SUCCESS);
        CHECK(count == 1u && display_properties[0].display != old_display &&
              display_properties[0].physicalResolution.width == 1600u &&
              display_properties[0].physicalResolution.height == 900u &&
              strcmp(old_display_name, "RinOS primary") == 0);
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, old_display, &count, mode_properties) ==
              RIN_VK_ERROR_INITIALIZATION_FAILED);
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, display_properties[0].display, &count,
                  mode_properties) == RIN_VK_SUCCESS);
        CHECK(count == 2u &&
              mode_properties[0].parameters.visibleRegion.width == 1600u &&
              mode_properties[0].parameters.visibleRegion.height == 900u);

        provider.display_count = 0u;
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
                  physical, &count, display_properties) == RIN_VK_SUCCESS);
        CHECK(count == 0u);
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, display_properties[0].display, &count,
                  mode_properties) == RIN_VK_ERROR_INITIALIZATION_FAILED);
    }
    exit_code = 0;

cleanup:
    if (instance) vkDestroyInstance(instance, NULL);
    if (wsi_bound &&
        rin_gpu_vulkan_icd_unbind_wsi_platform(&wsi) != RIN_GPU_VULKAN_OK)
        exit_code = 1;
    if (product_bound &&
        rin_gpu_vulkan_icd_unbind_product_platform(&software.platform) !=
            RIN_GPU_VULKAN_OK)
        exit_code = 1;
    if (runtime_bound &&
        rin_gpu_vulkan_icd_unbind_runtime(&runtime) != RIN_GPU_VULKAN_OK)
        exit_code = 1;
    if (software_initialized &&
        rin_gpu_vulkan_software_platform_shutdown(&software) !=
            RIN_VULKAN_PRODUCT_OK)
        exit_code = 1;
    if (instance_initialized &&
        rin_gpu_vulkan_runtime_shutdown(&runtime) != RIN_GPU_VULKAN_OK)
        exit_code = 1;
    return exit_code;
}
