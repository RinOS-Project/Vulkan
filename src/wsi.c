/* SPDX-License-Identifier: MIT */
#include <rinvulkan/wsi.h>

#include <stddef.h>
#include <string.h>

#define VULKAN_WSI_MAGIC UINT64_C(0x52494e5657534931)
#define VULKAN_WSI_SLOT_FREE 0u
#define VULKAN_WSI_SLOT_LIVE 1u
#define VULKAN_WSI_FIRST_IMAGE_TOKEN UINT64_C(0x1000)

typedef struct VulkanWsiSurfaceSlot {
    uint32_t slot_state;
    uint32_t surface_id;
    uint32_t image_count;
    uint32_t next_image;
    uint32_t mode;
    uint32_t suboptimal;
    uint32_t out_of_date;
    uint32_t reserved;
    RinGpuPresentationOutputV1 output;
    uint64_t image_tokens[RIN_GPU_VULKAN_WSI_MAX_IMAGES];
    uint64_t acquired_frames[RIN_GPU_VULKAN_WSI_MAX_IMAGES];
    uint64_t submitted_fences[RIN_GPU_VULKAN_WSI_MAX_IMAGES];
} VulkanWsiSurfaceSlot;

typedef struct VulkanWsiState {
    uint64_t magic;
    uint64_t next_image_token;
    uint32_t surface_count;
    uint32_t reserved;
    RinGpuPresentationRuntime presentation;
    VulkanWsiSurfaceSlot surfaces[RIN_GPU_VULKAN_WSI_MAX_SURFACES];
} VulkanWsiState;

_Static_assert(sizeof(VulkanWsiState) <=
                   sizeof(((RinGpuVulkanWsiRuntime*)0)->opaque),
               "RinGPU Vulkan WSI state exceeds public storage");

static VulkanWsiState* wsi_state(RinGpuVulkanWsiRuntime* runtime) {
    return runtime ? (VulkanWsiState*)runtime->opaque : NULL;
}

static int wsi_ready(const VulkanWsiState* state) {
    return state && state->magic == VULKAN_WSI_MAGIC;
}

static int map_presentation_result(int result) {
    switch (result) {
    case RIN_GPU_PRESENTATION_OK:
        return RIN_GPU_VULKAN_WSI_OK;
    case RIN_GPU_PRESENTATION_BUSY:
        return RIN_GPU_VULKAN_WSI_NOT_READY;
    case RIN_GPU_PRESENTATION_STALE:
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    case RIN_GPU_PRESENTATION_DEVICE_LOST:
        return RIN_GPU_VULKAN_WSI_DEVICE_LOST;
    case RIN_GPU_PRESENTATION_BACKEND:
        return RIN_GPU_VULKAN_WSI_BACKEND;
    case RIN_GPU_PRESENTATION_LIMIT:
        return RIN_GPU_VULKAN_WSI_LIMIT;
    case RIN_GPU_PRESENTATION_UNSUPPORTED:
        return RIN_GPU_VULKAN_WSI_UNSUPPORTED;
    case RIN_GPU_PRESENTATION_TIMEOUT:
        return RIN_GPU_VULKAN_WSI_BACKEND;
    default:
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    }
}

static int output_valid(const RinGpuPresentationOutputV1* output) {
    return output && output->struct_size >= sizeof(*output) &&
           output->version == RIN_GPU_PRESENTATION_VERSION &&
           output->display_id != UINT32_MAX && output->width != 0u &&
           output->height != 0u && output->refresh_millihertz != 0u &&
           output->format != 0u && output->output_generation != 0u &&
           output->output_generation != UINT64_MAX &&
           (output->flags & ~RIN_GPU_PRESENTATION_OUTPUT_KNOWN_FLAGS) == 0u &&
           (output->flags & RIN_GPU_PRESENTATION_OUTPUT_FIFO) != 0u &&
           output->reserved[0] == 0u && output->reserved[1] == 0u;
}

static int mode_valid_for_output(const RinGpuPresentationOutputV1* output,
                                 uint32_t mode) {
    uint32_t flag;
    if (mode == RIN_GPU_PRESENTATION_MODE_FIFO)
        flag = RIN_GPU_PRESENTATION_OUTPUT_FIFO;
    else if (mode == RIN_GPU_PRESENTATION_MODE_MAILBOX)
        flag = RIN_GPU_PRESENTATION_OUTPUT_MAILBOX;
    else if (mode == RIN_GPU_PRESENTATION_MODE_IMMEDIATE)
        flag = RIN_GPU_PRESENTATION_OUTPUT_IMMEDIATE;
    else
        return 0;
    return (output->flags & flag) != 0u;
}

static VulkanWsiSurfaceSlot* find_surface(VulkanWsiState* state,
                                          uint32_t surface_id) {
    for (uint32_t index = 0u; index < RIN_GPU_VULKAN_WSI_MAX_SURFACES;
         ++index) {
        VulkanWsiSurfaceSlot* surface = &state->surfaces[index];
        if (surface->slot_state == VULKAN_WSI_SLOT_LIVE &&
            surface->surface_id == surface_id)
            return surface;
    }
    return NULL;
}

static VulkanWsiSurfaceSlot* find_surface_for_display(
    VulkanWsiState* state, uint32_t display_id) {
    for (uint32_t index = 0u; index < RIN_GPU_VULKAN_WSI_MAX_SURFACES;
         ++index) {
        VulkanWsiSurfaceSlot* surface = &state->surfaces[index];
        if (surface->slot_state == VULKAN_WSI_SLOT_LIVE &&
            surface->output.display_id == display_id)
            return surface;
    }
    return NULL;
}

static int image_index_for_token(const VulkanWsiSurfaceSlot* surface,
                                 uint64_t image_token) {
    for (uint32_t index = 0u; index < surface->image_count; ++index) {
        if (surface->image_tokens[index] == image_token)
            return (int)index;
    }
    return -1;
}

static int token_in_use(const VulkanWsiState* state, uint64_t token) {
    for (uint32_t surface_index = 0u;
         surface_index < RIN_GPU_VULKAN_WSI_MAX_SURFACES; ++surface_index) {
        const VulkanWsiSurfaceSlot* surface = &state->surfaces[surface_index];
        if (surface->slot_state != VULKAN_WSI_SLOT_LIVE) continue;
        for (uint32_t image_index = 0u; image_index < surface->image_count;
             ++image_index) {
            if (surface->image_tokens[image_index] == token) return 1;
        }
    }
    return 0;
}

static uint64_t allocate_image_token(VulkanWsiState* state) {
    uint64_t candidate = state->next_image_token;
    for (uint32_t attempt = 0u;
         attempt < RIN_GPU_VULKAN_WSI_MAX_SURFACES *
                       RIN_GPU_VULKAN_WSI_MAX_IMAGES + 1u;
         ++attempt) {
        if (candidate != 0u && !token_in_use(state, candidate)) {
            state->next_image_token = candidate + 1u;
            if (state->next_image_token == 0u)
                state->next_image_token = VULKAN_WSI_FIRST_IMAGE_TOKEN;
            return candidate;
        }
        if (candidate == UINT64_MAX)
            candidate = VULKAN_WSI_FIRST_IMAGE_TOKEN;
        else
            ++candidate;
    }
    return 0u;
}

static int register_images(VulkanWsiState* state,
                           const RinGpuPresentationOutputV1* output,
                           uint32_t image_count, uint64_t* tokens_out) {
    uint32_t registered = 0u;
    int result = RIN_GPU_PRESENTATION_OK;
    for (uint32_t index = 0u; index < image_count; ++index) {
        RinGpuPresentationImageV1 image;
        uint64_t token = allocate_image_token(state);
        if (token == 0u) {
            result = RIN_GPU_PRESENTATION_LIMIT;
            goto fail;
        }
        memset(&image, 0, sizeof(image));
        image.struct_size = sizeof(image);
        image.version = RIN_GPU_PRESENTATION_VERSION;
        image.image_token = token;
        image.display_id = output->display_id;
        image.width = output->width;
        image.height = output->height;
        image.format = output->format;
        image.usage = RIN_GPU_PRESENTATION_IMAGE_RENDER_TARGET |
                      RIN_GPU_PRESENTATION_IMAGE_PRESENT;
        image.output_generation = output->output_generation;
        image.device_generation = output->device_generation;
        result = rin_gpu_presentation_register_image(&state->presentation,
                                                     &image);
        if (result != RIN_GPU_PRESENTATION_OK) goto fail;
        tokens_out[index] = token;
        ++registered;
    }
    return RIN_GPU_PRESENTATION_OK;

fail:
    for (uint32_t index = 0u; index < registered; ++index)
        (void)rin_gpu_presentation_unregister_image(
            &state->presentation, tokens_out[index]);
    return result;
}

static void clear_surface(VulkanWsiSurfaceSlot* surface) {
    memset(surface, 0, sizeof(*surface));
}

static int unregister_surface_images(VulkanWsiState* state,
                                     VulkanWsiSurfaceSlot* surface) {
    int first_error = RIN_GPU_PRESENTATION_OK;
    for (uint32_t index = 0u; index < surface->image_count; ++index) {
        int result = rin_gpu_presentation_unregister_image(
            &state->presentation, surface->image_tokens[index]);
        if (result != RIN_GPU_PRESENTATION_OK &&
            result != RIN_GPU_PRESENTATION_STALE && first_error ==
                                                       RIN_GPU_PRESENTATION_OK)
            first_error = result;
    }
    return first_error;
}

int rin_gpu_vulkan_wsi_runtime_init(
    RinGpuVulkanWsiRuntime* runtime, uint64_t device_generation,
    const RinGpuPresentationBackendV1* backend) {
    VulkanWsiState* state;
    int result;
    if (!runtime || device_generation == 0u || !backend) return -1;
    state = wsi_state(runtime);
    memset(state, 0, sizeof(*state));
    result = rin_gpu_presentation_runtime_init(&state->presentation,
                                               device_generation, backend);
    if (result != RIN_GPU_PRESENTATION_OK) return map_presentation_result(result);
    state->magic = VULKAN_WSI_MAGIC;
    state->next_image_token = VULKAN_WSI_FIRST_IMAGE_TOKEN;
    return RIN_GPU_VULKAN_WSI_OK;
}

int rin_gpu_vulkan_wsi_create_swapchain(
    RinGpuVulkanWsiRuntime* runtime, const RinGpuPresentationOutputV1* output,
    uint32_t image_count, uint32_t mode, uint32_t* surface_id_out) {
    VulkanWsiState* state = wsi_state(runtime);
    VulkanWsiSurfaceSlot* surface = NULL;
    RinGpuPresentationStatusV1 status;
    int result;
    if (surface_id_out) *surface_id_out = 0u;
    if (!wsi_ready(state) || !output_valid(output) ||
        image_count < RIN_GPU_VULKAN_WSI_MIN_IMAGES ||
        image_count > RIN_GPU_VULKAN_WSI_MAX_IMAGES ||
        !mode_valid_for_output(output, mode) || !surface_id_out)
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    memset(&status, 0, sizeof(status));
    status.struct_size = sizeof(status);
    status.version = RIN_GPU_PRESENTATION_VERSION;
    result = rin_gpu_presentation_get_status(&state->presentation, &status);
    if (result != RIN_GPU_PRESENTATION_OK ||
        output->device_generation != status.device_generation)
        return map_presentation_result(result == RIN_GPU_PRESENTATION_OK
                                           ? RIN_GPU_PRESENTATION_STALE
                                           : result);
    if (find_surface_for_display(state, output->display_id))
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    for (uint32_t index = 0u; index < RIN_GPU_VULKAN_WSI_MAX_SURFACES;
         ++index) {
        if (state->surfaces[index].slot_state == VULKAN_WSI_SLOT_FREE) {
            surface = &state->surfaces[index];
            break;
        }
    }
    if (!surface) return RIN_GPU_VULKAN_WSI_LIMIT;
    result = rin_gpu_presentation_register_output(&state->presentation,
                                                  output);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    memset(surface, 0, sizeof(*surface));
    surface->surface_id = (uint32_t)(surface - state->surfaces) + 1u;
    surface->image_count = image_count;
    surface->mode = mode;
    surface->output = *output;
    result = register_images(state, output, image_count, surface->image_tokens);
    if (result != RIN_GPU_PRESENTATION_OK) {
        (void)rin_gpu_presentation_remove_output(
            &state->presentation, output->display_id,
            output->output_generation + 1u);
        clear_surface(surface);
        return map_presentation_result(result);
    }
    surface->slot_state = VULKAN_WSI_SLOT_LIVE;
    ++state->surface_count;
    *surface_id_out = surface->surface_id;
    return RIN_GPU_VULKAN_WSI_OK;
}

int rin_gpu_vulkan_wsi_resize_surface(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id,
    const RinGpuPresentationOutputV1* output, uint32_t image_count) {
    VulkanWsiState* state = wsi_state(runtime);
    VulkanWsiSurfaceSlot* surface;
    uint64_t old_tokens[RIN_GPU_VULKAN_WSI_MAX_IMAGES];
    RinGpuPresentationStatusV1 status;
    uint32_t old_count;
    int result;
    if (!wsi_ready(state) || !output_valid(output) ||
        image_count < RIN_GPU_VULKAN_WSI_MIN_IMAGES ||
        image_count > RIN_GPU_VULKAN_WSI_MAX_IMAGES)
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    surface = find_surface(state, surface_id);
    if (!surface || output->display_id != surface->output.display_id ||
        output->device_generation != surface->output.device_generation ||
        output->output_generation <= surface->output.output_generation ||
        !mode_valid_for_output(output, surface->mode))
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    old_count = surface->image_count;
    memset(&status, 0, sizeof(status));
    status.struct_size = sizeof(status);
    status.version = RIN_GPU_PRESENTATION_VERSION;
    result = rin_gpu_presentation_get_status(&state->presentation, &status);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    if (status.image_count < old_count ||
        status.image_count - old_count >
            RIN_GPU_PRESENTATION_MAX_IMAGES - image_count)
        return RIN_GPU_VULKAN_WSI_LIMIT;
    memcpy(old_tokens, surface->image_tokens, sizeof(old_tokens));
    result = rin_gpu_presentation_remove_output(
        &state->presentation, surface->output.display_id,
        output->output_generation);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    for (uint32_t index = 0u; index < old_count; ++index)
        (void)rin_gpu_presentation_unregister_image(&state->presentation,
                                                   old_tokens[index]);
    surface->image_count = 0u;
    surface->output = *output;
    result = rin_gpu_presentation_update_output(&state->presentation,
                                                &surface->output);
    if (result != RIN_GPU_PRESENTATION_OK) {
        surface->out_of_date = 1u;
        return map_presentation_result(result);
    }
    result = register_images(state, &surface->output, image_count,
                             surface->image_tokens);
    if (result != RIN_GPU_PRESENTATION_OK) {
        surface->out_of_date = 1u;
        return map_presentation_result(result);
    }
    surface->image_count = image_count;
    surface->next_image = 0u;
    surface->suboptimal = 0u;
    surface->out_of_date = 0u;
    return RIN_GPU_VULKAN_WSI_OK;
}

int rin_gpu_vulkan_wsi_notify_output_change(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id,
    const RinGpuPresentationOutputV1* observed_output) {
    VulkanWsiState* state = wsi_state(runtime);
    VulkanWsiSurfaceSlot* surface;
    if (!wsi_ready(state) || !output_valid(observed_output))
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    surface = find_surface(state, surface_id);
    if (!surface || observed_output->display_id != surface->output.display_id ||
        observed_output->device_generation != surface->output.device_generation ||
        observed_output->output_generation <= surface->output.output_generation)
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    if (observed_output->width != surface->output.width ||
        observed_output->height != surface->output.height ||
        observed_output->format != surface->output.format ||
        !mode_valid_for_output(observed_output, surface->mode)) {
        surface->out_of_date = 1u;
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    }
    surface->suboptimal = 1u;
    return RIN_GPU_VULKAN_WSI_SUBOPTIMAL;
}

int rin_gpu_vulkan_wsi_remove_surface(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id) {
    VulkanWsiState* state = wsi_state(runtime);
    VulkanWsiSurfaceSlot* surface;
    uint64_t next_generation;
    int result;
    if (!wsi_ready(state)) return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    surface = find_surface(state, surface_id);
    if (!surface) return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    if (surface->output.output_generation == UINT64_MAX)
        return RIN_GPU_VULKAN_WSI_LIMIT;
    next_generation = surface->output.output_generation + 1u;
    result = rin_gpu_presentation_remove_output(
        &state->presentation, surface->output.display_id,
        next_generation);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    result = unregister_surface_images(state, surface);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    clear_surface(surface);
    --state->surface_count;
    return RIN_GPU_VULKAN_WSI_OK;
}

int rin_gpu_vulkan_wsi_acquire_next_image(
    RinGpuVulkanWsiRuntime* runtime, uint32_t surface_id,
    RinGpuVulkanWsiAcquireV1* acquire_out) {
    VulkanWsiState* state = wsi_state(runtime);
    VulkanWsiSurfaceSlot* surface;
    uint32_t start;
    if (acquire_out) memset(acquire_out, 0, sizeof(*acquire_out));
    if (!wsi_ready(state) || !acquire_out)
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    acquire_out->struct_size = sizeof(*acquire_out);
    acquire_out->version = RIN_GPU_VULKAN_WSI_VERSION;
    surface = find_surface(state, surface_id);
    if (!surface || surface->out_of_date)
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    start = surface->next_image;
    for (uint32_t offset = 0u; offset < surface->image_count; ++offset) {
        uint32_t image_index = (start + offset) % surface->image_count;
        RinGpuPresentationAcquireV1 acquire;
        int result;
        memset(&acquire, 0, sizeof(acquire));
        acquire.struct_size = sizeof(acquire);
        acquire.version = RIN_GPU_PRESENTATION_VERSION;
        acquire.display_id = surface->output.display_id;
        acquire.mode = surface->mode;
        acquire.image_token = surface->image_tokens[image_index];
        acquire.output_generation = surface->output.output_generation;
        acquire.device_generation = surface->output.device_generation;
        result = rin_gpu_presentation_begin_frame(&state->presentation,
                                                  &acquire);
        if (result == RIN_GPU_PRESENTATION_BUSY) continue;
        if (result != RIN_GPU_PRESENTATION_OK)
            return map_presentation_result(result);
        surface->next_image = (image_index + 1u) % surface->image_count;
        surface->acquired_frames[image_index] = acquire.frame_id;
        acquire_out->surface_id = surface_id;
        acquire_out->image_index = image_index;
        acquire_out->image_token = acquire.image_token;
        acquire_out->output_generation = acquire.output_generation;
        acquire_out->device_generation = acquire.device_generation;
        acquire_out->frame_id = acquire.frame_id;
        return surface->suboptimal ? RIN_GPU_VULKAN_WSI_SUBOPTIMAL
                                   : RIN_GPU_VULKAN_WSI_OK;
    }
    return RIN_GPU_VULKAN_WSI_NOT_READY;
}

int rin_gpu_vulkan_wsi_present(
    RinGpuVulkanWsiRuntime* runtime,
    const RinGpuVulkanWsiPresentV1* present, uint64_t* fence_value_out) {
    VulkanWsiState* state = wsi_state(runtime);
    VulkanWsiSurfaceSlot* surface;
    RinGpuPresentationSubmitV1 submit;
    int result;
    if (fence_value_out) *fence_value_out = 0u;
    if (!wsi_ready(state) || !present || !fence_value_out ||
        present->struct_size < sizeof(*present) ||
        present->version != RIN_GPU_VULKAN_WSI_VERSION ||
        present->surface_id == 0u ||
        (present->flags & ~RIN_GPU_PRESENTATION_SUBMIT_KNOWN_FLAGS) != 0u ||
        present->reserved[0] != 0u || present->reserved[1] != 0u)
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    surface = find_surface(state, present->surface_id);
    if (!surface || surface->out_of_date)
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    if (present->image_index >= surface->image_count ||
        present->image_token != surface->image_tokens[present->image_index] ||
        present->output_generation != surface->output.output_generation ||
        present->device_generation != surface->output.device_generation ||
        present->frame_id == 0u ||
        surface->acquired_frames[present->image_index] != present->frame_id)
        return RIN_GPU_VULKAN_WSI_OUT_OF_DATE;
    memset(&submit, 0, sizeof(submit));
    submit.struct_size = sizeof(submit);
    submit.version = RIN_GPU_PRESENTATION_VERSION;
    submit.display_id = surface->output.display_id;
    submit.mode = surface->mode;
    submit.image_token = present->image_token;
    submit.output_generation = present->output_generation;
    submit.device_generation = present->device_generation;
    submit.frame_id = present->frame_id;
    submit.flags = present->flags;
    submit.damage_count = present->damage_count;
    memcpy(submit.damage, present->damage, sizeof(submit.damage));
    result = rin_gpu_presentation_submit_frame(&state->presentation, &submit,
                                               fence_value_out);
    if (result != RIN_GPU_PRESENTATION_OK &&
        (result == RIN_GPU_PRESENTATION_BACKEND ||
         result == RIN_GPU_PRESENTATION_DEVICE_LOST))
        surface->acquired_frames[present->image_index] = 0u;
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    surface->acquired_frames[present->image_index] = 0u;
    surface->submitted_fences[present->image_index] = *fence_value_out;
    return surface->suboptimal ? RIN_GPU_VULKAN_WSI_SUBOPTIMAL
                               : RIN_GPU_VULKAN_WSI_OK;
}

int rin_gpu_vulkan_wsi_complete(
    RinGpuVulkanWsiRuntime* runtime,
    const RinGpuPresentationCompletionV1* completion) {
    VulkanWsiState* state = wsi_state(runtime);
    int result;
    if (!wsi_ready(state) || !completion)
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    result = rin_gpu_presentation_complete(&state->presentation, completion);
    if (result == RIN_GPU_PRESENTATION_OK ||
        result == RIN_GPU_PRESENTATION_TIMEOUT) {
        VulkanWsiSurfaceSlot* surface =
            find_surface_for_display(state, completion->display_id);
        if (surface) {
            int image_index = image_index_for_token(surface,
                                                    completion->image_token);
            if (image_index >= 0) {
                surface->submitted_fences[image_index] = 0u;
                surface->acquired_frames[image_index] = 0u;
            }
        }
    }
    return map_presentation_result(result);
}

int rin_gpu_vulkan_wsi_device_lost(RinGpuVulkanWsiRuntime* runtime) {
    VulkanWsiState* state = wsi_state(runtime);
    int result;
    if (!wsi_ready(state)) return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    result = rin_gpu_presentation_device_lost(&state->presentation);
    if (result == RIN_GPU_PRESENTATION_OK) {
        for (uint32_t index = 0u;
             index < RIN_GPU_VULKAN_WSI_MAX_SURFACES; ++index)
            state->surfaces[index].out_of_date = 1u;
    }
    return map_presentation_result(result);
}

int rin_gpu_vulkan_wsi_device_reset(
    RinGpuVulkanWsiRuntime* runtime, uint64_t next_device_generation) {
    VulkanWsiState* state = wsi_state(runtime);
    int result;
    if (!wsi_ready(state) || next_device_generation == 0u)
        return RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
    result = rin_gpu_presentation_device_reset(&state->presentation,
                                               next_device_generation);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    memset(state->surfaces, 0, sizeof(state->surfaces));
    state->surface_count = 0u;
    state->next_image_token = VULKAN_WSI_FIRST_IMAGE_TOKEN;
    return RIN_GPU_VULKAN_WSI_OK;
}

int rin_gpu_vulkan_wsi_get_presentation_status(
    RinGpuVulkanWsiRuntime* runtime,
    RinGpuPresentationStatusV1* status_out) {
    VulkanWsiState* state = wsi_state(runtime);
    return wsi_ready(state)
               ? map_presentation_result(
                     rin_gpu_presentation_get_status(&state->presentation,
                                                     status_out))
               : RIN_GPU_VULKAN_WSI_INVALID_ARGUMENT;
}
