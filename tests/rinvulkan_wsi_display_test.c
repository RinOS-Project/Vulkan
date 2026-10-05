/* SPDX-License-Identifier: MIT */
#include <rinvulkan/icd.h>
#include <rinvulkan/software_platform.h>

#include <stdio.h>
#include <string.h>

#define TEST_DISPLAY_SURFACE_CAPACITY 64u

typedef struct TestWsiProvider {
    uint64_t output_generation;
    uint64_t device_generation;
    int query_displays_result;
    int query_modes_result;
    int query_planes_result;
    int query_plane_capabilities_result;
    uint32_t display_count;
    uint32_t mode_count;
    uint32_t plane_count;
    uint32_t supported_display_count;
    uint32_t present_call_count;
    RinVulkanWsiDisplayV1 display;
    RinVulkanWsiModeV1 modes[RIN_VULKAN_WSI_MAX_MODES];
    RinVulkanWsiDisplayPlaneV2 planes[RIN_VULKAN_WSI_MAX_PLANES];
    uint64_t supported_display_cookies[RIN_VULKAN_WSI_MAX_DISPLAYS];
    RinVulkanWsiPlaneCapabilitiesV2 plane_capabilities;
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

    provider->plane_count = 1u;
    provider->planes[0].struct_size = sizeof(provider->planes[0]);
    provider->planes[0].version = RIN_VULKAN_WSI_PLATFORM_V2_VERSION;
    provider->planes[0].plane_index = 0u;
    provider->planes[0].current_stack_index = 0u;
    provider->planes[0].current_display_cookie = display->display_cookie;
    provider->planes[0].output_generation = provider->output_generation;
    provider->planes[0].device_generation = provider->device_generation;
    provider->supported_display_count = 1u;
    provider->supported_display_cookies[0] = display->display_cookie;
    provider->plane_capabilities.supported_alpha =
        RIN_VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR |
        RIN_VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
    provider->plane_capabilities.min_src_x = 1;
    provider->plane_capabilities.min_src_y = 2;
    provider->plane_capabilities.max_src_x = 3;
    provider->plane_capabilities.max_src_y = 4;
    provider->plane_capabilities.min_src_width = 1u;
    provider->plane_capabilities.min_src_height = 1u;
    provider->plane_capabilities.max_src_width = 4096u;
    provider->plane_capabilities.max_src_height = 2160u;
    provider->plane_capabilities.min_dst_x = -2;
    provider->plane_capabilities.min_dst_y = -3;
    provider->plane_capabilities.max_dst_x = 4;
    provider->plane_capabilities.max_dst_y = 5;
    provider->plane_capabilities.min_dst_width = 1u;
    provider->plane_capabilities.min_dst_height = 1u;
    provider->plane_capabilities.max_dst_width = 4096u;
    provider->plane_capabilities.max_dst_height = 2160u;
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

static int query_planes(void* context, uint64_t device_generation,
                        uint32_t capacity, uint32_t* count_out,
                        RinVulkanWsiDisplayPlaneV2* planes_out) {
    TestWsiProvider* provider = (TestWsiProvider*)context;
    uint32_t written;
    if (!provider || !count_out || (capacity != 0u && !planes_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    *count_out = 0u;
    if (provider->query_planes_result != RIN_VULKAN_WSI_PLATFORM_OK)
        return provider->query_planes_result;
    if (device_generation != provider->device_generation)
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    written = capacity < provider->plane_count ? capacity
                                                : provider->plane_count;
    if (written != 0u)
        memcpy(planes_out, provider->planes,
               sizeof(provider->planes[0]) * written);
    *count_out = written;
    return written < provider->plane_count
               ? RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
               : RIN_VULKAN_WSI_PLATFORM_OK;
}

static int query_plane_supported_displays(
    void* context, uint64_t device_generation, uint32_t plane_index,
    uint32_t capacity, uint32_t* count_out, uint64_t* display_cookies_out) {
    TestWsiProvider* provider = (TestWsiProvider*)context;
    uint32_t written;
    if (!provider || !count_out ||
        (capacity != 0u && !display_cookies_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    *count_out = 0u;
    if (device_generation != provider->device_generation)
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    if (plane_index >= provider->plane_count)
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    written = capacity < provider->supported_display_count
                  ? capacity
                  : provider->supported_display_count;
    if (written != 0u)
        memcpy(display_cookies_out, provider->supported_display_cookies,
               sizeof(provider->supported_display_cookies[0]) * written);
    *count_out = written;
    return written < provider->supported_display_count
               ? RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
               : RIN_VULKAN_WSI_PLATFORM_OK;
}

static int query_plane_capabilities(
    void* context, uint64_t device_generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    RinVulkanWsiPlaneCapabilitiesV2* capabilities_out) {
    TestWsiProvider* provider = (TestWsiProvider*)context;
    if (!provider || !capabilities_out)
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (provider->query_plane_capabilities_result !=
        RIN_VULKAN_WSI_PLATFORM_OK)
        return provider->query_plane_capabilities_result;
    if (device_generation != provider->device_generation)
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    if (display_cookie != provider->display.display_cookie ||
        output_generation != provider->output_generation)
        return RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
    if ((mode_cookie != provider->modes[0].mode_cookie &&
         mode_cookie != provider->modes[1].mode_cookie) ||
        plane_index >= provider->plane_count)
        return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    *capabilities_out = provider->plane_capabilities;
    return RIN_VULKAN_WSI_PLATFORM_OK;
}

static int present_unsupported(
    void* context, const RinVulkanWsiPresentRequestV1* request,
    uint64_t* present_token_out) {
    (void)request;
    if (context)
        ++((TestWsiProvider*)context)->present_call_count;
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
    RinVulkanWsiPlatformV1 legacy_wsi;
    RinVulkanWsiPlatformV2 wsi;
    TestWsiProvider provider;
    RinVkApplicationInfo application;
    RinVkInstanceCreateInfo instance_create;
    const char* unimplemented_display_extension =
        RIN_VK_KHR_DISPLAY_EXTENSION;
    RinVkInstance instance = NULL;
    RinVkPhysicalDevice physical_devices[1];
    RinVkDisplayPropertiesKHR display_properties[1];
    RinVkDisplayModePropertiesKHR mode_properties[2];
    RinVkDisplayModeCreateInfoKHR mode_create_info;
    RinVkDisplayModeKHR created_mode = 0u;
    RinVkDisplaySurfaceCreateInfoKHR surface_create_info;
    RinVkSurfaceKHR created_surface = 0u;
    RinVkSurfaceKHR stale_output_surface = 0u;
    RinVkDisplayPlanePropertiesKHR plane_properties[1];
    RinVkDisplayPlaneCapabilitiesKHR plane_capabilities;
    RinVkDisplayKHR supported_displays[1];
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
    wsi.version = RIN_VULKAN_WSI_PLATFORM_V2_VERSION;
    wsi.context = &provider;
    wsi.query_displays = query_displays;
    wsi.query_modes = query_modes;
    wsi.present = present_unsupported;
    wsi.poll_present = poll_present_unsupported;
    wsi.cancel_present = cancel_present_unsupported;
    wsi.query_planes = query_planes;
    wsi.query_plane_supported_displays = query_plane_supported_displays;
    wsi.query_plane_capabilities = query_plane_capabilities;
    memset(&legacy_wsi, 0, sizeof(legacy_wsi));
    legacy_wsi.struct_size = sizeof(legacy_wsi);
    legacy_wsi.version = RIN_VULKAN_WSI_PLATFORM_VERSION;
    legacy_wsi.context = &provider;
    legacy_wsi.query_displays = query_displays;
    legacy_wsi.query_modes = query_modes;
    legacy_wsi.present = present_unsupported;
    legacy_wsi.poll_present = poll_present_unsupported;
    legacy_wsi.cancel_present = cancel_present_unsupported;
    CHECK(rin_gpu_vulkan_icd_bind_wsi_platform(&legacy_wsi) ==
          RIN_GPU_VULKAN_OK);
    CHECK(rin_gpu_vulkan_icd_unbind_wsi_platform(&legacy_wsi) ==
          RIN_GPU_VULKAN_OK);

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
    instance_create.enabledExtensionCount = 1u;
    instance_create.ppEnabledExtensionNames =
        &unimplemented_display_extension;
    CHECK(vkCreateInstance(&instance_create, NULL, &instance) ==
          RIN_VK_ERROR_EXTENSION_NOT_PRESENT);
    CHECK(instance == NULL);
    instance_create.enabledExtensionCount = 0u;
    instance_create.ppEnabledExtensionNames = NULL;
    CHECK(vkCreateInstance(&instance_create, NULL, &instance) ==
          RIN_VK_SUCCESS);
    CHECK(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR")
          == NULL);
    CHECK(vk_icdGetPhysicalDeviceProcAddr(
              instance, "vkGetPhysicalDeviceDisplayPropertiesKHR") == NULL);
    CHECK(vkGetInstanceProcAddr(
              instance, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR") ==
          NULL);
    CHECK(vkGetInstanceProcAddr(
              instance, "vkGetDisplayPlaneSupportedDisplaysKHR") == NULL);
    CHECK(vkGetInstanceProcAddr(
              instance, "vkGetDisplayPlaneCapabilitiesKHR") == NULL);
    CHECK(vk_icdGetPhysicalDeviceProcAddr(
              instance, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR") ==
          NULL);
    CHECK(vk_icdGetPhysicalDeviceProcAddr(
              instance, "vkGetDisplayPlaneSupportedDisplaysKHR") == NULL);
    CHECK(vk_icdGetPhysicalDeviceProcAddr(
              instance, "vkGetDisplayPlaneCapabilitiesKHR") == NULL);
    CHECK(vkGetInstanceProcAddr(instance, "vkCreateDisplayModeKHR") == NULL);
    CHECK(vk_icdGetPhysicalDeviceProcAddr(
              instance, "vkCreateDisplayModeKHR") == NULL);
    CHECK(vkGetInstanceProcAddr(
              instance, "vkCreateDisplayPlaneSurfaceKHR") == NULL);
    CHECK(vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR") == NULL);

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
          RIN_VK_ERROR_EXTENSION_NOT_PRESENT);
    CHECK(rin_gpu_vulkan_icd_bind_wsi_platform_v2(&wsi) ==
          RIN_GPU_VULKAN_OK);
    wsi_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_unbind_wsi_platform_v2(&wsi) ==
          RIN_GPU_VULKAN_BUSY);

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
    CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
              physical, &count, NULL) == RIN_VK_SUCCESS);
    CHECK(count == 1u);
    count = 0u;
    CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
              physical, &count, plane_properties) == RIN_VK_INCOMPLETE);
    CHECK(count == 0u);
    count = 1u;
    CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
              physical, &count, plane_properties) == RIN_VK_SUCCESS);
    CHECK(count == 1u &&
          plane_properties[0].currentDisplay ==
              display_properties[0].display &&
          plane_properties[0].currentStackIndex == 0u);
    count = 0u;
    CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(
              physical, 0u, &count, NULL) == RIN_VK_SUCCESS);
    CHECK(count == 1u);
    count = 1u;
    CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(
              physical, 0u, &count, supported_displays) == RIN_VK_SUCCESS);
    CHECK(count == 1u &&
          supported_displays[0] == display_properties[0].display);
    provider.supported_display_cookies[0] = UINT64_C(0xdead);
    count = 1u;
    CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(
              physical, 0u, &count, supported_displays) ==
          RIN_VK_ERROR_OUT_OF_DATE_KHR);
    CHECK(count == 0u);
    provider.supported_display_cookies[0] = provider.display.display_cookie;

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
    memset(&mode_create_info, 0, sizeof(mode_create_info));
    mode_create_info.sType = RIN_VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR;
    mode_create_info.parameters = mode_properties[1].parameters;
    CHECK(vkCreateDisplayModeKHR(physical, display_properties[0].display,
              &mode_create_info, NULL, &created_mode) == RIN_VK_SUCCESS);
    CHECK(created_mode == mode_properties[1].displayMode);
    mode_create_info.parameters.visibleRegion.width = 2048u;
    created_mode = UINT64_C(0xfeed);
    CHECK(vkCreateDisplayModeKHR(physical, display_properties[0].display,
              &mode_create_info, NULL, &created_mode) ==
          RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(created_mode == 0u);
    mode_create_info.parameters.visibleRegion.width = 1280u;
    mode_create_info.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    CHECK(vkCreateDisplayModeKHR(physical, display_properties[0].display,
              &mode_create_info, NULL, &created_mode) ==
          RIN_VK_ERROR_INITIALIZATION_FAILED);
    mode_create_info.sType = RIN_VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR;
    CHECK(vkGetDisplayPlaneCapabilitiesKHR(
              physical, mode_properties[0].displayMode, 0u,
              &plane_capabilities) == RIN_VK_SUCCESS);
    CHECK(plane_capabilities.supportedAlpha ==
              (RIN_VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR |
               RIN_VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR) &&
          plane_capabilities.minSrcExtent.width == 1u &&
          plane_capabilities.maxSrcExtent.width == 4096u &&
          plane_capabilities.minSrcPosition.x == 1 &&
          plane_capabilities.maxSrcPosition.y == 4 &&
          plane_capabilities.minDstPosition.x == -2 &&
          plane_capabilities.maxDstPosition.y == 5 &&
          plane_capabilities.maxDstExtent.height == 2160u);

    memset(&surface_create_info, 0, sizeof(surface_create_info));
    surface_create_info.sType =
        RIN_VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surface_create_info.displayMode = mode_properties[0].displayMode;
    surface_create_info.planeIndex = 0u;
    surface_create_info.planeStackIndex = 0u;
    surface_create_info.transform =
        RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surface_create_info.globalAlpha = 0.75f;
    surface_create_info.alphaMode =
        RIN_VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
    surface_create_info.imageExtent.width = 1920u;
    surface_create_info.imageExtent.height = 1080u;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_SUCCESS);
    CHECK(created_surface != 0u);
    CHECK(provider.present_call_count == 0u &&
          provider.display.current_mode_cookie == UINT64_C(0xabc2) &&
          provider.output_generation == 11u);
    {
        const RinVkSurfaceKHR stale_surface = created_surface;
        vkDestroySurfaceKHR(instance, created_surface, NULL);
        created_surface = 0u;
        CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info,
                  NULL, &created_surface) == RIN_VK_SUCCESS);
        CHECK(created_surface != 0u && created_surface != stale_surface);
        vkDestroySurfaceKHR(instance, stale_surface, NULL);
        vkDestroySurfaceKHR(instance, created_surface, NULL);
        created_surface = 0u;
    }
    {
        RinVkSurfaceKHR surface_pool[TEST_DISPLAY_SURFACE_CAPACITY];
        uint32_t surface_index;
        for (surface_index = 0u;
             surface_index < TEST_DISPLAY_SURFACE_CAPACITY; ++surface_index) {
            CHECK(vkCreateDisplayPlaneSurfaceKHR(
                      instance, &surface_create_info, NULL,
                      &surface_pool[surface_index]) == RIN_VK_SUCCESS);
            CHECK(surface_pool[surface_index] != 0u);
        }
        created_surface = UINT64_C(0xfeed);
        CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info,
                  NULL, &created_surface) ==
              RIN_VK_ERROR_OUT_OF_HOST_MEMORY);
        CHECK(created_surface == 0u);
        for (surface_index = 0u;
             surface_index < TEST_DISPLAY_SURFACE_CAPACITY; ++surface_index)
            vkDestroySurfaceKHR(instance, surface_pool[surface_index], NULL);
        CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info,
                  NULL, &created_surface) == RIN_VK_SUCCESS);
        CHECK(created_surface != 0u && created_surface != surface_pool[0]);
        vkDestroySurfaceKHR(instance, created_surface, NULL);
        created_surface = 0u;
    }
    CHECK(provider.present_call_count == 0u &&
          provider.display.current_mode_cookie == UINT64_C(0xabc2) &&
          provider.output_generation == 11u);
    surface_create_info.planeStackIndex = 1u;
    created_surface = UINT64_C(0xfeed);
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(created_surface == 0u);
    surface_create_info.planeStackIndex = 0u;
    surface_create_info.alphaMode =
        RIN_VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(created_surface == 0u);
    surface_create_info.alphaMode =
        RIN_VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
    surface_create_info.globalAlpha = 1.01f;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(created_surface == 0u);
    surface_create_info.globalAlpha = 0.75f;
    surface_create_info.transform = UINT32_C(0x00000002);
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(created_surface == 0u);
    surface_create_info.transform =
        RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surface_create_info.imageExtent.width = 4097u;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_INITIALIZATION_FAILED);
    CHECK(created_surface == 0u);
    surface_create_info.imageExtent.width = 1920u;
    surface_create_info.pNext = &mode_create_info;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(created_surface == 0u);
    surface_create_info.pNext = NULL;
    provider.supported_display_cookies[0] = UINT64_C(0xdead);
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_OUT_OF_DATE_KHR);
    CHECK(created_surface == 0u);
    provider.supported_display_cookies[0] = provider.display.display_cookie;
    provider.query_plane_capabilities_result =
        RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, NULL,
              &created_surface) == RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(created_surface == 0u);
    provider.query_plane_capabilities_result = RIN_VULKAN_WSI_PLATFORM_OK;

    provider.plane_capabilities.max_src_width = 0u;
    CHECK(vkGetDisplayPlaneCapabilitiesKHR(
              physical, mode_properties[0].displayMode, 0u,
              &plane_capabilities) == RIN_VK_ERROR_DEVICE_LOST);
    provider.plane_capabilities.max_src_width = 4096u;
    provider.query_planes_result = RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    count = 1u;
    CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
              physical, &count, plane_properties) ==
          RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    provider.query_planes_result = RIN_VULKAN_WSI_PLATFORM_OK;
    provider.query_plane_capabilities_result =
        RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    CHECK(vkGetDisplayPlaneCapabilitiesKHR(
              physical, mode_properties[0].displayMode, 0u,
              &plane_capabilities) == RIN_VK_ERROR_FEATURE_NOT_PRESENT);
    provider.query_plane_capabilities_result = RIN_VULKAN_WSI_PLATFORM_OK;

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
        CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info,
                  NULL, &stale_output_surface) == RIN_VK_SUCCESS);
        CHECK(stale_output_surface != 0u);
        provider.output_generation += 1u;
        provider.display.output_generation = provider.output_generation;
        provider.planes[0].output_generation = provider.output_generation;
        provider.display.width = 1600u;
        provider.display.height = 900u;
        provider.modes[0].output_generation = provider.output_generation;
        provider.modes[0].width = provider.display.width;
        provider.modes[0].height = provider.display.height;
        provider.modes[1].output_generation = provider.output_generation;
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, old_display, &count, mode_properties) ==
              RIN_VK_ERROR_OUT_OF_DATE_KHR);
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
                  physical, &count, display_properties) == RIN_VK_SUCCESS);
        CHECK(count == 1u && display_properties[0].display != old_display &&
              display_properties[0].physicalResolution.width == 1600u &&
              display_properties[0].physicalResolution.height == 900u &&
              strcmp(old_display_name, "RinOS primary") == 0);
        created_surface = UINT64_C(0xfeed);
        CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info,
                  NULL, &created_surface) ==
              RIN_VK_ERROR_INITIALIZATION_FAILED);
        CHECK(created_surface == 0u);
        vkDestroySurfaceKHR(instance, stale_output_surface, NULL);
        stale_output_surface = 0u;
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
                  physical, &count, plane_properties) == RIN_VK_SUCCESS);
        CHECK(count == 1u &&
              plane_properties[0].currentDisplay ==
                  display_properties[0].display);
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
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
                  physical, &count, plane_properties) ==
              RIN_VK_ERROR_OUT_OF_DATE_KHR);
        CHECK(count == 0u);
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, display_properties[0].display, &count,
                  mode_properties) == RIN_VK_ERROR_INITIALIZATION_FAILED);
    }
    {
        RinVkSurfaceKHR live_surfaces[TEST_DISPLAY_SURFACE_CAPACITY];
        uint32_t surface_index;
        provider.display_count = 1u;
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
                  physical, &count, display_properties) == RIN_VK_SUCCESS);
        CHECK(count == 1u);
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, display_properties[0].display, &count,
                  mode_properties) == RIN_VK_SUCCESS);
        CHECK(count == 2u);
        surface_create_info.displayMode = mode_properties[0].displayMode;
        surface_create_info.imageExtent.width = 1600u;
        surface_create_info.imageExtent.height = 900u;
        for (surface_index = 0u;
             surface_index < TEST_DISPLAY_SURFACE_CAPACITY; ++surface_index) {
            CHECK(vkCreateDisplayPlaneSurfaceKHR(
                      instance, &surface_create_info, NULL,
                      &live_surfaces[surface_index]) == RIN_VK_SUCCESS);
        }
        vkDestroyInstance(instance, NULL);
        instance = NULL;
        CHECK(vkCreateInstance(&instance_create, NULL, &instance) ==
              RIN_VK_SUCCESS);
        CHECK(vkGetInstanceProcAddr(
                  instance, "vkCreateDisplayPlaneSurfaceKHR") == NULL);
        count = 1u;
        CHECK(vkEnumeratePhysicalDevices(
                  instance, &count, physical_devices) == RIN_VK_SUCCESS);
        CHECK(count == 1u);
        physical = physical_devices[0];
        count = 1u;
        CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(
                  physical, &count, display_properties) == RIN_VK_SUCCESS);
        CHECK(count == 1u);
        count = 2u;
        CHECK(vkGetDisplayModePropertiesKHR(
                  physical, display_properties[0].display, &count,
                  mode_properties) == RIN_VK_SUCCESS);
        CHECK(count == 2u);
        surface_create_info.displayMode = mode_properties[0].displayMode;
        CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info,
                  NULL, &created_surface) == RIN_VK_SUCCESS);
        CHECK(created_surface != 0u && created_surface != live_surfaces[0]);
        vkDestroySurfaceKHR(instance, created_surface, NULL);
        created_surface = 0u;
    }
    exit_code = 0;

cleanup:
    if (instance) vkDestroyInstance(instance, NULL);
    if (wsi_bound &&
        rin_gpu_vulkan_icd_unbind_wsi_platform_v2(&wsi) !=
            RIN_GPU_VULKAN_OK)
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
