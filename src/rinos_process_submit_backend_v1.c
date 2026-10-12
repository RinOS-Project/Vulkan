/* SPDX-License-Identifier: MIT */
#include <rinvulkan/rinos_process_submit_backend_v1.h>
#include <ringpu/rin_shader.h>

#include <stddef.h>
#include <string.h>

#define RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_NONE 0u
#define RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE 1u
#define RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING 2u
#define RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST 3u

static int rinos_range_valid(const void* pointer, size_t size) {
    uintptr_t start;
    if (!pointer || size == 0u) return 0;
    start = (uintptr_t)pointer;
    return size - 1u <= UINTPTR_MAX - start;
}

static int rinos_ranges_overlap(const void* first, size_t first_size,
                                const void* second, size_t second_size) {
    uintptr_t first_start;
    uintptr_t second_start;
    if (!rinos_range_valid(first, first_size) ||
        !rinos_range_valid(second, second_size))
        return 1;
    first_start = (uintptr_t)first;
    second_start = (uintptr_t)second;
    return first_start <= second_start
               ? second_start - first_start < first_size
               : first_start - second_start < second_size;
}

static int rinos_try_lock(volatile uint32_t* lock) {
    uint32_t expected = 0u;
    return __atomic_compare_exchange_n(lock, &expected, 1u, 0,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

static int rinos_map_sdk_result(RinResult result) {
    switch (result) {
    case RIN_SUCCESS:
        return RIN_VULKAN_PRODUCT_OK;
    case RIN_ERROR_INVALID_ARGUMENT:
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    case RIN_ERROR_ACCESS_DENIED:
        return RIN_VULKAN_PRODUCT_ACCESS_DENIED;
    case RIN_ERROR_BUSY:
    case RIN_ERROR_WOULD_BLOCK:
        return RIN_VULKAN_PRODUCT_BUSY;
    case RIN_ERROR_TIMEOUT:
        return RIN_VULKAN_PRODUCT_TIMEOUT;
    case RIN_ERROR_NOT_SUPPORTED:
        return RIN_VULKAN_PRODUCT_UNSUPPORTED;
    case RIN_ERROR_STALE_HANDLE:
        return RIN_VULKAN_PRODUCT_LOST;
    case RIN_ERROR_ABI_MISMATCH:
    case RIN_ERROR_INTEGRITY:
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    default:
        return RIN_VULKAN_PRODUCT_BACKEND_FAILED;
    }
}

static int rinos_queue_ids_valid(const uint32_t* queue_ids,
                                 uint32_t queue_count) {
    uint32_t index;
    if (!queue_ids || queue_count == 0u ||
        queue_count > RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_MAX_QUEUES)
        return 0;
    for (index = 0u; index < queue_count; ++index) {
        uint32_t prior;
        if (queue_ids[index] >= RIN_VULKAN_PRODUCT_MAX_QUEUES) return 0;
        for (prior = 0u; prior < index; ++prior) {
            if (queue_ids[prior] == queue_ids[index]) return 0;
        }
    }
    return 1;
}

static int rinos_create_process_compute_pipeline_v2(
    void* context, uint64_t pipeline_cookie, const void* rin_shader_ir,
    uint64_t shader_ir_size, uint64_t* pipeline_token_out);
static int rinos_destroy_process_compute_pipeline_v2(
    void* context, uint64_t pipeline_token);
static int rinos_create_process_compute_pipeline_v3(
    void* context, uint64_t pipeline_cookie, const void* rin_shader_ir,
    uint64_t shader_ir_size, uint64_t* pipeline_token_out);
static int rinos_destroy_process_compute_pipeline_v3(
    void* context, uint64_t pipeline_token);
static int rinos_create_process_compute_bind_group_v3(
    void* context, uint64_t bind_group_token, uint64_t pipeline_token,
    const RinVulkanProductProcessComputeBindingV8* bindings,
    uint32_t binding_count);
static int rinos_destroy_process_compute_bind_group_v3(
    void* context, uint64_t bind_group_token);

static volatile uint64_t g_next_process_resource_cookie = UINT64_C(1);

static int rinos_detach_locked(
    RinVulkanRinOSProcessSubmitBackendV1* backend) {
    int first_error = RIN_VULKAN_PRODUCT_OK;
    uint32_t index;
    for (index = 0u; index < backend->queue_count; ++index) {
        RinVulkanRinOSProcessQueueV1* queue = &backend->queues[index];
        RinResult sdk_result;
        if (queue->active == 0u) continue;
        if (queue->active != 1u || queue->token == 0u) {
            if (first_error == RIN_VULKAN_PRODUCT_OK)
                first_error = RIN_VULKAN_PRODUCT_PROTOCOL;
            continue;
        }
        sdk_result = rin_gpu_process_queue_release_v1(queue->token);
        if (sdk_result == RIN_SUCCESS) {
            memset(queue, 0, sizeof(*queue));
        } else if (first_error == RIN_VULKAN_PRODUCT_OK) {
            first_error = rinos_map_sdk_result(sdk_result);
        }
    }
    if (first_error != RIN_VULKAN_PRODUCT_OK) {
        backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return first_error;
    }
    memset(backend, 0, sizeof(*backend));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_submit_process_commands(
    void* context, uint32_t queue_id,
    const RinGpuBackendCommandV1* commands, uint32_t command_count,
    const RinVulkanProductResourceV1* resources, uint32_t resource_count,
    RinVulkanProductProcessSubmitReceiptV1* receipt_out) {
    RinVulkanRinOSProcessSubmitBackendV1* backend =
        (RinVulkanRinOSProcessSubmitBackendV1*)context;
    RinGpuProcessResourceV1 sdk_resources[
        RIN_GPU_PROCESS_MAX_RESOURCES_V1];
    RinGpuProcessSubmitDescV1 descriptor;
    RinGpuProcessSubmitReceiptV1 sdk_receipt;
    RinVulkanRinOSProcessQueueV1* selected_queue = NULL;
    uint64_t command_bytes;
    uint32_t index;
    int result;

    if (!backend || backend != context || !receipt_out ||
        !rinos_range_valid(receipt_out, sizeof(*receipt_out)) ||
        command_count == 0u ||
        command_count > RIN_GPU_PROCESS_MAX_COMMANDS_V1 ||
        resource_count > RIN_GPU_PROCESS_MAX_RESOURCES_V1 ||
        sizeof(RinGpuBackendCommandV1) !=
            RIN_GPU_PROCESS_COMMAND_RECORD_SIZE_V1 ||
        !rinos_range_valid(commands, sizeof(*commands) * command_count) ||
        (resource_count != 0u &&
         (!resources || !rinos_range_valid(
                            resources, sizeof(*resources) * resource_count))) ||
        rinos_ranges_overlap(backend, sizeof(*backend), receipt_out,
                             sizeof(*receipt_out)) ||
        rinos_ranges_overlap(backend, sizeof(*backend), commands,
                             sizeof(*commands) * command_count) ||
        (resource_count != 0u &&
         rinos_ranges_overlap(backend, sizeof(*backend), resources,
                              sizeof(*resources) * resource_count)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;

    memset(receipt_out, 0, sizeof(*receipt_out));
    receipt_out->struct_size = sizeof(*receipt_out);
    receipt_out->version =
        RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION;

    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version !=
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_VERSION ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE) {
        result = backend->state == RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST
                     ? RIN_VULKAN_PRODUCT_LOST
                     : RIN_VULKAN_PRODUCT_STATE;
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return result;
    }
    for (index = 0u; index < backend->queue_count; ++index) {
        if (backend->queues[index].queue_id == queue_id &&
            backend->queues[index].active == 1u &&
            backend->queues[index].token != 0u) {
            selected_queue = &backend->queues[index];
            break;
        }
    }
    if (!selected_queue) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }

    memset(sdk_resources, 0, sizeof(sdk_resources));
    for (index = 0u; index < resource_count; ++index) {
        if (resources[index].allocation_handle == 0u ||
            resources[index].reserved != 0u ||
            resources[index].required_gpu_access == 0u ||
            (resources[index].required_gpu_access &
             ~(RIN_GPU_ALLOCATION_GPU_READ |
               RIN_GPU_ALLOCATION_GPU_WRITE)) != 0u) {
            __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
            return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
        }
        sdk_resources[index].allocation = resources[index].allocation_handle;
        sdk_resources[index].required_gpu_access =
            resources[index].required_gpu_access;
    }

    command_bytes = (uint64_t)command_count * sizeof(*commands);
    if (command_bytes == 0u) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.struct_size = sizeof(descriptor);
    descriptor.version = RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V1;
    descriptor.commands.address = (uint64_t)(uintptr_t)commands;
    descriptor.commands.size = command_bytes;
    descriptor.command_count = command_count;
    descriptor.command_record_size = sizeof(*commands);
    descriptor.resource_count = resource_count;
    if (resource_count != 0u) {
        descriptor.resources.address = (uint64_t)(uintptr_t)sdk_resources;
        descriptor.resources.size =
            (uint64_t)resource_count * sizeof(sdk_resources[0]);
    }

    memset(&sdk_receipt, 0, sizeof(sdk_receipt));
    result = rinos_map_sdk_result(rin_gpu_process_submit_v1(
        selected_queue->token, &descriptor, &sdk_receipt));
    if (result == RIN_VULKAN_PRODUCT_OK) {
        if (sdk_receipt.struct_size != sizeof(sdk_receipt) ||
            sdk_receipt.version != RIN_GPU_PROCESS_SUBMIT_RECEIPT_VERSION_V1 ||
            sdk_receipt.queue_id != queue_id || sdk_receipt.reserved0 != 0u ||
            sdk_receipt.sequence == 0u ||
            sdk_receipt.completion_value == 0u ||
            sdk_receipt.device_epoch == 0u ||
            sdk_receipt.iommu_map_generation == 0u ||
            sdk_receipt.reserved[0] != 0u || sdk_receipt.reserved[1] != 0u) {
            backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST;
            result = RIN_VULKAN_PRODUCT_PROTOCOL;
        } else {
            receipt_out->queue_id = sdk_receipt.queue_id;
            receipt_out->sequence = sdk_receipt.sequence;
            receipt_out->completion_value = sdk_receipt.completion_value;
            receipt_out->device_epoch = sdk_receipt.device_epoch;
            receipt_out->iommu_map_generation =
                sdk_receipt.iommu_map_generation;
        }
    }
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return result;
}

int rin_vulkan_rinos_process_submit_backend_v1_init(
    RinVulkanRinOSProcessSubmitBackendV1* backend,
    RinVulkanProductPlatformV5* parent,
    uint64_t device_id, uint64_t device_generation,
    const uint32_t* queue_ids, uint32_t queue_count) {
    RinResult sdk_result = RIN_SUCCESS;
    uint32_t index;
    int result;

    if (!backend || !parent || device_id == 0u || device_generation == 0u ||
        parent->struct_size != sizeof(*parent) ||
        parent->version != RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION ||
        !rinos_range_valid(backend, sizeof(*backend)) ||
        !rinos_range_valid(parent, sizeof(*parent)) ||
        rinos_ranges_overlap(backend, sizeof(*backend), parent,
                             sizeof(*parent)) ||
        queue_count == 0u ||
        queue_count >
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_MAX_QUEUES ||
        !rinos_range_valid(queue_ids, sizeof(*queue_ids) * queue_count) ||
        rinos_ranges_overlap(backend, sizeof(*backend), queue_ids,
                             sizeof(*queue_ids) * queue_count) ||
        !rinos_queue_ids_valid(queue_ids, queue_count))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (backend->struct_size != 0u || backend->version != 0u ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_NONE) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    memset(backend, 0, sizeof(*backend));
    __atomic_store_n(&backend->lock, 1u, __ATOMIC_RELAXED);
    backend->struct_size = sizeof(*backend);
    backend->version =
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_VERSION;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    backend->device_id = device_id;
    backend->device_generation = device_generation;
    backend->queue_count = queue_count;
    backend->platform.struct_size = sizeof(backend->platform);
    backend->platform.version = RIN_VULKAN_PRODUCT_PLATFORM_V6_VERSION;
    backend->platform.base = parent;
    backend->platform.context = backend;
    backend->platform.submit_process_commands = rinos_submit_process_commands;

    for (index = 0u; index < queue_count; ++index) {
        RinVulkanRinOSProcessQueueV1* queue = &backend->queues[index];
        queue->queue_id = queue_ids[index];
        sdk_result = rin_gpu_process_queue_bind_v1(
            device_id, device_generation, queue_ids[index], &queue->token);
        if (sdk_result == RIN_SUCCESS && queue->token != 0u) {
            queue->active = 1u;
            continue;
        }
        if (queue->token != 0u) queue->active = 1u;
        if (sdk_result == RIN_SUCCESS) {
            sdk_result = RIN_ERROR_ABI_MISMATCH;
        }
        result = rinos_detach_locked(backend);
        return result == RIN_VULKAN_PRODUCT_OK
                   ? rinos_map_sdk_result(sdk_result)
                   : result;
    }
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_submit_backend_v1_valid(
    const RinVulkanRinOSProcessSubmitBackendV1* backend) {
    const RinVulkanProductPlatformV6* platform;
    uint32_t index;
    if (!backend || __atomic_load_n(&backend->lock, __ATOMIC_ACQUIRE) != 0u ||
        backend->struct_size != sizeof(*backend) ||
        backend->version !=
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_VERSION ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE ||
        backend->device_id == 0u || backend->device_generation == 0u ||
        backend->queue_count == 0u ||
        backend->queue_count >
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_MAX_QUEUES ||
        backend->reserved0 != 0u || backend->reserved[0] != 0u ||
        backend->reserved[1] != 0u)
        return 0;
    platform = &backend->platform;
    if (platform->struct_size != sizeof(*platform) ||
        platform->version != RIN_VULKAN_PRODUCT_PLATFORM_V6_VERSION ||
        !platform->base || platform->context != backend ||
        platform->submit_process_commands != rinos_submit_process_commands ||
        platform->reserved[0] != 0u || platform->reserved[1] != 0u)
        return 0;
    for (index = 0u; index < backend->queue_count; ++index) {
        const RinVulkanRinOSProcessQueueV1* queue = &backend->queues[index];
        uint32_t prior;
        if (queue->queue_id >= RIN_VULKAN_PRODUCT_MAX_QUEUES ||
            queue->active != 1u || queue->token == 0u)
            return 0;
        for (prior = 0u; prior < index; ++prior) {
            if (backend->queues[prior].queue_id == queue->queue_id ||
                backend->queues[prior].token == queue->token)
                return 0;
        }
    }
    return 1;
}

RinVulkanProductPlatformV6*
rin_vulkan_rinos_process_submit_backend_v1_platform(
    RinVulkanRinOSProcessSubmitBackendV1* backend) {
    return rin_vulkan_rinos_process_submit_backend_v1_valid(backend)
               ? &backend->platform
               : NULL;
}

int rin_vulkan_rinos_process_submit_backend_v1_detach(
    RinVulkanRinOSProcessSubmitBackendV1* backend) {
    int result;
    if (!backend) return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version !=
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_VERSION ||
        (backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
         backend->state !=
             RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING &&
         backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST) ||
        backend->queue_count == 0u ||
        backend->queue_count >
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V1_MAX_QUEUES) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    result = rinos_detach_locked(backend);
    return result;
}

static int rinos_compute_backend_v2_header_valid(
        const RinVulkanRinOSProcessSubmitBackendV2* backend) {
    const RinVulkanProductPlatformV7* platform;
    if (!backend || backend->struct_size != sizeof(*backend) ||
        backend->version !=
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_VERSION ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE ||
        backend->reserved[0] != 0u || backend->reserved[1] != 0u ||
        !rin_vulkan_rinos_process_submit_backend_v1_valid(
            &backend->submit_backend))
        return 0;
    platform = &backend->platform;
    return platform->struct_size == sizeof(*platform) &&
           platform->version == RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION &&
           platform->base == &backend->submit_backend.platform &&
           platform->context == backend &&
           platform->create_process_compute_pipeline ==
               rinos_create_process_compute_pipeline_v2 &&
           platform->destroy_process_compute_pipeline ==
               rinos_destroy_process_compute_pipeline_v2 &&
           platform->reserved[0] == 0u && platform->reserved[1] == 0u;
}

static RinVulkanRinOSProcessComputePipelineV2*
rinos_compute_pipeline_v2_find(
        RinVulkanRinOSProcessSubmitBackendV2* backend,
        uint64_t pipeline_token) {
    uint32_t index;
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_MAX_COMPUTE_PIPELINES;
         ++index) {
        RinVulkanRinOSProcessComputePipelineV2* pipeline =
            &backend->compute_pipelines[index];
        if (pipeline->pipeline_token == pipeline_token &&
            pipeline->pipeline_active == 1u)
            return pipeline;
    }
    return NULL;
}

static int rinos_destroy_compute_pipeline_v2_locked(
        RinVulkanRinOSProcessSubmitBackendV2* backend,
        RinVulkanRinOSProcessComputePipelineV2* pipeline) {
    int result;
    if (!backend || !pipeline ||
        (pipeline->pipeline_active != 0u &&
         pipeline->pipeline_active != 1u) ||
        (pipeline->shader_module_active != 0u &&
         pipeline->shader_module_active != 1u))
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    if (pipeline->pipeline_active != 0u) {
        result = rinos_map_sdk_result(rin_gpu_process_object_destroy_v1(
            backend->submit_backend.device_id,
            backend->submit_backend.device_generation,
            pipeline->pipeline_token));
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
        pipeline->pipeline_active = 0u;
    }
    if (pipeline->shader_module_active != 0u) {
        result = rinos_map_sdk_result(rin_gpu_process_object_destroy_v1(
            backend->submit_backend.device_id,
            backend->submit_backend.device_generation,
            pipeline->shader_module_token));
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
        pipeline->shader_module_active = 0u;
    }
    memset(pipeline, 0, sizeof(*pipeline));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_create_process_compute_pipeline_v2(
        void* context, uint64_t pipeline_cookie, const void* rin_shader_ir,
        uint64_t shader_ir_size, uint64_t* pipeline_token_out) {
    RinVulkanRinOSProcessSubmitBackendV2* backend =
        (RinVulkanRinOSProcessSubmitBackendV2*)context;
    RinVulkanRinOSProcessComputePipelineV2* slot = NULL;
    uint64_t shader_token;
    uint32_t index;
    int result;
    RinResult sdk_result;

    if (pipeline_token_out) *pipeline_token_out = 0u;
    if (!backend || !pipeline_token_out || pipeline_cookie == 0u ||
        !rin_shader_ir || shader_ir_size < sizeof(RinShaderHeaderV1) ||
        shader_ir_size > RIN_GPU_PROCESS_SHADER_MAX_BYTES_V1 ||
        shader_ir_size > SIZE_MAX ||
        !rinos_range_valid(rin_shader_ir, (size_t)shader_ir_size) ||
        rinos_ranges_overlap(backend, sizeof(*backend), rin_shader_ir,
                             (size_t)shader_ir_size) ||
        rinos_ranges_overlap(backend, sizeof(*backend), pipeline_token_out,
                             sizeof(*pipeline_token_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_compute_backend_v2_header_valid(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    shader_token =
        ((uint64_t)RIN_VULKAN_RINOS_PROCESS_SHADER_MODULE_TOKEN_TAG << 48u) |
        (pipeline_cookie & UINT64_C(0x0000FFFFFFFFFFFF));
    if (shader_token == 0u || shader_token == pipeline_cookie) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_MAX_COMPUTE_PIPELINES;
         ++index) {
        RinVulkanRinOSProcessComputePipelineV2* candidate =
            &backend->compute_pipelines[index];
        if (candidate->pipeline_active == 0u &&
            candidate->shader_module_active == 0u && slot == NULL)
            slot = candidate;
        if (candidate->pipeline_token == pipeline_cookie ||
            candidate->shader_module_token == pipeline_cookie ||
            candidate->pipeline_token == shader_token ||
            candidate->shader_module_token == shader_token) {
            __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
            return RIN_VULKAN_PRODUCT_BUSY;
        }
    }
    if (!slot) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_LIMIT;
    }
    slot->pipeline_token = pipeline_cookie;
    slot->shader_module_token = shader_token;
    sdk_result = rin_gpu_process_shader_module_create_v1(
        backend->submit_backend.device_id,
        backend->submit_backend.device_generation, shader_token,
        rin_shader_ir, shader_ir_size);
    result = rinos_map_sdk_result(sdk_result);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        memset(slot, 0, sizeof(*slot));
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return result;
    }
    slot->shader_module_active = 1u;
    sdk_result = rin_gpu_process_compute_pipeline_create_v1(
        backend->submit_backend.device_id,
        backend->submit_backend.device_generation, pipeline_cookie,
        shader_token);
    result = rinos_map_sdk_result(sdk_result);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        const int cleanup_result =
            rinos_destroy_compute_pipeline_v2_locked(backend, slot);
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return cleanup_result == RIN_VULKAN_PRODUCT_OK ? result
                                                       : cleanup_result;
    }
    slot->pipeline_active = 1u;
    *pipeline_token_out = pipeline_cookie;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_destroy_process_compute_pipeline_v2(
        void* context, uint64_t pipeline_token) {
    RinVulkanRinOSProcessSubmitBackendV2* backend =
        (RinVulkanRinOSProcessSubmitBackendV2*)context;
    RinVulkanRinOSProcessComputePipelineV2* pipeline;
    int result;
    if (!backend || pipeline_token == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_compute_backend_v2_header_valid(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    pipeline = rinos_compute_pipeline_v2_find(backend, pipeline_token);
    if (!pipeline) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    result = rinos_destroy_compute_pipeline_v2_locked(backend, pipeline);
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return result;
}

int rin_vulkan_rinos_process_submit_backend_v2_init(
    RinVulkanRinOSProcessSubmitBackendV2* backend,
    RinVulkanProductPlatformV5* parent, uint64_t device_id,
    uint64_t device_generation, const uint32_t* queue_ids,
    uint32_t queue_count) {
    int result;
    if (!backend || !parent ||
        !rinos_range_valid(backend, sizeof(*backend)) ||
        rinos_ranges_overlap(backend, sizeof(*backend), parent,
                             sizeof(*parent)) ||
        backend->struct_size != 0u || backend->version != 0u ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_NONE ||
        backend->reserved[0] != 0u || backend->reserved[1] != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    memset(backend, 0, sizeof(*backend));
    __atomic_store_n(&backend->lock, 1u, __ATOMIC_RELAXED);
    backend->struct_size = sizeof(*backend);
    backend->version = RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_VERSION;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);

    result = rin_vulkan_rinos_process_submit_backend_v1_init(
        &backend->submit_backend, parent, device_id, device_generation,
        queue_ids, queue_count);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        if (backend->submit_backend.state ==
                RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_NONE) {
            memset(backend, 0, sizeof(*backend));
            return result;
        }
        return result;
    }
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    backend->platform.struct_size = sizeof(backend->platform);
    backend->platform.version = RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION;
    backend->platform.base = &backend->submit_backend.platform;
    backend->platform.context = backend;
    backend->platform.create_process_compute_pipeline =
        rinos_create_process_compute_pipeline_v2;
    backend->platform.destroy_process_compute_pipeline =
        rinos_destroy_process_compute_pipeline_v2;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_submit_backend_v2_valid(
        const RinVulkanRinOSProcessSubmitBackendV2* backend) {
    uint32_t index;
    if (__atomic_load_n(&backend->lock, __ATOMIC_ACQUIRE) != 0u ||
        !rinos_compute_backend_v2_header_valid(backend))
        return 0;
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_MAX_COMPUTE_PIPELINES;
         ++index) {
        const RinVulkanRinOSProcessComputePipelineV2* pipeline =
            &backend->compute_pipelines[index];
        if ((pipeline->pipeline_active == 0u &&
             pipeline->shader_module_active == 0u) !=
                (pipeline->pipeline_token == 0u &&
                 pipeline->shader_module_token == 0u) ||
            (pipeline->pipeline_active != 0u &&
             pipeline->pipeline_active != 1u) ||
            (pipeline->shader_module_active != 0u &&
             pipeline->shader_module_active != 1u) ||
            (pipeline->pipeline_active != 0u &&
             pipeline->pipeline_token == 0u) ||
            (pipeline->shader_module_active != 0u &&
             pipeline->shader_module_token == 0u))
            return 0;
    }
    return 1;
}

RinVulkanProductPlatformV7*
rin_vulkan_rinos_process_submit_backend_v2_platform(
        RinVulkanRinOSProcessSubmitBackendV2* backend) {
    return rin_vulkan_rinos_process_submit_backend_v2_valid(backend)
               ? &backend->platform
               : NULL;
}

int rin_vulkan_rinos_process_submit_backend_v2_detach(
        RinVulkanRinOSProcessSubmitBackendV2* backend) {
    uint32_t index;
    int first_error = RIN_VULKAN_PRODUCT_OK;
    int submit_result;
    if (!backend || !rinos_try_lock(&backend->lock))
        return backend ? RIN_VULKAN_PRODUCT_BUSY
                       : RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version !=
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_VERSION ||
        (backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
         backend->state !=
             RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_MAX_COMPUTE_PIPELINES;
         ++index) {
        RinVulkanRinOSProcessComputePipelineV2* pipeline =
            &backend->compute_pipelines[index];
        int result;
        if (pipeline->pipeline_active == 0u &&
            pipeline->shader_module_active == 0u)
            continue;
        result = rinos_destroy_compute_pipeline_v2_locked(backend, pipeline);
        if (result != RIN_VULKAN_PRODUCT_OK &&
            first_error == RIN_VULKAN_PRODUCT_OK)
            first_error = result;
    }
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    if (first_error != RIN_VULKAN_PRODUCT_OK) return first_error;

    submit_result = rin_vulkan_rinos_process_submit_backend_v1_detach(
        &backend->submit_backend);
    if (submit_result != RIN_VULKAN_PRODUCT_OK) return submit_result;
    memset(backend, 0, sizeof(*backend));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_process_v3_header_valid(
        const RinVulkanRinOSProcessSubmitBackendV3* backend) {
    uint32_t index;
    if (!backend || backend->struct_size != sizeof(*backend) ||
        backend->version != RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_VERSION ||
        (backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
         backend->state !=
             RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING) ||
        backend->reserved[0] != 0u || backend->reserved[1] != 0u ||
        !rin_vulkan_rinos_process_submit_backend_v2_valid(
            &backend->compute_backend))
        return 0;
    if (backend->platform_v7.struct_size !=
            sizeof(backend->platform_v7) ||
        backend->platform_v7.version !=
            RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION ||
        backend->platform_v7.base !=
            &backend->compute_backend.submit_backend.platform ||
        backend->platform_v7.context != backend ||
        backend->platform_v7.create_process_compute_pipeline !=
            rinos_create_process_compute_pipeline_v3 ||
        backend->platform_v7.destroy_process_compute_pipeline !=
            rinos_destroy_process_compute_pipeline_v3 ||
        backend->platform_v7.reserved[0] != 0u ||
        backend->platform_v7.reserved[1] != 0u ||
        backend->platform_v8.struct_size !=
            sizeof(backend->platform_v8) ||
        backend->platform_v8.version !=
            RIN_VULKAN_PRODUCT_PLATFORM_V8_VERSION ||
        backend->platform_v8.base != &backend->platform_v7 ||
        backend->platform_v8.context != backend ||
        backend->platform_v8.create_process_compute_bind_group !=
            rinos_create_process_compute_bind_group_v3 ||
        backend->platform_v8.destroy_process_compute_bind_group !=
            rinos_destroy_process_compute_bind_group_v3 ||
        backend->platform_v8.reserved[0] != 0u ||
        backend->platform_v8.reserved[1] != 0u)
        return 0;
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS;
         ++index) {
        const RinVulkanRinOSProcessComputeBindGroupV3* group =
            &backend->bind_groups[index];
        uint32_t binding_index;
        if (group->bind_group_token == 0u) {
            if (group->pipeline_token != 0u ||
                group->bind_group_active != 0u || group->binding_count != 0u)
                return 0;
            for (binding_index = 0u;
                 binding_index <
                     RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS;
                 ++binding_index)
                if (group->bindings[binding_index].resource_cookie != 0u ||
                    group->bindings[binding_index].active != 0u ||
                    group->bindings[binding_index].reserved != 0u)
                    return 0;
            continue;
        }
        if (group->pipeline_token == 0u ||
            group->bind_group_active > 1u || group->binding_count == 0u ||
            group->binding_count >
                RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS ||
            (group->bind_group_active != 0u &&
             group->bindings[0].resource_cookie == 0u))
            return 0;
        for (binding_index = 0u;
             binding_index <
                 RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS;
             ++binding_index) {
            const RinVulkanRinOSProcessComputeBindingV3* binding =
                &group->bindings[binding_index];
            if ((binding->active != 0u && binding->active != 1u) ||
                binding->reserved != 0u ||
                (binding->active != 0u) !=
                    (binding->resource_cookie != 0u) ||
                (binding_index < group->binding_count &&
                 binding->resource_cookie == 0u &&
                 group->bind_group_active != 0u) ||
                (binding_index >= group->binding_count &&
                 (binding->active != 0u ||
                  binding->resource_cookie != 0u)))
                return 0;
        }
        {
            uint32_t prior;
            for (prior = 0u; prior < index; ++prior)
                if (backend->bind_groups[prior].bind_group_token ==
                    group->bind_group_token)
                    return 0;
        }
    }
    return 1;
}

static RinVulkanRinOSProcessComputeBindGroupV3*
rinos_process_v3_find_bind_group(
        RinVulkanRinOSProcessSubmitBackendV3* backend,
        uint64_t bind_group_token) {
    uint32_t index;
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS;
         ++index)
        if (backend->bind_groups[index].bind_group_token == bind_group_token)
            return &backend->bind_groups[index];
    return NULL;
}

static uint64_t rinos_process_v3_allocate_resource_cookie(void) {
    uint64_t next = __atomic_load_n(&g_next_process_resource_cookie,
                                    __ATOMIC_RELAXED);
    for (;;) {
        uint64_t expected;
        if (next == 0u || next > UINT64_C(0x0000FFFFFFFFFFFF)) return 0u;
        expected = next;
        if (__atomic_compare_exchange_n(&g_next_process_resource_cookie,
                                       &expected, next + 1u, 0,
                                       __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            return (RIN_VULKAN_RINOS_PROCESS_RESOURCE_COOKIE_TAG << 48u) |
                   next;
        next = expected;
    }
}

static int rinos_process_v3_destroy_bind_group_locked(
        RinVulkanRinOSProcessSubmitBackendV3* backend,
        RinVulkanRinOSProcessComputeBindGroupV3* group) {
    uint32_t index;
    int result;
    if (!backend || !group || group->bind_group_token == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (group->bind_group_active != 0u) {
        result = rinos_map_sdk_result(rin_gpu_process_object_destroy_v1(
            backend->compute_backend.submit_backend.device_id,
            backend->compute_backend.submit_backend.device_generation,
            group->bind_group_token));
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
        group->bind_group_active = 0u;
    }
    for (index = 0u; index < group->binding_count; ++index) {
        RinVulkanRinOSProcessComputeBindingV3* binding =
            &group->bindings[index];
        if (binding->active == 0u) continue;
        result = rinos_map_sdk_result(rin_gpu_process_resource_unbind_v1(
            backend->compute_backend.submit_backend.device_id,
            backend->compute_backend.submit_backend.device_generation,
            binding->resource_cookie));
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
        memset(binding, 0, sizeof(*binding));
    }
    memset(group, 0, sizeof(*group));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_process_v3_retry_pending_groups_locked(
        RinVulkanRinOSProcessSubmitBackendV3* backend) {
    uint32_t index;
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS;
         ++index) {
        RinVulkanRinOSProcessComputeBindGroupV3* group =
            &backend->bind_groups[index];
        int result;
        if (group->bind_group_token == 0u ||
            group->bind_group_active != 0u)
            continue;
        result = rinos_process_v3_destroy_bind_group_locked(backend, group);
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
    }
    return RIN_VULKAN_PRODUCT_OK;
}

static uint32_t rinos_process_v3_sdk_access(uint32_t access) {
    uint32_t sdk_access = 0u;
    if ((access & RIN_GPU_RESOURCE_READ) != 0u)
        sdk_access |= RIN_GPU_ALLOCATION_GPU_READ;
    if ((access & RIN_GPU_RESOURCE_WRITE) != 0u)
        sdk_access |= RIN_GPU_ALLOCATION_GPU_WRITE;
    return sdk_access;
}

static int rinos_create_process_compute_pipeline_v3(
        void* context, uint64_t pipeline_cookie, const void* rin_shader_ir,
        uint64_t shader_ir_size, uint64_t* pipeline_token_out) {
    RinVulkanRinOSProcessSubmitBackendV3* backend =
        (RinVulkanRinOSProcessSubmitBackendV3*)context;
    int result;
    if (!backend || !pipeline_token_out)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_process_v3_header_valid(backend) ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    result = rinos_process_v3_retry_pending_groups_locked(backend);
    if (result == RIN_VULKAN_PRODUCT_OK)
        result = backend->compute_backend.platform
                     .create_process_compute_pipeline(
                         backend->compute_backend.platform.context,
                         pipeline_cookie, rin_shader_ir, shader_ir_size,
                         pipeline_token_out);
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return result;
}

static int rinos_destroy_process_compute_pipeline_v3(
        void* context, uint64_t pipeline_token) {
    RinVulkanRinOSProcessSubmitBackendV3* backend =
        (RinVulkanRinOSProcessSubmitBackendV3*)context;
    uint32_t index;
    int result;
    if (!backend || pipeline_token == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_process_v3_header_valid(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    result = rinos_process_v3_retry_pending_groups_locked(backend);
    for (index = 0u;
         result == RIN_VULKAN_PRODUCT_OK &&
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS;
         ++index) {
        RinVulkanRinOSProcessComputeBindGroupV3* group =
            &backend->bind_groups[index];
        if (group->bind_group_token == 0u ||
            group->pipeline_token != pipeline_token)
            continue;
        result = rinos_process_v3_destroy_bind_group_locked(backend, group);
    }
    if (result == RIN_VULKAN_PRODUCT_OK)
        result = backend->compute_backend.platform
                     .destroy_process_compute_pipeline(
                         backend->compute_backend.platform.context,
                         pipeline_token);
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return result;
}

static int rinos_create_process_compute_bind_group_v3(
        void* context, uint64_t bind_group_token, uint64_t pipeline_token,
        const RinVulkanProductProcessComputeBindingV8* bindings,
        uint32_t binding_count) {
    RinVulkanRinOSProcessSubmitBackendV3* backend =
        (RinVulkanRinOSProcessSubmitBackendV3*)context;
    RinVulkanRinOSProcessComputeBindGroupV3* group = NULL;
    RinGpuProcessComputeBindingV1 sdk_bindings[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS];
    uint8_t seen_indices[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS] = {0};
    size_t bindings_bytes;
    uint32_t index;
    int result;
    if (!backend || bind_group_token == 0u || pipeline_token == 0u ||
        bind_group_token == pipeline_token || !bindings || binding_count == 0u ||
        binding_count >
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BINDINGS)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    bindings_bytes = sizeof(*bindings) * (size_t)binding_count;
    if (!rinos_range_valid(bindings, bindings_bytes) ||
        rinos_ranges_overlap(backend, sizeof(*backend), bindings,
                             bindings_bytes))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    for (index = 0u; index < binding_count; ++index) {
        const RinVulkanProductProcessComputeBindingV8* binding =
            &bindings[index];
        uint32_t prior;
        if (binding->struct_size != sizeof(*binding) ||
            binding->version !=
                RIN_VULKAN_PRODUCT_PROCESS_COMPUTE_BINDING_V8_VERSION ||
            binding->resource_index >= binding_count ||
            seen_indices[binding->resource_index] != 0u ||
            binding->resource_cookie == 0u ||
            binding->allocation_handle == 0u ||
            binding->resource_size_bytes == 0u ||
            binding->allocation_offset >
                UINT64_MAX - binding->resource_size_bytes ||
            binding->offset > binding->resource_size_bytes ||
            binding->size_bytes == 0u ||
            binding->size_bytes >
                binding->resource_size_bytes - binding->offset ||
            binding->access == 0u ||
            (binding->access & ~(RIN_GPU_RESOURCE_READ |
                                 RIN_GPU_RESOURCE_WRITE)) != 0u ||
            binding->reserved[0] != 0u || binding->reserved[1] != 0u)
            return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
        seen_indices[binding->resource_index] = 1u;
        for (prior = 0u; prior < index; ++prior)
            if (bindings[prior].resource_cookie == binding->resource_cookie)
                return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_process_v3_header_valid(backend) ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE ||
        !rinos_compute_pipeline_v2_find(
            &backend->compute_backend, pipeline_token) ||
        rinos_process_v3_find_bind_group(backend, bind_group_token)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    result = rinos_process_v3_retry_pending_groups_locked(backend);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return result;
    }
    for (index = 0u;
         index <
             RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS;
         ++index)
        if (backend->bind_groups[index].bind_group_token == 0u) {
            group = &backend->bind_groups[index];
            break;
        }
    if (!group) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_LIMIT;
    }
    memset(group, 0, sizeof(*group));
    group->bind_group_token = bind_group_token;
    group->pipeline_token = pipeline_token;
    group->binding_count = binding_count;
    memset(sdk_bindings, 0, sizeof(sdk_bindings));
    for (index = 0u; index < binding_count; ++index) {
        const RinVulkanProductProcessComputeBindingV8* source =
            &bindings[index];
        RinVulkanRinOSProcessComputeBindingV3* owned =
            &group->bindings[source->resource_index];
        RinGpuProcessResourceBindingV1 resource_binding;
        const uint64_t resource_cookie =
            rinos_process_v3_allocate_resource_cookie();
        const uint32_t sdk_access = rinos_process_v3_sdk_access(
            source->access);
        RinResult sdk_result;
        if (resource_cookie == 0u || sdk_access == 0u) {
            result = RIN_VULKAN_PRODUCT_LIMIT;
            goto rollback;
        }
        owned->resource_cookie = resource_cookie;
        memset(&resource_binding, 0, sizeof(resource_binding));
        resource_binding.struct_size = sizeof(resource_binding);
        resource_binding.version =
            RIN_GPU_PROCESS_RESOURCE_BINDING_VERSION_V1;
        resource_binding.resource_cookie = resource_cookie;
        resource_binding.allocation = source->allocation_handle;
        resource_binding.allocation_offset = source->allocation_offset;
        resource_binding.size_bytes = source->resource_size_bytes;
        resource_binding.required_gpu_access = sdk_access;
        sdk_result = rin_gpu_process_resource_bind_v1(
            backend->compute_backend.submit_backend.device_id,
            backend->compute_backend.submit_backend.device_generation,
            &resource_binding);
        result = rinos_map_sdk_result(sdk_result);
        if (result != RIN_VULKAN_PRODUCT_OK) {
            memset(owned, 0, sizeof(*owned));
            goto rollback;
        }
        owned->active = 1u;
        sdk_bindings[source->resource_index].struct_size =
            sizeof(sdk_bindings[source->resource_index]);
        sdk_bindings[source->resource_index].version =
            RIN_GPU_PROCESS_COMPUTE_BINDING_VERSION_V1;
        sdk_bindings[source->resource_index].resource_cookie = resource_cookie;
        sdk_bindings[source->resource_index].offset = source->offset;
        sdk_bindings[source->resource_index].size_bytes = source->size_bytes;
        sdk_bindings[source->resource_index].access = sdk_access;
    }
    result = rinos_map_sdk_result(rin_gpu_process_compute_bind_group_create_v1(
        backend->compute_backend.submit_backend.device_id,
        backend->compute_backend.submit_backend.device_generation,
        bind_group_token, pipeline_token, sdk_bindings, binding_count));
    if (result == RIN_VULKAN_PRODUCT_OK) {
        group->bind_group_active = 1u;
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_OK;
    }

rollback:
    {
        const int original_result = result;
        const int cleanup_result =
            rinos_process_v3_destroy_bind_group_locked(backend, group);
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return cleanup_result == RIN_VULKAN_PRODUCT_OK ? original_result
                                                       : cleanup_result;
    }
}

static int rinos_destroy_process_compute_bind_group_v3(
        void* context, uint64_t bind_group_token) {
    RinVulkanRinOSProcessSubmitBackendV3* backend =
        (RinVulkanRinOSProcessSubmitBackendV3*)context;
    RinVulkanRinOSProcessComputeBindGroupV3* group;
    int result;
    if (!backend || bind_group_token == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_process_v3_header_valid(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    group = rinos_process_v3_find_bind_group(backend, bind_group_token);
    if (!group) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    result = rinos_process_v3_destroy_bind_group_locked(backend, group);
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return result;
}

int rin_vulkan_rinos_process_submit_backend_v3_init(
        RinVulkanRinOSProcessSubmitBackendV3* backend,
        RinVulkanProductPlatformV5* parent, uint64_t device_id,
        uint64_t device_generation, const uint32_t* queue_ids,
        uint32_t queue_count) {
    int result;
    if (!backend || !parent ||
        !rinos_range_valid(backend, sizeof(*backend)) ||
        rinos_ranges_overlap(backend, sizeof(*backend), parent,
                             sizeof(*parent)) ||
        backend->struct_size != 0u || backend->version != 0u ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_NONE ||
        backend->reserved[0] != 0u || backend->reserved[1] != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(backend, 0, sizeof(*backend));
    backend->struct_size = sizeof(*backend);
    backend->version = RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_VERSION;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    result = rin_vulkan_rinos_process_submit_backend_v2_init(
        &backend->compute_backend, parent, device_id, device_generation,
        queue_ids, queue_count);
    if (result != RIN_VULKAN_PRODUCT_OK) return result;
    backend->platform_v7.struct_size = sizeof(backend->platform_v7);
    backend->platform_v7.version = RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION;
    backend->platform_v7.base =
        &backend->compute_backend.submit_backend.platform;
    backend->platform_v7.context = backend;
    backend->platform_v7.create_process_compute_pipeline =
        rinos_create_process_compute_pipeline_v3;
    backend->platform_v7.destroy_process_compute_pipeline =
        rinos_destroy_process_compute_pipeline_v3;
    backend->platform_v8.struct_size = sizeof(backend->platform_v8);
    backend->platform_v8.version = RIN_VULKAN_PRODUCT_PLATFORM_V8_VERSION;
    backend->platform_v8.base = &backend->platform_v7;
    backend->platform_v8.context = backend;
    backend->platform_v8.create_process_compute_bind_group =
        rinos_create_process_compute_bind_group_v3;
    backend->platform_v8.destroy_process_compute_bind_group =
        rinos_destroy_process_compute_bind_group_v3;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE;
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_submit_backend_v3_valid(
        const RinVulkanRinOSProcessSubmitBackendV3* backend) {
    return backend && __atomic_load_n(&backend->lock, __ATOMIC_ACQUIRE) == 0u &&
           backend->state == RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
           rinos_process_v3_header_valid(backend);
}

RinVulkanProductPlatformV7*
rin_vulkan_rinos_process_submit_backend_v3_platform_v7(
        RinVulkanRinOSProcessSubmitBackendV3* backend) {
    return rin_vulkan_rinos_process_submit_backend_v3_valid(backend)
               ? &backend->platform_v7
               : NULL;
}

RinVulkanProductPlatformV8*
rin_vulkan_rinos_process_submit_backend_v3_platform_v8(
        RinVulkanRinOSProcessSubmitBackendV3* backend) {
    return rin_vulkan_rinos_process_submit_backend_v3_valid(backend)
               ? &backend->platform_v8
               : NULL;
}

int rin_vulkan_rinos_process_submit_backend_v3_detach(
        RinVulkanRinOSProcessSubmitBackendV3* backend) {
    uint32_t index;
    int result;
    if (!backend || !rinos_try_lock(&backend->lock))
        return backend ? RIN_VULKAN_PRODUCT_BUSY
                       : RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version != RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_VERSION ||
        (backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
         backend->state !=
             RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    if (backend->compute_backend.struct_size == sizeof(backend->compute_backend) &&
        backend->compute_backend.version ==
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V2_VERSION) {
        for (index = 0u;
             index <
                 RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V3_MAX_COMPUTE_BIND_GROUPS;
             ++index) {
            RinVulkanRinOSProcessComputeBindGroupV3* group =
                &backend->bind_groups[index];
            if (group->bind_group_token == 0u) continue;
            result = rinos_process_v3_destroy_bind_group_locked(backend, group);
            if (result != RIN_VULKAN_PRODUCT_OK) {
                __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
                return result;
            }
        }
    }
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    if (backend->compute_backend.struct_size == 0u) {
        memset(backend, 0, sizeof(*backend));
        return RIN_VULKAN_PRODUCT_OK;
    }
    result = rin_vulkan_rinos_process_submit_backend_v2_detach(
        &backend->compute_backend);
    if (result != RIN_VULKAN_PRODUCT_OK) return result;
    memset(backend, 0, sizeof(*backend));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_v4_mapping_valid(
        const RinVulkanRinOSProcessSubmitBackendV4* backend) {
    uint32_t index;
    uint32_t other;
    uint32_t physical_queue_count = 0u;
    if (!backend || backend->mapping_count == 0u ||
        backend->mapping_count >
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_MAX_QUEUE_MAPPINGS)
        return 0;
    for (index = 0u; index < backend->mapping_count; ++index) {
        uint32_t physical_seen = 0u;
        for (other = 0u; other < index; ++other) {
            if (backend->mappings[other].logical_queue_id ==
                backend->mappings[index].logical_queue_id)
                return 0;
            if (backend->mappings[other].physical_queue_id ==
                backend->mappings[index].physical_queue_id)
                physical_seen = 1u;
        }
        if (!physical_seen) ++physical_queue_count;
    }
    if (backend->compute_backend.compute_backend.submit_backend.queue_count !=
        physical_queue_count)
        return 0;
    for (index = 0u; index < backend->mapping_count; ++index) {
        uint32_t queue_index;
        int found = 0;
        const RinVulkanRinOSProcessSubmitBackendV1* submit_backend =
            &backend->compute_backend.compute_backend.submit_backend;
        for (queue_index = 0u; queue_index < submit_backend->queue_count;
             ++queue_index) {
            if (submit_backend->queues[queue_index].queue_id ==
                    backend->mappings[index].physical_queue_id &&
                submit_backend->queues[queue_index].active == 1u &&
                submit_backend->queues[queue_index].token != 0u) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

static int rinos_submit_process_commands_v4(
        void* context, uint32_t logical_queue_id,
        const RinGpuBackendCommandV1* commands, uint32_t command_count,
        const RinVulkanProductResourceV1* resources, uint32_t resource_count,
        RinVulkanProductProcessSubmitReceiptV1* receipt_out) {
    RinVulkanRinOSProcessSubmitBackendV4* backend =
        (RinVulkanRinOSProcessSubmitBackendV4*)context;
    RinVulkanProductProcessSubmitReceiptV1 physical_receipt;
    RinVulkanProductSubmitProcessCommandsV6Fn submit;
    uint32_t index;
    uint32_t physical_queue_id = 0u;
    int found = 0;
    int result;

    if (!backend || backend != context || !receipt_out ||
        !rinos_range_valid(receipt_out, sizeof(*receipt_out)) ||
        command_count == 0u ||
        command_count > RIN_GPU_PROCESS_MAX_COMMANDS_V1 ||
        resource_count > RIN_GPU_PROCESS_MAX_RESOURCES_V1 ||
        !rinos_range_valid(commands, sizeof(*commands) * command_count) ||
        (resource_count != 0u &&
         (!resources || !rinos_range_valid(
                            resources, sizeof(*resources) * resource_count))) ||
        rinos_ranges_overlap(backend, sizeof(*backend), receipt_out,
                             sizeof(*receipt_out)) ||
        rinos_ranges_overlap(receipt_out, sizeof(*receipt_out), commands,
                             sizeof(*commands) * command_count) ||
        (resource_count != 0u &&
         rinos_ranges_overlap(receipt_out, sizeof(*receipt_out), resources,
                              sizeof(*resources) * resource_count)) ||
        rinos_ranges_overlap(backend, sizeof(*backend), commands,
                             sizeof(*commands) * command_count) ||
        (resource_count != 0u &&
         rinos_ranges_overlap(backend, sizeof(*backend), resources,
                              sizeof(*resources) * resource_count)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;

    memset(receipt_out, 0, sizeof(*receipt_out));
    receipt_out->struct_size = sizeof(*receipt_out);
    receipt_out->version =
        RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version !=
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_VERSION ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE ||
        !rinos_v4_mapping_valid(backend)) {
        result = backend->state == RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST
                     ? RIN_VULKAN_PRODUCT_LOST
                     : RIN_VULKAN_PRODUCT_STATE;
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return result;
    }
    for (index = 0u; index < backend->mapping_count; ++index) {
        if (backend->mappings[index].logical_queue_id == logical_queue_id) {
            physical_queue_id = backend->mappings[index].physical_queue_id;
            found = 1;
            break;
        }
    }
    if (!found) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }

    submit = backend->compute_backend.compute_backend.submit_backend
                 .platform.submit_process_commands;
    memset(&physical_receipt, 0, sizeof(physical_receipt));
    result = submit(
        backend->compute_backend.compute_backend.submit_backend.platform
            .context,
        physical_queue_id, commands, command_count, resources, resource_count,
        &physical_receipt);
    if (result == RIN_VULKAN_PRODUCT_OK) {
        if (physical_receipt.struct_size != sizeof(physical_receipt) ||
            physical_receipt.version !=
                RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION ||
            physical_receipt.queue_id != physical_queue_id ||
            physical_receipt.sequence == 0u ||
            physical_receipt.completion_value == 0u ||
            physical_receipt.device_epoch == 0u ||
            physical_receipt.iommu_map_generation == 0u ||
            physical_receipt.reserved0 != 0u ||
            physical_receipt.reserved[0] != 0u ||
            physical_receipt.reserved[1] != 0u) {
            backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST;
            result = RIN_VULKAN_PRODUCT_PROTOCOL;
        } else {
            *receipt_out = physical_receipt;
            receipt_out->queue_id = logical_queue_id;
        }
    }
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return result;
}

static int rinos_v4_platform_valid(
        const RinVulkanRinOSProcessSubmitBackendV4* backend) {
    const RinVulkanProductPlatformV6* physical_v6;
    const RinVulkanProductPlatformV7* physical_v7;
    const RinVulkanProductPlatformV8* physical_v8;
    if (!backend || !rinos_v4_mapping_valid(backend) ||
        !rin_vulkan_rinos_process_submit_backend_v3_valid(
            &backend->compute_backend))
        return 0;
    physical_v6 =
        &backend->compute_backend.compute_backend.submit_backend.platform;
    physical_v7 = &backend->compute_backend.platform_v7;
    physical_v8 = &backend->compute_backend.platform_v8;
    return backend->platform_v6.struct_size == sizeof(backend->platform_v6) &&
           backend->platform_v6.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V6_VERSION &&
           backend->platform_v6.base == physical_v6->base &&
           backend->platform_v6.context == backend &&
           backend->platform_v6.submit_process_commands ==
               rinos_submit_process_commands_v4 &&
           backend->platform_v6.reserved[0] == 0u &&
           backend->platform_v6.reserved[1] == 0u &&
           backend->platform_v7.struct_size == sizeof(backend->platform_v7) &&
           backend->platform_v7.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION &&
           backend->platform_v7.base == &backend->platform_v6 &&
           backend->platform_v7.context == physical_v7->context &&
           backend->platform_v7.create_process_compute_pipeline ==
               physical_v7->create_process_compute_pipeline &&
           backend->platform_v7.destroy_process_compute_pipeline ==
               physical_v7->destroy_process_compute_pipeline &&
           backend->platform_v7.reserved[0] == 0u &&
           backend->platform_v7.reserved[1] == 0u &&
           backend->platform_v8.struct_size == sizeof(backend->platform_v8) &&
           backend->platform_v8.version ==
               RIN_VULKAN_PRODUCT_PLATFORM_V8_VERSION &&
           backend->platform_v8.base == &backend->platform_v7 &&
           backend->platform_v8.context == physical_v8->context &&
           backend->platform_v8.create_process_compute_bind_group ==
               physical_v8->create_process_compute_bind_group &&
           backend->platform_v8.destroy_process_compute_bind_group ==
               physical_v8->destroy_process_compute_bind_group &&
           backend->platform_v8.reserved[0] == 0u &&
           backend->platform_v8.reserved[1] == 0u &&
           backend->reserved0 == 0u && backend->reserved[0] == 0u &&
           backend->reserved[1] == 0u;
}

int rin_vulkan_rinos_process_submit_backend_v4_init(
        RinVulkanRinOSProcessSubmitBackendV4* backend,
        RinVulkanProductPlatformV5* parent, uint64_t device_id,
        uint64_t device_generation,
        const RinVulkanRinOSProcessQueueMapV4* mappings,
        uint32_t mapping_count) {
    uint32_t physical_queue_ids[
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_MAX_QUEUE_MAPPINGS];
    uint32_t physical_queue_count = 0u;
    uint32_t index;
    int result;

    if (!backend || !parent || !mappings || mapping_count == 0u ||
        mapping_count >
            RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_MAX_QUEUE_MAPPINGS ||
        !rinos_range_valid(backend, sizeof(*backend)) ||
        !rinos_range_valid(parent, sizeof(*parent)) ||
        !rinos_range_valid(mappings, sizeof(*mappings) * mapping_count) ||
        rinos_ranges_overlap(backend, sizeof(*backend), parent,
                             sizeof(*parent)) ||
        rinos_ranges_overlap(backend, sizeof(*backend), mappings,
                             sizeof(*mappings) * mapping_count) ||
        backend->struct_size != 0u || backend->version != 0u ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_NONE ||
        backend->lock != 0u || backend->reserved0 != 0u ||
        backend->reserved[0] != 0u || backend->reserved[1] != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;

    for (index = 0u; index < mapping_count; ++index) {
        uint32_t previous;
        uint32_t found = 0u;
        for (previous = 0u; previous < index; ++previous) {
            if (mappings[previous].logical_queue_id ==
                mappings[index].logical_queue_id)
                return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
            if (mappings[previous].physical_queue_id ==
                mappings[index].physical_queue_id)
                found = 1u;
        }
        if (!found)
            physical_queue_ids[physical_queue_count++] =
                mappings[index].physical_queue_id;
    }

    memset(backend, 0, sizeof(*backend));
    __atomic_store_n(&backend->lock, 1u, __ATOMIC_RELAXED);
    backend->struct_size = sizeof(*backend);
    backend->version = RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_VERSION;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    backend->mapping_count = mapping_count;
    memcpy(backend->mappings, mappings, sizeof(*mappings) * mapping_count);
    result = rin_vulkan_rinos_process_submit_backend_v3_init(
        &backend->compute_backend, parent, device_id, device_generation,
        physical_queue_ids, physical_queue_count);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return result;
    }

    backend->platform_v6 =
        backend->compute_backend.compute_backend.submit_backend.platform;
    backend->platform_v6.context = backend;
    backend->platform_v6.submit_process_commands =
        rinos_submit_process_commands_v4;
    backend->platform_v7 = backend->compute_backend.platform_v7;
    backend->platform_v7.base = &backend->platform_v6;
    backend->platform_v8 = backend->compute_backend.platform_v8;
    backend->platform_v8.base = &backend->platform_v7;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_submit_backend_v4_valid(
        const RinVulkanRinOSProcessSubmitBackendV4* backend) {
    return backend && __atomic_load_n(&backend->lock, __ATOMIC_ACQUIRE) == 0u &&
           backend->struct_size == sizeof(*backend) &&
           backend->version ==
               RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_VERSION &&
           backend->state == RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
           backend->reserved0 == 0u && backend->reserved[0] == 0u &&
           backend->reserved[1] == 0u && rinos_v4_platform_valid(backend);
}

RinVulkanProductPlatformV7*
rin_vulkan_rinos_process_submit_backend_v4_platform_v7(
        RinVulkanRinOSProcessSubmitBackendV4* backend) {
    return rin_vulkan_rinos_process_submit_backend_v4_valid(backend)
               ? &backend->platform_v7
               : NULL;
}

RinVulkanProductPlatformV8*
rin_vulkan_rinos_process_submit_backend_v4_platform_v8(
        RinVulkanRinOSProcessSubmitBackendV4* backend) {
    return rin_vulkan_rinos_process_submit_backend_v4_valid(backend)
               ? &backend->platform_v8
               : NULL;
}

int rin_vulkan_rinos_process_submit_backend_v4_detach(
        RinVulkanRinOSProcessSubmitBackendV4* backend) {
    int result;
    if (!backend || !rinos_try_lock(&backend->lock))
        return backend ? RIN_VULKAN_PRODUCT_BUSY
                       : RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version != RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V4_VERSION ||
        (backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
         backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING &&
         backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_LOST)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    if (backend->compute_backend.struct_size == 0u) {
        memset(backend, 0, sizeof(*backend));
        return RIN_VULKAN_PRODUCT_OK;
    }
    result = rin_vulkan_rinos_process_submit_backend_v3_detach(
        &backend->compute_backend);
    if (result != RIN_VULKAN_PRODUCT_OK) return result;
    memset(backend, 0, sizeof(*backend));
    return RIN_VULKAN_PRODUCT_OK;
}

static int rinos_v5_create_process_query(
    void* context, uint64_t query_token, uint32_t query_type);
static int rinos_v5_destroy_process_query(void* context,
                                          uint64_t query_token);
static int rinos_v5_get_process_query_result(
    void* context, uint64_t query_token,
    RinVulkanProductProcessQueryResultV9* result_out);

static int rinos_v5_valid_unlocked(
        const RinVulkanRinOSProcessSubmitBackendV5* backend) {
    uint32_t index;
    uint32_t active_count = 0u;
    const RinVulkanProductPlatformV8* platform_v8;
    if (!backend || backend->struct_size != sizeof(*backend) ||
        backend->version != RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_VERSION ||
        backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE ||
        backend->reserved[0] != 0u || backend->reserved[1] != 0u ||
        !rin_vulkan_rinos_process_submit_backend_v4_valid(
            &backend->submit_backend))
        return 0;
    platform_v8 = rin_vulkan_rinos_process_submit_backend_v4_platform_v8(
        (RinVulkanRinOSProcessSubmitBackendV4*)&backend->submit_backend);
    if (!platform_v8 || backend->platform_v9.struct_size !=
                            sizeof(backend->platform_v9) ||
        backend->platform_v9.version !=
            RIN_VULKAN_PRODUCT_PLATFORM_V9_VERSION ||
        backend->platform_v9.base != platform_v8 ||
        backend->platform_v9.context != backend ||
        backend->platform_v9.create_process_query !=
            rinos_v5_create_process_query ||
        backend->platform_v9.destroy_process_query !=
            rinos_v5_destroy_process_query ||
        backend->platform_v9.get_process_query_result !=
            rinos_v5_get_process_query_result ||
        backend->platform_v9.reserved[0] != 0u ||
        backend->platform_v9.reserved[1] != 0u)
        return 0;
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES;
         ++index) {
        const RinVulkanRinOSProcessQueryV5* query = &backend->queries[index];
        uint32_t prior;
        if (query->active == 0u) {
            if (query->query_token != 0u || query->reserved != 0u) return 0;
            continue;
        }
        if (query->active != 1u || query->query_token == 0u ||
            query->reserved != 0u)
            return 0;
        ++active_count;
        for (prior = 0u; prior < index; ++prior)
            if (backend->queries[prior].active == 1u &&
                backend->queries[prior].query_token == query->query_token)
                return 0;
    }
    return active_count <=
           RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES;
}

static int rinos_v5_query_find(
        RinVulkanRinOSProcessSubmitBackendV5* backend, uint64_t query_token,
        uint32_t* index_out) {
    uint32_t index;
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES;
         ++index) {
        if (backend->queries[index].active == 1u &&
            backend->queries[index].query_token == query_token) {
            if (index_out) *index_out = index;
            return 1;
        }
    }
    return 0;
}

static int rinos_v5_create_process_query(
        void* context, uint64_t query_token, uint32_t query_type) {
    RinVulkanRinOSProcessSubmitBackendV5* backend =
        (RinVulkanRinOSProcessSubmitBackendV5*)context;
    RinGpuProcessQueryDescV1 descriptor;
    uint32_t free_index = RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES;
    uint32_t index;
    RinResult sdk_result;
    if (!backend || query_token == 0u ||
        (query_type != RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1 &&
         query_type != RIN_GPU_PROCESS_QUERY_OCCLUSION_V1 &&
         query_type != RIN_GPU_PROCESS_QUERY_PIPELINE_STATISTICS_V1))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_v5_valid_unlocked(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    if (rinos_v5_query_find(backend, query_token, NULL)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_BUSY;
    }
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES;
         ++index)
        if (backend->queries[index].active == 0u) {
            free_index = index;
            break;
        }
    if (free_index ==
        RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_LIMIT;
    }
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.struct_size = sizeof(descriptor);
    descriptor.version = RIN_GPU_PROCESS_QUERY_DESC_VERSION_V1;
    descriptor.query_type = query_type;
    sdk_result = rin_gpu_process_query_create_v1(
        backend->submit_backend.compute_backend.compute_backend.submit_backend
            .device_id,
        backend->submit_backend.compute_backend.compute_backend.submit_backend
            .device_generation,
        query_token, &descriptor);
    if (sdk_result == RIN_SUCCESS) {
        backend->queries[free_index].query_token = query_token;
        backend->queries[free_index].reserved = 0u;
        backend->queries[free_index].active = 1u;
    }
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return rinos_map_sdk_result(sdk_result);
}

static int rinos_v5_destroy_process_query(void* context,
                                         uint64_t query_token) {
    RinVulkanRinOSProcessSubmitBackendV5* backend =
        (RinVulkanRinOSProcessSubmitBackendV5*)context;
    uint32_t index;
    RinResult sdk_result;
    if (!backend || query_token == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_v5_valid_unlocked(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    if (!rinos_v5_query_find(backend, query_token, &index)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    sdk_result = rin_gpu_process_object_destroy_v1(
        backend->submit_backend.compute_backend.compute_backend.submit_backend
            .device_id,
        backend->submit_backend.compute_backend.compute_backend.submit_backend
            .device_generation,
        query_token);
    if (sdk_result == RIN_SUCCESS)
        memset(&backend->queries[index], 0, sizeof(backend->queries[index]));
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    return rinos_map_sdk_result(sdk_result);
}

static int rinos_v5_get_process_query_result(
        void* context, uint64_t query_token,
        RinVulkanProductProcessQueryResultV9* result_out) {
    RinVulkanRinOSProcessSubmitBackendV5* backend =
        (RinVulkanRinOSProcessSubmitBackendV5*)context;
    RinGpuProcessQueryResultV1 sdk_result_data;
    RinResult sdk_result;
    if (!backend || query_token == 0u || !result_out ||
        !rinos_range_valid(result_out, sizeof(*result_out)))
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(result_out, 0, sizeof(*result_out));
    result_out->struct_size = sizeof(*result_out);
    result_out->version =
        RIN_VULKAN_PRODUCT_PROCESS_QUERY_RESULT_V9_VERSION;
    if (!rinos_try_lock(&backend->lock)) return RIN_VULKAN_PRODUCT_BUSY;
    if (!rinos_v5_valid_unlocked(backend)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    if (!rinos_v5_query_find(backend, query_token, NULL)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    }
    memset(&sdk_result_data, 0, sizeof(sdk_result_data));
    sdk_result_data.struct_size = sizeof(sdk_result_data);
    sdk_result_data.version = RIN_GPU_PROCESS_QUERY_RESULT_VERSION_V1;
    sdk_result = rin_gpu_process_query_result_v1(
        backend->submit_backend.compute_backend.compute_backend.submit_backend
            .device_id,
        backend->submit_backend.compute_backend.compute_backend.submit_backend
            .device_generation,
        query_token, &sdk_result_data);
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    if (sdk_result != RIN_SUCCESS) return rinos_map_sdk_result(sdk_result);
    if (sdk_result_data.struct_size != sizeof(sdk_result_data) ||
        sdk_result_data.version != RIN_GPU_PROCESS_QUERY_RESULT_VERSION_V1 ||
        sdk_result_data.available > 1u || sdk_result_data.reserved0 != 0u ||
        sdk_result_data.reserved1 != 0u)
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    result_out->query_type = sdk_result_data.query_type;
    result_out->available = sdk_result_data.available;
    memcpy(result_out->values, sdk_result_data.values,
           sizeof(result_out->values));
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_submit_backend_v5_init(
        RinVulkanRinOSProcessSubmitBackendV5* backend,
        RinVulkanProductPlatformV5* parent, uint64_t device_id,
        uint64_t device_generation,
        const RinVulkanRinOSProcessQueueMapV4* mappings,
        uint32_t mapping_count) {
    int result;
    if (!backend || !parent || device_id == 0u || device_generation == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    memset(backend, 0, sizeof(*backend));
    __atomic_store_n(&backend->lock, 1u, __ATOMIC_RELAXED);
    backend->struct_size = sizeof(*backend);
    backend->version = RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_VERSION;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    result = rin_vulkan_rinos_process_submit_backend_v4_init(
        &backend->submit_backend, parent, device_id, device_generation,
        mappings, mapping_count);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return result;
    }
    backend->platform_v9.struct_size = sizeof(backend->platform_v9);
    backend->platform_v9.version = RIN_VULKAN_PRODUCT_PLATFORM_V9_VERSION;
    backend->platform_v9.base =
        rin_vulkan_rinos_process_submit_backend_v4_platform_v8(
            &backend->submit_backend);
    backend->platform_v9.context = backend;
    backend->platform_v9.create_process_query =
        rinos_v5_create_process_query;
    backend->platform_v9.destroy_process_query =
        rinos_v5_destroy_process_query;
    backend->platform_v9.get_process_query_result =
        rinos_v5_get_process_query_result;
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE;
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    if (!rin_vulkan_rinos_process_submit_backend_v5_valid(backend)) {
        backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    }
    return RIN_VULKAN_PRODUCT_OK;
}

int rin_vulkan_rinos_process_submit_backend_v5_valid(
        const RinVulkanRinOSProcessSubmitBackendV5* backend) {
    return backend && __atomic_load_n(&backend->lock, __ATOMIC_ACQUIRE) == 0u &&
           rinos_v5_valid_unlocked(backend);
}

RinVulkanProductPlatformV9*
rin_vulkan_rinos_process_submit_backend_v5_platform_v9(
        RinVulkanRinOSProcessSubmitBackendV5* backend) {
    return rin_vulkan_rinos_process_submit_backend_v5_valid(backend)
               ? &backend->platform_v9
               : NULL;
}

int rin_vulkan_rinos_process_submit_backend_v5_detach(
        RinVulkanRinOSProcessSubmitBackendV5* backend) {
    uint32_t index;
    int first_error = RIN_VULKAN_PRODUCT_OK;
    int result;
    if (!backend || !rinos_try_lock(&backend->lock))
        return backend ? RIN_VULKAN_PRODUCT_BUSY
                       : RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (backend->struct_size != sizeof(*backend) ||
        backend->version != RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_VERSION ||
        (backend->state != RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_ACTIVE &&
         backend->state !=
             RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING)) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return RIN_VULKAN_PRODUCT_STATE;
    }
    backend->state = RIN_VULKAN_RINOS_PROCESS_BACKEND_STATE_CLEANUP_PENDING;
    for (index = 0u;
         index < RIN_VULKAN_RINOS_PROCESS_SUBMIT_BACKEND_V5_MAX_QUERIES;
         ++index) {
        RinVulkanRinOSProcessQueryV5* query = &backend->queries[index];
        RinResult sdk_result;
        if (query->active == 0u) continue;
        if (query->active != 1u || query->query_token == 0u ||
            query->reserved != 0u) {
            if (first_error == RIN_VULKAN_PRODUCT_OK)
                first_error = RIN_VULKAN_PRODUCT_PROTOCOL;
            continue;
        }
        sdk_result = rin_gpu_process_object_destroy_v1(
            backend->submit_backend.compute_backend.compute_backend
                .submit_backend.device_id,
            backend->submit_backend.compute_backend.compute_backend
                .submit_backend.device_generation,
            query->query_token);
        if (sdk_result == RIN_SUCCESS)
            memset(query, 0, sizeof(*query));
        else if (first_error == RIN_VULKAN_PRODUCT_OK)
            first_error = rinos_map_sdk_result(sdk_result);
    }
    if (first_error != RIN_VULKAN_PRODUCT_OK) {
        __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
        return first_error;
    }
    __atomic_store_n(&backend->lock, 0u, __ATOMIC_RELEASE);
    result = rin_vulkan_rinos_process_submit_backend_v4_detach(
        &backend->submit_backend);
    if (result != RIN_VULKAN_PRODUCT_OK) return result;
    memset(backend, 0, sizeof(*backend));
    return RIN_VULKAN_PRODUCT_OK;
}
