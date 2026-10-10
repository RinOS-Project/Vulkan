/* SPDX-License-Identifier: MIT */
#include <rinvulkan/rinos_process_submit_backend_v1.h>

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
