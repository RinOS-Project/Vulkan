/* SPDX-License-Identifier: MIT */
#include <rinvulkan/rinos_process_submit_backend_v1.h>

#include <stdio.h>
#include <string.h>

static RinResult bind_result;
static RinResult release_result;
static RinResult submit_result;
static RinGpuProcessQueueV1 fail_release_token;
static RinGpuProcessSubmitReceiptV1 supplied_receipt;
static uint32_t bind_calls;
static uint32_t fail_bind_queue_id;
static uint32_t release_calls;
static uint32_t submit_calls;
static uint32_t observed_queue_id;
static RinGpuProcessQueueV1 observed_queue_token;
static const RinGpuBackendCommandV1* observed_commands;
static uint32_t observed_command_count;
static RinGpuProcessResourceV1 observed_resources[
    RIN_GPU_PROCESS_MAX_RESOURCES_V1];
static uint32_t observed_resource_count;

RinResult rin_gpu_process_queue_bind_v1(
    uint64_t device_id, uint64_t device_generation, uint32_t queue_id,
    RinGpuProcessQueueV1* queue_out) {
    ++bind_calls;
    if (!queue_out || device_id != 0x1111u || device_generation != 0x2222u)
        return RIN_ERROR_INVALID_ARGUMENT;
    *queue_out = 0u;
    if (queue_id == fail_bind_queue_id) return RIN_ERROR_NOT_SUPPORTED;
    if (bind_result != RIN_SUCCESS) return bind_result;
    *queue_out = UINT64_C(0x9000) + queue_id;
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_queue_release_v1(RinGpuProcessQueueV1 queue) {
    ++release_calls;
    if (queue == 0u) return RIN_ERROR_INVALID_ARGUMENT;
    return queue == fail_release_token ? release_result : RIN_SUCCESS;
}

RinResult rin_gpu_process_submit_v1(
    RinGpuProcessQueueV1 queue,
    const RinGpuProcessSubmitDescV1* descriptor,
    RinGpuProcessSubmitReceiptV1* receipt_out) {
    ++submit_calls;
    if (!descriptor || !receipt_out ||
        descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V1 ||
        descriptor->command_record_size != sizeof(RinGpuBackendCommandV1))
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_queue_token = queue;
    observed_queue_id = (uint32_t)(queue - UINT64_C(0x9000));
    observed_commands = (const RinGpuBackendCommandV1*)
        (uintptr_t)descriptor->commands.address;
    observed_command_count = descriptor->command_count;
    observed_resource_count = descriptor->resource_count;
    if (descriptor->resource_count != 0u) {
        const RinGpuProcessResourceV1* source =
            (const RinGpuProcessResourceV1*)
                (uintptr_t)descriptor->resources.address;
        memcpy(observed_resources, source,
               descriptor->resource_count * sizeof(*source));
    }
    *receipt_out = supplied_receipt;
    return submit_result;
}

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, \
                    __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

static void reset_mocks(void) {
    bind_result = RIN_SUCCESS;
    fail_bind_queue_id = UINT32_MAX;
    release_result = RIN_SUCCESS;
    fail_release_token = 0u;
    submit_result = RIN_SUCCESS;
    memset(&supplied_receipt, 0, sizeof(supplied_receipt));
    supplied_receipt.struct_size = sizeof(supplied_receipt);
    supplied_receipt.version = RIN_GPU_PROCESS_SUBMIT_RECEIPT_VERSION_V1;
    supplied_receipt.queue_id = 2u;
    supplied_receipt.sequence = 17u;
    supplied_receipt.completion_value = 19u;
    supplied_receipt.device_epoch = 23u;
    supplied_receipt.iommu_map_generation = 29u;
    bind_calls = 0u;
    release_calls = 0u;
    submit_calls = 0u;
    observed_queue_id = 0u;
    observed_queue_token = 0u;
    observed_commands = NULL;
    observed_command_count = 0u;
    memset(observed_resources, 0, sizeof(observed_resources));
    observed_resource_count = 0u;
}

int main(void) {
    RinVulkanRinOSProcessSubmitBackendV1 backend = {0};
    RinVulkanProductPlatformV5 parent = {0};
    uint32_t queue_ids[2] = {0u, 2u};
    RinGpuBackendCommandV1 commands[2] = {{0}};
    RinVulkanProductResourceV1 resources[2] = {
        {UINT64_C(0xA001), RIN_GPU_ALLOCATION_GPU_READ, 0u},
        {UINT64_C(0xA002), RIN_GPU_ALLOCATION_GPU_WRITE, 0u}};
    RinVulkanProductProcessSubmitReceiptV1 receipt;
    RinVulkanProductPlatformV6* platform;

    reset_mocks();
    parent.struct_size = sizeof(parent);
    parent.version = RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION;
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_init(
              &backend, &parent, 0x1111u, 0x2222u, queue_ids, 2u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(bind_calls == 2u && backend.queue_count == 2u &&
          rin_vulkan_rinos_process_submit_backend_v1_valid(&backend));
    platform = rin_vulkan_rinos_process_submit_backend_v1_platform(&backend);
    CHECK(platform != NULL && platform->base == &parent &&
          platform->context == &backend &&
          platform->submit_process_commands != NULL);

    memset(&receipt, 0xA5, sizeof(receipt));
    CHECK(platform->submit_process_commands(
              platform->context, 2u, commands, 2u, resources, 2u,
              &receipt) == RIN_VULKAN_PRODUCT_OK);
    CHECK(submit_calls == 1u && observed_queue_token == 0x9002u &&
          observed_queue_id == 2u && observed_commands == commands &&
          observed_command_count == 2u && observed_resource_count == 2u &&
          observed_resources[0].allocation == UINT64_C(0xA001) &&
          observed_resources[0].required_gpu_access ==
              RIN_GPU_ALLOCATION_GPU_READ &&
          observed_resources[1].allocation == UINT64_C(0xA002) &&
          observed_resources[1].required_gpu_access ==
              RIN_GPU_ALLOCATION_GPU_WRITE);
    CHECK(receipt.struct_size == sizeof(receipt) &&
          receipt.version ==
              RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION &&
          receipt.queue_id == 2u && receipt.sequence == 17u &&
          receipt.completion_value == 19u && receipt.device_epoch == 23u &&
          receipt.iommu_map_generation == 29u);

    CHECK(platform->submit_process_commands(
              platform->context, 7u, commands, 1u, NULL, 0u, &receipt) ==
          RIN_VULKAN_PRODUCT_INVALID_ARGUMENT);
    CHECK(submit_calls == 1u);

    resources[0].reserved = 1u;
    CHECK(platform->submit_process_commands(
              platform->context, 0u, commands, 1u, resources, 1u,
              &receipt) == RIN_VULKAN_PRODUCT_INVALID_ARGUMENT);
    CHECK(submit_calls == 1u);
    resources[0].reserved = 0u;

    release_result = RIN_ERROR_BUSY;
    fail_release_token = UINT64_C(0x9000);
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_detach(&backend) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(backend.state == 2u && backend.queues[0].active == 1u &&
          backend.queues[1].active == 0u && release_calls == 2u);
    release_result = RIN_SUCCESS;
    fail_release_token = 0u;
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_detach(&backend) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend.state == 0u && release_calls == 3u);

    reset_mocks();
    memset(&backend, 0, sizeof(backend));
    fail_bind_queue_id = 2u;
    release_result = RIN_ERROR_BUSY;
    fail_release_token = UINT64_C(0x9000);
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_init(
              &backend, &parent, 0x1111u, 0x2222u, queue_ids, 2u) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(backend.state == 2u && backend.queues[0].active == 1u &&
          backend.queues[1].active == 0u && release_calls == 1u);
    release_result = RIN_SUCCESS;
    fail_release_token = 0u;
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_detach(&backend) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(release_calls == 2u);

    reset_mocks();
    memset(&backend, 0, sizeof(backend));
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_init(
              &backend, &parent, 0x1111u, 0x2222u, queue_ids, 2u) ==
          RIN_VULKAN_PRODUCT_OK);
    supplied_receipt.queue_id = 0u;
    CHECK(backend.platform.submit_process_commands(
              &backend, 2u, commands, 1u, NULL, 0u, &receipt) ==
          RIN_VULKAN_PRODUCT_PROTOCOL);
    CHECK(backend.state == 3u &&
          rin_vulkan_rinos_process_submit_backend_v1_platform(&backend) ==
              NULL);
    CHECK(rin_vulkan_rinos_process_submit_backend_v1_detach(&backend) ==
          RIN_VULKAN_PRODUCT_OK);

    puts("RinVulkan RinOS process-submit adapter: PASS");
    return 0;
}
