/* SPDX-License-Identifier: MIT */

#include <rinvulkan/descriptor_runtime.h>

#include <string.h>

#define RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG UINT64_C(0x4c)
#define RIN_GPU_VULKAN_DESCRIPTOR_POOL_TAG UINT64_C(0x50)
#define RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG UINT64_C(0x53)

static int runtime_valid(const RinGpuVulkanDescriptorRuntimeV1* runtime) {
    return runtime && runtime->initialized == 1u &&
           runtime->handle_secret != 0u;
}

static RinGpuVulkanDescriptorHandleV1 make_handle(
        uint64_t tag, uint32_t index, uint32_t generation,
        uint64_t secret) {
    uint64_t raw = (tag << 56u) | ((uint64_t)generation << 24u) |
                   (uint64_t)(index + 1u);
    RinGpuVulkanDescriptorHandleV1 encoded = raw ^ secret;
    return encoded == 0u ? raw : encoded;
}

static int decode_handle(RinGpuVulkanDescriptorHandleV1 handle, uint64_t tag,
                         uint64_t secret, uint32_t capacity,
                         uint32_t* index_out, uint32_t* generation_out) {
    uint64_t raw;
    uint32_t index;
    if (handle == 0u || !index_out || !generation_out || secret == 0u)
        return 0;
    raw = handle ^ secret;
    if ((raw >> 56u) != tag) return 0;
    index = (uint32_t)(raw & UINT64_C(0xffffff));
    if (index == 0u || index > capacity || (uint32_t)(raw >> 24u) == 0u)
        return 0;
    *index_out = index - 1u;
    *generation_out = (uint32_t)(raw >> 24u);
    return 1;
}

static uint32_t next_generation(uint32_t generation) {
    return generation == UINT32_MAX ? 1u : generation + 1u;
}

static int descriptor_type_valid(uint32_t descriptor_type) {
    return descriptor_type >= RIN_GPU_VULKAN_DESCRIPTOR_UNIFORM_BUFFER &&
           descriptor_type <= RIN_GPU_VULKAN_DESCRIPTOR_COMPARISON_SAMPLER;
}

static int layout_binding_valid(
        const RinGpuVulkanDescriptorSetLayoutBindingV1* binding) {
    return binding && descriptor_type_valid(binding->descriptor_type) &&
           binding->set != UINT32_MAX && binding->binding != UINT32_MAX &&
           binding->descriptor_count != 0u &&
           binding->descriptor_count <= RIN_SHADER_MAX_RESOURCES &&
           binding->stage_flags != 0u &&
           (binding->reserved & ~RIN_GPU_VULKAN_DESCRIPTOR_BINDING_DYNAMIC) ==
               0u;
}

static int layout_binding_index(
        const RinGpuVulkanDescriptorRuntimeV1* runtime, uint32_t layout_index,
        uint32_t set, uint32_t binding, uint32_t* index_out) {
    uint32_t index;
    if (!runtime || layout_index >= RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS ||
        !index_out) return 0;
    for (index = 0u; index < runtime->layouts[layout_index].binding_count;
         ++index) {
        const RinGpuVulkanDescriptorSetLayoutBindingV1* candidate =
            &runtime->layouts[layout_index].bindings[index];
        if (candidate->set == set && candidate->binding == binding) {
            *index_out = index;
            return 1;
        }
    }
    return 0;
}

static int write_key_unique(
        const RinGpuVulkanDescriptorWriteV1* writes, uint32_t count,
        uint32_t index) {
    uint32_t prior;
    for (prior = 0u; prior < index; ++prior) {
        if (writes[prior].set == writes[index].set &&
            writes[prior].binding == writes[index].binding &&
            writes[prior].array_element == writes[index].array_element)
            return 0;
    }
    (void)count;
    return 1;
}

static const RinGpuVulkanDescriptorWriteV1* find_set_write(
        const RinGpuVulkanDescriptorRuntimeV1* runtime, uint32_t set_index,
        uint32_t set, uint32_t binding, uint32_t array_element) {
    uint32_t index;
    if (!runtime || set_index >= RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS)
        return NULL;
    for (index = 0u; index < runtime->sets[set_index].write_count; ++index) {
        const RinGpuVulkanDescriptorWriteV1* write =
            &runtime->sets[set_index].writes[index];
        if (write->set == set && write->binding == binding &&
            write->array_element == array_element)
            return write;
    }
    return NULL;
}

int rin_gpu_vulkan_descriptor_runtime_init(
        RinGpuVulkanDescriptorRuntimeV1* runtime, uint64_t handle_secret) {
    if (!runtime || handle_secret == 0u) return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    runtime->handle_secret = handle_secret;
    runtime->initialized = 1u;
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_runtime_shutdown(
        RinGpuVulkanDescriptorRuntimeV1* runtime) {
    uint32_t index;
    if (!runtime_valid(runtime)) return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS; ++index)
        if (runtime->sets[index].state != 0u)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS; ++index)
        if (runtime->pools[index].state != 0u)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS; ++index)
        if (runtime->layouts[index].state != 0u)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    memset(runtime, 0, sizeof(*runtime));
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_runtime_is_empty(
        const RinGpuVulkanDescriptorRuntimeV1* runtime) {
    uint32_t index;
    if (!runtime_valid(runtime)) return 0;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS; ++index)
        if (runtime->sets[index].state != 0u) return 0;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS; ++index)
        if (runtime->pools[index].state != 0u) return 0;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS; ++index)
        if (runtime->layouts[index].state != 0u) return 0;
    return 1;
}

int rin_gpu_vulkan_descriptor_layout_create(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        const RinGpuVulkanDescriptorSetLayoutBindingV1* bindings,
        uint32_t binding_count, RinGpuVulkanDescriptorHandleV1* layout_out) {
    uint32_t slot_index;
    if (!layout_out || !runtime_valid(runtime) || binding_count == 0u ||
        binding_count > RIN_SHADER_MAX_RESOURCES || !bindings)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    *layout_out = 0u;
    for (slot_index = 0u; slot_index < binding_count; ++slot_index) {
        uint32_t prior;
        if (!layout_binding_valid(&bindings[slot_index]))
            return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
        for (prior = 0u; prior < slot_index; ++prior)
            if (bindings[prior].set == bindings[slot_index].set &&
                bindings[prior].binding == bindings[slot_index].binding)
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    }
    for (slot_index = 0u; slot_index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS;
         ++slot_index) {
        if (runtime->layouts[slot_index].state != 0u) continue;
        runtime->layout_generation = next_generation(runtime->layout_generation);
        runtime->layouts[slot_index].generation = runtime->layout_generation;
        runtime->layouts[slot_index].binding_count = binding_count;
        memcpy(runtime->layouts[slot_index].bindings, bindings,
               sizeof(*bindings) * binding_count);
        runtime->layouts[slot_index].state = 1u;
        *layout_out = make_handle(RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                                  slot_index, runtime->layout_generation,
                                  runtime->handle_secret);
        return RIN_GPU_VULKAN_GRAPHICS_OK;
    }
    return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
}

int rin_gpu_vulkan_descriptor_layout_destroy(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 layout) {
    uint32_t index;
    uint32_t generation;
    uint32_t set_index;
    if (!runtime_valid(runtime) ||
        !decode_handle(layout, RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS, &index,
                       &generation) || runtime->layouts[index].state == 0u ||
        runtime->layouts[index].generation != generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    for (set_index = 0u; set_index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS;
         ++set_index)
        if (runtime->sets[set_index].state != 0u &&
            runtime->sets[set_index].layout == layout)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    memset(&runtime->layouts[index], 0, sizeof(runtime->layouts[index]));
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_pool_create_v2(
        RinGpuVulkanDescriptorRuntimeV1* runtime, uint32_t max_sets,
        const uint32_t* descriptor_limits, uint32_t descriptor_limit_count,
        RinGpuVulkanDescriptorHandleV1* pool_out) {
    uint32_t index;
    if (!pool_out || !runtime_valid(runtime) || max_sets == 0u ||
        max_sets > RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS ||
        descriptor_limit_count > 8u ||
        (descriptor_limit_count != 0u && !descriptor_limits))
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    *pool_out = 0u;
    for (index = 0u; index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS; ++index) {
        if (runtime->pools[index].state != 0u) continue;
        runtime->pool_generation = next_generation(runtime->pool_generation);
        runtime->pools[index].generation = runtime->pool_generation;
        runtime->pools[index].max_sets = max_sets;
        runtime->pools[index].live_sets = 0u;
        runtime->pools[index].has_limits = descriptor_limit_count != 0u;
        if (descriptor_limit_count != 0u)
            memcpy(runtime->pools[index].descriptor_limits,
                   descriptor_limits,
                   sizeof(*descriptor_limits) * descriptor_limit_count);
        runtime->pools[index].state = 1u;
        *pool_out = make_handle(RIN_GPU_VULKAN_DESCRIPTOR_POOL_TAG, index,
                                runtime->pool_generation,
                                runtime->handle_secret);
        return RIN_GPU_VULKAN_GRAPHICS_OK;
    }
    return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
}

int rin_gpu_vulkan_descriptor_pool_create(
        RinGpuVulkanDescriptorRuntimeV1* runtime, uint32_t max_sets,
        RinGpuVulkanDescriptorHandleV1* pool_out) {
    return rin_gpu_vulkan_descriptor_pool_create_v2(
        runtime, max_sets, NULL, 0u, pool_out);
}

int rin_gpu_vulkan_descriptor_pool_destroy(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 pool) {
    uint32_t index;
    uint32_t generation;
    if (!runtime_valid(runtime) ||
        !decode_handle(pool, RIN_GPU_VULKAN_DESCRIPTOR_POOL_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS, &index,
                       &generation) || runtime->pools[index].state == 0u ||
        runtime->pools[index].generation != generation ||
        runtime->pools[index].live_sets != 0u)
        return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    memset(&runtime->pools[index], 0, sizeof(runtime->pools[index]));
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_set_allocate(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 pool,
        RinGpuVulkanDescriptorHandleV1 layout,
        RinGpuVulkanDescriptorHandleV1* set_out) {
    uint32_t pool_index;
    uint32_t pool_generation;
    uint32_t layout_index;
    uint32_t layout_generation;
    uint32_t set_index;
    uint32_t descriptor_counts[8];
    uint32_t binding_index;
    if (!set_out || !runtime_valid(runtime))
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    *set_out = 0u;
    if (!decode_handle(pool, RIN_GPU_VULKAN_DESCRIPTOR_POOL_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS, &pool_index,
                       &pool_generation) || runtime->pools[pool_index].state == 0u ||
        runtime->pools[pool_index].generation != pool_generation ||
        !decode_handle(layout, RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS, &layout_index,
                       &layout_generation) || runtime->layouts[layout_index].state == 0u ||
        runtime->layouts[layout_index].generation != layout_generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    if (runtime->pools[pool_index].live_sets >=
        runtime->pools[pool_index].max_sets)
        return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
    memset(descriptor_counts, 0, sizeof(descriptor_counts));
    for (binding_index = 0u;
         binding_index < runtime->layouts[layout_index].binding_count;
         ++binding_index) {
        const RinGpuVulkanDescriptorSetLayoutBindingV1* binding =
            &runtime->layouts[layout_index].bindings[binding_index];
        if (binding->descriptor_type >= 8u ||
            UINT32_MAX - descriptor_counts[binding->descriptor_type] <
                binding->descriptor_count)
            return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
        descriptor_counts[binding->descriptor_type] +=
            binding->descriptor_count;
    }
    if (runtime->pools[pool_index].has_limits != 0u) {
        for (binding_index = 0u; binding_index < 8u; ++binding_index)
            if (runtime->pools[pool_index]
                    .descriptor_used[binding_index] >
                    runtime->pools[pool_index]
                        .descriptor_limits[binding_index] ||
                descriptor_counts[binding_index] >
                    runtime->pools[pool_index]
                        .descriptor_limits[binding_index] -
                    runtime->pools[pool_index]
                        .descriptor_used[binding_index])
                return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
    }
    for (set_index = 0u; set_index < RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS;
         ++set_index) {
        if (runtime->sets[set_index].state != 0u) continue;
        runtime->set_generation = next_generation(runtime->set_generation);
        runtime->sets[set_index].generation = runtime->set_generation;
        runtime->sets[set_index].pool_generation = pool_generation;
        runtime->sets[set_index].layout_generation = layout_generation;
        runtime->sets[set_index].pool = pool;
        runtime->sets[set_index].layout = layout;
        runtime->sets[set_index].state = 1u;
        ++runtime->pools[pool_index].live_sets;
        if (runtime->pools[pool_index].has_limits != 0u)
            for (binding_index = 0u; binding_index < 8u; ++binding_index)
                runtime->pools[pool_index].descriptor_used[binding_index] +=
                    descriptor_counts[binding_index];
        *set_out = make_handle(RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG, set_index,
                               runtime->set_generation, runtime->handle_secret);
        return RIN_GPU_VULKAN_GRAPHICS_OK;
    }
    return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
}

int rin_gpu_vulkan_descriptor_set_free(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 set) {
    uint32_t set_index;
    uint32_t set_generation;
    uint32_t pool_index;
    uint32_t pool_generation;
    uint32_t layout_index;
    uint32_t layout_generation;
    uint32_t binding_index;
    if (!runtime_valid(runtime) ||
        !decode_handle(set, RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS, &set_index,
                       &set_generation) || runtime->sets[set_index].state == 0u ||
        runtime->sets[set_index].generation != set_generation ||
        !decode_handle(runtime->sets[set_index].pool,
                       RIN_GPU_VULKAN_DESCRIPTOR_POOL_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_POOLS, &pool_index,
                       &pool_generation) || runtime->pools[pool_index].state == 0u ||
        runtime->pools[pool_index].generation != pool_generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    if (!decode_handle(runtime->sets[set_index].layout,
                       RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS, &layout_index,
                       &layout_generation) ||
        runtime->layouts[layout_index].state == 0u ||
        runtime->layouts[layout_index].generation != layout_generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    if (runtime->pools[pool_index].live_sets != 0u)
        --runtime->pools[pool_index].live_sets;
    if (runtime->pools[pool_index].has_limits != 0u)
        for (binding_index = 0u;
             binding_index < runtime->layouts[layout_index].binding_count;
             ++binding_index) {
            uint32_t type = runtime->layouts[layout_index]
                                .bindings[binding_index]
                                .descriptor_type;
            uint32_t count = runtime->layouts[layout_index]
                                 .bindings[binding_index]
                                 .descriptor_count;
            if (type >= 8u ||
                runtime->pools[pool_index].descriptor_used[type] < count)
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
            runtime->pools[pool_index].descriptor_used[type] -= count;
        }
    memset(&runtime->sets[set_index], 0, sizeof(runtime->sets[set_index]));
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_set_free_from_pool(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 pool,
        RinGpuVulkanDescriptorHandleV1 set) {
    uint32_t set_index;
    uint32_t set_generation;
    if (!runtime_valid(runtime) ||
        !decode_handle(set, RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS, &set_index,
                       &set_generation) || runtime->sets[set_index].state == 0u ||
        runtime->sets[set_index].generation != set_generation ||
        runtime->sets[set_index].pool != pool)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    return rin_gpu_vulkan_descriptor_set_free(runtime, set);
}

int rin_gpu_vulkan_descriptor_set_update(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 set,
        const RinGpuVulkanDescriptorWriteV1* writes, uint32_t write_count) {
    uint32_t set_index;
    uint32_t set_generation;
    uint32_t layout_index;
    uint32_t layout_generation;
    uint32_t index;
    if (!runtime_valid(runtime) || !writes || write_count == 0u ||
        write_count > RIN_SHADER_MAX_RESOURCES ||
        !decode_handle(set, RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS, &set_index,
                       &set_generation) || runtime->sets[set_index].state == 0u ||
        runtime->sets[set_index].generation != set_generation ||
        !decode_handle(runtime->sets[set_index].layout,
                       RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS, &layout_index,
                       &layout_generation) || runtime->layouts[layout_index].state == 0u ||
        runtime->layouts[layout_index].generation != layout_generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    for (index = 0u; index < write_count; ++index) {
        uint32_t binding_index;
        uint32_t prior;
        const RinGpuVulkanDescriptorSetLayoutBindingV1* binding;
        if (!write_key_unique(writes, write_count, index) ||
            writes[index].resource == 0u || writes[index].reserved != 0u ||
            (writes[index].flags & ~RIN_GPU_GRAPHICS_BINDING_KNOWN_FLAGS) != 0u ||
            !layout_binding_index(runtime, layout_index, writes[index].set,
                                  writes[index].binding, &binding_index))
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        binding = &runtime->layouts[layout_index].bindings[binding_index];
        if (writes[index].descriptor_type != binding->descriptor_type ||
            writes[index].array_element >= binding->descriptor_count)
            return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        for (prior = 0u; prior < runtime->sets[set_index].write_count;
             ++prior)
            if (runtime->sets[set_index].writes[prior].set == writes[index].set &&
                runtime->sets[set_index].writes[prior].binding == writes[index].binding &&
                runtime->sets[set_index].writes[prior].array_element ==
                    writes[index].array_element)
                runtime->sets[set_index].writes[prior] = writes[index];
    }
    for (index = 0u; index < write_count; ++index) {
        uint32_t prior;
        int replaced = 0;
        for (prior = 0u; prior < runtime->sets[set_index].write_count;
             ++prior) {
            if (runtime->sets[set_index].writes[prior].set == writes[index].set &&
                runtime->sets[set_index].writes[prior].binding == writes[index].binding &&
                runtime->sets[set_index].writes[prior].array_element ==
                    writes[index].array_element) {
                replaced = 1;
                break;
            }
        }
        if (replaced) continue;
        if (runtime->sets[set_index].write_count >= RIN_SHADER_MAX_RESOURCES)
            return RIN_GPU_VULKAN_GRAPHICS_LIMIT;
        runtime->sets[set_index].writes[
            runtime->sets[set_index].write_count++] = writes[index];
    }
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_set_validate_dynamic_offsets(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 set, const uint32_t* offsets,
        uint32_t offset_count, uint32_t* consumed_count_out) {
    uint32_t set_index;
    uint32_t set_generation;
    uint32_t layout_index;
    uint32_t layout_generation;
    uint32_t consumed = 0u;
    uint32_t binding_index;
    if (!consumed_count_out || !runtime_valid(runtime) ||
        (offset_count != 0u && !offsets) ||
        !decode_handle(set, RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS, &set_index,
                       &set_generation) || runtime->sets[set_index].state == 0u ||
        runtime->sets[set_index].generation != set_generation ||
        !decode_handle(runtime->sets[set_index].layout,
                       RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS, &layout_index,
                       &layout_generation) || runtime->layouts[layout_index].state == 0u ||
        runtime->layouts[layout_index].generation != layout_generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    for (binding_index = 0u;
         binding_index < runtime->layouts[layout_index].binding_count;
         ++binding_index) {
        const RinGpuVulkanDescriptorSetLayoutBindingV1* binding =
            &runtime->layouts[layout_index].bindings[binding_index];
        uint32_t array_element;
        if ((binding->reserved & RIN_GPU_VULKAN_DESCRIPTOR_BINDING_DYNAMIC) ==
            0u)
            continue;
        for (array_element = 0u; array_element < binding->descriptor_count;
             ++array_element) {
            const RinGpuVulkanDescriptorWriteV1* write = find_set_write(
                runtime, set_index, binding->set, binding->binding,
                array_element);
            uint32_t dynamic_offset;
            if (!write || consumed >= offset_count ||
                !offsets || UINT64_MAX - write->offset < offsets[consumed])
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
            dynamic_offset = offsets[consumed++];
            if ((uint64_t)dynamic_offset > write->size_bytes ||
                write->offset + dynamic_offset > UINT64_MAX - write->size_bytes)
                return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        }
    }
    if (consumed != offset_count) return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
    *consumed_count_out = consumed;
    return RIN_GPU_VULKAN_GRAPHICS_OK;
}

int rin_gpu_vulkan_descriptor_set_build_plan(
        RinGpuVulkanDescriptorRuntimeV1* runtime,
        RinGpuVulkanDescriptorHandleV1 set,
        const RinSpirvTranslationInfoV1* vertex,
        const RinSpirvTranslationInfoV1* fragment,
        RinGpuVulkanDescriptorSetPlanV1* plan_out) {
    uint32_t set_index;
    uint32_t set_generation;
    uint32_t layout_index;
    uint32_t layout_generation;
    uint32_t layout_binding;
    if (!plan_out || !runtime_valid(runtime) ||
        !decode_handle(set, RIN_GPU_VULKAN_DESCRIPTOR_SET_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS, &set_index,
                       &set_generation) || runtime->sets[set_index].state == 0u ||
        runtime->sets[set_index].generation != set_generation ||
        !decode_handle(runtime->sets[set_index].layout,
                       RIN_GPU_VULKAN_DESCRIPTOR_LAYOUT_TAG,
                       runtime->handle_secret,
                       RIN_GPU_VULKAN_DESCRIPTOR_MAX_LAYOUTS, &layout_index,
                       &layout_generation) || runtime->layouts[layout_index].state == 0u ||
        runtime->layouts[layout_index].generation != layout_generation)
        return RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT;
    for (layout_binding = 0u;
         layout_binding < runtime->layouts[layout_index].binding_count;
         ++layout_binding) {
        const RinGpuVulkanDescriptorSetLayoutBindingV1* binding =
            &runtime->layouts[layout_index].bindings[layout_binding];
        uint32_t array_element;
        for (array_element = 0u; array_element < binding->descriptor_count;
             ++array_element) {
            uint32_t write_index;
            int found = 0;
            for (write_index = 0u;
                 write_index < runtime->sets[set_index].write_count;
                 ++write_index) {
                const RinGpuVulkanDescriptorWriteV1* write =
                    &runtime->sets[set_index].writes[write_index];
                if (write->set == binding->set &&
                    write->binding == binding->binding &&
                    write->array_element == array_element) {
                    found = 1;
                    break;
                }
            }
            if (!found) return RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE;
        }
    }
    return ringpu_vulkan_graphics_build_descriptor_set(
        vertex, fragment, runtime->layouts[layout_index].bindings,
        runtime->layouts[layout_index].binding_count,
        runtime->sets[set_index].writes, runtime->sets[set_index].write_count,
        plan_out);
}
