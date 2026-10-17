/* SPDX-License-Identifier: MIT */
#include <rinvulkan/rinos_process_submit_backend_v1.h>
#include <ringpu/rin_shader.h>

#include <stdio.h>
#include <string.h>

static RinResult bind_result;
static RinResult release_result;
static RinResult submit_result;
static RinResult shader_create_result;
static RinResult pipeline_create_result;
static RinResult graphics_pipeline_create_result;
static RinResult sampler_create_result;
static RinResult object_destroy_result;
static RinResult resource_bind_result;
static RinResult resource_unbind_result;
static RinResult bind_group_create_result;
static RinResult query_create_result;
static RinResult query_read_result;
static uint32_t fail_resource_bind_call;
static RinGpuProcessQueueV1 fail_release_token;
static RinGpuProcessSubmitReceiptV1 supplied_receipt;
static uint32_t bind_calls;
static uint32_t fail_bind_queue_id;
static uint32_t release_calls;
static uint32_t submit_calls;
static uint32_t observed_submit_version;
static uint32_t observed_queue_id;
static RinGpuProcessQueueV1 observed_queue_token;
static const RinGpuBackendCommandV1* observed_commands;
static uint32_t observed_command_count;
static RinGpuProcessResourceV1 observed_resources[
    RIN_GPU_PROCESS_MAX_RESOURCES_V1];
static uint32_t observed_resource_count;
static uint32_t shader_create_calls;
static uint32_t pipeline_create_calls;
static uint32_t graphics_pipeline_create_calls;
static uint32_t sampler_create_calls;
static uint32_t object_destroy_calls;
static uint32_t resource_bind_calls;
static uint32_t resource_unbind_calls;
static uint32_t bind_group_create_calls;
static RinGpuProcessResourceBindingV1 observed_resource_bindings[64];
static uint64_t active_resource_cookies[64];
static RinGpuProcessComputeBindingV1 observed_compute_bindings[64];
static uint32_t observed_compute_binding_count;
static uint64_t observed_bind_group_token;
static uint64_t observed_bind_group_pipeline_token;
static uint64_t observed_shader_token;
static uint64_t observed_pipeline_token;
static uint64_t observed_pipeline_shader_token;
static uint64_t observed_graphics_pipeline_token;
static uint64_t observed_graphics_vertex_token;
static uint64_t observed_graphics_fragment_token;
static RinGpuProcessGraphicsPipelineDescV1 observed_graphics_pipeline_desc;
static uint64_t observed_sampler_token;
static RinGpuProcessSamplerDescV1 observed_sampler_desc;
static uint64_t observed_destroy_tokens[8];
static uint32_t query_create_calls;
static uint32_t query_read_calls;
static uint64_t observed_query_token;
static RinGpuProcessQueryDescV1 observed_query_desc;
static RinGpuProcessQueryResultV1 supplied_query_result;

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
    observed_submit_version = RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V1;
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

RinResult rin_gpu_process_submit_v2(
    RinGpuProcessQueueV1 queue,
    const RinGpuProcessSubmitDescV2* descriptor,
    RinGpuProcessSubmitReceiptV1* receipt_out) {
    ++submit_calls;
    if (!descriptor || !receipt_out ||
        descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V2 ||
        descriptor->reserved[0] != 0u || descriptor->reserved[1] != 0u ||
        descriptor->command_count == 0u ||
        descriptor->command_record_size != sizeof(RinGpuBackendCommandV1) ||
        descriptor->commands.address == 0u ||
        descriptor->commands.size !=
            (uint64_t)descriptor->command_count *
                descriptor->command_record_size)
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_submit_version = RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V2;
    observed_queue_token = queue;
    observed_queue_id = (uint32_t)(queue - UINT64_C(0x9000));
    observed_commands = (const RinGpuBackendCommandV1*)
        (uintptr_t)descriptor->commands.address;
    observed_command_count = descriptor->command_count;
    observed_resource_count = 0u;
    *receipt_out = supplied_receipt;
    return submit_result;
}

RinResult rin_gpu_process_shader_module_create_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t shader_module_token, const void* rin_shader_ir,
    uint64_t shader_ir_size) {
    ++shader_create_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) ||
        shader_module_token == 0u || !rin_shader_ir ||
        shader_ir_size != sizeof(RinShaderHeaderV1))
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_shader_token = shader_module_token;
    return shader_create_result;
}

RinResult rin_gpu_process_compute_pipeline_create_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t pipeline_token, uint64_t shader_module_token) {
    ++pipeline_create_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || pipeline_token == 0u ||
        shader_module_token == 0u || pipeline_token == shader_module_token)
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_pipeline_token = pipeline_token;
    observed_pipeline_shader_token = shader_module_token;
    return pipeline_create_result;
}

RinResult rin_gpu_process_graphics_pipeline_create_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t pipeline_token, uint64_t vertex_shader_token,
    uint64_t fragment_shader_token,
    const RinGpuProcessGraphicsPipelineDescV1* descriptor) {
    ++graphics_pipeline_create_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || pipeline_token == 0u ||
        vertex_shader_token == 0u || fragment_shader_token == 0u ||
        pipeline_token == vertex_shader_token ||
        pipeline_token == fragment_shader_token ||
        vertex_shader_token == fragment_shader_token || !descriptor ||
        descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version !=
            RIN_GPU_PROCESS_GRAPHICS_PIPELINE_DESC_VERSION_V1 ||
        descriptor->color_format != RIN_GPU_FORMAT_RGBA8_UNORM ||
        descriptor->primitive_topology != RIN_GPU_PRIMITIVE_TRIANGLE_LIST)
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_graphics_pipeline_token = pipeline_token;
    observed_graphics_vertex_token = vertex_shader_token;
    observed_graphics_fragment_token = fragment_shader_token;
    observed_graphics_pipeline_desc = *descriptor;
    return graphics_pipeline_create_result;
}

RinResult rin_gpu_process_sampler_create_v1(
    uint64_t device_id, uint64_t device_generation, uint64_t sampler_token,
    const RinGpuProcessSamplerDescV1* descriptor) {
    ++sampler_create_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || sampler_token == 0u ||
        !descriptor || descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_SAMPLER_DESC_VERSION_V1)
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_sampler_token = sampler_token;
    observed_sampler_desc = *descriptor;
    return sampler_create_result;
}

RinResult rin_gpu_process_resource_bind_v1(
    uint64_t device_id, uint64_t device_generation,
    const RinGpuProcessResourceBindingV1* binding) {
    uint32_t index;
    ++resource_bind_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || !binding ||
        binding->struct_size != sizeof(*binding) ||
        binding->version != RIN_GPU_PROCESS_RESOURCE_BINDING_VERSION_V1 ||
        binding->resource_cookie == 0u || binding->allocation == 0u ||
        binding->size_bytes == 0u || binding->required_gpu_access == 0u ||
        binding->reserved != 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    if (resource_bind_calls <
        sizeof(observed_resource_bindings) /
            sizeof(observed_resource_bindings[0]))
        observed_resource_bindings[resource_bind_calls - 1u] = *binding;
    if (resource_bind_calls == fail_resource_bind_call)
        return RIN_ERROR_NOT_SUPPORTED;
    if (resource_bind_result != RIN_SUCCESS) return resource_bind_result;
    for (index = 0u; index <
         sizeof(active_resource_cookies) / sizeof(active_resource_cookies[0]);
         ++index) {
        if (active_resource_cookies[index] == binding->resource_cookie)
            return RIN_ERROR_BUSY;
        if (active_resource_cookies[index] == 0u) {
            active_resource_cookies[index] = binding->resource_cookie;
            return RIN_SUCCESS;
        }
    }
    return RIN_ERROR_TABLE_FULL;
}

RinResult rin_gpu_process_resource_unbind_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t resource_cookie) {
    uint32_t index;
    ++resource_unbind_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || resource_cookie == 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    if (resource_unbind_result != RIN_SUCCESS) return resource_unbind_result;
    for (index = 0u; index <
         sizeof(active_resource_cookies) / sizeof(active_resource_cookies[0]);
         ++index) {
        if (active_resource_cookies[index] == resource_cookie) {
            active_resource_cookies[index] = 0u;
            return RIN_SUCCESS;
        }
    }
    return RIN_ERROR_STALE_HANDLE;
}

RinResult rin_gpu_process_compute_bind_group_create_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t bind_group_token, uint64_t pipeline_token,
    const RinGpuProcessComputeBindingV1* bindings, uint32_t binding_count) {
    ++bind_group_create_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || bind_group_token == 0u ||
        pipeline_token == 0u || !bindings || binding_count == 0u ||
        binding_count >
            sizeof(observed_compute_bindings) /
                sizeof(observed_compute_bindings[0]))
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_bind_group_token = bind_group_token;
    observed_bind_group_pipeline_token = pipeline_token;
    observed_compute_binding_count = binding_count;
    memcpy(observed_compute_bindings, bindings,
           binding_count * sizeof(*bindings));
    return bind_group_create_result;
}

RinResult rin_gpu_process_object_destroy_v1(
    uint64_t device_id, uint64_t device_generation, uint64_t object_token) {
    if (object_destroy_calls <
        sizeof(observed_destroy_tokens) / sizeof(observed_destroy_tokens[0]))
        observed_destroy_tokens[object_destroy_calls] = object_token;
    ++object_destroy_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || object_token == 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    return object_destroy_result;
}

RinResult rin_gpu_process_query_create_v1(
    uint64_t device_id, uint64_t device_generation, uint64_t query_token,
    const RinGpuProcessQueryDescV1* descriptor) {
    ++query_create_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) || query_token == 0u ||
        !descriptor || descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_QUERY_DESC_VERSION_V1 ||
        descriptor->flags != 0u || descriptor->reserved0 != 0u ||
        descriptor->reserved1 != 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    observed_query_token = query_token;
    observed_query_desc = *descriptor;
    return query_create_result;
}

RinResult rin_gpu_process_query_result_v1(
    uint64_t device_id, uint64_t device_generation, uint64_t query_token,
    RinGpuProcessQueryResultV1* result_out) {
    ++query_read_calls;
    if (device_id != UINT64_C(0x1111) ||
        device_generation != UINT64_C(0x2222) ||
        query_token != observed_query_token || !result_out ||
        result_out->struct_size != sizeof(*result_out) ||
        result_out->version != RIN_GPU_PROCESS_QUERY_RESULT_VERSION_V1)
        return RIN_ERROR_INVALID_ARGUMENT;
    if (query_read_result != RIN_SUCCESS) return query_read_result;
    *result_out = supplied_query_result;
    return RIN_SUCCESS;
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
    shader_create_result = RIN_SUCCESS;
    pipeline_create_result = RIN_SUCCESS;
    graphics_pipeline_create_result = RIN_SUCCESS;
    sampler_create_result = RIN_SUCCESS;
    object_destroy_result = RIN_SUCCESS;
    resource_bind_result = RIN_SUCCESS;
    resource_unbind_result = RIN_SUCCESS;
    bind_group_create_result = RIN_SUCCESS;
    query_create_result = RIN_SUCCESS;
    query_read_result = RIN_SUCCESS;
    fail_resource_bind_call = 0u;
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
    observed_submit_version = 0u;
    observed_queue_id = 0u;
    observed_queue_token = 0u;
    observed_commands = NULL;
    observed_command_count = 0u;
    memset(observed_resources, 0, sizeof(observed_resources));
    observed_resource_count = 0u;
    shader_create_calls = 0u;
    pipeline_create_calls = 0u;
    graphics_pipeline_create_calls = 0u;
    sampler_create_calls = 0u;
    object_destroy_calls = 0u;
    resource_bind_calls = 0u;
    resource_unbind_calls = 0u;
    bind_group_create_calls = 0u;
    memset(observed_resource_bindings, 0,
           sizeof(observed_resource_bindings));
    memset(active_resource_cookies, 0, sizeof(active_resource_cookies));
    memset(observed_compute_bindings, 0, sizeof(observed_compute_bindings));
    observed_compute_binding_count = 0u;
    observed_bind_group_token = 0u;
    observed_bind_group_pipeline_token = 0u;
    observed_shader_token = 0u;
    observed_pipeline_token = 0u;
    observed_pipeline_shader_token = 0u;
    observed_graphics_pipeline_token = 0u;
    observed_graphics_vertex_token = 0u;
    observed_graphics_fragment_token = 0u;
    memset(&observed_graphics_pipeline_desc, 0,
           sizeof(observed_graphics_pipeline_desc));
    observed_sampler_token = 0u;
    memset(&observed_sampler_desc, 0, sizeof(observed_sampler_desc));
    memset(observed_destroy_tokens, 0, sizeof(observed_destroy_tokens));
    query_create_calls = 0u;
    query_read_calls = 0u;
    observed_query_token = 0u;
    memset(&observed_query_desc, 0, sizeof(observed_query_desc));
    memset(&supplied_query_result, 0, sizeof(supplied_query_result));
    supplied_query_result.struct_size = sizeof(supplied_query_result);
    supplied_query_result.version = RIN_GPU_PROCESS_QUERY_RESULT_VERSION_V1;
    supplied_query_result.query_type = RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1;
}

int main(void) {
    RinVulkanRinOSProcessSubmitBackendV1 backend = {0};
    RinVulkanRinOSProcessSubmitBackendV2 backend_v2 = {0};
    RinVulkanRinOSProcessSubmitBackendV3 backend_v3 = {0};
    RinVulkanRinOSProcessSubmitBackendV4 backend_v4 = {0};
    RinVulkanRinOSProcessSubmitBackendV5 backend_v5 = {0};
    RinVulkanRinOSProcessSubmitBackendV6 backend_v6 = {0};
    RinVulkanRinOSProcessSubmitBackendV7 backend_v7 = {0};
    RinVulkanProductPlatformV5 parent = {0};
    uint32_t queue_ids[2] = {0u, 2u};
    RinVulkanRinOSProcessQueueMapV4 queue_mappings[3] = {
        {0u, 2u}, {1u, 0u}, {2u, 2u}};
    RinGpuBackendCommandV1 commands[2] = {{0}};
    RinVulkanProductResourceV1 resources[2] = {
        {UINT64_C(0xA001), RIN_GPU_ALLOCATION_GPU_READ, 0u},
        {UINT64_C(0xA002), RIN_GPU_ALLOCATION_GPU_WRITE, 0u}};
    RinVulkanProductProcessSubmitReceiptV1 receipt;
    RinVulkanProductPlatformV6* platform;
    RinVulkanProductPlatformV7* platform_v7;
    RinVulkanProductPlatformV7* platform_v7_v3;
    RinVulkanProductPlatformV8* platform_v8;
    RinVulkanProductPlatformV7* platform_v7_v4;
    RinVulkanProductPlatformV8* platform_v8_v4;
    RinVulkanProductPlatformV9* platform_v9;
    RinVulkanProductPlatformV10* platform_v10;
    RinVulkanProductPlatformV11* platform_v11;
    RinVulkanProductProcessComputeBindingV8 process_bindings[2] = {{0}};
    uint8_t shader_ir[sizeof(RinShaderHeaderV1)] = {0};
    uint64_t process_pipeline_token = 0u;
    const uint64_t pipeline_cookie = UINT64_C(0x5258000100000001);
    const uint64_t shader_token = UINT64_C(0x5260000100000001);

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
    CHECK(submit_calls == 1u &&
          observed_submit_version == RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V2 &&
          observed_queue_token == 0x9002u && observed_queue_id == 2u &&
          observed_commands == commands && observed_command_count == 2u &&
          observed_resource_count == 0u);
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

    reset_mocks();
    CHECK(rin_vulkan_rinos_process_submit_backend_v2_init(
              &backend_v2, &parent, 0x1111u, 0x2222u, queue_ids, 2u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v2_valid(&backend_v2) &&
          bind_calls == 2u);
    platform_v7 =
        rin_vulkan_rinos_process_submit_backend_v2_platform(&backend_v2);
    CHECK(platform_v7 != NULL &&
          platform_v7->version == RIN_VULKAN_PRODUCT_PLATFORM_V7_VERSION &&
          platform_v7->base == &backend_v2.submit_backend.platform &&
          platform_v7->create_process_compute_pipeline != NULL &&
          platform_v7->destroy_process_compute_pipeline != NULL);
    CHECK(platform_v7->create_process_compute_pipeline(
              platform_v7->context, pipeline_cookie, shader_ir,
              sizeof(shader_ir), &process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(process_pipeline_token == pipeline_cookie &&
          shader_create_calls == 1u && pipeline_create_calls == 1u &&
          observed_shader_token == shader_token &&
          observed_pipeline_token == pipeline_cookie &&
          observed_pipeline_shader_token == shader_token &&
          backend_v2.compute_pipelines[0].pipeline_active == 1u &&
          backend_v2.compute_pipelines[0].shader_module_active == 1u);

    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(platform_v7->destroy_process_compute_pipeline(
              platform_v7->context, process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(object_destroy_calls == 1u &&
          observed_destroy_tokens[0] == pipeline_cookie &&
          backend_v2.compute_pipelines[0].pipeline_active == 1u &&
          backend_v2.compute_pipelines[0].shader_module_active == 1u);
    object_destroy_result = RIN_SUCCESS;
    CHECK(platform_v7->destroy_process_compute_pipeline(
              platform_v7->context, process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(object_destroy_calls == 3u &&
          observed_destroy_tokens[1] == pipeline_cookie &&
          observed_destroy_tokens[2] == shader_token &&
          backend_v2.compute_pipelines[0].pipeline_token == 0u &&
          rin_vulkan_rinos_process_submit_backend_v2_valid(&backend_v2));

    pipeline_create_result = RIN_ERROR_NOT_SUPPORTED;
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(platform_v7->create_process_compute_pipeline(
              platform_v7->context, UINT64_C(0x5258000100000002), shader_ir,
              sizeof(shader_ir), &process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(process_pipeline_token == 0u && shader_create_calls == 2u &&
          pipeline_create_calls == 2u && object_destroy_calls == 4u &&
          observed_destroy_tokens[3] == UINT64_C(0x5260000100000002) &&
          rin_vulkan_rinos_process_submit_backend_v2_valid(&backend_v2));
    object_destroy_result = RIN_SUCCESS;
    CHECK(rin_vulkan_rinos_process_submit_backend_v2_detach(&backend_v2) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v2.state == 0u && object_destroy_calls == 5u &&
          observed_destroy_tokens[4] == UINT64_C(0x5260000100000002) &&
          release_calls == 2u);

    reset_mocks();
    CHECK(rin_vulkan_rinos_process_submit_backend_v3_init(
              &backend_v3, &parent, 0x1111u, 0x2222u, queue_ids, 2u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v3_valid(&backend_v3));
    platform_v7_v3 =
        rin_vulkan_rinos_process_submit_backend_v3_platform_v7(&backend_v3);
    platform_v8 =
        rin_vulkan_rinos_process_submit_backend_v3_platform_v8(&backend_v3);
    CHECK(platform_v7_v3 && platform_v8 &&
          platform_v8->base == platform_v7_v3 &&
          platform_v7_v3->base == &backend_v3.compute_backend
                                         .submit_backend.platform &&
          platform_v8->create_process_compute_bind_group != NULL &&
          platform_v8->destroy_process_compute_bind_group != NULL);
    CHECK(platform_v7_v3->create_process_compute_pipeline(
              platform_v7_v3->context, UINT64_C(0x5258000100000101),
              shader_ir, sizeof(shader_ir), &process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(process_pipeline_token == UINT64_C(0x5258000100000101));
    process_bindings[0].struct_size = sizeof(process_bindings[0]);
    process_bindings[0].version =
        RIN_VULKAN_PRODUCT_PROCESS_COMPUTE_BINDING_V8_VERSION;
    process_bindings[0].resource_index = 0u;
    process_bindings[0].access = RIN_GPU_RESOURCE_READ;
    process_bindings[0].resource_cookie = UINT64_C(0x5242000100000001);
    process_bindings[0].allocation_handle = UINT64_C(0xA001);
    process_bindings[0].allocation_offset = 128u;
    process_bindings[0].resource_size_bytes = 1024u;
    process_bindings[0].offset = 16u;
    process_bindings[0].size_bytes = 128u;
    process_bindings[1].struct_size = sizeof(process_bindings[1]);
    process_bindings[1].version =
        RIN_VULKAN_PRODUCT_PROCESS_COMPUTE_BINDING_V8_VERSION;
    process_bindings[1].resource_index = 1u;
    process_bindings[1].access = RIN_GPU_RESOURCE_READ | RIN_GPU_RESOURCE_WRITE;
    process_bindings[1].resource_cookie = UINT64_C(0x5242000100000002);
    process_bindings[1].allocation_handle = UINT64_C(0xA002);
    process_bindings[1].allocation_offset = 256u;
    process_bindings[1].resource_size_bytes = 2048u;
    process_bindings[1].offset = 32u;
    process_bindings[1].size_bytes = 256u;
    CHECK(platform_v8->create_process_compute_bind_group(
              platform_v8->context, UINT64_C(0x5262000100000101),
              process_pipeline_token, process_bindings, 2u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(resource_bind_calls == 2u && bind_group_create_calls == 1u &&
          active_resource_cookies[0] != 0u &&
          active_resource_cookies[1] != 0u &&
          active_resource_cookies[0] != active_resource_cookies[1] &&
          (active_resource_cookies[0] >> 48u) ==
              RIN_VULKAN_RINOS_PROCESS_RESOURCE_COOKIE_TAG &&
          observed_resource_bindings[0].allocation == UINT64_C(0xA001) &&
          observed_resource_bindings[0].allocation_offset == 128u &&
          observed_resource_bindings[0].size_bytes == 1024u &&
          observed_resource_bindings[0].required_gpu_access ==
              RIN_GPU_ALLOCATION_GPU_READ &&
          observed_resource_bindings[1].allocation == UINT64_C(0xA002) &&
          observed_resource_bindings[1].required_gpu_access ==
              (RIN_GPU_ALLOCATION_GPU_READ | RIN_GPU_ALLOCATION_GPU_WRITE) &&
          observed_bind_group_token == UINT64_C(0x5262000100000101) &&
          observed_bind_group_pipeline_token == process_pipeline_token &&
          observed_compute_binding_count == 2u &&
          observed_compute_bindings[0].resource_cookie ==
              active_resource_cookies[0] &&
          observed_compute_bindings[0].offset == 16u &&
          observed_compute_bindings[0].size_bytes == 128u &&
          observed_compute_bindings[0].access ==
              RIN_GPU_ALLOCATION_GPU_READ &&
          observed_compute_bindings[1].resource_cookie ==
              active_resource_cookies[1] &&
          observed_compute_bindings[1].offset == 32u &&
          observed_compute_bindings[1].size_bytes == 256u &&
          observed_compute_bindings[1].access ==
              (RIN_GPU_ALLOCATION_GPU_READ | RIN_GPU_ALLOCATION_GPU_WRITE));
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(platform_v8->destroy_process_compute_bind_group(
              platform_v8->context, UINT64_C(0x5262000100000101)) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(resource_unbind_calls == 0u &&
          backend_v3.bind_groups[0].bind_group_active == 1u &&
          rin_vulkan_rinos_process_submit_backend_v3_valid(&backend_v3));
    object_destroy_result = RIN_SUCCESS;
    CHECK(platform_v8->destroy_process_compute_bind_group(
              platform_v8->context, UINT64_C(0x5262000100000101)) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(resource_unbind_calls == 2u && active_resource_cookies[0] == 0u &&
          active_resource_cookies[1] == 0u &&
          backend_v3.bind_groups[0].bind_group_token == 0u);
    CHECK(platform_v7_v3->destroy_process_compute_pipeline(
              platform_v7_v3->context, process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v3_detach(&backend_v3) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v3.state == 0u && active_resource_cookies[0] == 0u &&
          active_resource_cookies[1] == 0u && release_calls == 2u);

    reset_mocks();
    memset(&backend_v3, 0, sizeof(backend_v3));
    CHECK(rin_vulkan_rinos_process_submit_backend_v3_init(
              &backend_v3, &parent, 0x1111u, 0x2222u, queue_ids, 2u) ==
          RIN_VULKAN_PRODUCT_OK);
    platform_v7_v3 =
        rin_vulkan_rinos_process_submit_backend_v3_platform_v7(&backend_v3);
    platform_v8 =
        rin_vulkan_rinos_process_submit_backend_v3_platform_v8(&backend_v3);
    CHECK(platform_v7_v3 && platform_v8);
    CHECK(platform_v7_v3->create_process_compute_pipeline(
              platform_v7_v3->context, UINT64_C(0x5258000100000102),
              shader_ir, sizeof(shader_ir), &process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_OK);
    fail_resource_bind_call = 2u;
    CHECK(platform_v8->create_process_compute_bind_group(
              platform_v8->context, UINT64_C(0x5262000100000102),
              process_pipeline_token, process_bindings, 2u) ==
          RIN_VULKAN_PRODUCT_UNSUPPORTED);
    CHECK(resource_bind_calls == 2u && resource_unbind_calls == 1u &&
          bind_group_create_calls == 0u && active_resource_cookies[0] == 0u &&
          backend_v3.bind_groups[0].bind_group_token == 0u);
    CHECK(platform_v7_v3->destroy_process_compute_pipeline(
              platform_v7_v3->context, process_pipeline_token) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v3_detach(&backend_v3) ==
          RIN_VULKAN_PRODUCT_OK);

    reset_mocks();
    queue_mappings[1].logical_queue_id = 0u;
    CHECK(rin_vulkan_rinos_process_submit_backend_v4_init(
              &backend_v4, &parent, 0x1111u, 0x2222u, queue_mappings, 3u) ==
          RIN_VULKAN_PRODUCT_INVALID_ARGUMENT);
    CHECK(bind_calls == 0u && backend_v4.struct_size == 0u);
    queue_mappings[1].logical_queue_id = 1u;
    CHECK(rin_vulkan_rinos_process_submit_backend_v4_init(
              &backend_v4, &parent, 0x1111u, 0x2222u, queue_mappings, 3u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(bind_calls == 2u && backend_v4.mapping_count == 3u &&
          rin_vulkan_rinos_process_submit_backend_v4_valid(&backend_v4));
    platform_v7_v4 =
        rin_vulkan_rinos_process_submit_backend_v4_platform_v7(&backend_v4);
    platform_v8_v4 =
        rin_vulkan_rinos_process_submit_backend_v4_platform_v8(&backend_v4);
    CHECK(platform_v7_v4 && platform_v8_v4 &&
          platform_v8_v4->base == platform_v7_v4 &&
          platform_v7_v4->base == &backend_v4.platform_v6 &&
          backend_v4.platform_v6.base == &parent &&
          backend_v4.platform_v6.context == &backend_v4 &&
          platform_v8_v4->create_process_compute_bind_group != NULL);
    commands[0].type = RIN_GPU_BACKEND_COMMAND_COPY_BUFFER;
    commands[0].value.buffer_copy.source_cookie = UINT64_C(0xA11CE001);
    commands[0].value.buffer_copy.source_offset = 32u;
    commands[0].value.buffer_copy.destination_cookie = UINT64_C(0xA11CE002);
    commands[0].value.buffer_copy.destination_offset = 64u;
    commands[0].value.buffer_copy.size_bytes = 128u;
    CHECK(backend_v4.platform_v6.submit_process_commands(
              backend_v4.platform_v6.context, 0u, commands, 1u, NULL, 0u,
              &receipt) == RIN_VULKAN_PRODUCT_OK);
    CHECK(submit_calls == 1u && observed_queue_token == UINT64_C(0x9002) &&
          observed_queue_id == 2u &&
          observed_submit_version == RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V2 &&
          observed_resource_count == 0u && observed_commands == commands &&
          observed_commands[0].type == RIN_GPU_BACKEND_COMMAND_COPY_BUFFER &&
          observed_commands[0].value.buffer_copy.source_cookie ==
              UINT64_C(0xA11CE001) &&
          observed_commands[0].value.buffer_copy.source_offset == 32u &&
          observed_commands[0].value.buffer_copy.destination_cookie ==
              UINT64_C(0xA11CE002) &&
          observed_commands[0].value.buffer_copy.destination_offset == 64u &&
          observed_commands[0].value.buffer_copy.size_bytes == 128u &&
          receipt.queue_id == 0u &&
          receipt.sequence == 17u);
    memset(&commands[0], 0, sizeof(commands[0]));
    supplied_receipt.queue_id = 0u;
    CHECK(backend_v4.platform_v6.submit_process_commands(
              backend_v4.platform_v6.context, 1u, commands, 1u, NULL, 0u,
              &receipt) == RIN_VULKAN_PRODUCT_OK);
    CHECK(submit_calls == 2u && observed_queue_token == UINT64_C(0x9000) &&
          observed_queue_id == 0u && receipt.queue_id == 1u);
    supplied_receipt.queue_id = 2u;
    CHECK(backend_v4.platform_v6.submit_process_commands(
              backend_v4.platform_v6.context, 2u, commands, 1u, NULL, 0u,
              &receipt) == RIN_VULKAN_PRODUCT_OK);
    CHECK(submit_calls == 3u && observed_queue_token == UINT64_C(0x9002) &&
          observed_queue_id == 2u && receipt.queue_id == 2u);
    CHECK(backend_v4.platform_v6.submit_process_commands(
              backend_v4.platform_v6.context, 3u, commands, 1u, NULL, 0u,
              &receipt) == RIN_VULKAN_PRODUCT_INVALID_ARGUMENT);
    CHECK(submit_calls == 3u);
    release_result = RIN_ERROR_BUSY;
    fail_release_token = UINT64_C(0x9002);
    CHECK(rin_vulkan_rinos_process_submit_backend_v4_detach(&backend_v4) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(backend_v4.state == 2u &&
          backend_v4.compute_backend.compute_backend.submit_backend.queues[0]
                  .active == 1u);
    CHECK(backend_v4.platform_v6.submit_process_commands(
              backend_v4.platform_v6.context, 0u, commands, 1u, NULL, 0u,
              &receipt) == RIN_VULKAN_PRODUCT_STATE);
    CHECK(submit_calls == 3u);
    release_result = RIN_SUCCESS;
    fail_release_token = 0u;
    CHECK(rin_vulkan_rinos_process_submit_backend_v4_detach(&backend_v4) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v4.state == 0u && release_calls >= 3u);

    reset_mocks();
    CHECK(rin_vulkan_rinos_process_submit_backend_v5_init(
              &backend_v5, &parent, 0x1111u, 0x2222u,
              queue_mappings, 3u) == RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v5_valid(&backend_v5));
    platform_v9 = rin_vulkan_rinos_process_submit_backend_v5_platform_v9(
        &backend_v5);
    CHECK(platform_v9 && platform_v9->base ==
              &backend_v5.submit_backend.platform_v8 &&
          platform_v9->create_process_query &&
          platform_v9->destroy_process_query &&
          platform_v9->get_process_query_result);
    CHECK(platform_v9->create_process_query(
              platform_v9->context, UINT64_C(0x5263000100000201),
              RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1) == RIN_VULKAN_PRODUCT_OK);
    CHECK(query_create_calls == 1u &&
          observed_query_token == UINT64_C(0x5263000100000201) &&
          observed_query_desc.query_type ==
              RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1 &&
          backend_v5.queries[0].active == 1u);
    supplied_query_result.available = 0u;
    supplied_query_result.values[0] = 0u;
    {
        RinVulkanProductProcessQueryResultV9 query_result = {0};
        CHECK(platform_v9->get_process_query_result(
                  platform_v9->context, observed_query_token,
                  &query_result) == RIN_VULKAN_PRODUCT_OK);
        CHECK(query_read_calls == 1u && query_result.struct_size ==
                  sizeof(query_result) && query_result.version ==
                  RIN_VULKAN_PRODUCT_PROCESS_QUERY_RESULT_V9_VERSION &&
              query_result.query_type ==
                  RIN_VULKAN_PRODUCT_QUERY_TYPE_TIMESTAMP_V9 &&
              query_result.available == 0u && query_result.values[0] == 0u);
        supplied_query_result.available = 1u;
        supplied_query_result.values[0] = UINT64_C(0x123456789);
        CHECK(platform_v9->get_process_query_result(
                  platform_v9->context, observed_query_token,
                  &query_result) == RIN_VULKAN_PRODUCT_OK);
        CHECK(query_read_calls == 2u && query_result.available == 1u &&
              query_result.values[0] == UINT64_C(0x123456789));
    }
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(platform_v9->destroy_process_query(
              platform_v9->context, observed_query_token) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(rin_vulkan_rinos_process_submit_backend_v5_valid(&backend_v5) &&
          backend_v5.queries[0].active == 1u);
    object_destroy_result = RIN_SUCCESS;
    CHECK(platform_v9->destroy_process_query(
              platform_v9->context, observed_query_token) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v5.queries[0].active == 0u &&
          backend_v5.queries[0].query_token == 0u);

    CHECK(platform_v9->create_process_query(
              platform_v9->context, UINT64_C(0x5263000100000202),
              RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1) == RIN_VULKAN_PRODUCT_OK);
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(rin_vulkan_rinos_process_submit_backend_v5_detach(&backend_v5) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(backend_v5.state == 2u && backend_v5.queries[0].active == 1u);
    object_destroy_result = RIN_SUCCESS;
    CHECK(rin_vulkan_rinos_process_submit_backend_v5_detach(&backend_v5) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v5.state == 0u && release_calls == 2u);

    reset_mocks();
    CHECK(rin_vulkan_rinos_process_submit_backend_v6_init(
              &backend_v6, &parent, 0x1111u, 0x2222u, queue_mappings, 3u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v6_valid(&backend_v6));
    platform_v10 =
        rin_vulkan_rinos_process_submit_backend_v6_platform_v10(&backend_v6);
    CHECK(platform_v10 && platform_v10->base ==
              &backend_v6.submit_backend.platform_v9 &&
          platform_v10->create_process_graphics_pipeline &&
          platform_v10->destroy_process_graphics_pipeline);
    CHECK(platform_v10->create_process_graphics_pipeline(
              platform_v10->context, pipeline_cookie, shader_ir,
              sizeof(shader_ir), shader_ir, sizeof(shader_ir),
              RIN_GPU_FORMAT_RGBA8_UNORM,
              RIN_GPU_PRIMITIVE_TRIANGLE_LIST) == RIN_VULKAN_PRODUCT_OK);
    CHECK(shader_create_calls == 2u && graphics_pipeline_create_calls == 1u &&
          observed_graphics_pipeline_token == pipeline_cookie &&
          observed_graphics_vertex_token ==
              ((RIN_VULKAN_RINOS_PROCESS_GRAPHICS_VERTEX_TOKEN_TAG << 48u) |
               (pipeline_cookie & UINT64_C(0x0000FFFFFFFFFFFF))) &&
          observed_graphics_fragment_token ==
              ((RIN_VULKAN_RINOS_PROCESS_GRAPHICS_FRAGMENT_TOKEN_TAG << 48u) |
               (pipeline_cookie & UINT64_C(0x0000FFFFFFFFFFFF))) &&
          observed_graphics_pipeline_desc.struct_size ==
              sizeof(observed_graphics_pipeline_desc) &&
          backend_v6.graphics_pipelines[0].pipeline_active == 1u);
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(platform_v10->destroy_process_graphics_pipeline(
              platform_v10->context, pipeline_cookie) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(backend_v6.graphics_pipelines[0].pipeline_active == 1u);
    object_destroy_result = RIN_SUCCESS;
    CHECK(platform_v10->destroy_process_graphics_pipeline(
              platform_v10->context, pipeline_cookie) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(object_destroy_calls == 4u &&
          observed_destroy_tokens[1] == pipeline_cookie &&
          observed_destroy_tokens[2] == observed_graphics_fragment_token &&
          observed_destroy_tokens[3] == observed_graphics_vertex_token &&
          backend_v6.graphics_pipelines[0].pipeline_token == 0u);

    graphics_pipeline_create_result = RIN_ERROR_NOT_SUPPORTED;
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(platform_v10->create_process_graphics_pipeline(
              platform_v10->context, pipeline_cookie + 1u, shader_ir,
              sizeof(shader_ir), shader_ir, sizeof(shader_ir),
              RIN_GPU_FORMAT_RGBA8_UNORM,
              RIN_GPU_PRIMITIVE_TRIANGLE_LIST) == RIN_VULKAN_PRODUCT_BUSY);
    CHECK(graphics_pipeline_create_calls == 2u &&
          backend_v6.graphics_pipelines[0].pipeline_token ==
              pipeline_cookie + 1u &&
          backend_v6.graphics_pipelines[0].pipeline_active == 0u &&
          backend_v6.graphics_pipelines[0].fragment_shader_active == 1u &&
          backend_v6.graphics_pipelines[0].vertex_shader_active == 1u &&
          rin_vulkan_rinos_process_submit_backend_v6_valid(&backend_v6) &&
          observed_destroy_tokens[4] != 0u &&
          observed_destroy_tokens[5] != 0u);

    graphics_pipeline_create_result = RIN_SUCCESS;
    object_destroy_result = RIN_SUCCESS;
    CHECK(platform_v10->create_process_graphics_pipeline(
              platform_v10->context, pipeline_cookie + 2u, shader_ir,
              sizeof(shader_ir), shader_ir, sizeof(shader_ir),
              RIN_GPU_FORMAT_RGBA8_UNORM,
              RIN_GPU_PRIMITIVE_TRIANGLE_LIST) == RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v6.graphics_pipelines[0].pipeline_token ==
              pipeline_cookie + 2u &&
          backend_v6.graphics_pipelines[0].pipeline_active == 1u &&
          observed_destroy_tokens[6] ==
              ((RIN_VULKAN_RINOS_PROCESS_GRAPHICS_FRAGMENT_TOKEN_TAG << 48u) |
               ((pipeline_cookie + 1u) & UINT64_C(0x0000FFFFFFFFFFFF))) &&
          observed_destroy_tokens[7] ==
              ((RIN_VULKAN_RINOS_PROCESS_GRAPHICS_VERTEX_TOKEN_TAG << 48u) |
               ((pipeline_cookie + 1u) & UINT64_C(0x0000FFFFFFFFFFFF))));
    object_destroy_result = RIN_ERROR_BUSY;
    CHECK(rin_vulkan_rinos_process_submit_backend_v6_detach(&backend_v6) ==
          RIN_VULKAN_PRODUCT_BUSY);
    CHECK(backend_v6.state == 2u &&
          backend_v6.graphics_pipelines[0].pipeline_active == 1u);
    object_destroy_result = RIN_SUCCESS;
    CHECK(rin_vulkan_rinos_process_submit_backend_v6_detach(&backend_v6) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(backend_v6.state == 0u && release_calls == 2u);

    reset_mocks();
    CHECK(rin_vulkan_rinos_process_submit_backend_v7_init(
              &backend_v7, &parent, 0x1111u, 0x2222u, queue_mappings, 3u) ==
          RIN_VULKAN_PRODUCT_OK);
    CHECK(rin_vulkan_rinos_process_submit_backend_v7_valid(&backend_v7));
    platform_v11 =
        rin_vulkan_rinos_process_submit_backend_v7_platform_v11(&backend_v7);
    CHECK(platform_v11 &&
          platform_v11->base ==
              &backend_v7.submit_backend.platform_v10 &&
          platform_v11->create_process_sampler &&
          platform_v11->destroy_process_sampler);
    {
        RinVulkanProductSamplerDescV11 sampler_desc = {0};
        const uint64_t sampler_token = UINT64_C(0x5264000100000101);
        sampler_desc.struct_size = sizeof(sampler_desc);
        sampler_desc.version =
            RIN_VULKAN_PRODUCT_PROCESS_SAMPLER_DESC_V11_VERSION;
        sampler_desc.min_filter = 0u;
        sampler_desc.mag_filter = 1u;
        sampler_desc.mipmap_mode = 1u;
        sampler_desc.address_mode_u = 0u;
        sampler_desc.address_mode_v = 1u;
        sampler_desc.address_mode_w = 2u;
        sampler_desc.compare_enable = 1u;
        sampler_desc.compare_op = 6u;
        CHECK(platform_v11->create_process_sampler(
                  platform_v11->context, sampler_token, &sampler_desc) ==
              RIN_VULKAN_PRODUCT_OK);
        CHECK(sampler_create_calls == 1u &&
              observed_sampler_token == sampler_token &&
              observed_sampler_desc.min_filter ==
                  RIN_GPU_PROCESS_SAMPLER_FILTER_NEAREST_V1 &&
              observed_sampler_desc.mag_filter ==
                  RIN_GPU_PROCESS_SAMPLER_FILTER_LINEAR_V1 &&
              observed_sampler_desc.mip_filter ==
                  RIN_GPU_PROCESS_SAMPLER_FILTER_LINEAR_V1 &&
              observed_sampler_desc.address_u ==
                  RIN_GPU_PROCESS_SAMPLER_ADDRESS_REPEAT_V1 &&
              observed_sampler_desc.address_v ==
                  RIN_GPU_PROCESS_SAMPLER_ADDRESS_MIRRORED_REPEAT_V1 &&
              observed_sampler_desc.address_w ==
                  RIN_GPU_PROCESS_SAMPLER_ADDRESS_CLAMP_TO_EDGE_V1 &&
              observed_sampler_desc.compare_op ==
                  RIN_GPU_PROCESS_SAMPLER_COMPARE_GREATER_EQUAL_V1 &&
              observed_sampler_desc.max_anisotropy == 1u &&
              observed_sampler_desc.min_lod == 0.0f &&
              observed_sampler_desc.max_lod == 32.0f &&
              backend_v7.samplers[0].active == 1u);
        sampler_desc.address_mode_u = 3u;
        CHECK(platform_v11->create_process_sampler(
                  platform_v11->context, sampler_token + 1u,
                  &sampler_desc) == RIN_VULKAN_PRODUCT_UNSUPPORTED);
        CHECK(sampler_create_calls == 1u &&
              backend_v7.samplers[0].sampler_token == sampler_token);
        object_destroy_result = RIN_ERROR_BUSY;
        CHECK(platform_v11->destroy_process_sampler(
                  platform_v11->context, sampler_token) ==
              RIN_VULKAN_PRODUCT_BUSY);
        CHECK(backend_v7.samplers[0].active == 1u &&
              backend_v7.samplers[0].sampler_token == sampler_token);
        object_destroy_result = RIN_SUCCESS;
        CHECK(platform_v11->destroy_process_sampler(
                  platform_v11->context, sampler_token) ==
              RIN_VULKAN_PRODUCT_OK);
        CHECK(backend_v7.samplers[0].sampler_token == 0u);

        sampler_desc.address_mode_u = 0u;
        sampler_create_result = RIN_ERROR_NOT_SUPPORTED;
        CHECK(platform_v11->create_process_sampler(
                  platform_v11->context, sampler_token + 2u,
                  &sampler_desc) == RIN_VULKAN_PRODUCT_UNSUPPORTED);
        CHECK(backend_v7.samplers[0].sampler_token == 0u);
        sampler_create_result = RIN_SUCCESS;
        CHECK(platform_v11->create_process_sampler(
                  platform_v11->context, sampler_token + 3u,
                  &sampler_desc) == RIN_VULKAN_PRODUCT_OK);
        object_destroy_result = RIN_ERROR_BUSY;
        CHECK(rin_vulkan_rinos_process_submit_backend_v7_detach(&backend_v7) ==
              RIN_VULKAN_PRODUCT_BUSY);
        CHECK(backend_v7.state == 2u && backend_v7.samplers[0].active == 1u);
        object_destroy_result = RIN_SUCCESS;
        CHECK(rin_vulkan_rinos_process_submit_backend_v7_detach(&backend_v7) ==
              RIN_VULKAN_PRODUCT_OK);
        CHECK(backend_v7.state == 0u && release_calls == 2u);
    }

    puts("RinVulkan RinOS process-submit/compute/graphics/sampler adapter: PASS");
    return 0;
}
