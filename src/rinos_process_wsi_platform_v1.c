/* SPDX-License-Identifier: MIT */
#include "atomic_compat.h"

#include <rinvulkan/rinos_process_wsi_platform_v1.h>

#include <rin/gpu.h>
#include <rinvulkan/profile.h>

#include <stddef.h>
#include <string.h>

#define RINOS_WSI_STATE_READY 1u
#define RINOS_WSI_STATE_BOUND 2u
#define RINOS_WSI_PRESENT_FREE 0u
#define RINOS_WSI_PRESENT_RESERVED 1u
#define RINOS_WSI_PRESENT_ACTIVE 2u
#define RINOS_WSI_PRESENT_BUSY 3u

static int rinos_wsi_zero(const void* pointer, size_t size) {
    const uint8_t* bytes = (const uint8_t*)pointer;
    size_t index;
    if (!pointer && size != 0u) return 0;
    for (index = 0u; index < size; ++index)
        if (bytes[index] != 0u) return 0;
    return 1;
}

static int rinos_wsi_range_valid(const void* pointer, size_t size) {
    uintptr_t start;
    if (!pointer || size == 0u) return 0;
    start = (uintptr_t)pointer;
    return size - 1u <= UINTPTR_MAX - start;
}

static int rinos_wsi_overlap(const void* first, size_t first_size,
                             const void* second, size_t second_size) {
    uintptr_t first_start;
    uintptr_t second_start;
    if (!rinos_wsi_range_valid(first, first_size) ||
        !rinos_wsi_range_valid(second, second_size))
        return 1;
    first_start = (uintptr_t)first;
    second_start = (uintptr_t)second;
    return first_start <= second_start
               ? second_start - first_start < first_size
               : first_start - second_start < second_size;
}

static int rinos_wsi_profile_valid(
    const RinVulkanRinOSCatalogDeviceV1* device) {
    uint8_t uuid_or = 0u;
    uint32_t index;
    if (!device || device->device_id == 0u ||
        device->device_generation == 0u ||
        device->profile.struct_size != sizeof(device->profile) ||
        device->profile.version != RIN_GPU_VULKAN_PHYSICAL_VERSION ||
        device->profile.vendor_id == 0u || device->profile.device_id == 0u ||
        device->profile.device_epoch == 0u ||
        device->route_count == 0u ||
        device->route_count > RIN_GPU_VULKAN_CATALOG_MAX_QUEUE_ROUTES ||
        device->reserved != 0u)
        return 0;
    for (index = 0u; index < sizeof(device->profile.device_uuid); ++index)
        uuid_or |= device->profile.device_uuid[index];
    if (uuid_or == 0u || device->profile.queue_family_count == 0u ||
        device->profile.queue_family_count > RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES)
        return 0;
    for (index = 0u; index < device->route_count; ++index) {
        const RinGpuVulkanQueueRouteIpcV1* route = &device->routes[index];
        uint32_t prior;
        if (route->reserved != 0u ||
            route->queue_family_index >= device->profile.queue_family_count ||
            route->queue_index >=
                device->profile.queue_families[route->queue_family_index]
                    .queue_count)
            return 0;
        for (prior = 0u; prior < index; ++prior)
            if (device->routes[prior].queue_family_index ==
                    route->queue_family_index &&
                device->routes[prior].queue_index == route->queue_index)
                return 0;
    }
    for (; index < RIN_GPU_VULKAN_CATALOG_MAX_QUEUE_ROUTES; ++index) {
        const RinGpuVulkanQueueRouteIpcV1* route = &device->routes[index];
        if (route->queue_family_index != 0u || route->queue_index != 0u ||
            route->physical_queue_id != 0u || route->reserved != 0u)
            return 0;
    }
    return 1;
}

static int rinos_wsi_platform_shape_valid(
    const RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    uint32_t index;
    if (!adapter || adapter->magic !=
                        RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAGIC ||
        adapter->version !=
            RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_VERSION ||
        adapter->struct_size != sizeof(*adapter) ||
        (__atomic_load_n(&adapter->state, __ATOMIC_ACQUIRE) !=
             RINOS_WSI_STATE_READY &&
         __atomic_load_n(&adapter->state, __ATOMIC_ACQUIRE) !=
             RINOS_WSI_STATE_BOUND) ||
        adapter->device_count == 0u ||
        adapter->device_count > RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES ||
        adapter->reserved0 != 0u || adapter->reserved1 != 0u ||
        !rinos_wsi_zero(adapter->reserved, sizeof(adapter->reserved)))
        return 0;
    for (index = 0u; index < adapter->device_count; ++index) {
        uint32_t prior;
        if (!rinos_wsi_profile_valid(&adapter->devices[index])) return 0;
        for (prior = 0u; prior < index; ++prior) {
            if (adapter->devices[index].device_id ==
                    adapter->devices[prior].device_id ||
                adapter->devices[index].device_generation ==
                    adapter->devices[prior].device_generation)
                return 0;
        }
    }
    return 1;
}

static int rinos_wsi_callback_enter(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    if (!rinos_wsi_platform_shape_valid(adapter) ||
        __atomic_load_n(&adapter->state, __ATOMIC_ACQUIRE) !=
            RINOS_WSI_STATE_BOUND)
        return 0;
    __atomic_add_fetch(&adapter->active_calls, 1u, __ATOMIC_ACQUIRE);
    if (__atomic_load_n(&adapter->state, __ATOMIC_ACQUIRE) !=
        RINOS_WSI_STATE_BOUND) {
        __atomic_sub_fetch(&adapter->active_calls, 1u, __ATOMIC_RELEASE);
        return 0;
    }
    return 1;
}

static void rinos_wsi_present_lock(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    while (__atomic_exchange_n(&adapter->present_lock, 1u,
                               __ATOMIC_ACQUIRE) != 0u)
        __atomic_signal_fence(__ATOMIC_SEQ_CST);
}

static void rinos_wsi_present_unlock(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    __atomic_store_n(&adapter->present_lock, 0u, __ATOMIC_RELEASE);
}

static int rinos_wsi_present_reserve(
    RinVulkanRinOSProcessWsiPlatformV1* adapter, uint64_t display_cookie,
    uint64_t device_generation, uint64_t output_generation,
    uint32_t* slot_out) {
    uint32_t index;
    if (slot_out) *slot_out = UINT32_MAX;
    if (!adapter || !slot_out || display_cookie == 0u ||
        device_generation == 0u || output_generation == 0u)
        return 0;
    rinos_wsi_present_lock(adapter);
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS;
         ++index) {
        RinVulkanRinOSProcessWsiPresentV1* present =
            &adapter->presents[index];
        if (present->state != RINOS_WSI_PRESENT_FREE) continue;
        memset(present, 0, sizeof(*present));
        present->display_cookie = display_cookie;
        present->device_generation = device_generation;
        present->output_generation = output_generation;
        present->state = RINOS_WSI_PRESENT_RESERVED;
        *slot_out = index;
        rinos_wsi_present_unlock(adapter);
        return 1;
    }
    rinos_wsi_present_unlock(adapter);
    return 0;
}

static uint64_t rinos_wsi_next_public_token(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    uint32_t attempt;
    for (attempt = 0u;
         attempt < RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS +
                       1u;
         ++attempt) {
        uint32_t index;
        uint64_t candidate = adapter->next_present_token;
        int used = 0;
        if (candidate == 0u) candidate = 1u;
        adapter->next_present_token =
            candidate == UINT64_MAX ? 1u : candidate + 1u;
        for (index = 0u;
             index < RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS;
             ++index) {
            const RinVulkanRinOSProcessWsiPresentV1* present =
                &adapter->presents[index];
            if (present->state != RINOS_WSI_PRESENT_FREE &&
                present->present_token == candidate) {
                used = 1;
                break;
            }
        }
        if (!used) return candidate;
    }
    return 0u;
}

static int rinos_wsi_present_publish(
    RinVulkanRinOSProcessWsiPlatformV1* adapter, uint32_t slot,
    uint64_t sdk_token, uint64_t* public_token_out) {
    RinVulkanRinOSProcessWsiPresentV1* present;
    uint64_t public_token;
    if (public_token_out) *public_token_out = 0u;
    if (!adapter || !public_token_out || sdk_token == 0u ||
        slot >= RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS)
        return 0;
    rinos_wsi_present_lock(adapter);
    present = &adapter->presents[slot];
    if (present->state != RINOS_WSI_PRESENT_RESERVED) {
        rinos_wsi_present_unlock(adapter);
        return 0;
    }
    public_token = rinos_wsi_next_public_token(adapter);
    if (public_token == 0u) {
        rinos_wsi_present_unlock(adapter);
        return 0;
    }
    present->present_token = public_token;
    present->sdk_present_token = sdk_token;
    present->state = RINOS_WSI_PRESENT_ACTIVE;
    *public_token_out = public_token;
    rinos_wsi_present_unlock(adapter);
    return 1;
}

static void rinos_wsi_present_release_reservation(
    RinVulkanRinOSProcessWsiPlatformV1* adapter, uint32_t slot) {
    if (!adapter ||
        slot >= RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS)
        return;
    rinos_wsi_present_lock(adapter);
    if (adapter->presents[slot].state == RINOS_WSI_PRESENT_RESERVED)
        memset(&adapter->presents[slot], 0,
               sizeof(adapter->presents[slot]));
    rinos_wsi_present_unlock(adapter);
}

static int rinos_wsi_present_claim(
    RinVulkanRinOSProcessWsiPlatformV1* adapter, uint64_t display_cookie,
    uint64_t public_token, uint32_t* slot_out,
    RinVulkanRinOSProcessWsiPresentV1* present_out) {
    uint32_t index;
    int result = RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
    if (slot_out) *slot_out = UINT32_MAX;
    if (present_out) memset(present_out, 0, sizeof(*present_out));
    if (!adapter || !slot_out || !present_out || display_cookie == 0u ||
        public_token == 0u)
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    rinos_wsi_present_lock(adapter);
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS;
         ++index) {
        RinVulkanRinOSProcessWsiPresentV1* present =
            &adapter->presents[index];
        if (present->state == RINOS_WSI_PRESENT_FREE ||
            present->present_token != public_token ||
            present->display_cookie != display_cookie)
            continue;
        if (present->state == RINOS_WSI_PRESENT_BUSY ||
            present->state == RINOS_WSI_PRESENT_RESERVED) {
            result = RIN_VULKAN_WSI_PLATFORM_NOT_READY;
            break;
        }
        if (present->state != RINOS_WSI_PRESENT_ACTIVE) {
            result = RIN_VULKAN_WSI_PLATFORM_BACKEND;
            break;
        }
        *present_out = *present;
        present->state = RINOS_WSI_PRESENT_BUSY;
        *slot_out = index;
        result = RIN_VULKAN_WSI_PLATFORM_OK;
        break;
    }
    rinos_wsi_present_unlock(adapter);
    return result;
}

static void rinos_wsi_present_finish_claim(
    RinVulkanRinOSProcessWsiPlatformV1* adapter, uint32_t slot,
    int retire) {
    if (!adapter ||
        slot >= RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS)
        return;
    rinos_wsi_present_lock(adapter);
    if (adapter->presents[slot].state == RINOS_WSI_PRESENT_BUSY) {
        if (retire)
            memset(&adapter->presents[slot], 0,
                   sizeof(adapter->presents[slot]));
        else
            adapter->presents[slot].state = RINOS_WSI_PRESENT_ACTIVE;
    }
    rinos_wsi_present_unlock(adapter);
}

static int rinos_wsi_presents_empty(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    uint32_t index;
    int empty = 1;
    rinos_wsi_present_lock(adapter);
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAX_PRESENTS;
         ++index)
        if (adapter->presents[index].state != RINOS_WSI_PRESENT_FREE) {
            empty = 0;
            break;
        }
    rinos_wsi_present_unlock(adapter);
    return empty;
}

static void rinos_wsi_callback_leave(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    __atomic_sub_fetch(&adapter->active_calls, 1u, __ATOMIC_RELEASE);
}

static const RinVulkanRinOSCatalogDeviceV1* rinos_wsi_find_generation(
    const RinVulkanRinOSProcessWsiPlatformV1* adapter,
    uint64_t device_generation) {
    uint32_t index;
    if (!adapter || device_generation == 0u) return NULL;
    for (index = 0u; index < adapter->device_count; ++index)
        if (adapter->devices[index].device_generation == device_generation)
            return &adapter->devices[index];
    return NULL;
}

static int rinos_wsi_result_from_process(uint32_t result) {
    switch (result) {
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_OK_V1:
        return RIN_VULKAN_WSI_PLATFORM_OK;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_INCOMPLETE_V1:
        return RIN_VULKAN_WSI_PLATFORM_INCOMPLETE;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_NOT_READY_V1:
        return RIN_VULKAN_WSI_PLATFORM_NOT_READY;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_OUT_OF_DATE_V1:
        return RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_DEVICE_LOST_V1:
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_UNSUPPORTED_V1:
        return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_BACKEND_V1:
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_INVALID_ARGUMENT_V1:
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_LIMIT_V1:
        return RIN_VULKAN_WSI_PLATFORM_LIMIT;
    default:
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    }
}

static int rinos_wsi_sdk_status(RinResult status) {
    switch (status) {
    case RIN_SUCCESS:
        return RIN_VULKAN_WSI_PLATFORM_OK;
    case RIN_ERROR_INVALID_ARGUMENT:
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    case RIN_ERROR_BUSY:
    case RIN_ERROR_WOULD_BLOCK:
    case RIN_ERROR_TIMEOUT:
        return RIN_VULKAN_WSI_PLATFORM_NOT_READY;
    case RIN_ERROR_STALE_HANDLE:
        return RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
    case RIN_ERROR_NOT_SUPPORTED:
        return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    case RIN_ERROR_ACCESS_DENIED:
        return RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST;
    case RIN_ERROR_ABI_MISMATCH:
    case RIN_ERROR_INTEGRITY:
    default:
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    }
}

static int rinos_wsi_device_request(
    RinVulkanRinOSProcessWsiPlatformV1* adapter, uint64_t device_generation,
    RinGpuProcessDirectDisplayRequestV1* request,
    RinGpuProcessDirectDisplayResponseV1* response) {
    const RinVulkanRinOSCatalogDeviceV1* device;
    RinResult sdk_result;
    int result;
    if (!request || !response || !rinos_wsi_callback_enter(adapter))
        return RIN_VULKAN_WSI_PLATFORM_NOT_READY;
    device = rinos_wsi_find_generation(adapter, device_generation);
    if (!device) {
        rinos_wsi_callback_leave(adapter);
        return RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
    }
    request->struct_size = sizeof(*request);
    request->version = RIN_GPU_PROCESS_DIRECT_DISPLAY_REQUEST_VERSION_V1;
    request->device_id = device->device_id;
    request->device_generation = device->device_generation;
    memset(response, 0, sizeof(*response));
    sdk_result = rin_gpu_process_direct_display_dispatch_v1(request, response);
    result = rinos_wsi_sdk_status(sdk_result);
    if (result == RIN_VULKAN_WSI_PLATFORM_OK &&
        (response->struct_size != sizeof(*response) ||
         response->version !=
             RIN_GPU_PROCESS_DIRECT_DISPLAY_RESPONSE_VERSION_V1 ||
         response->operation != request->operation ||
         response->reserved0 != 0u ||
         !rinos_wsi_zero(response->reserved, sizeof(response->reserved))))
        result = RIN_VULKAN_WSI_PLATFORM_BACKEND;
    if (result == RIN_VULKAN_WSI_PLATFORM_OK)
        result = rinos_wsi_result_from_process(response->result);
    rinos_wsi_callback_leave(adapter);
    return result;
}

static int rinos_wsi_sdk_format_to_vk(uint32_t sdk_format,
                                      uint32_t* vk_format_out) {
    if (vk_format_out) *vk_format_out = 0u;
    if (!vk_format_out) return 0;
    switch (sdk_format) {
    case RIN_GPU_PROCESS_DIRECT_DISPLAY_PIXEL_FORMAT_RGBA8_UNORM_V1:
        *vk_format_out = RIN_VK_FORMAT_R8G8B8A8_UNORM;
        return 1;
    default:
        return 0;
    }
}

static int rinos_wsi_vk_format_to_sdk(uint32_t vk_format,
                                      uint32_t* sdk_format_out) {
    if (sdk_format_out) *sdk_format_out = 0u;
    if (!sdk_format_out) return 0;
    switch (vk_format) {
    case RIN_VK_FORMAT_R8G8B8A8_UNORM:
        *sdk_format_out =
            RIN_GPU_PROCESS_DIRECT_DISPLAY_PIXEL_FORMAT_RGBA8_UNORM_V1;
        return 1;
    default:
        return 0;
    }
}

static int rinos_wsi_sdk_color_to_vk(int32_t sdk_color,
                                     int32_t* vk_color_out) {
    if (vk_color_out) *vk_color_out = 0;
    if (!vk_color_out) return 0;
    switch (sdk_color) {
    case (int32_t)RIN_GPU_PROCESS_DIRECT_DISPLAY_COLOR_ENCODING_SRGB_NONLINEAR_V1:
        *vk_color_out = RIN_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return 1;
    default:
        return 0;
    }
}

static int rinos_wsi_vk_schedule_to_sdk(int32_t vk_schedule,
                                        int32_t* sdk_schedule_out) {
    if (sdk_schedule_out) *sdk_schedule_out = 0;
    if (!sdk_schedule_out) return 0;
    switch (vk_schedule) {
    case RIN_VK_PRESENT_MODE_FIFO_KHR:
        *sdk_schedule_out =
            (int32_t)RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_FIFO_V1;
        return 1;
    default:
        return 0;
    }
}

static int rinos_wsi_sdk_schedule_to_vk(int32_t sdk_schedule,
                                        int32_t* vk_schedule_out) {
    if (vk_schedule_out) *vk_schedule_out = 0;
    if (!vk_schedule_out) return 0;
    switch (sdk_schedule) {
    case (int32_t)RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_FIFO_V1:
        *vk_schedule_out = RIN_VK_PRESENT_MODE_FIFO_KHR;
        return 1;
    default:
        return 0;
    }
}

static int rinos_wsi_query_displays(
    void* context, uint64_t generation, uint32_t capacity,
    uint32_t* count_out, RinVulkanWsiDisplayV1* displays_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    uint32_t index;
    int result;
    if (count_out) *count_out = 0u;
    if (!count_out || capacity > RIN_VULKAN_WSI_MAX_DISPLAYS ||
        (capacity != 0u && !displays_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (capacity != 0u)
        memset(displays_out, 0, capacity * sizeof(*displays_out));
    memset(&request, 0, sizeof(request));
    request.operation = RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_DISPLAYS_V1;
    request.capacity = capacity;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK &&
        result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return result;
    if (response.count > capacity ||
        response.count > RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_DISPLAYS_V1)
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    for (index = 0u; index < response.count; ++index) {
        const RinGpuProcessDirectDisplayV1* source =
            &response.data.displays[index];
        RinVulkanWsiDisplayV1* target = &displays_out[index];
        if (source->struct_size != sizeof(*source) ||
            source->version != RIN_GPU_PROCESS_DIRECT_DISPLAY_REQUEST_VERSION_V1 ||
            source->display_cookie == 0u || source->output_generation == 0u ||
            source->device_generation != generation || source->width == 0u ||
            source->height == 0u || source->reserved0 != 0u ||
            !rinos_wsi_zero(source->reserved, sizeof(source->reserved)) ||
            !rinos_wsi_sdk_format_to_vk(source->pixel_format, &target->format))
            return RIN_VULKAN_WSI_PLATFORM_BACKEND;
        target->struct_size = sizeof(*target);
        target->version = RIN_VULKAN_WSI_PLATFORM_VERSION;
        target->display_cookie = source->display_cookie;
        target->output_generation = source->output_generation;
        target->device_generation = source->device_generation;
        target->current_mode_cookie = source->current_mode_cookie;
        target->width = source->width;
        target->height = source->height;
        target->refresh_millihertz = source->refresh_millihertz;
        target->physical_width_mm = source->physical_width_mm;
        target->physical_height_mm = source->physical_height_mm;
        target->flags = source->flags;
        target->plane_count = source->plane_count;
        target->mode_count = source->mode_count;
        memcpy(target->display_name, source->display_name,
               sizeof(target->display_name));
    }
    *count_out = response.count;
    return result;
}

static int rinos_wsi_query_modes(
    void* context, uint64_t generation, uint64_t display_cookie,
    uint64_t output_generation, uint32_t capacity, uint32_t* count_out,
    RinVulkanWsiModeV1* modes_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    uint32_t index;
    int result;
    if (count_out) *count_out = 0u;
    if (!count_out || display_cookie == 0u || output_generation == 0u ||
        capacity > RIN_VULKAN_WSI_MAX_MODES ||
        (capacity != 0u && !modes_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (capacity != 0u)
        memset(modes_out, 0, capacity * sizeof(*modes_out));
    memset(&request, 0, sizeof(request));
    request.operation = RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_MODES_V1;
    request.capacity = capacity;
    request.display_cookie = display_cookie;
    request.output_generation = output_generation;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK &&
        result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return result;
    if (response.count > capacity ||
        response.count > RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_MODES_V1)
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    for (index = 0u; index < response.count; ++index) {
        const RinGpuProcessDirectDisplayModeV1* source =
            &response.data.modes[index];
        RinVulkanWsiModeV1* target = &modes_out[index];
        if (source->struct_size != sizeof(*source) ||
            source->version != RIN_GPU_PROCESS_DIRECT_DISPLAY_REQUEST_VERSION_V1 ||
            source->display_cookie != display_cookie ||
            source->output_generation != output_generation ||
            source->mode_cookie == 0u || source->width == 0u ||
            source->height == 0u || source->reserved0 != 0u ||
            source->reserved != 0u ||
            !rinos_wsi_sdk_format_to_vk(source->pixel_format, &target->format))
            return RIN_VULKAN_WSI_PLATFORM_BACKEND;
        target->struct_size = sizeof(*target);
        target->version = RIN_VULKAN_WSI_PLATFORM_VERSION;
        target->display_cookie = source->display_cookie;
        target->mode_cookie = source->mode_cookie;
        target->output_generation = source->output_generation;
        target->width = source->width;
        target->height = source->height;
        target->refresh_millihertz = source->refresh_millihertz;
        target->flags = source->flags;
    }
    *count_out = response.count;
    return result;
}

static int rinos_wsi_query_planes(
    void* context, uint64_t generation, uint32_t capacity,
    uint32_t* count_out, RinVulkanWsiDisplayPlaneV2* planes_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    uint32_t index;
    int result;
    if (count_out) *count_out = 0u;
    if (!count_out || capacity > RIN_VULKAN_WSI_MAX_PLANES ||
        (capacity != 0u && !planes_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (capacity != 0u)
        memset(planes_out, 0, capacity * sizeof(*planes_out));
    memset(&request, 0, sizeof(request));
    request.operation = RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_PLANES_V1;
    request.capacity = capacity;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK &&
        result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return result;
    if (response.count > capacity ||
        response.count > RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_PLANES_V1)
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    for (index = 0u; index < response.count; ++index) {
        const RinGpuProcessDirectDisplayPlaneV1* source =
            &response.data.planes[index];
        RinVulkanWsiDisplayPlaneV2* target = &planes_out[index];
        if (source->struct_size != sizeof(*source) ||
            source->version != RIN_GPU_PROCESS_DIRECT_DISPLAY_REQUEST_VERSION_V1 ||
            source->plane_index != index || source->reserved0 != 0u ||
            !rinos_wsi_zero(source->reserved, sizeof(source->reserved)))
            return RIN_VULKAN_WSI_PLATFORM_BACKEND;
        target->struct_size = sizeof(*target);
        target->version = RIN_VULKAN_WSI_PLATFORM_V2_VERSION;
        target->plane_index = source->plane_index;
        target->current_stack_index = source->current_stack_index;
        target->current_display_cookie = source->current_display_cookie;
        target->output_generation = source->output_generation;
        target->device_generation = source->device_generation;
        target->flags = source->flags;
    }
    *count_out = response.count;
    return result;
}

static int rinos_wsi_query_plane_displays(
    void* context, uint64_t generation, uint32_t plane_index,
    uint32_t capacity, uint32_t* count_out, uint64_t* displays_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    int result;
    if (count_out) *count_out = 0u;
    if (!count_out || plane_index >= RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_PLANES_V1 ||
        capacity > RIN_VULKAN_WSI_MAX_DISPLAYS ||
        (capacity != 0u && !displays_out))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (capacity != 0u)
        memset(displays_out, 0, capacity * sizeof(*displays_out));
    memset(&request, 0, sizeof(request));
    request.operation =
        RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_PLANE_DISPLAYS_V1;
    request.capacity = capacity;
    request.plane_index = plane_index;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK &&
        result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return result;
    if (response.count > capacity ||
        response.count > RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_DISPLAYS_V1)
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    if (response.count != 0u)
        memcpy(displays_out, response.data.display_cookies,
               response.count * sizeof(*displays_out));
    *count_out = response.count;
    return result;
}

static int rinos_wsi_query_plane_capabilities(
    void* context, uint64_t generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    RinVulkanWsiPlaneCapabilitiesV2* capabilities_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    int result;
    if (capabilities_out) memset(capabilities_out, 0, sizeof(*capabilities_out));
    if (!capabilities_out || display_cookie == 0u ||
        output_generation == 0u || mode_cookie == 0u ||
        plane_index >= RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_PLANES_V1)
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    memset(&request, 0, sizeof(request));
    request.operation =
        RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_PLANE_CAPABILITIES_V1;
    request.display_cookie = display_cookie;
    request.output_generation = output_generation;
    request.mode_cookie = mode_cookie;
    request.plane_index = plane_index;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK) return result;
    memcpy(capabilities_out, &response.data.plane_capabilities,
           sizeof(*capabilities_out));
    return RIN_VULKAN_WSI_PLATFORM_OK;
}

static int rinos_wsi_query_surface_support(
    void* context, uint64_t generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    uint32_t queue_family_index, uint32_t queue_flags, uint32_t queue_count,
    uint32_t* supported_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    int result;
    if (supported_out) *supported_out = 0u;
    if (!supported_out || display_cookie == 0u || output_generation == 0u ||
        mode_cookie == 0u || queue_count == 0u || queue_flags == 0u ||
        (queue_flags & ~RIN_GPU_VK_QUEUE_KNOWN) != 0u)
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    memset(&request, 0, sizeof(request));
    request.operation =
        RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_QUEUE_SUPPORT_V1;
    request.display_cookie = display_cookie;
    request.output_generation = output_generation;
    request.mode_cookie = mode_cookie;
    request.plane_index = plane_index;
    request.queue_family_index = queue_family_index;
    request.queue_flags = queue_flags;
    request.queue_count = queue_count;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK) return result;
    if (response.data.queue_supported > 1u)
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    *supported_out = response.data.queue_supported;
    return RIN_VULKAN_WSI_PLATFORM_OK;
}

static int rinos_wsi_query_surface_properties(
    void* context, uint64_t generation, uint64_t display_cookie,
    uint64_t output_generation, uint64_t mode_cookie, uint32_t plane_index,
    RinVulkanWsiSurfacePropertiesV4* properties_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    const RinGpuProcessDirectDisplaySurfacePropertiesV1* source;
    uint32_t index;
    int result;
    if (properties_out) memset(properties_out, 0, sizeof(*properties_out));
    if (!properties_out || display_cookie == 0u ||
        output_generation == 0u || mode_cookie == 0u ||
        plane_index >= RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_PLANES_V1)
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    memset(&request, 0, sizeof(request));
    request.operation =
        RIN_GPU_PROCESS_DIRECT_DISPLAY_QUERY_SURFACE_PROPERTIES_V1;
    request.display_cookie = display_cookie;
    request.output_generation = output_generation;
    request.mode_cookie = mode_cookie;
    request.plane_index = plane_index;
    result = rinos_wsi_device_request(adapter, generation, &request,
                                     &response);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK) return result;
    source = &response.data.surface_properties;
    if (source->struct_size != sizeof(*source) ||
        source->version != RIN_GPU_PROCESS_DIRECT_DISPLAY_RESPONSE_VERSION_V1 ||
        source->format_count > RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_FORMATS_V1 ||
        source->present_schedule_count >
            RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_PRESENT_MODES_V1 ||
        !rinos_wsi_zero(source->reserved0, sizeof(source->reserved0)) ||
        !rinos_wsi_zero(source->reserved, sizeof(source->reserved)))
        return RIN_VULKAN_WSI_PLATFORM_BACKEND;
    properties_out->struct_size = sizeof(*properties_out);
    properties_out->version = RIN_VULKAN_WSI_PLATFORM_V4_VERSION;
    properties_out->min_image_count = source->min_image_count;
    properties_out->max_image_count = source->max_image_count;
    properties_out->current_extent_width = source->current_extent_width;
    properties_out->current_extent_height = source->current_extent_height;
    properties_out->min_image_extent_width = source->min_image_extent_width;
    properties_out->min_image_extent_height = source->min_image_extent_height;
    properties_out->max_image_extent_width = source->max_image_extent_width;
    properties_out->max_image_extent_height = source->max_image_extent_height;
    properties_out->max_image_array_layers = source->max_image_array_layers;
    properties_out->supported_transforms = source->supported_transforms;
    properties_out->current_transform = source->current_transform;
    properties_out->supported_composite_alpha =
        source->supported_composite_alpha;
    properties_out->supported_usage_flags = source->supported_usage_flags;
    properties_out->format_count = source->format_count;
    properties_out->present_mode_count = source->present_schedule_count;
    for (index = 0u; index < source->format_count; ++index) {
        uint32_t format = 0u;
        if (!rinos_wsi_sdk_format_to_vk(
                (uint32_t)source->formats[index].pixel_format,
                &format) ||
            !rinos_wsi_sdk_color_to_vk(
                source->formats[index].color_encoding,
                &properties_out->formats[index].color_space))
            return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
        properties_out->formats[index].format = (int32_t)format;
    }
    for (index = 0u; index < source->present_schedule_count; ++index) {
        int32_t mode = 0;
        if (!rinos_wsi_sdk_schedule_to_vk(
                source->present_schedules[index], &mode))
            return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
        properties_out->present_modes[index] = mode;
    }
    return RIN_VULKAN_WSI_PLATFORM_OK;
}

static int rinos_wsi_resolve_device_generation(
    void* context, const RinVulkanWsiPhysicalDeviceIdentityV1* identity,
    uint64_t* generation_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    uint32_t index;
    const RinVulkanRinOSCatalogDeviceV1* match = NULL;
    if (generation_out) *generation_out = 0u;
    if (!identity || !generation_out ||
        !rinos_wsi_range_valid(identity, sizeof(*identity)) ||
        identity->struct_size != sizeof(*identity) ||
        identity->version != RIN_VULKAN_WSI_PHYSICAL_DEVICE_IDENTITY_V1_VERSION ||
        identity->vendor_id == 0u || identity->device_id == 0u ||
        identity->product_device_epoch == 0u ||
        !rinos_wsi_zero(identity->reserved, sizeof(identity->reserved)) ||
        !rinos_wsi_callback_enter(adapter))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    for (index = 0u; index < adapter->device_count; ++index) {
        const RinVulkanRinOSCatalogDeviceV1* candidate =
            &adapter->devices[index];
        if (candidate->profile.vendor_id == identity->vendor_id &&
            candidate->profile.device_id == identity->device_id &&
            candidate->profile.device_epoch ==
                identity->product_device_epoch &&
            memcmp(candidate->profile.device_uuid, identity->device_uuid,
                   sizeof(identity->device_uuid)) == 0) {
            if (match) {
                rinos_wsi_callback_leave(adapter);
                return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
            }
            match = candidate;
        }
    }
    if (match) *generation_out = match->device_generation;
    rinos_wsi_callback_leave(adapter);
    return match ? RIN_VULKAN_WSI_PLATFORM_OK
                 : RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE;
}

static int rinos_wsi_present_v2(
    void* context, const RinVulkanWsiPresentRequestV2* request_in,
    uint64_t* token_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    RinGpuProcessDirectDisplayPresentRequestV2 present_v2;
    RinGpuProcessDirectDisplayPresentRequestV2* present;
    uint32_t sdk_format;
    int32_t sdk_schedule;
    int result;
    if (token_out) *token_out = 0u;
    if (!request_in || !token_out ||
        !rinos_wsi_range_valid(request_in, sizeof(*request_in)) ||
        rinos_wsi_overlap(adapter, sizeof(*adapter), request_in,
                          sizeof(*request_in)) ||
        rinos_wsi_overlap(adapter, sizeof(*adapter), token_out,
                          sizeof(*token_out)) ||
        request_in->struct_size != sizeof(*request_in) ||
        request_in->version != RIN_VULKAN_WSI_PRESENT_REQUEST_V2_VERSION ||
        request_in->display_cookie == 0u || request_in->mode_cookie == 0u ||
        request_in->allocation_handle == 0u ||
        request_in->allocation_size == 0u ||
        request_in->allocation_offset > UINT64_MAX - request_in->allocation_size ||
        request_in->output_generation == 0u ||
        request_in->device_generation == 0u || request_in->frame_id == 0u ||
        request_in->width == 0u || request_in->height == 0u ||
        request_in->flags != 0u ||
        !rinos_wsi_zero(request_in->reserved, sizeof(request_in->reserved)) ||
        !rinos_wsi_vk_format_to_sdk(request_in->format, &sdk_format) ||
        !rinos_wsi_vk_schedule_to_sdk((int32_t)request_in->present_mode,
                                      &sdk_schedule))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (request_in->image_layout.struct_size !=
            sizeof(request_in->image_layout) ||
        request_in->image_layout.version !=
            RIN_VULKAN_WSI_IMAGE_LAYOUT_V1_VERSION ||
        request_in->image_layout.kind !=
            RIN_VULKAN_WSI_IMAGE_LAYOUT_KIND_LINEAR ||
        request_in->image_layout.plane_count != 1u ||
        request_in->image_layout.modifier != 0u ||
        !rinos_wsi_zero(request_in->image_layout.reserved,
                        sizeof(request_in->image_layout.reserved)))
        return RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    if (!rinos_wsi_zero(
            request_in->image_layout.planes +
                request_in->image_layout.plane_count,
            (RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_LAYOUT_PLANES_V1 -
             request_in->image_layout.plane_count) *
                sizeof(request_in->image_layout.planes[0])))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    if (!rinos_wsi_callback_enter(adapter))
        return RIN_VULKAN_WSI_PLATFORM_NOT_READY;
    {
        uint32_t slot = UINT32_MAX;
        uint64_t sdk_token = 0u;
        if (!rinos_wsi_present_reserve(
                adapter, request_in->display_cookie,
                request_in->device_generation,
                request_in->output_generation, &slot)) {
            rinos_wsi_callback_leave(adapter);
            return RIN_VULKAN_WSI_PLATFORM_LIMIT;
        }
        memset(&request, 0, sizeof(request));
        memset(&present_v2, 0, sizeof(present_v2));
        request.operation = RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_V1;
        request.device_generation = request_in->device_generation;
        present = &present_v2;
        present->struct_size = sizeof(*present);
        present->version = RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_VERSION_V2;
        present->display_cookie = request_in->display_cookie;
        present->mode_cookie = request_in->mode_cookie;
        present->allocation = request_in->allocation_handle;
        present->allocation_offset = request_in->allocation_offset;
        present->allocation_size = request_in->allocation_size;
        present->output_generation = request_in->output_generation;
        present->device_generation = request_in->device_generation;
        present->frame_id = request_in->frame_id;
        present->image_index = request_in->image_index;
        present->width = request_in->width;
        present->height = request_in->height;
        present->pixel_format = sdk_format;
        present->present_schedule = (uint32_t)sdk_schedule;
        present->flags = request_in->flags;
        present->image_layout.struct_size = sizeof(present->image_layout);
        present->image_layout.version =
            RIN_GPU_PROCESS_DIRECT_DISPLAY_IMAGE_LAYOUT_VERSION_V1;
        present->image_layout.kind = request_in->image_layout.kind;
        present->image_layout.plane_count =
            request_in->image_layout.plane_count;
        present->image_layout.modifier = request_in->image_layout.modifier;
        for (uint32_t index = 0u;
             index < RIN_GPU_PROCESS_DIRECT_DISPLAY_MAX_LAYOUT_PLANES_V1;
             ++index) {
            present->image_layout.planes[index].offset_bytes =
                request_in->image_layout.planes[index].offset_bytes;
            present->image_layout.planes[index].row_pitch_bytes =
                request_in->image_layout.planes[index].row_pitch_bytes;
            present->image_layout.planes[index].size_bytes =
                request_in->image_layout.planes[index].size_bytes;
        }
        memcpy(&request.present, &present_v2, sizeof(present_v2));
        result = rinos_wsi_device_request(
            adapter, request_in->device_generation, &request, &response);
        if (result == RIN_VULKAN_WSI_PLATFORM_OK) {
            sdk_token = response.data.present_token;
            if (!rinos_wsi_present_publish(adapter, slot, sdk_token,
                                           token_out))
                result = RIN_VULKAN_WSI_PLATFORM_BACKEND;
        }
        if (result != RIN_VULKAN_WSI_PLATFORM_OK)
            rinos_wsi_present_release_reservation(adapter, slot);
    }
    rinos_wsi_callback_leave(adapter);
    return result;
}

static int rinos_wsi_poll_present(
    void* context, uint64_t display_cookie, uint64_t token,
    RinVulkanWsiPresentStatusV1* status_out) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    RinVulkanRinOSProcessWsiPresentV1 present;
    uint32_t slot = UINT32_MAX;
    int result;
    if (status_out) memset(status_out, 0, sizeof(*status_out));
    if (!status_out || display_cookie == 0u || token == 0u ||
        !rinos_wsi_callback_enter(adapter))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    result = rinos_wsi_present_claim(adapter, display_cookie, token, &slot,
                                     &present);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK) {
        rinos_wsi_callback_leave(adapter);
        return result;
    }
    memset(&request, 0, sizeof(request));
    request.operation = RIN_GPU_PROCESS_DIRECT_DISPLAY_POLL_V1;
    request.display_cookie = display_cookie;
    request.present_token = present.sdk_present_token;
    memset(&response, 0, sizeof(response));
    result = rinos_wsi_device_request(adapter, present.device_generation,
                                     &request, &response);
    if (result == RIN_VULKAN_WSI_PLATFORM_OK &&
        (response.result != RIN_GPU_PROCESS_DIRECT_DISPLAY_OK_V1 ||
         response.data.present_status.struct_size !=
             sizeof(response.data.present_status) ||
         response.data.present_status.version !=
             RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_STATUS_VERSION_V1 ||
         (response.data.present_status.state !=
              RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_PENDING_V1 &&
          response.data.present_status.state !=
              RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_COMPLETE_V1) ||
         response.data.present_status.output_generation == 0u ||
         response.data.present_status.device_generation !=
             present.device_generation ||
         response.data.present_status.output_generation !=
             present.output_generation ||
         response.data.present_status.reserved0 != 0u ||
         !rinos_wsi_zero(response.data.present_status.reserved,
                         sizeof(response.data.present_status.reserved))))
        result = RIN_VULKAN_WSI_PLATFORM_BACKEND;
    if (result == RIN_VULKAN_WSI_PLATFORM_OK) {
        status_out->struct_size = sizeof(*status_out);
        status_out->version = RIN_VULKAN_WSI_PLATFORM_VERSION;
        status_out->state = response.data.present_status.state;
        status_out->output_generation =
            response.data.present_status.output_generation;
        status_out->device_generation =
            response.data.present_status.device_generation;
    }
    rinos_wsi_present_finish_claim(
        adapter, slot,
        result == RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE ||
            (result == RIN_VULKAN_WSI_PLATFORM_OK &&
             response.data.present_status.state ==
                 RIN_GPU_PROCESS_DIRECT_DISPLAY_PRESENT_COMPLETE_V1));
    rinos_wsi_callback_leave(adapter);
    return result;
}

static int rinos_wsi_cancel_present(void* context, uint64_t display_cookie,
                                    uint64_t token) {
    RinVulkanRinOSProcessWsiPlatformV1* adapter =
        (RinVulkanRinOSProcessWsiPlatformV1*)context;
    RinGpuProcessDirectDisplayRequestV1 request;
    RinGpuProcessDirectDisplayResponseV1 response;
    RinVulkanRinOSProcessWsiPresentV1 present;
    uint32_t slot = UINT32_MAX;
    int result;
    if (display_cookie == 0u || token == 0u ||
        !rinos_wsi_callback_enter(adapter))
        return RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT;
    result = rinos_wsi_present_claim(adapter, display_cookie, token, &slot,
                                     &present);
    if (result != RIN_VULKAN_WSI_PLATFORM_OK) {
        rinos_wsi_callback_leave(adapter);
        return result;
    }
    memset(&request, 0, sizeof(request));
    request.operation = RIN_GPU_PROCESS_DIRECT_DISPLAY_CANCEL_V1;
    request.display_cookie = display_cookie;
    request.present_token = present.sdk_present_token;
    memset(&response, 0, sizeof(response));
    result = rinos_wsi_device_request(adapter, present.device_generation,
                                     &request, &response);
    if (result == RIN_VULKAN_WSI_PLATFORM_OK &&
        response.result != RIN_GPU_PROCESS_DIRECT_DISPLAY_OK_V1)
        result = rinos_wsi_result_from_process(response.result);
    rinos_wsi_present_finish_claim(
        adapter, slot,
        result == RIN_VULKAN_WSI_PLATFORM_OK ||
            result == RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE);
    rinos_wsi_callback_leave(adapter);
    return result;
}

int rin_vulkan_rinos_process_wsi_platform_v1_init(
    RinVulkanRinOSProcessWsiPlatformV1* adapter,
    const RinVulkanRinOSCatalogV1* catalog) {
    uint32_t index;
    if (!adapter || !catalog ||
        !rinos_wsi_range_valid(adapter, sizeof(*adapter)) ||
        !rinos_wsi_range_valid(catalog, sizeof(*catalog)) ||
        rinos_wsi_overlap(adapter, sizeof(*adapter), catalog,
                         sizeof(*catalog)) ||
        !rinos_wsi_zero(adapter, sizeof(*adapter)) ||
        catalog->struct_size != sizeof(*catalog) ||
        catalog->version != RIN_VULKAN_RINOS_CATALOG_V1_VERSION ||
        catalog->state != RIN_VULKAN_RINOS_CATALOG_STATE_ACTIVE ||
        catalog->device_count == 0u ||
        catalog->device_count > RIN_VULKAN_RINOS_CATALOG_MAX_DEVICES)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    for (index = 0u; index < catalog->device_count; ++index) {
        uint32_t prior;
        if (!rinos_wsi_profile_valid(&catalog->devices[index]))
            return RIN_GPU_VULKAN_SECURITY;
        for (prior = 0u; prior < index; ++prior) {
            if (catalog->devices[index].device_id ==
                    catalog->devices[prior].device_id ||
                catalog->devices[index].device_generation ==
                    catalog->devices[prior].device_generation)
                return RIN_GPU_VULKAN_SECURITY;
        }
    }
    adapter->magic = RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_MAGIC;
    adapter->version = RIN_VULKAN_RINOS_PROCESS_WSI_PLATFORM_V1_VERSION;
    adapter->struct_size = sizeof(*adapter);
    adapter->state = RINOS_WSI_STATE_READY;
    adapter->device_count = catalog->device_count;
    adapter->next_present_token = 1u;
    memcpy(adapter->devices, catalog->devices,
           catalog->device_count * sizeof(adapter->devices[0]));
    adapter->platform.base.base.struct_size = sizeof(adapter->platform);
    adapter->platform.base.base.version =
        RIN_VULKAN_WSI_PLATFORM_V6_VERSION;
    adapter->platform.base.base.context = adapter;
    adapter->platform.base.base.query_displays = rinos_wsi_query_displays;
    adapter->platform.base.base.query_modes = rinos_wsi_query_modes;
    /* V6 uses present_v2; it never forwards a layout-bearing request through
     * the legacy V1 callback. */
    adapter->platform.base.base.present = NULL;
    adapter->platform.base.base.poll_present = rinos_wsi_poll_present;
    adapter->platform.base.base.cancel_present = rinos_wsi_cancel_present;
    adapter->platform.base.base.query_planes = rinos_wsi_query_planes;
    adapter->platform.base.base.query_plane_supported_displays =
        rinos_wsi_query_plane_displays;
    adapter->platform.base.base.query_plane_capabilities =
        rinos_wsi_query_plane_capabilities;
    adapter->platform.base.base.query_surface_support =
        rinos_wsi_query_surface_support;
    adapter->platform.base.base.query_surface_properties =
        rinos_wsi_query_surface_properties;
    adapter->platform.base.resolve_device_generation =
        rinos_wsi_resolve_device_generation;
    adapter->platform.base.generation_context = adapter;
    adapter->platform.present_v2 = rinos_wsi_present_v2;
    adapter->platform.present_v2_context = adapter;
    return rin_vulkan_rinos_process_wsi_platform_v1_valid(adapter)
               ? RIN_GPU_VULKAN_OK
               : RIN_GPU_VULKAN_INVALID_ARGUMENT;
}

int rin_vulkan_rinos_process_wsi_platform_v1_valid(
    const RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    if (!rinos_wsi_platform_shape_valid(adapter) ||
        adapter->platform.base.base.struct_size !=
            sizeof(adapter->platform) ||
        adapter->platform.base.base.version !=
            RIN_VULKAN_WSI_PLATFORM_V6_VERSION ||
        adapter->platform.base.base.context != adapter ||
        !adapter->platform.base.base.query_displays ||
        !adapter->platform.base.base.query_modes ||
        adapter->platform.base.base.present != NULL ||
        !adapter->platform.base.base.poll_present ||
        !adapter->platform.base.base.cancel_present ||
        !adapter->platform.base.base.query_planes ||
        !adapter->platform.base.base.query_plane_supported_displays ||
        !adapter->platform.base.base.query_plane_capabilities ||
        !adapter->platform.base.base.query_surface_support ||
        !adapter->platform.base.base.query_surface_properties ||
        !adapter->platform.base.resolve_device_generation ||
        adapter->platform.base.generation_context != adapter ||
        !adapter->platform.present_v2 ||
        adapter->platform.present_v2_context != adapter ||
        adapter->reserved0 != 0u || adapter->reserved1 != 0u ||
        !rinos_wsi_zero(adapter->reserved, sizeof(adapter->reserved)))
        return 0;
    return 1;
}

RinVulkanWsiPlatformV6*
rin_vulkan_rinos_process_wsi_platform_v1_get_platform(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    return rin_vulkan_rinos_process_wsi_platform_v1_valid(adapter)
               ? &adapter->platform
               : NULL;
}

int rin_vulkan_rinos_process_wsi_platform_v1_bind(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    uint32_t expected = RINOS_WSI_STATE_READY;
    if (!rin_vulkan_rinos_process_wsi_platform_v1_valid(adapter) ||
        __atomic_load_n(&adapter->active_calls, __ATOMIC_ACQUIRE) != 0u ||
        !rinos_wsi_presents_empty(adapter))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&adapter->state, &expected,
                                     RINOS_WSI_STATE_BOUND, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return RIN_GPU_VULKAN_BUSY;
    if (rin_gpu_vulkan_icd_bind_wsi_platform_v6(&adapter->platform) !=
        RIN_GPU_VULKAN_OK) {
        __atomic_store_n(&adapter->state, RINOS_WSI_STATE_READY,
                         __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_BUSY;
    }
    return RIN_GPU_VULKAN_OK;
}

int rin_vulkan_rinos_process_wsi_platform_v1_unbind(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    int result;
    if (!rin_vulkan_rinos_process_wsi_platform_v1_valid(adapter) ||
        __atomic_load_n(&adapter->state, __ATOMIC_ACQUIRE) !=
            RINOS_WSI_STATE_BOUND)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (__atomic_load_n(&adapter->active_calls, __ATOMIC_ACQUIRE) != 0u ||
        !rinos_wsi_presents_empty(adapter))
        return RIN_GPU_VULKAN_BUSY;
    result = rin_gpu_vulkan_icd_unbind_wsi_platform_v6(&adapter->platform);
    if (result != RIN_GPU_VULKAN_OK) return result;
    if (__atomic_load_n(&adapter->active_calls, __ATOMIC_ACQUIRE) != 0u)
        return RIN_GPU_VULKAN_BUSY;
    __atomic_store_n(&adapter->state, RINOS_WSI_STATE_READY,
                     __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_OK;
}

int rin_vulkan_rinos_process_wsi_platform_v1_destroy(
    RinVulkanRinOSProcessWsiPlatformV1* adapter) {
    if (!rin_vulkan_rinos_process_wsi_platform_v1_valid(adapter) ||
        __atomic_load_n(&adapter->state, __ATOMIC_ACQUIRE) !=
            RINOS_WSI_STATE_READY ||
        __atomic_load_n(&adapter->active_calls, __ATOMIC_ACQUIRE) != 0u ||
        !rinos_wsi_presents_empty(adapter))
        return RIN_GPU_VULKAN_BUSY;
    memset(adapter, 0, sizeof(*adapter));
    return RIN_GPU_VULKAN_OK;
}
