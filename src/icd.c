/* SPDX-License-Identifier: MIT */

#include <rinvulkan/icd.h>
#include <rinvulkan/command_runtime.h>
#include <rinvulkan/descriptor_runtime.h>

#include <string.h>

#define RIN_VK_ICD_BINDING_TRANSITION UINTPTR_MAX
#define RIN_VK_ICD_CALL_RETRIES 4096u
#define RIN_VK_FEATURE_CHAIN_MAX 8u
#define RIN_VK_PROPERTY_CHAIN_MAX 8u
#define RIN_VK_MAX_BUFFERS 128u
#define RIN_VK_MAX_IMAGES 128u
#define RIN_VK_MAX_MEMORIES 64u
#define RIN_VK_MAX_FENCES 128u
#define RIN_VK_MAX_SEMAPHORES 128u
#define RIN_VK_MAX_SUBMISSIONS 64u
#define RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS 4u
#define RIN_VK_MAX_SUBMIT_SEMAPHORES 8u
#define RIN_VK_MAX_IMAGE_VIEWS 128u
#define RIN_VK_MAX_SAMPLERS 128u
#define RIN_VK_MAX_PIPELINE_LAYOUTS 64u
#define RIN_VK_RESOURCE_ALIGNMENT UINT64_C(2097152)
#define RIN_VK_BUFFER_TAG UINT64_C(0x5242)
#define RIN_VK_IMAGE_TAG UINT64_C(0x5249)
#define RIN_VK_MEMORY_TAG UINT64_C(0x524d)
#define RIN_VK_FENCE_TAG UINT64_C(0x5246)
#define RIN_VK_SEMAPHORE_TAG UINT64_C(0x5253)
#define RIN_VK_IMAGE_VIEW_TAG UINT64_C(0x5256)
#define RIN_VK_SAMPLER_TAG UINT64_C(0x5254)
#define RIN_VK_PIPELINE_LAYOUT_TAG UINT64_C(0x5250)

typedef struct RinVkBaseFeatureStructure {
    RinVkStructureType sType;
    void* pNext;
} RinVkBaseFeatureStructure;

typedef struct RinVkFeatureChain {
    RinVkPhysicalDeviceVulkan12Features* vulkan12;
    RinVkPhysicalDeviceVulkan13Features* vulkan13;
} RinVkFeatureChain;

typedef struct RinVkPropertyChain {
    RinVkPhysicalDeviceVulkan11Properties* vulkan11;
    RinVkPhysicalDeviceVulkan12Properties* vulkan12;
    RinVkPhysicalDeviceVulkan13Properties* vulkan13;
} RinVkPropertyChain;

static void zero_vulkan11_properties(
        RinVkPhysicalDeviceVulkan11Properties* properties);
static void zero_vulkan12_properties(
        RinVkPhysicalDeviceVulkan12Properties* properties);
static void zero_vulkan13_properties(
        RinVkPhysicalDeviceVulkan13Properties* properties);

struct RinVkPhysicalDevice_T {
    uintptr_t loader_magic;
    RinGpuVulkanHandle owner_instance;
    RinGpuVulkanHandle runtime_handle;
};

struct RinVkInstance_T {
    uintptr_t loader_magic;
    uint32_t state;
    uint32_t reserved;
    RinGpuVulkanHandle runtime_handle;
    struct RinVkPhysicalDevice_T
        physical_devices[RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
};

struct RinVkQueue_T {
    uintptr_t loader_magic;
    struct RinVkDevice_T* device;
    uint32_t queue_family_index;
    uint32_t queue_index;
    volatile uint32_t submit_lock;
    uint32_t reserved;
};

struct RinVkDevice_T {
    uintptr_t loader_magic;
    uint32_t state;
    uint32_t reserved;
    RinGpuVulkanHandle runtime_handle;
    RinGpuVulkanHandle owner_instance;
    RinGpuVulkanDevicePlanV1 plan;
    RinGpuVulkanPhysicalDeviceV2 physical_profile;
    RinGpuVulkanDescriptorRuntimeV1 descriptor_runtime;
    volatile uint32_t descriptor_validation_error;
    uint32_t timeline_enabled;
    uint32_t queue_count;
    uint32_t reserved_queue;
    struct RinVkQueue_T queues[RIN_VULKAN_PRODUCT_MAX_QUEUES];
};

typedef struct RinVkMemorySlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint64_t product_allocation;
    uint64_t gpu_virtual_address;
    uint64_t requested_size;
    uint32_t memory_type_index;
    uint32_t bound_resource_count;
} RinVkMemorySlot;

typedef struct RinVkBufferSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    RinVkMemorySlot* memory;
    uint32_t memory_generation;
    uint32_t usage;
    uint64_t size;
    uint64_t memory_offset;
} RinVkBufferSlot;

typedef struct RinVkImageSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    RinVkMemorySlot* memory;
    uint32_t memory_generation;
    int32_t format;
    uint32_t usage;
    uint64_t memory_size;
    uint64_t memory_offset;
    uint32_t width;
    uint32_t height;
    uint32_t samples;
} RinVkImageSlot;

typedef struct RinVkImageViewSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    RinVkImageSlot* image;
    uint32_t format;
    uint32_t aspect_mask;
} RinVkImageViewSlot;

typedef struct RinVkSamplerSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t mipmap_mode;
    uint32_t address_mode_u;
    uint32_t address_mode_v;
    uint32_t address_mode_w;
    uint32_t compare_enable;
    uint32_t compare_op;
} RinVkSamplerSlot;

typedef struct RinVkPipelineLayoutSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint32_t set_layout_count;
    RinVkDescriptorSetLayout set_layouts[RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
} RinVkPipelineLayoutSlot;

typedef struct RinVkFenceSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    volatile uint32_t signaled;
    volatile uint32_t pending;
} RinVkFenceSlot;

typedef struct RinVkSemaphoreSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint32_t type;
    uint32_t reserved_type;
    volatile uint32_t signaled;
    volatile uint32_t pending;
    volatile uint64_t value;
    uint64_t pending_value;
} RinVkSemaphoreSlot;

typedef struct RinVkSubmissionSlot {
    uint32_t state;
    uint32_t command_buffer_count;
    struct RinVkDevice_T* owner;
    uint32_t queue_id;
    uint32_t reserved;
    uint64_t sequence;
    uint64_t completion_value;
    RinVkFence fence;
    uint32_t wait_semaphore_count;
    uint32_t signal_semaphore_count;
    RinVkSemaphore signal_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t signal_semaphore_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinGpuVulkanCommandBufferV1*
        command_buffers[RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS];
    RinGpuVulkanTransferPacketV1 packet;
    RinGpuVulkanTransferPacketV2 extended_packet;
    uint32_t packet_version;
} RinVkSubmissionSlot;

static uintptr_t g_runtime_binding;
static uint32_t g_active_calls;
static uintptr_t g_product_binding;
static uint32_t g_active_product_calls;
static struct RinVkInstance_T g_instances[RIN_GPU_VULKAN_MAX_INSTANCES];
static struct RinVkDevice_T g_devices[RIN_GPU_VULKAN_MAX_DEVICES];
static RinGpuVulkanCommandRuntimeV1 g_command_runtime;
static RinVkMemorySlot g_memories[RIN_VK_MAX_MEMORIES];
static RinVkBufferSlot g_buffers[RIN_VK_MAX_BUFFERS];
static RinVkImageSlot g_images[RIN_VK_MAX_IMAGES];
static RinVkImageViewSlot g_image_views[RIN_VK_MAX_IMAGE_VIEWS];
static RinVkSamplerSlot g_samplers[RIN_VK_MAX_SAMPLERS];
static RinVkPipelineLayoutSlot g_pipeline_layouts[RIN_VK_MAX_PIPELINE_LAYOUTS];
static RinVkFenceSlot g_fences[RIN_VK_MAX_FENCES];
static RinVkSemaphoreSlot g_semaphores[RIN_VK_MAX_SEMAPHORES];
static RinVkSubmissionSlot g_submissions[RIN_VK_MAX_SUBMISSIONS];

static int all_zero(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t index;
    for (index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    return 1;
}

static int collect_feature_chain(void* first, int allow_unknown,
                                 RinVkFeatureChain* chain) {
    RinVkBaseFeatureStructure* node =
        (RinVkBaseFeatureStructure*)first;
    void* seen[RIN_VK_FEATURE_CHAIN_MAX];
    uint32_t count = 0u;
    uint32_t index;
    memset(chain, 0, sizeof(*chain));
    while (node && count < RIN_VK_FEATURE_CHAIN_MAX) {
        for (index = 0u; index < count; ++index) {
            if (seen[index] == node) return 0;
        }
        seen[count++] = node;
        if (node->sType ==
            RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            if (chain->vulkan12) return 0;
            chain->vulkan12 =
                (RinVkPhysicalDeviceVulkan12Features*)node;
        } else if (node->sType ==
                   RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
            if (chain->vulkan13) return 0;
            chain->vulkan13 =
                (RinVkPhysicalDeviceVulkan13Features*)node;
        } else if (!allow_unknown) {
            return 0;
        }
        node = (RinVkBaseFeatureStructure*)node->pNext;
    }
    return node == NULL;
}

static int collect_property_chain(void* first, RinVkPropertyChain* chain) {
    RinVkBaseFeatureStructure* node =
        (RinVkBaseFeatureStructure*)first;
    void* seen[RIN_VK_PROPERTY_CHAIN_MAX];
    uint32_t count = 0u;
    uint32_t index;
    memset(chain, 0, sizeof(*chain));
    while (node && count < RIN_VK_PROPERTY_CHAIN_MAX) {
        for (index = 0u; index < count; ++index) {
            if (seen[index] == node) return 0;
        }
        seen[count++] = node;
        if (node->sType ==
            RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES) {
            zero_vulkan11_properties(
                (RinVkPhysicalDeviceVulkan11Properties*)node);
            if (chain->vulkan11) return 0;
            chain->vulkan11 =
                (RinVkPhysicalDeviceVulkan11Properties*)node;
        } else if (node->sType ==
                   RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES) {
            zero_vulkan12_properties(
                (RinVkPhysicalDeviceVulkan12Properties*)node);
            if (chain->vulkan12) return 0;
            chain->vulkan12 =
                (RinVkPhysicalDeviceVulkan12Properties*)node;
        } else if (node->sType ==
                   RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES) {
            zero_vulkan13_properties(
                (RinVkPhysicalDeviceVulkan13Properties*)node);
            if (chain->vulkan13) return 0;
            chain->vulkan13 =
                (RinVkPhysicalDeviceVulkan13Properties*)node;
        }
        node = (RinVkBaseFeatureStructure*)node->pNext;
    }
    return node == NULL;
}

static void zero_vulkan11_properties(
        RinVkPhysicalDeviceVulkan11Properties* properties) {
    memset(&properties->deviceUUID, 0,
           offsetof(RinVkPhysicalDeviceVulkan11Properties,
                    maxMemoryAllocationSize) +
               sizeof(properties->maxMemoryAllocationSize) -
               offsetof(RinVkPhysicalDeviceVulkan11Properties,
                        deviceUUID));
}

static void zero_vulkan12_properties(
        RinVkPhysicalDeviceVulkan12Properties* properties) {
    memset(&properties->driverID, 0,
           offsetof(RinVkPhysicalDeviceVulkan12Properties,
                    framebufferIntegerColorSampleCounts) +
               sizeof(properties->framebufferIntegerColorSampleCounts) -
               offsetof(RinVkPhysicalDeviceVulkan12Properties,
                        driverID));
}

static void zero_vulkan13_properties(
        RinVkPhysicalDeviceVulkan13Properties* properties) {
    memset(&properties->minSubgroupSize, 0,
           offsetof(RinVkPhysicalDeviceVulkan13Properties,
                    maxBufferSize) +
               sizeof(properties->maxBufferSize) -
               offsetof(RinVkPhysicalDeviceVulkan13Properties,
                        minSubgroupSize));
}

static void zero_property_chain(const RinVkPropertyChain* chain) {
    if (chain->vulkan11) zero_vulkan11_properties(chain->vulkan11);
    if (chain->vulkan12) zero_vulkan12_properties(chain->vulkan12);
    if (chain->vulkan13) zero_vulkan13_properties(chain->vulkan13);
}

static int requested_vulkan12_features(
        const RinVkPhysicalDeviceVulkan12Features* features,
        uint64_t* required) {
    RinVkPhysicalDeviceVulkan12Features snapshot;
    if (!features) return 1;
    snapshot = *features;
    if (snapshot.sType !=
            RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES ||
        snapshot.descriptorIndexing > 1u ||
        snapshot.timelineSemaphore > 1u ||
        snapshot.bufferDeviceAddress > 1u)
        return 0;
    if (snapshot.descriptorIndexing != 0u)
        *required |= RIN_GPU_VK_FEATURE_DESCRIPTOR_INDEXING;
    if (snapshot.timelineSemaphore != 0u)
        *required |= RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE;
    if (snapshot.bufferDeviceAddress != 0u)
        *required |= RIN_GPU_VK_FEATURE_BUFFER_DEVICE_ADDRESS;
    snapshot.descriptorIndexing = 0u;
    snapshot.timelineSemaphore = 0u;
    snapshot.bufferDeviceAddress = 0u;
    return all_zero(&snapshot.samplerMirrorClampToEdge,
                    offsetof(RinVkPhysicalDeviceVulkan12Features,
                             subgroupBroadcastDynamicId) +
                        sizeof(snapshot.subgroupBroadcastDynamicId) -
                        offsetof(RinVkPhysicalDeviceVulkan12Features,
                                 samplerMirrorClampToEdge));
}

static int requested_vulkan13_features(
        const RinVkPhysicalDeviceVulkan13Features* features,
        uint64_t* required) {
    RinVkPhysicalDeviceVulkan13Features snapshot;
    if (!features) return 1;
    snapshot = *features;
    if (snapshot.sType !=
            RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES ||
        snapshot.synchronization2 > 1u ||
        snapshot.dynamicRendering > 1u || snapshot.maintenance4 > 1u)
        return 0;
    if (snapshot.synchronization2 != 0u)
        *required |= RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2;
    if (snapshot.dynamicRendering != 0u)
        *required |= RIN_GPU_VK_FEATURE_DYNAMIC_RENDERING;
    if (snapshot.maintenance4 != 0u)
        *required |= RIN_GPU_VK_FEATURE_MAINTENANCE_4;
    snapshot.synchronization2 = 0u;
    snapshot.dynamicRendering = 0u;
    snapshot.maintenance4 = 0u;
    return all_zero(&snapshot.robustImageAccess,
                    offsetof(RinVkPhysicalDeviceVulkan13Features,
                             maintenance4) +
                        sizeof(snapshot.maintenance4) -
                        offsetof(RinVkPhysicalDeviceVulkan13Features,
                                 robustImageAccess));
}

static void publish_legacy_features(
        const RinGpuVulkanPhysicalDeviceV2* profile,
        RinVkPhysicalDeviceFeatures* features) {
    memset(features, 0, sizeof(*features));
    if ((profile->features & RIN_GPU_VK_ICD_FEATURES &
         RIN_GPU_VK_FEATURE_SAMPLER_ANISOTROPY) != 0u)
        features->samplerAnisotropy = 1u;
}

static int name_equal(const char* actual, const char* expected) {
    size_t index;
    if (!actual || !expected) return 0;
    for (index = 0u; index < 64u; ++index) {
        if (actual[index] != expected[index]) return 0;
        if (expected[index] == '\0') return 1;
    }
    return 0;
}

static int copy_name(char destination[RIN_GPU_VULKAN_NAME_MAX],
                     const char* source) {
    size_t index;
    memset(destination, 0, RIN_GPU_VULKAN_NAME_MAX);
    if (!source) return 1;
    for (index = 0u; index < RIN_GPU_VULKAN_NAME_MAX; ++index) {
        destination[index] = source[index];
        if (source[index] == '\0') return 1;
    }
    memset(destination, 0, RIN_GPU_VULKAN_NAME_MAX);
    return 0;
}

static int api_version_supported(uint32_t version) {
    uint32_t variant = version >> 29u;
    uint32_t major = (version >> 22u) & 0x7fu;
    uint32_t minor = (version >> 12u) & 0x3ffu;
    return variant == 0u && major == 1u &&
           version <= RIN_GPU_VK_ICD_API_VERSION && minor <= 0u;
}

static int timeline_extension_enabled(const RinVkDeviceCreateInfo* info) {
    uint32_t index;
    if (!info || info->enabledExtensionCount == 0u) return 0;
    if (!info->ppEnabledExtensionNames) return 0;
    for (index = 0u; index < info->enabledExtensionCount; ++index) {
        if (!info->ppEnabledExtensionNames[index]) return 0;
        if (name_equal(info->ppEnabledExtensionNames[index],
                       RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION))
            return 1;
    }
    return 0;
}

static int device_extensions_valid(const RinVkDeviceCreateInfo* info) {
    uint32_t index;
    if (!info || info->enabledExtensionCount > 1u) return 0;
    if (info->enabledExtensionCount == 0u)
        return info->ppEnabledExtensionNames == NULL;
    if (!timeline_extension_enabled(info)) return 0;
    for (index = 0u; index < info->enabledExtensionCount; ++index)
        if (!info->ppEnabledExtensionNames[index]) return 0;
    return 1;
}

static RinGpuVulkanRuntimeV1* acquire_runtime(void) {
    uint32_t attempt;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        uintptr_t binding =
            __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE);
        if (binding == 0u || binding == RIN_VK_ICD_BINDING_TRANSITION)
            return NULL;
        __atomic_add_fetch(&g_active_calls, 1u, __ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) ==
            binding)
            return (RinGpuVulkanRuntimeV1*)binding;
        __atomic_sub_fetch(&g_active_calls, 1u, __ATOMIC_RELEASE);
    }
    return NULL;
}

static void release_runtime(void) {
    __atomic_sub_fetch(&g_active_calls, 1u, __ATOMIC_RELEASE);
}

static RinVulkanProductPlatformV1* acquire_product(void) {
    uint32_t attempt;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        uintptr_t binding =
            __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE);
        if (binding == 0u || binding == RIN_VK_ICD_BINDING_TRANSITION)
            return NULL;
        __atomic_add_fetch(&g_active_product_calls, 1u, __ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) ==
            binding)
            return (RinVulkanProductPlatformV1*)binding;
        __atomic_sub_fetch(&g_active_product_calls, 1u, __ATOMIC_RELEASE);
    }
    return NULL;
}

static void release_product(void) {
    __atomic_sub_fetch(&g_active_product_calls, 1u, __ATOMIC_RELEASE);
}

static RinVkResult map_result(int result) {
    switch (result) {
    case RIN_GPU_VULKAN_OK:
        return RIN_VK_SUCCESS;
    case RIN_GPU_VULKAN_INCOMPLETE:
        return RIN_VK_INCOMPLETE;
    case RIN_GPU_VULKAN_BUSY:
        return RIN_VK_NOT_READY;
    case RIN_GPU_VULKAN_LIMIT:
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    case RIN_GPU_VULKAN_UNSUPPORTED:
        return RIN_VK_ERROR_INCOMPATIBLE_DRIVER;
    case RIN_GPU_VULKAN_INVALID_ARGUMENT:
    case RIN_GPU_VULKAN_SECURITY:
    case RIN_GPU_VULKAN_INVALID_HANDLE:
    case RIN_GPU_VULKAN_NOT_INITIALIZED:
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    default:
        return RIN_VK_ERROR_UNKNOWN;
    }
}

static RinVkResult map_command_result(int result) {
    switch (result) {
    case RIN_GPU_VULKAN_COMMAND_OK:
        return RIN_VK_SUCCESS;
    case RIN_GPU_VULKAN_COMMAND_LIMIT:
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    case RIN_GPU_VULKAN_COMMAND_UNSUPPORTED:
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    case RIN_GPU_VULKAN_COMMAND_INVALID_ARGUMENT:
    case RIN_GPU_VULKAN_COMMAND_INVALID_HANDLE:
    case RIN_GPU_VULKAN_COMMAND_INVALID_STATE:
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    default:
        return RIN_VK_ERROR_UNKNOWN;
    }
}

static RinVkResult map_product_result(int result) {
    switch (result) {
    case RIN_VULKAN_PRODUCT_OK:
        return RIN_VK_SUCCESS;
    case RIN_VULKAN_PRODUCT_NO_SPACE:
    case RIN_VULKAN_PRODUCT_LIMIT:
        return RIN_VK_ERROR_OUT_OF_DEVICE_MEMORY;
    case RIN_VULKAN_PRODUCT_LOST:
    case RIN_VULKAN_PRODUCT_PROTOCOL:
        return RIN_VK_ERROR_DEVICE_LOST;
    case RIN_VULKAN_PRODUCT_BUSY:
        return RIN_VK_NOT_READY;
    case RIN_VULKAN_PRODUCT_INVALID_ARGUMENT:
    case RIN_VULKAN_PRODUCT_STATE:
    case RIN_VULKAN_PRODUCT_BACKEND_FAILED:
    case RIN_VULKAN_PRODUCT_TIMEOUT:
    case RIN_VULKAN_PRODUCT_RECOVERY_REQUIRED:
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    default:
        return RIN_VK_ERROR_UNKNOWN;
    }
}

static RinGpuVulkanCommandPoolHandleV1 command_pool_to_core(
        RinVkCommandPool pool) {
#if UINTPTR_MAX == UINT64_MAX
    return (RinGpuVulkanCommandPoolHandleV1)(uintptr_t)pool;
#else
    return (RinGpuVulkanCommandPoolHandleV1)pool;
#endif
}

static RinVkCommandPool command_pool_from_core(
        RinGpuVulkanCommandPoolHandleV1 pool) {
#if UINTPTR_MAX == UINT64_MAX
    return (RinVkCommandPool)(uintptr_t)pool;
#else
    return (RinVkCommandPool)pool;
#endif
}

static struct RinVkInstance_T* instance_slot(RinVkInstance instance) {
    uintptr_t address = (uintptr_t)instance;
    uintptr_t first = (uintptr_t)&g_instances[0];
    uintptr_t end = (uintptr_t)&g_instances[RIN_GPU_VULKAN_MAX_INSTANCES];
    uintptr_t offset;
    uint32_t index;
    if (!instance || address < first || address >= end) return NULL;
    offset = address - first;
    if ((offset % sizeof(g_instances[0])) != 0u) return NULL;
    index = (uint32_t)(offset / sizeof(g_instances[0]));
    if (__atomic_load_n(&g_instances[index].state, __ATOMIC_ACQUIRE) != 1u)
        return NULL;
    return &g_instances[index];
}

static struct RinVkPhysicalDevice_T* physical_slot(
        RinVkPhysicalDevice physical_device,
        struct RinVkInstance_T** instance_out) {
    uint32_t instance_index;
    uint32_t physical_index;
    if (!physical_device) return NULL;
    for (instance_index = 0u;
         instance_index < RIN_GPU_VULKAN_MAX_INSTANCES; ++instance_index) {
        struct RinVkInstance_T* instance = &g_instances[instance_index];
        if (__atomic_load_n(&instance->state, __ATOMIC_ACQUIRE) != 1u)
            continue;
        for (physical_index = 0u;
             physical_index < RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES;
             ++physical_index) {
            struct RinVkPhysicalDevice_T* candidate =
                &instance->physical_devices[physical_index];
            if (candidate == physical_device &&
                (candidate->loader_magic & UINT32_MAX) ==
                    RIN_VK_ICD_LOADER_MAGIC &&
                candidate->owner_instance == instance->runtime_handle &&
                candidate->runtime_handle != 0u) {
                if (instance_out) *instance_out = instance;
                return candidate;
            }
        }
    }
    return NULL;
}

static struct RinVkDevice_T* device_slot(RinVkDevice device) {
    uintptr_t address = (uintptr_t)device;
    uintptr_t first = (uintptr_t)&g_devices[0];
    uintptr_t end = (uintptr_t)&g_devices[RIN_GPU_VULKAN_MAX_DEVICES];
    uintptr_t offset;
    uint32_t index;
    if (!device || address < first || address >= end) return NULL;
    offset = address - first;
    if ((offset % sizeof(g_devices[0])) != 0u) return NULL;
    index = (uint32_t)(offset / sizeof(g_devices[0]));
    if (__atomic_load_n(&g_devices[index].state, __ATOMIC_ACQUIRE) != 1u)
        return NULL;
    return &g_devices[index];
}

static uint64_t resource_handle(uint64_t tag, uint32_t index,
                                uint32_t generation) {
    return (tag << 48u) | ((uint64_t)generation << 16u) |
           (uint64_t)(index + 1u);
}

static RinVkMemorySlot* memory_slot(RinVkDevice device,
                                    RinVkDeviceMemory handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkMemorySlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_MEMORY_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_MEMORIES ||
        generation == 0u)
        return NULL;
    slot = &g_memories[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkBufferSlot* buffer_slot(RinVkDevice device,
                                    RinVkBuffer handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkBufferSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_BUFFER_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_BUFFERS ||
        generation == 0u)
        return NULL;
    slot = &g_buffers[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkImageSlot* image_slot(RinVkDevice device, RinVkImage handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkImageSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_IMAGE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_IMAGES ||
        generation == 0u)
        return NULL;
    slot = &g_images[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkImageViewSlot* image_view_slot(RinVkDevice device,
                                           RinVkImageView handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkImageViewSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_IMAGE_VIEW_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_IMAGE_VIEWS ||
        generation == 0u)
        return NULL;
    slot = &g_image_views[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkSamplerSlot* sampler_slot(RinVkDevice device,
                                      RinVkSampler handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkSamplerSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_SAMPLER_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_SAMPLERS ||
        generation == 0u)
        return NULL;
    slot = &g_samplers[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkPipelineLayoutSlot* pipeline_layout_slot(
        RinVkDevice device, RinVkPipelineLayout handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkPipelineLayoutSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_PIPELINE_LAYOUT_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_PIPELINE_LAYOUTS ||
        generation == 0u)
        return NULL;
    slot = &g_pipeline_layouts[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkFenceSlot* fence_slot(RinVkDevice device, RinVkFence handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkFenceSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_FENCE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_FENCES ||
        generation == 0u)
        return NULL;
    slot = &g_fences[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkSemaphoreSlot* semaphore_slot(
        RinVkDevice device, RinVkSemaphore handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkSemaphoreSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_SEMAPHORE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_SEMAPHORES ||
        generation == 0u)
        return NULL;
    slot = &g_semaphores[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkFenceSlot* reserve_fence_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_FENCES; ++index) {
        RinVkFenceSlot* slot = &g_fences[index];
        uint32_t expected = 0u;
        uint32_t generation;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        generation = slot->generation;
        if (generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        memset(slot, 0, sizeof(*slot));
        slot->generation = generation + 1u;
        slot->owner = owner;
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkSemaphoreSlot* reserve_semaphore_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_SEMAPHORES; ++index) {
        RinVkSemaphoreSlot* slot = &g_semaphores[index];
        uint32_t expected = 0u;
        uint32_t generation;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        generation = slot->generation;
        if (generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        memset(slot, 0, sizeof(*slot));
        slot->generation = generation + 1u;
        slot->owner = owner;
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static void clear_fence_slot(RinVkFenceSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    __atomic_store_n(&slot->signaled, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&slot->pending, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_semaphore_slot(RinVkSemaphoreSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    slot->type = RIN_VK_SEMAPHORE_TYPE_BINARY;
    slot->reserved_type = 0u;
    __atomic_store_n(&slot->signaled, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&slot->pending, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&slot->value, 0u, __ATOMIC_RELEASE);
    slot->pending_value = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static RinVkMemorySlot* reserve_memory_slot(uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_MEMORIES; ++index) {
        RinVkMemorySlot* slot = &g_memories[index];
        uint32_t expected = 0u;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        if (slot->generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        ++slot->generation;
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkBufferSlot* reserve_buffer_slot(uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_BUFFERS; ++index) {
        RinVkBufferSlot* slot = &g_buffers[index];
        uint32_t expected = 0u;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        if (slot->generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        ++slot->generation;
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkImageSlot* reserve_image_slot(uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_IMAGES; ++index) {
        RinVkImageSlot* slot = &g_images[index];
        uint32_t expected = 0u;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        if (slot->generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        ++slot->generation;
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkImageViewSlot* reserve_image_view_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_IMAGE_VIEWS; ++index) {
        RinVkImageViewSlot* slot = &g_image_views[index];
        uint32_t expected = 0u;
        uint32_t generation;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        generation = slot->generation;
        if (generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        memset(slot, 0, sizeof(*slot));
        slot->generation = generation + 1u;
        slot->owner = owner;
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkSamplerSlot* reserve_sampler_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_SAMPLERS; ++index) {
        RinVkSamplerSlot* slot = &g_samplers[index];
        uint32_t expected = 0u;
        uint32_t generation;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        generation = slot->generation;
        if (generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        memset(slot, 0, sizeof(*slot));
        slot->generation = generation + 1u;
        slot->owner = owner;
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkPipelineLayoutSlot* reserve_pipeline_layout_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_PIPELINE_LAYOUTS; ++index) {
        RinVkPipelineLayoutSlot* slot = &g_pipeline_layouts[index];
        uint32_t expected = 0u;
        uint32_t generation;
        if (!__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            continue;
        generation = slot->generation;
        if (generation == UINT32_MAX) {
            __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
            continue;
        }
        memset(slot, 0, sizeof(*slot));
        slot->generation = generation + 1u;
        slot->owner = owner;
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static void clear_memory_slot(RinVkMemorySlot* slot) {
    slot->owner = NULL;
    slot->product_allocation = 0u;
    slot->gpu_virtual_address = 0u;
    slot->requested_size = 0u;
    slot->memory_type_index = 0u;
    slot->bound_resource_count = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_buffer_slot(RinVkBufferSlot* slot) {
    slot->owner = NULL;
    slot->memory = NULL;
    slot->memory_generation = 0u;
    slot->usage = 0u;
    slot->size = 0u;
    slot->memory_offset = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static int product_matches_device(RinVulkanProductPlatformV1* product,
                                  const struct RinVkDevice_T* device) {
    RinVulkanProductStatusV1 status;
    if (!product || !device ||
        product->get_status(product->context, &status) !=
            RIN_VULKAN_PRODUCT_OK)
        return 0;
    return status.struct_size == sizeof(status) &&
           status.version == RIN_VULKAN_PRODUCT_PLATFORM_VERSION &&
           status.flags == RIN_VULKAN_PRODUCT_STATUS_READY &&
           status.iommu_domain_cookie == device->plan.iommu_domain_cookie &&
           status.device_epoch == device->plan.device_epoch &&
           status.queue_count != 0u;
}

static uint32_t memory_type_bits(const struct RinVkDevice_T* device) {
    uint32_t count = device->physical_profile.memory_type_count;
    if (count == 0u || count > 31u) return 0u;
    return (UINT32_C(1) << count) - 1u;
}

static int image_memory_size(const struct RinVkDevice_T* device,
                             const RinVkImageCreateInfo* request,
                             uint64_t* size_out) {
    uint64_t pixels;

    if (!device || !request || !size_out ||
        request->extent.width == 0u || request->extent.height == 0u ||
        request->extent.depth != 1u || request->arrayLayers != 1u ||
        request->mipLevels != 1u ||
        (request->samples != RIN_VK_SAMPLE_COUNT_1_BIT &&
         request->samples != RIN_VK_SAMPLE_COUNT_2_BIT &&
         request->samples != RIN_VK_SAMPLE_COUNT_4_BIT) ||
        request->extent.width > device->physical_profile.max_image_dimension_2d ||
        request->extent.height > device->physical_profile.max_image_dimension_2d ||
        request->extent.width > UINT64_MAX / request->extent.height)
        return 0;
    pixels = (uint64_t)request->extent.width * request->extent.height;
    if (pixels > UINT64_MAX / 4u ||
        pixels * 4u > UINT64_MAX / request->samples)
        return 0;
    *size_out = pixels * 4u * request->samples;
    return *size_out != 0u;
}

static int resource_slots_active(void) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_MEMORIES; ++index) {
        const uint32_t state = __atomic_load_n(&g_memories[index].state,
                                                __ATOMIC_ACQUIRE);
        if (state == 1u || state == 2u)
            return 1;
    }
    for (index = 0u; index < RIN_VK_MAX_BUFFERS; ++index) {
        const uint32_t state = __atomic_load_n(&g_buffers[index].state,
                                                __ATOMIC_ACQUIRE);
        if (state == 1u || state == 2u)
            return 1;
    }
    for (index = 0u; index < RIN_VK_MAX_IMAGES; ++index) {
        const uint32_t state = __atomic_load_n(&g_images[index].state,
                                                __ATOMIC_ACQUIRE);
        if (state == 1u || state == 2u)
            return 1;
    }
    return 0;
}

static struct RinVkQueue_T* queue_slot(RinVkQueue queue) {
    uint32_t index;
    uint32_t queue_index;

    if (!queue) return NULL;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        struct RinVkDevice_T* device = &g_devices[index];
        if (__atomic_load_n(&device->state, __ATOMIC_ACQUIRE) != 1u)
            continue;
        for (queue_index = 0u; queue_index < device->queue_count;
             ++queue_index) {
            struct RinVkQueue_T* candidate = &device->queues[queue_index];
            if (queue == candidate &&
                (candidate->loader_magic & UINT32_MAX) ==
                    RIN_VK_ICD_LOADER_MAGIC &&
                candidate->device == device &&
                candidate->queue_index == queue_index) {
                return candidate;
            }
        }
    }
    return NULL;
}

static int submission_slots_active(void) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const uint32_t state = __atomic_load_n(&g_submissions[index].state,
                                                __ATOMIC_ACQUIRE);
        if (state == 1u || state == 2u) return 1;
    }
    return 0;
}

static int device_submission_slots_active(
        const struct RinVkDevice_T* device) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* slot = &g_submissions[index];
        const uint32_t state = __atomic_load_n(&slot->state,
                                                __ATOMIC_ACQUIRE);
        if ((state == 1u || state == 2u) && slot->owner == device) {
            return 1;
        }
    }
    return 0;
}

static RinVkSubmissionSlot* reserve_submission_slot(void) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        RinVkSubmissionSlot* slot = &g_submissions[index];
        uint32_t expected = 0u;
        if (__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            memset(slot, 0, sizeof(*slot));
            __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
            return slot;
        }
    }
    return NULL;
}

static void clear_submission_slot(RinVkSubmissionSlot* slot) {
    memset(slot, 0, sizeof(*slot));
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_image_slot(RinVkImageSlot* slot) {
    slot->owner = NULL;
    slot->memory = NULL;
    slot->memory_generation = 0u;
    slot->format = 0;
    slot->usage = 0u;
    slot->memory_size = 0u;
    slot->memory_offset = 0u;
    slot->width = 0u;
    slot->height = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_image_view_slot(RinVkImageViewSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    slot->image = NULL;
    slot->format = 0u;
    slot->aspect_mask = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_sampler_slot(RinVkSamplerSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    slot->mag_filter = 0u;
    slot->min_filter = 0u;
    slot->mipmap_mode = 0u;
    slot->address_mode_u = 0u;
    slot->address_mode_v = 0u;
    slot->address_mode_w = 0u;
    slot->compare_enable = 0u;
    slot->compare_op = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_pipeline_layout_slot(RinVkPipelineLayoutSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    slot->set_layout_count = 0u;
    memset(slot->set_layouts, 0, sizeof(slot->set_layouts));
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static int image_has_views(const RinVkImageSlot* image) {
    uint32_t index;
    if (!image) return 0;
    for (index = 0u; index < RIN_VK_MAX_IMAGE_VIEWS; ++index)
        if (__atomic_load_n(&g_image_views[index].state, __ATOMIC_ACQUIRE) ==
                1u &&
            g_image_views[index].image == image)
            return 1;
    return 0;
}

static int device_view_sampler_active(
        const struct RinVkDevice_T* device) {
    uint32_t index;
    if (!device) return 0;
    for (index = 0u; index < RIN_VK_MAX_IMAGE_VIEWS; ++index)
        if ((__atomic_load_n(&g_image_views[index].state, __ATOMIC_ACQUIRE) ==
                 1u ||
             __atomic_load_n(&g_image_views[index].state, __ATOMIC_ACQUIRE) ==
                 2u) &&
            g_image_views[index].owner == device)
            return 1;
    for (index = 0u; index < RIN_VK_MAX_SAMPLERS; ++index)
        if ((__atomic_load_n(&g_samplers[index].state, __ATOMIC_ACQUIRE) ==
                 1u ||
             __atomic_load_n(&g_samplers[index].state, __ATOMIC_ACQUIRE) ==
                 2u) &&
            g_samplers[index].owner == device)
            return 1;
    for (index = 0u; index < RIN_VK_MAX_PIPELINE_LAYOUTS; ++index)
        if ((__atomic_load_n(&g_pipeline_layouts[index].state,
                             __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&g_pipeline_layouts[index].state,
                             __ATOMIC_ACQUIRE) == 2u) &&
            g_pipeline_layouts[index].owner == device)
            return 1;
    return 0;
}

static uint32_t descriptor_runtime_type(uint32_t type) {
    switch (type) {
    case RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return RIN_GPU_VULKAN_DESCRIPTOR_UNIFORM_BUFFER;
    case RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_BUFFER;
    case RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return RIN_GPU_VULKAN_DESCRIPTOR_SAMPLED_IMAGE;
    case RIN_VK_DESCRIPTOR_TYPE_SAMPLER:
        return RIN_GPU_VULKAN_DESCRIPTOR_SAMPLER;
    default:
        return 0u;
    }
}

static RinVkResult map_descriptor_result(int result) {
    switch (result) {
    case RIN_GPU_VULKAN_GRAPHICS_OK:
        return RIN_VK_SUCCESS;
    case RIN_GPU_VULKAN_GRAPHICS_LIMIT:
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    case RIN_GPU_VULKAN_GRAPHICS_INCOMPATIBLE:
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    case RIN_GPU_VULKAN_GRAPHICS_INVALID_ARGUMENT:
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    default:
        return RIN_VK_ERROR_UNKNOWN;
    }
}

static volatile uint32_t g_sync_lock;

static int sync_try_lock(void) {
    return __atomic_exchange_n(&g_sync_lock, 1u, __ATOMIC_ACQUIRE) == 0u;
}

static void sync_unlock(void) {
    __atomic_store_n(&g_sync_lock, 0u, __ATOMIC_RELEASE);
}

static int semaphore_list_contains(const RinVkSemaphore* list,
                                   uint32_t count, RinVkSemaphore handle) {
    uint32_t index;
    if (!list || handle == 0u) return 0;
    for (index = 0u; index < count; ++index)
        if (list[index] == handle) return 1;
    return 0;
}

static int fence_list_contains(const RinVkFence* list, uint32_t count,
                               RinVkFence handle) {
    uint32_t index;
    if (!list || handle == 0u) return 0;
    for (index = 0u; index < count; ++index)
        if (list[index] == handle) return 1;
    return 0;
}

static int queue_submission_slots_active(
        const struct RinVkDevice_T* device, uint32_t queue_id) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* slot = &g_submissions[index];
        const uint32_t state = __atomic_load_n(&slot->state,
                                                __ATOMIC_ACQUIRE);
        if ((state == 1u || state == 2u) && slot->owner == device &&
            slot->queue_id == queue_id) {
            return 1;
        }
    }
    return 0;
}

static void complete_submission_sync(const RinVkSubmissionSlot* submission) {
    RinVkFenceSlot* fence;
    uint32_t index;

    if (!submission || !submission->owner) return;
    fence = fence_slot((RinVkDevice)submission->owner, submission->fence);
    if (fence && __atomic_load_n(&fence->pending, __ATOMIC_ACQUIRE) != 0u) {
        __atomic_store_n(&fence->pending, 0u, __ATOMIC_RELEASE);
        __atomic_store_n(&fence->signaled, 1u, __ATOMIC_RELEASE);
    }
    for (index = 0u; index < submission->signal_semaphore_count; ++index) {
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)submission->owner,
            submission->signal_semaphores[index]);
        if (semaphore &&
            __atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u) {
            __atomic_store_n(&semaphore->pending, 0u, __ATOMIC_RELEASE);
            if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
                __atomic_store_n(
                    &semaphore->value,
                    submission->signal_semaphore_values[index],
                    __ATOMIC_RELEASE);
            } else {
                __atomic_store_n(&semaphore->signaled, 1u,
                                 __ATOMIC_RELEASE);
            }
        }
    }
}

static void cancel_submission_sync(const RinVkSubmissionSlot* submission) {
    uint32_t index;
    RinVkFenceSlot* fence;

    if (!submission || !submission->owner) return;
    fence = fence_slot((RinVkDevice)submission->owner, submission->fence);
    if (fence) __atomic_store_n(&fence->pending, 0u, __ATOMIC_RELEASE);
    for (index = 0u; index < submission->signal_semaphore_count; ++index) {
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)submission->owner,
            submission->signal_semaphores[index]);
        if (semaphore)
            __atomic_store_n(&semaphore->pending, 0u, __ATOMIC_RELEASE);
    }
}

static void cleanup_device_sync_objects(struct RinVkDevice_T* device) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_FENCES; ++index) {
        RinVkFenceSlot* fence = &g_fences[index];
        if ((__atomic_load_n(&fence->state, __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&fence->state, __ATOMIC_ACQUIRE) == 2u) &&
            fence->owner == device) {
            clear_fence_slot(fence);
        }
    }
    for (index = 0u; index < RIN_VK_MAX_SEMAPHORES; ++index) {
        RinVkSemaphoreSlot* semaphore = &g_semaphores[index];
        if ((__atomic_load_n(&semaphore->state, __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&semaphore->state, __ATOMIC_ACQUIRE) == 2u) &&
            semaphore->owner == device) {
            clear_semaphore_slot(semaphore);
        }
    }
}

static int append_submission_resource(
    RinVulkanProductResourceV1* resources, uint32_t* resource_count,
    uint64_t allocation, uint32_t required_access) {
    uint32_t index;

    if (!resources || !resource_count || allocation == 0u ||
        required_access == 0u) {
        return 0;
    }
    for (index = 0u; index < *resource_count; ++index) {
        if (resources[index].allocation_handle == allocation) {
            resources[index].required_gpu_access |= required_access;
            return 1;
        }
    }
    if (*resource_count >= RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION) {
        return 0;
    }
    resources[*resource_count].allocation_handle = allocation;
    resources[*resource_count].required_gpu_access = required_access;
    resources[*resource_count].reserved = 0u;
    ++*resource_count;
    return 1;
}

static void abort_device_submissions(struct RinVkDevice_T* device) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        RinVkSubmissionSlot* slot = &g_submissions[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
            slot->owner != device) {
            continue;
        }
        rin_gpu_vulkan_command_buffers_abort(
            &g_command_runtime, slot->command_buffer_count,
            slot->command_buffers);
        cancel_submission_sync(slot);
        clear_submission_slot(slot);
    }
}

static RinVkResult maintain_device_submissions(struct RinVkDevice_T* device) {
    RinVulkanProductPlatformV1* product;
    RinVulkanProductReportV1 report;
    uint32_t index;
    int result;

    if (!device) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    if (!submission_slots_active()) {
        sync_unlock();
        return RIN_VK_SUCCESS;
    }
    product = acquire_product();
    if (!product || !product_matches_device(product, device)) {
        if (product) release_product();
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    memset(&report, 0, sizeof(report));
    result = product->poll(product->context, &report);
    release_product();
    if (result != RIN_VULKAN_PRODUCT_OK) {
        const RinVkResult mapped = map_product_result(result);
        if (mapped == RIN_VK_ERROR_DEVICE_LOST) {
            abort_device_submissions(device);
        }
        sync_unlock();
        return mapped;
    }
    if (report.struct_size != sizeof(report) ||
        report.version != RIN_VULKAN_PRODUCT_PLATFORM_VERSION ||
        (report.flags & ~RIN_VULKAN_PRODUCT_REPORT_KNOWN) != 0u) {
        abort_device_submissions(device);
        sync_unlock();
        return RIN_VK_ERROR_DEVICE_LOST;
    }
    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        RinVkSubmissionSlot* slot = &g_submissions[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
            slot->owner != device) {
            continue;
        }
        if (slot->queue_id >= RIN_VULKAN_PRODUCT_MAX_QUEUES ||
            slot->completion_value == 0u ||
            slot->completion_value > report.completed_values[slot->queue_id]) {
            continue;
        }
        if (rin_gpu_vulkan_command_buffers_complete(
                &g_command_runtime, slot->command_buffer_count,
                slot->command_buffers) != RIN_GPU_VULKAN_COMMAND_OK) {
            abort_device_submissions(device);
            sync_unlock();
            return RIN_VK_ERROR_DEVICE_LOST;
        }
        complete_submission_sync(slot);
        clear_submission_slot(slot);
    }
    if ((report.flags & (RIN_VULKAN_PRODUCT_REPORT_RESET |
                         RIN_VULKAN_PRODUCT_REPORT_WATCHDOG |
                         RIN_VULKAN_PRODUCT_REPORT_DEVICE_FAULT)) != 0u) {
        abort_device_submissions(device);
        sync_unlock();
        return RIN_VK_ERROR_DEVICE_LOST;
    }
    sync_unlock();
    return RIN_VK_SUCCESS;
}

/* Device destruction is allowed to invalidate child buffers, but it never
 * pretends a product allocation was released when its exact destroy callback
 * failed.  The device remains live for an explicit retry in that case. */
static int cleanup_device_resources(struct RinVkDevice_T* device) {
    RinVulkanProductPlatformV1* product = NULL;
    uint32_t index;
    int has_memory = 0;
    for (index = 0u; index < RIN_VK_MAX_BUFFERS; ++index) {
        RinVkBufferSlot* buffer = &g_buffers[index];
        if (__atomic_load_n(&buffer->state, __ATOMIC_ACQUIRE) == 1u &&
            buffer->owner == device) {
            if (buffer->memory && buffer->memory_generation ==
                                      buffer->memory->generation &&
                buffer->memory->bound_resource_count != 0u)
                --buffer->memory->bound_resource_count;
            clear_buffer_slot(buffer);
        }
    }
    for (index = 0u; index < RIN_VK_MAX_IMAGES; ++index) {
        RinVkImageSlot* image = &g_images[index];
        if (__atomic_load_n(&image->state, __ATOMIC_ACQUIRE) == 1u &&
            image->owner == device) {
            if (image->memory && image->memory_generation ==
                                      image->memory->generation &&
                image->memory->bound_resource_count != 0u)
                --image->memory->bound_resource_count;
            clear_image_slot(image);
        }
    }
    for (index = 0u; index < RIN_VK_MAX_MEMORIES; ++index) {
        if (__atomic_load_n(&g_memories[index].state,
                            __ATOMIC_ACQUIRE) == 1u &&
            g_memories[index].owner == device) {
            has_memory = 1;
            break;
        }
    }
    if (!has_memory) return 1;
    product = acquire_product();
    if (!product || !product_matches_device(product, device)) {
        if (product) release_product();
        return 0;
    }
    for (index = 0u; index < RIN_VK_MAX_MEMORIES; ++index) {
        RinVkMemorySlot* memory = &g_memories[index];
        if (__atomic_load_n(&memory->state, __ATOMIC_ACQUIRE) != 1u ||
            memory->owner != device)
            continue;
        if (memory->bound_resource_count != 0u ||
            product->destroy_allocation(
                product->context, memory->product_allocation) !=
                RIN_VULKAN_PRODUCT_OK) {
            release_product();
            return 0;
        }
        clear_memory_slot(memory);
    }
    release_product();
    return 1;
}

static int call_create_instance(RinGpuVulkanRuntimeV1* runtime,
                                const RinGpuVulkanInstanceRequestV1* request,
                                RinGpuVulkanHandle* handle) {
    uint32_t attempt;
    int result = RIN_GPU_VULKAN_BUSY;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        result = rin_gpu_vulkan_create_instance(runtime, request, handle);
        if (result != RIN_GPU_VULKAN_BUSY) break;
    }
    return result;
}

static int call_destroy_instance(RinGpuVulkanRuntimeV1* runtime,
                                 RinGpuVulkanHandle handle) {
    uint32_t attempt;
    int result = RIN_GPU_VULKAN_BUSY;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        result = rin_gpu_vulkan_destroy_instance(runtime, handle);
        if (result != RIN_GPU_VULKAN_BUSY) break;
    }
    return result;
}

static int call_enumerate_physical(
        RinGpuVulkanRuntimeV1* runtime, RinGpuVulkanHandle instance,
        uint32_t* count, RinGpuVulkanHandle* handles) {
    uint32_t attempt;
    int result = RIN_GPU_VULKAN_BUSY;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        uint32_t capacity = *count;
        result = rin_gpu_vulkan_enumerate_physical_devices(
            runtime, instance, count, handles);
        if (result != RIN_GPU_VULKAN_BUSY) break;
        *count = capacity;
    }
    return result;
}

static int call_query_physical(
        RinGpuVulkanRuntimeV1* runtime, RinGpuVulkanHandle instance,
        RinGpuVulkanHandle physical,
        RinGpuVulkanPhysicalDeviceV2* profile) {
    uint32_t attempt;
    int result = RIN_GPU_VULKAN_BUSY;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        result = rin_gpu_vulkan_query_physical_device(
            runtime, instance, physical, profile);
        if (result != RIN_GPU_VULKAN_BUSY) break;
    }
    return result;
}

static int call_create_device(
        RinGpuVulkanRuntimeV1* runtime, RinGpuVulkanHandle instance,
        RinGpuVulkanHandle physical,
        const RinGpuVulkanCreateRequestV1* request,
        RinGpuVulkanHandle* device, RinGpuVulkanDevicePlanV1* plan) {
    uint32_t attempt;
    int result = RIN_GPU_VULKAN_BUSY;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        result = rin_gpu_vulkan_create_device(runtime, instance, physical,
                                              request, device, plan);
        if (result != RIN_GPU_VULKAN_BUSY) break;
    }
    return result;
}

static int call_destroy_device(RinGpuVulkanRuntimeV1* runtime,
                               RinGpuVulkanHandle instance,
                               RinGpuVulkanHandle device) {
    uint32_t attempt;
    int result = RIN_GPU_VULKAN_BUSY;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        result = rin_gpu_vulkan_destroy_device(runtime, instance, device);
        if (result != RIN_GPU_VULKAN_BUSY) break;
    }
    return result;
}

int rin_gpu_vulkan_icd_bind_runtime(RinGpuVulkanRuntimeV1* runtime) {
    uintptr_t expected = 0u;
    uintptr_t binding = (uintptr_t)runtime;
    if (!runtime || binding == RIN_VK_ICD_BINDING_TRANSITION ||
        runtime->struct_size != sizeof(*runtime) ||
        runtime->version != RIN_GPU_VULKAN_RUNTIME_VERSION ||
        __atomic_load_n(&runtime->initialized, __ATOMIC_ACQUIRE) != 1u)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&g_runtime_binding, &expected, binding,
                                     0, __ATOMIC_RELEASE,
                                     __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_unbind_runtime(RinGpuVulkanRuntimeV1* runtime) {
    uintptr_t expected;
    uint32_t index;
    if (!runtime) return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (__atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) != 0u)
        return RIN_GPU_VULKAN_BUSY;
    expected = (uintptr_t)runtime;
    if (!__atomic_compare_exchange_n(
            &g_runtime_binding, &expected, RIN_VK_ICD_BINDING_TRANSITION, 0,
            __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        return expected == 0u ? RIN_GPU_VULKAN_NOT_INITIALIZED
                              : RIN_GPU_VULKAN_BUSY;
    if (__atomic_load_n(&g_active_calls, __ATOMIC_ACQUIRE) != 0u) {
        __atomic_store_n(&g_runtime_binding, (uintptr_t)runtime,
                         __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_BUSY;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        if (__atomic_load_n(&g_instances[index].state,
                            __ATOMIC_ACQUIRE) != 0u) {
            __atomic_store_n(&g_runtime_binding, (uintptr_t)runtime,
                             __ATOMIC_RELEASE);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        if (__atomic_load_n(&g_devices[index].state,
                            __ATOMIC_ACQUIRE) != 0u) {
            __atomic_store_n(&g_runtime_binding, (uintptr_t)runtime,
                             __ATOMIC_RELEASE);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    __atomic_store_n(&g_runtime_binding, 0u, __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_bind_product_platform(
    RinVulkanProductPlatformV1* platform) {
    RinVulkanProductStatusV1 status;
    uintptr_t expected = 0u;
    uintptr_t binding = (uintptr_t)platform;
    uintptr_t runtime_binding =
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE);
    if (!platform || binding == RIN_VK_ICD_BINDING_TRANSITION ||
        runtime_binding == 0u ||
        runtime_binding == RIN_VK_ICD_BINDING_TRANSITION ||
        platform->struct_size != sizeof(*platform) ||
        platform->version != RIN_VULKAN_PRODUCT_PLATFORM_VERSION ||
        !platform->context || !platform->get_status || !platform->poll ||
        !platform->destroy_allocation || !platform->prepare_submission ||
        !platform->submit || !platform->allocate ||
        !platform->query_allocation ||
        platform->get_status(platform->context, &status) !=
            RIN_VULKAN_PRODUCT_OK ||
        status.struct_size != sizeof(status) ||
        status.version != RIN_VULKAN_PRODUCT_PLATFORM_VERSION ||
        status.flags != RIN_VULKAN_PRODUCT_STATUS_READY ||
        status.queue_count == 0u || status.pending_submission_count != 0u ||
        status.active_resource_lease_count != 0u ||
        status.completed_values[0] != 0u)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&g_product_binding, &expected, binding,
                                     0, __ATOMIC_RELEASE,
                                     __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_unbind_product_platform(
    RinVulkanProductPlatformV1* platform) {
    uintptr_t expected;
    if (!platform) return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    expected = (uintptr_t)platform;
    if (!__atomic_compare_exchange_n(
            &g_product_binding, &expected, RIN_VK_ICD_BINDING_TRANSITION, 0,
            __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        return expected == 0u ? RIN_GPU_VULKAN_NOT_INITIALIZED
                              : RIN_GPU_VULKAN_BUSY;
    if (__atomic_load_n(&g_active_product_calls, __ATOMIC_ACQUIRE) != 0u ||
        resource_slots_active() || submission_slots_active()) {
        __atomic_store_n(&g_product_binding, (uintptr_t)platform,
                         __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_BUSY;
    }
    __atomic_store_n(&g_product_binding, 0u, __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_OK;
}

RinVkResult rin_gpu_vulkan_icd_maintain(RinVkDevice device) {
    struct RinVkDevice_T* slot = device_slot(device);
    RinVkResult result;
    uint32_t index;

    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < slot->queue_count; ++index) {
        if (__atomic_exchange_n(&slot->queues[index].submit_lock, 1u,
                                __ATOMIC_ACQUIRE) != 0u) {
            while (index != 0u) {
                --index;
                __atomic_store_n(&slot->queues[index].submit_lock, 0u,
                                 __ATOMIC_RELEASE);
            }
            return RIN_VK_NOT_READY;
        }
    }
    result = maintain_device_submissions(slot);
    while (index != 0u) {
        --index;
        __atomic_store_n(&slot->queues[index].submit_lock, 0u,
                         __ATOMIC_RELEASE);
    }
    return result;
}

RinVkResult RIN_VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* version) {
    if (!version) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (*version < 2u) return RIN_VK_ERROR_INCOMPATIBLE_DRIVER;
    if (*version > RIN_VK_ICD_INTERFACE_VERSION)
        *version = RIN_VK_ICD_INTERFACE_VERSION;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkEnumerateInstanceVersion(
        uint32_t* api_version) {
    if (!api_version) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *api_version = RIN_GPU_VK_ICD_API_VERSION;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkEnumerateInstanceExtensionProperties(
        const char* layer_name, uint32_t* property_count,
        RinVkExtensionProperties* properties) {
    (void)properties;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *property_count = 0u;
    return layer_name ? RIN_VK_ERROR_LAYER_NOT_PRESENT : RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkEnumerateDeviceExtensionProperties(
        RinVkPhysicalDevice physical_device, const char* layer_name,
        uint32_t* property_count, RinVkExtensionProperties* properties) {
    RinVkExtensionProperties extension;
    uint32_t capacity;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!physical_slot(physical_device, NULL))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (layer_name) {
        *property_count = 0u;
        return RIN_VK_ERROR_LAYER_NOT_PRESENT;
    }
    capacity = *property_count;
    *property_count = properties ? (capacity == 0u ? 0u : 1u) : 1u;
    if (!properties || capacity == 0u) {
        return properties && capacity == 0u ? RIN_VK_INCOMPLETE
                                            : RIN_VK_SUCCESS;
    }
    memset(&extension, 0, sizeof(extension));
    memcpy(extension.extensionName, RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION,
           sizeof(RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION));
    extension.specVersion = 2u;
    properties[0] = extension;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkEnumerateInstanceLayerProperties(
        uint32_t* property_count, RinVkLayerProperties* properties) {
    (void)properties;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *property_count = 0u;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateInstance(
        const RinVkInstanceCreateInfo* create_info, const void* allocator,
        RinVkInstance* instance_out) {
    RinGpuVulkanInstanceRequestV1 request;
    RinGpuVulkanRuntimeV1* runtime;
    struct RinVkInstance_T* slot = NULL;
    uint32_t index;
    uint32_t expected;
    int result;
    (void)allocator;

    if (!instance_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *instance_out = NULL;
    if (!create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->pNext || create_info->flags != 0u)
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (create_info->enabledLayerCount != 0u)
        return RIN_VK_ERROR_LAYER_NOT_PRESENT;
    if (create_info->enabledExtensionCount != 0u)
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;

    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = RIN_GPU_VULKAN_RUNTIME_VERSION;
    request.api_version = RIN_GPU_VK_MAKE_VERSION(1u, 0u, 0u);
    if (create_info->pApplicationInfo) {
        const RinVkApplicationInfo* application =
            create_info->pApplicationInfo;
        if (application->sType != RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO ||
            application->pNext)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        request.api_version = application->apiVersion != 0u
                                  ? application->apiVersion
                                  : RIN_GPU_VK_MAKE_VERSION(1u, 0u, 0u);
        if (!api_version_supported(request.api_version))
            return RIN_VK_ERROR_INCOMPATIBLE_DRIVER;
        request.application_version = application->applicationVersion;
        request.engine_version = application->engineVersion;
        if (!copy_name(request.application_name,
                       application->pApplicationName) ||
            !copy_name(request.engine_name, application->pEngineName))
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }

    runtime = acquire_runtime();
    if (!runtime) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        expected = 0u;
        if (__atomic_compare_exchange_n(&g_instances[index].state,
                                        &expected, 2u, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            slot = &g_instances[index];
            break;
        }
    }
    if (!slot) {
        release_runtime();
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    }
    memset(slot->physical_devices, 0, sizeof(slot->physical_devices));
    slot->loader_magic = RIN_VK_ICD_LOADER_MAGIC;
    slot->reserved = 0u;
    slot->runtime_handle = 0u;
    result = call_create_instance(runtime, &request, &slot->runtime_handle);
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK) {
        slot->loader_magic = 0u;
        slot->runtime_handle = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return map_result(result);
    }
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *instance_out = slot;
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyInstance(RinVkInstance instance,
                                      const void* allocator) {
    struct RinVkInstance_T* slot = instance_slot(instance);
    RinGpuVulkanRuntimeV1* runtime;
    uint32_t expected = 1u;
    int result;
    (void)allocator;
    if (!slot ||
        !__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                     __ATOMIC_ACQUIRE,
                                     __ATOMIC_RELAXED))
        return;
    runtime = acquire_runtime();
    if (!runtime) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    result = call_destroy_instance(runtime, slot->runtime_handle);
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    memset(slot->physical_devices, 0, sizeof(slot->physical_devices));
    slot->runtime_handle = 0u;
    slot->loader_magic = 0u;
    slot->reserved = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

RinVkResult RIN_VKAPI_CALL vkEnumeratePhysicalDevices(
        RinVkInstance instance, uint32_t* physical_device_count,
        RinVkPhysicalDevice* physical_devices) {
    struct RinVkInstance_T* slot = instance_slot(instance);
    RinGpuVulkanRuntimeV1* runtime;
    RinGpuVulkanHandle handles[RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
    uint32_t capacity;
    uint32_t count;
    uint32_t index;
    int result;
    if (!physical_device_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *physical_device_count;
    *physical_device_count = 0u;
    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    runtime = acquire_runtime();
    if (!runtime) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!physical_devices) {
        count = 0u;
        result = call_enumerate_physical(runtime, slot->runtime_handle,
                                         &count, NULL);
    } else {
        count = capacity < RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES
                    ? capacity
                    : RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES;
        memset(handles, 0, sizeof(handles));
        result = call_enumerate_physical(runtime, slot->runtime_handle,
                                         &count, handles);
    }
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK &&
        result != RIN_GPU_VULKAN_INCOMPLETE)
        return map_result(result);
    if (physical_devices) {
        for (index = 0u; index < count; ++index) {
            struct RinVkPhysicalDevice_T* physical =
                &slot->physical_devices[index];
            physical->loader_magic = RIN_VK_ICD_LOADER_MAGIC;
            physical->owner_instance = slot->runtime_handle;
            physical->runtime_handle = handles[index];
            physical_devices[index] = physical;
        }
    }
    *physical_device_count = count;
    return map_result(result);
}

static int get_physical_profile(
        RinVkPhysicalDevice physical_device,
        RinGpuVulkanPhysicalDeviceV2* profile) {
    struct RinVkInstance_T* instance;
    struct RinVkPhysicalDevice_T* physical =
        physical_slot(physical_device, &instance);
    RinGpuVulkanRuntimeV1* runtime;
    int result;
    if (!profile) return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    memset(profile, 0, sizeof(*profile));
    if (!physical) return RIN_GPU_VULKAN_INVALID_HANDLE;
    runtime = acquire_runtime();
    if (!runtime) return RIN_GPU_VULKAN_NOT_INITIALIZED;
    result = call_query_physical(runtime, instance->runtime_handle,
                                 physical->runtime_handle, profile);
    release_runtime();
    return result;
}

void RIN_VKAPI_CALL vkGetPhysicalDeviceFeatures(
        RinVkPhysicalDevice physical_device,
        RinVkPhysicalDeviceFeatures* features) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    if (!features) return;
    memset(features, 0, sizeof(*features));
    if (get_physical_profile(physical_device, &profile) !=
        RIN_GPU_VULKAN_OK)
        return;
    publish_legacy_features(&profile, features);
}

void RIN_VKAPI_CALL vkGetPhysicalDeviceFeatures2(
        RinVkPhysicalDevice physical_device,
        RinVkPhysicalDeviceFeatures2* features) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinVkFeatureChain chain;
    if (!features) return;
    memset(&features->features, 0, sizeof(features->features));
    if (features->sType != RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 ||
        !collect_feature_chain(features->pNext, 1, &chain))
        return;
    if (chain.vulkan12) {
        memset(&chain.vulkan12->samplerMirrorClampToEdge, 0,
               offsetof(RinVkPhysicalDeviceVulkan12Features,
                        subgroupBroadcastDynamicId) +
                   sizeof(chain.vulkan12->subgroupBroadcastDynamicId) -
                   offsetof(RinVkPhysicalDeviceVulkan12Features,
                            samplerMirrorClampToEdge));
    }
    if (chain.vulkan13) {
        memset(&chain.vulkan13->robustImageAccess, 0,
               offsetof(RinVkPhysicalDeviceVulkan13Features,
                        maintenance4) +
                   sizeof(chain.vulkan13->maintenance4) -
                   offsetof(RinVkPhysicalDeviceVulkan13Features,
                            robustImageAccess));
    }
    if (get_physical_profile(physical_device, &profile) !=
        RIN_GPU_VULKAN_OK)
        return;
    publish_legacy_features(&profile, &features->features);
    if (chain.vulkan12) {
        chain.vulkan12->descriptorIndexing = 0u;
        chain.vulkan12->timelineSemaphore =
            (profile.features & RIN_GPU_VK_ICD_FEATURES &
             RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE) != 0u;
        chain.vulkan12->bufferDeviceAddress = 0u;
    }
    if (chain.vulkan13) {
        chain.vulkan13->synchronization2 = 0u;
        chain.vulkan13->dynamicRendering = 0u;
        chain.vulkan13->maintenance4 = 0u;
    }
}

static void publish_legacy_properties(
        const RinGpuVulkanPhysicalDeviceV2* profile,
        RinVkPhysicalDeviceProperties* properties) {
    uint32_t index;
    memset(properties, 0, sizeof(*properties));
    properties->apiVersion = profile->api_version < RIN_GPU_VK_ICD_API_VERSION
                                 ? profile->api_version
                                 : RIN_GPU_VK_ICD_API_VERSION;
    properties->driverVersion = profile->driver_version;
    properties->vendorID = profile->vendor_id;
    properties->deviceID = profile->device_id;
    properties->deviceType = (RinVkPhysicalDeviceType)profile->device_type;
    memcpy(properties->deviceName, profile->device_name,
           sizeof(profile->device_name));
    memcpy(properties->pipelineCacheUUID, profile->pipeline_cache_uuid,
           sizeof(properties->pipelineCacheUUID));
    properties->limits.maxImageDimension2D =
        profile->max_image_dimension_2d;
    properties->limits.maxPushConstantsSize =
        profile->max_push_constants_size;
    properties->limits.maxMemoryAllocationCount =
        profile->max_memory_allocation_count;
    properties->limits.maxBoundDescriptorSets =
        profile->max_bound_descriptor_sets;
    properties->limits.maxPerStageResources =
        profile->max_per_stage_resources;
    properties->limits.maxSamplerAnisotropy =
        profile->max_sampler_anisotropy;
    properties->limits.timestampPeriod =
        (float)profile->timestamp_period_ns_x1000 / 1000.0f;
    for (index = 0u; index < profile->queue_family_count; ++index) {
        if ((profile->queue_families[index].flags &
             (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE)) ==
                (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE) &&
            profile->queue_families[index].timestamp_valid_bits != 0u) {
            properties->limits.timestampComputeAndGraphics = 1u;
            break;
        }
    }
}

void RIN_VKAPI_CALL vkGetPhysicalDeviceProperties(
        RinVkPhysicalDevice physical_device,
        RinVkPhysicalDeviceProperties* properties) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    if (!properties) return;
    memset(properties, 0, sizeof(*properties));
    if (get_physical_profile(physical_device, &profile) !=
        RIN_GPU_VULKAN_OK)
        return;
    publish_legacy_properties(&profile, properties);
}

void RIN_VKAPI_CALL vkGetPhysicalDeviceProperties2(
        RinVkPhysicalDevice physical_device,
        RinVkPhysicalDeviceProperties2* properties) {
    static const char driver_name[] = "RinGPU";
    static const char driver_info[] = "RinOS source-only profile";
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinVkPropertyChain chain;
    if (!properties) return;
    memset(&properties->properties, 0, sizeof(properties->properties));
    if (properties->sType !=
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2)
        return;
    if (!collect_property_chain(properties->pNext, &chain)) {
        zero_property_chain(&chain);
        return;
    }
    zero_property_chain(&chain);
    if (get_physical_profile(physical_device, &profile) !=
        RIN_GPU_VULKAN_OK)
        return;
    publish_legacy_properties(&profile, &properties->properties);
    (void)driver_name;
    (void)driver_info;
}

void RIN_VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
        RinVkPhysicalDevice physical_device, uint32_t* property_count,
        RinVkQueueFamilyProperties* properties) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    uint32_t capacity;
    uint32_t count;
    uint32_t index;
    if (!property_count) return;
    capacity = *property_count;
    *property_count = 0u;
    if (get_physical_profile(physical_device, &profile) !=
        RIN_GPU_VULKAN_OK)
        return;
    if (!properties) {
        *property_count = profile.queue_family_count;
        return;
    }
    count = capacity < profile.queue_family_count
                ? capacity
                : profile.queue_family_count;
    for (index = 0u; index < count; ++index) {
        memset(&properties[index], 0, sizeof(properties[index]));
        properties[index].queueFlags = profile.queue_families[index].flags &
            (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
             RIN_GPU_VK_QUEUE_TRANSFER);
        properties[index].queueCount =
            profile.queue_families[index].queue_count;
        properties[index].timestampValidBits =
            profile.queue_families[index].timestamp_valid_bits;
        properties[index].minImageTransferGranularity.width = 1u;
        properties[index].minImageTransferGranularity.height = 1u;
        properties[index].minImageTransferGranularity.depth = 1u;
    }
    *property_count = count;
}

void RIN_VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
        RinVkPhysicalDevice physical_device,
        RinVkPhysicalDeviceMemoryProperties* properties) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    uint32_t index;
    if (!properties) return;
    memset(properties, 0, sizeof(*properties));
    if (get_physical_profile(physical_device, &profile) !=
        RIN_GPU_VULKAN_OK)
        return;
    properties->memoryTypeCount = profile.memory_type_count;
    for (index = 0u; index < profile.memory_type_count; ++index) {
        properties->memoryTypes[index].propertyFlags =
            profile.memory_types[index].property_flags;
        properties->memoryTypes[index].heapIndex =
            profile.memory_types[index].heap_index;
    }
    properties->memoryHeapCount = profile.memory_heap_count;
    for (index = 0u; index < profile.memory_heap_count; ++index) {
        properties->memoryHeaps[index].size =
            profile.memory_heaps[index].size_bytes;
        properties->memoryHeaps[index].flags =
            profile.memory_heaps[index].flags;
    }
}

RinVkResult RIN_VKAPI_CALL vkCreateDevice(
        RinVkPhysicalDevice physical_device,
        const RinVkDeviceCreateInfo* create_info, const void* allocator,
        RinVkDevice* device_out) {
    struct RinVkInstance_T* instance;
    struct RinVkPhysicalDevice_T* physical;
    struct RinVkDevice_T* slot = NULL;
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanCreateRequestV1 request;
    RinGpuVulkanRuntimeV1* runtime;
    RinVkPhysicalDeviceFeatures features;
    RinVkFeatureChain feature_chain;
    uint64_t chain_features = 0u;
    const RinVkDeviceQueueCreateInfo* queue;
    uint32_t expected;
    uint32_t index;
    int result;
    (void)allocator;

    if (!device_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *device_out = NULL;
    physical = physical_slot(physical_device, &instance);
    if (!physical || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!collect_feature_chain((void*)create_info->pNext, 0,
                               &feature_chain) ||
        !requested_vulkan12_features(feature_chain.vulkan12,
                                     &chain_features) ||
        !requested_vulkan13_features(feature_chain.vulkan13,
                                     &chain_features))
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if ((chain_features & RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE) != 0u &&
        !timeline_extension_enabled(create_info))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (create_info->flags != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->enabledLayerCount != 0u)
        return RIN_VK_ERROR_LAYER_NOT_PRESENT;
    if (!device_extensions_valid(create_info))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (create_info->queueCreateInfoCount != 1u ||
        !create_info->pQueueCreateInfos)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    queue = &create_info->pQueueCreateInfos[0];
    if (queue->sType != RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO ||
        queue->pNext || queue->flags != 0u || queue->queueCount == 0u ||
        queue->queueCount > RIN_VULKAN_PRODUCT_MAX_QUEUES ||
        !queue->pQueuePriorities)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < queue->queueCount; ++index) {
        if (queue->pQueuePriorities[index] !=
                queue->pQueuePriorities[index] ||
            queue->pQueuePriorities[index] < 0.0f ||
            queue->pQueuePriorities[index] > 1.0f)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    memset(&features, 0, sizeof(features));
    if (create_info->pEnabledFeatures) {
        features = *create_info->pEnabledFeatures;
        if (features.samplerAnisotropy > 1u)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        features.samplerAnisotropy = 0u;
        if (!all_zero(&features, sizeof(features)))
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }

    runtime = acquire_runtime();
    if (!runtime) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = call_query_physical(runtime, instance->runtime_handle,
                                 physical->runtime_handle, &profile);
    if (result != RIN_GPU_VULKAN_OK) {
        release_runtime();
        return map_result(result);
    }
    if (queue->queueFamilyIndex >= profile.queue_family_count ||
        (profile.queue_families[queue->queueFamilyIndex].flags &
         (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
          RIN_GPU_VK_QUEUE_TRANSFER)) !=
         (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
          RIN_GPU_VK_QUEUE_TRANSFER)) {
        release_runtime();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    if (queue->queueCount > profile.queue_families[queue->queueFamilyIndex].queue_count) {
        release_runtime();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    for (index = 0u; index < queue->queueFamilyIndex; ++index) {
        if ((profile.queue_families[index].flags &
             (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
              RIN_GPU_VK_QUEUE_TRANSFER)) ==
            (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
             RIN_GPU_VK_QUEUE_TRANSFER)) {
            release_runtime();
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (create_info->pEnabledFeatures &&
        create_info->pEnabledFeatures->samplerAnisotropy != 0u &&
        (profile.features & RIN_GPU_VK_ICD_FEATURES &
         RIN_GPU_VK_FEATURE_SAMPLER_ANISOTROPY) == 0u) {
        release_runtime();
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }
    if ((RIN_GPU_VK_ICD_FEATURES & chain_features) != chain_features ||
        (profile.features & chain_features) != chain_features) {
        release_runtime();
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_DEVICES; ++index) {
        expected = 0u;
        if (__atomic_compare_exchange_n(&g_devices[index].state, &expected,
                                        2u, 0, __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            slot = &g_devices[index];
            break;
        }
    }
    if (!slot) {
        release_runtime();
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    }
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = RIN_GPU_VULKAN_PROFILE_VERSION;
    request.api_version = RIN_GPU_VK_API_1_3;
    request.max_in_flight = RIN_GPU_VULKAN_MAX_IN_FLIGHT;
    if (create_info->pEnabledFeatures &&
        create_info->pEnabledFeatures->samplerAnisotropy != 0u)
        request.required_features |=
            RIN_GPU_VK_FEATURE_SAMPLER_ANISOTROPY;
    request.required_features |= chain_features;
    memset(&slot->plan, 0, sizeof(slot->plan));
    memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
    slot->runtime_handle = 0u;
    slot->owner_instance = instance->runtime_handle;
    result = call_create_device(runtime, instance->runtime_handle,
                                physical->runtime_handle, &request,
                                &slot->runtime_handle, &slot->plan);
    if (result != RIN_GPU_VULKAN_OK) {
        release_runtime();
        memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
        slot->runtime_handle = 0u;
        slot->owner_instance = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return result == RIN_GPU_VULKAN_UNSUPPORTED
                   ? RIN_VK_ERROR_FEATURE_NOT_PRESENT
                   : map_result(result);
    }
    if (slot->plan.primary_queue_family != queue->queueFamilyIndex) {
        result = call_destroy_device(runtime, slot->owner_instance,
                                     slot->runtime_handle);
        release_runtime();
        if (result != RIN_GPU_VULKAN_OK) {
            slot->loader_magic = RIN_VK_ICD_LOADER_MAGIC;
            slot->reserved = 0u;
            __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
        memset(&slot->plan, 0, sizeof(slot->plan));
        memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
        slot->runtime_handle = 0u;
        slot->owner_instance = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    release_runtime();
    slot->loader_magic = RIN_VK_ICD_LOADER_MAGIC;
    slot->reserved = 0u;
    slot->timeline_enabled =
        (chain_features & RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE) != 0u;
    slot->physical_profile = profile;
    if (rin_gpu_vulkan_descriptor_runtime_init(
            &slot->descriptor_runtime,
            ((uint64_t)(uintptr_t)slot ^ slot->runtime_handle ^
             UINT64_C(0x9e3779b97f4a7c15))) !=
        RIN_GPU_VULKAN_GRAPHICS_OK) {
        runtime = acquire_runtime();
        if (runtime) {
            (void)call_destroy_device(runtime, slot->owner_instance,
                                       slot->runtime_handle);
            release_runtime();
        }
        memset(&slot->plan, 0, sizeof(slot->plan));
        memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
        slot->runtime_handle = 0u;
        slot->owner_instance = 0u;
        slot->loader_magic = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    __atomic_store_n(&slot->descriptor_validation_error, 0u,
                     __ATOMIC_RELEASE);
    slot->queue_count = queue->queueCount;
    slot->reserved_queue = 0u;
    memset(slot->queues, 0, sizeof(slot->queues));
    for (index = 0u; index < slot->queue_count; ++index) {
        slot->queues[index].loader_magic = RIN_VK_ICD_LOADER_MAGIC;
        slot->queues[index].device = slot;
        slot->queues[index].queue_family_index = queue->queueFamilyIndex;
        slot->queues[index].queue_index = index;
    }
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *device_out = slot;
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyDevice(RinVkDevice device,
                                    const void* allocator) {
    struct RinVkDevice_T* slot = device_slot(device);
    RinGpuVulkanRuntimeV1* runtime;
    uint32_t expected = 1u;
    int result;
    (void)allocator;
    if (!slot || maintain_device_submissions(slot) != RIN_VK_SUCCESS ||
        device_submission_slots_active(slot) ||
        !__atomic_compare_exchange_n(&slot->state, &expected, 2u, 0,
                                     __ATOMIC_ACQUIRE,
                                     __ATOMIC_RELAXED))
        return;
    if (!rin_gpu_vulkan_descriptor_runtime_is_empty(
            &slot->descriptor_runtime)) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    if (device_view_sampler_active(slot)) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    if (!cleanup_device_resources(slot)) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    runtime = acquire_runtime();
    if (!runtime) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    result = call_destroy_device(runtime, slot->owner_instance,
                                 slot->runtime_handle);
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK) {
        __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
        return;
    }
    (void)rin_gpu_vulkan_command_owner_cleanup(
        &g_command_runtime, (uintptr_t)slot);
    cleanup_device_sync_objects(slot);
    (void)rin_gpu_vulkan_descriptor_runtime_shutdown(
        &slot->descriptor_runtime);
    __atomic_store_n(&slot->descriptor_validation_error, 0u,
                     __ATOMIC_RELEASE);
    slot->timeline_enabled = 0u;
    memset(&slot->plan, 0, sizeof(slot->plan));
    memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
    memset(slot->queues, 0, sizeof(slot->queues));
    slot->queue_count = 0u;
    slot->reserved_queue = 0u;
    slot->runtime_handle = 0u;
    slot->owner_instance = 0u;
    slot->loader_magic = 0u;
    slot->reserved = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

void RIN_VKAPI_CALL vkGetDeviceQueue(RinVkDevice device,
                                     uint32_t queue_family_index,
                                     uint32_t queue_index,
                                     RinVkQueue* queue_out) {
    struct RinVkDevice_T* slot;
    if (!queue_out) return;
    *queue_out = NULL;
    slot = device_slot(device);
    if (!slot || queue_family_index != slot->plan.primary_queue_family ||
        queue_index >= slot->queue_count)
        return;
    *queue_out = &slot->queues[queue_index];
}

RinVkResult RIN_VKAPI_CALL vkDeviceWaitIdle(RinVkDevice device) {
    struct RinVkDevice_T* slot = device_slot(device);
    RinVkResult result;

    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions(slot);
    if (result != RIN_VK_SUCCESS) return result;
    return device_submission_slots_active(slot) ? RIN_VK_NOT_READY
                                                 : RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkQueueWaitIdle(RinVkQueue queue) {
    struct RinVkQueue_T* queue_value = queue_slot(queue);
    RinVkResult result;

    if (!queue_value) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions(queue_value->device);
    if (result != RIN_VK_SUCCESS) return result;
    return queue_submission_slots_active(queue_value->device,
                                         queue_value->queue_index)
               ? RIN_VK_NOT_READY
               : RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateFence(
        RinVkDevice device, const RinVkFenceCreateInfo* create_info,
        const void* allocator, RinVkFence* fence_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkFenceSlot* fence;
    uint32_t index;
    (void)allocator;

    if (!fence_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *fence_out = 0u;
    if (!owner || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO ||
        create_info->pNext ||
        (create_info->flags & ~RIN_VK_FENCE_CREATE_KNOWN) != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    fence = reserve_fence_slot(owner, &index);
    if (!fence) {
        sync_unlock();
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    __atomic_store_n(&fence->signaled,
                     (create_info->flags & RIN_VK_FENCE_CREATE_SIGNALED_BIT)
                         != 0u,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&fence->pending, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&fence->state, 1u, __ATOMIC_RELEASE);
    *fence_out = resource_handle(RIN_VK_FENCE_TAG, index,
                                 fence->generation);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyFence(RinVkDevice device, RinVkFence fence,
                                   const void* allocator) {
    RinVkFenceSlot* slot;
    (void)allocator;
    if (fence == 0u || !sync_try_lock()) return;
    slot = fence_slot(device, fence);
    if (slot) clear_fence_slot(slot);
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkResetFences(
        RinVkDevice device, uint32_t fence_count, const RinVkFence* fences) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    RinVkResult result = RIN_VK_SUCCESS;

    if (!owner || fence_count == 0u || !fences)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    for (index = 0u; index < fence_count; ++index) {
        RinVkFenceSlot* slot = fence_slot(owner, fences[index]);
        if (!slot) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            break;
        }
        if (fence_list_contains(fences, index, fences[index])) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            break;
        }
        if (__atomic_load_n(&slot->pending, __ATOMIC_ACQUIRE) != 0u) {
            result = RIN_VK_NOT_READY;
            break;
        }
    }
    if (result == RIN_VK_SUCCESS) {
        for (index = 0u; index < fence_count; ++index) {
            RinVkFenceSlot* slot = fence_slot(owner, fences[index]);
            __atomic_store_n(&slot->signaled, 0u, __ATOMIC_RELEASE);
        }
    }
    sync_unlock();
    return result;
}

RinVkResult RIN_VKAPI_CALL vkGetFenceStatus(RinVkDevice device,
                                             RinVkFence fence) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkFenceSlot* slot;
    RinVkResult result;

    if (!owner) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions(owner);
    if (result != RIN_VK_SUCCESS) return result;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    slot = fence_slot(owner, fence);
    if (!slot) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    result = __atomic_load_n(&slot->signaled, __ATOMIC_ACQUIRE) != 0u
                 ? RIN_VK_SUCCESS
                 : RIN_VK_NOT_READY;
    sync_unlock();
    return result;
}

RinVkResult RIN_VKAPI_CALL vkWaitForFences(
        RinVkDevice device, uint32_t fence_count, const RinVkFence* fences,
        uint32_t wait_all, uint64_t timeout) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    uint32_t signaled_count = 0u;
    RinVkResult result;
    (void)timeout;

    if (!owner || fence_count == 0u || !fences || wait_all > 1u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions(owner);
    if (result != RIN_VK_SUCCESS) return result;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    for (index = 0u; index < fence_count; ++index) {
        RinVkFenceSlot* slot = fence_slot(owner, fences[index]);
        if (!slot || fence_list_contains(fences, index, fences[index])) {
            sync_unlock();
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
        if (__atomic_load_n(&slot->signaled, __ATOMIC_ACQUIRE) != 0u)
            ++signaled_count;
    }
    if ((wait_all != 0u && signaled_count == fence_count) ||
        (wait_all == 0u && signaled_count != 0u)) {
        sync_unlock();
        return RIN_VK_SUCCESS;
    }
    sync_unlock();
    return timeout == 0u ? RIN_VK_NOT_READY : RIN_VK_TIMEOUT;
}

RinVkResult RIN_VKAPI_CALL vkCreateSemaphore(
        RinVkDevice device, const RinVkSemaphoreCreateInfo* create_info,
        const void* allocator, RinVkSemaphore* semaphore_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkSemaphoreSlot* semaphore;
    const RinVkSemaphoreTypeCreateInfo* type_info;
    uint32_t semaphore_type = RIN_VK_SEMAPHORE_TYPE_BINARY;
    uint64_t initial_value = 0u;
    uint32_t index;
    (void)allocator;

    if (!semaphore_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *semaphore_out = 0u;
    if (!owner || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO ||
        (create_info->flags & ~RIN_VK_SEMAPHORE_CREATE_KNOWN) != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    type_info = (const RinVkSemaphoreTypeCreateInfo*)create_info->pNext;
    if (type_info) {
        if (type_info->sType !=
                RIN_VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO ||
            type_info->pNext || type_info->semaphoreType !=
                RIN_VK_SEMAPHORE_TYPE_TIMELINE ||
            !owner->timeline_enabled)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        semaphore_type = type_info->semaphoreType;
        initial_value = type_info->initialValue;
    }
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    semaphore = reserve_semaphore_slot(owner, &index);
    if (!semaphore) {
        sync_unlock();
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    semaphore->type = semaphore_type;
    __atomic_store_n(&semaphore->signaled, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&semaphore->pending, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&semaphore->value, initial_value, __ATOMIC_RELEASE);
    semaphore->pending_value = 0u;
    __atomic_store_n(&semaphore->state, 1u, __ATOMIC_RELEASE);
    *semaphore_out = resource_handle(RIN_VK_SEMAPHORE_TAG, index,
                                     semaphore->generation);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroySemaphore(
        RinVkDevice device, RinVkSemaphore semaphore, const void* allocator) {
    RinVkSemaphoreSlot* slot;
    (void)allocator;
    if (semaphore == 0u || !sync_try_lock()) return;
    slot = semaphore_slot(device, semaphore);
    if (slot) clear_semaphore_slot(slot);
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkGetSemaphoreCounterValue(
        RinVkDevice device, RinVkSemaphore semaphore, uint64_t* value_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkSemaphoreSlot* slot;
    RinVkResult result;
    if (!owner || !value_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions(owner);
    if (result != RIN_VK_SUCCESS) return result;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    slot = semaphore_slot(owner, semaphore);
    if (!slot || slot->type != RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    *value_out = __atomic_load_n(&slot->value, __ATOMIC_ACQUIRE);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkSignalSemaphore(
        RinVkDevice device, RinVkSemaphore semaphore, uint64_t value) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkSemaphoreSlot* slot;
    uint64_t current;
    if (!owner) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    slot = semaphore_slot(owner, semaphore);
    if (!slot || slot->type != RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    if (__atomic_load_n(&slot->pending, __ATOMIC_ACQUIRE) != 0u) {
        sync_unlock();
        return RIN_VK_NOT_READY;
    }
    current = __atomic_load_n(&slot->value, __ATOMIC_ACQUIRE);
    if (value < current) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    __atomic_store_n(&slot->value, value, __ATOMIC_RELEASE);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkWaitSemaphores(
        RinVkDevice device, const RinVkSemaphoreWaitInfo* wait_info,
        uint64_t timeout) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    uint32_t satisfied = 0u;
    RinVkResult result;
    if (!owner || !wait_info ||
        wait_info->sType != RIN_VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO ||
        wait_info->pNext ||
        (wait_info->flags & ~RIN_VK_SEMAPHORE_WAIT_ANY_BIT) != 0u ||
        wait_info->semaphoreCount == 0u ||
        wait_info->semaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        !wait_info->pSemaphores || !wait_info->pValues)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions(owner);
    if (result != RIN_VK_SUCCESS) return result;
    if (!sync_try_lock()) return RIN_VK_NOT_READY;
    for (index = 0u; index < wait_info->semaphoreCount; ++index) {
        RinVkSemaphoreSlot* slot = semaphore_slot(
            owner, wait_info->pSemaphores[index]);
        uint64_t current;
        if (!slot || slot->type != RIN_VK_SEMAPHORE_TYPE_TIMELINE ||
            semaphore_list_contains(wait_info->pSemaphores, index,
                                    wait_info->pSemaphores[index])) {
            sync_unlock();
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
        current = __atomic_load_n(&slot->value, __ATOMIC_ACQUIRE);
        if (current >= wait_info->pValues[index]) ++satisfied;
    }
    if ((wait_info->flags == 0u && satisfied == wait_info->semaphoreCount) ||
        (wait_info->flags != 0u && satisfied != 0u)) {
        sync_unlock();
        return RIN_VK_SUCCESS;
    }
    sync_unlock();
    return timeout == 0u ? RIN_VK_NOT_READY : RIN_VK_TIMEOUT;
}

RinVkResult RIN_VKAPI_CALL vkCreateCommandPool(
        RinVkDevice device,
        const RinVkCommandPoolCreateInfo* create_info,
        const void* allocator, RinVkCommandPool* pool_out) {
    struct RinVkDevice_T* slot;
    RinVkCommandPoolCreateInfo snapshot;
    RinGpuVulkanCommandPoolHandleV1 core_pool = 0u;
    int result;
    (void)allocator;
    if (!pool_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!create_info) {
        *pool_out = (RinVkCommandPool)0;
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    snapshot = *create_info;
    *pool_out = (RinVkCommandPool)0;
    slot = device_slot(device);
    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (snapshot.sType != RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO ||
        snapshot.pNext)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if ((snapshot.flags & RIN_VK_COMMAND_POOL_CREATE_PROTECTED_BIT) != 0u)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if ((snapshot.flags &
         ~(RIN_VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
           RIN_VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)) != 0u ||
        snapshot.queueFamilyIndex != slot->plan.primary_queue_family)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = rin_gpu_vulkan_command_pool_create(
        &g_command_runtime, (uintptr_t)slot,
        snapshot.queueFamilyIndex, snapshot.flags, &core_pool);
    if (result != RIN_GPU_VULKAN_COMMAND_OK)
        return map_command_result(result);
    *pool_out = command_pool_from_core(core_pool);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyCommandPool(
        RinVkDevice device, RinVkCommandPool command_pool,
        const void* allocator) {
    struct RinVkDevice_T* slot = device_slot(device);
    (void)allocator;
    if (!slot || command_pool == (RinVkCommandPool)0) return;
    (void)rin_gpu_vulkan_command_pool_destroy(
        &g_command_runtime, (uintptr_t)slot,
        command_pool_to_core(command_pool));
}

RinVkResult RIN_VKAPI_CALL vkResetCommandPool(
        RinVkDevice device, RinVkCommandPool command_pool,
        uint32_t flags) {
    struct RinVkDevice_T* slot = device_slot(device);
    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    return map_command_result(rin_gpu_vulkan_command_pool_reset(
        &g_command_runtime, (uintptr_t)slot,
        command_pool_to_core(command_pool), flags));
}

RinVkResult RIN_VKAPI_CALL vkAllocateCommandBuffers(
        RinVkDevice device,
        const RinVkCommandBufferAllocateInfo* allocate_info,
        RinVkCommandBuffer* command_buffers) {
    struct RinVkDevice_T* slot;
    RinVkCommandBufferAllocateInfo snapshot;
    RinGpuVulkanCommandBufferV1*
        allocated[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];
    uint32_t index;
    int result;
    if (!allocate_info || !command_buffers)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    snapshot = *allocate_info;
    if (snapshot.commandBufferCount == 0u ||
        snapshot.commandBufferCount > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS)
        return snapshot.commandBufferCount >
                       RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS
                   ? RIN_VK_ERROR_OUT_OF_HOST_MEMORY
                   : RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < snapshot.commandBufferCount; ++index)
        command_buffers[index] = NULL;
    slot = device_slot(device);
    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (snapshot.sType !=
            RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO ||
        snapshot.pNext)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(allocated, 0, sizeof(allocated));
    result = rin_gpu_vulkan_command_buffers_allocate(
        &g_command_runtime, (uintptr_t)slot,
        command_pool_to_core(snapshot.commandPool),
        (uint32_t)snapshot.level, snapshot.commandBufferCount,
        RIN_VK_ICD_LOADER_MAGIC, allocated);
    if (result != RIN_GPU_VULKAN_COMMAND_OK)
        return map_command_result(result);
    for (index = 0u; index < snapshot.commandBufferCount; ++index)
        command_buffers[index] =
            (RinVkCommandBuffer)(void*)allocated[index];
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkFreeCommandBuffers(
        RinVkDevice device, RinVkCommandPool command_pool,
        uint32_t command_buffer_count,
        const RinVkCommandBuffer* command_buffers) {
    struct RinVkDevice_T* slot = device_slot(device);
    RinGpuVulkanCommandBufferV1*
        snapshot[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];
    uint32_t index;
    if (!slot || !command_buffers || command_buffer_count == 0u ||
        command_buffer_count > RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS)
        return;
    for (index = 0u; index < command_buffer_count; ++index)
        snapshot[index] =
            (RinGpuVulkanCommandBufferV1*)(void*)command_buffers[index];
    (void)rin_gpu_vulkan_command_buffers_free(
        &g_command_runtime, (uintptr_t)slot,
        command_pool_to_core(command_pool),
        command_buffer_count, snapshot);
}

RinVkResult RIN_VKAPI_CALL vkBeginCommandBuffer(
        RinVkCommandBuffer command_buffer,
        const RinVkCommandBufferBeginInfo* begin_info) {
    RinVkCommandBufferBeginInfo snapshot;
    if (!begin_info) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    snapshot = *begin_info;
    if (snapshot.sType != RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO ||
        snapshot.pNext)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    return map_command_result(rin_gpu_vulkan_command_buffer_begin(
        &g_command_runtime,
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer,
        snapshot.flags));
}

RinVkResult RIN_VKAPI_CALL vkEndCommandBuffer(
        RinVkCommandBuffer command_buffer) {
    return map_command_result(rin_gpu_vulkan_command_buffer_end(
        &g_command_runtime,
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer));
}

RinVkResult RIN_VKAPI_CALL vkResetCommandBuffer(
        RinVkCommandBuffer command_buffer, uint32_t flags) {
    return map_command_result(rin_gpu_vulkan_command_buffer_reset(
        &g_command_runtime,
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer, flags));
}

static int checked_buffer_address(const RinVkBufferSlot* buffer,
                                  uint64_t offset, uint64_t size,
                                  uint64_t* address_out) {
    uint64_t address;

    if (!buffer || !buffer->memory || !address_out ||
        offset > buffer->size || size > buffer->size - offset ||
        buffer->memory_generation != buffer->memory->generation ||
        buffer->memory->product_allocation == 0u ||
        buffer->memory->gpu_virtual_address == 0u ||
        buffer->memory_offset > UINT64_MAX -
                                    buffer->memory->gpu_virtual_address) {
        return 0;
    }
    address = buffer->memory->gpu_virtual_address + buffer->memory_offset;
    if (offset > UINT64_MAX - address ||
        size > UINT64_MAX - (address + offset)) {
        return 0;
    }
    *address_out = address + offset;
    return 1;
}

void RIN_VKAPI_CALL vkCmdCopyBuffer(
    RinVkCommandBuffer command_buffer, RinVkBuffer src_buffer,
    RinVkBuffer dst_buffer, uint32_t region_count,
    const RinVkBufferCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanBufferCopyCommandV1 copies[RIN_GPU_VULKAN_COMMAND_MAX_COPIES];
    RinVkBufferSlot* source;
    RinVkBufferSlot* destination;
    struct RinVkDevice_T* owner;
    uintptr_t owner_address = 0u;
    uint32_t copy_count = 0u;
    uint32_t index;
    int valid = 1;

    if (rin_gpu_vulkan_command_buffer_owner(
            &g_command_runtime, core, &owner_address) !=
            RIN_GPU_VULKAN_COMMAND_OK ||
        !(owner = device_slot((RinVkDevice)(void*)owner_address)) ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_COPIES ||
        (region_count != 0u && !regions)) {
        valid = 0;
        goto done;
    }
    source = buffer_slot((RinVkDevice)owner, src_buffer);
    destination = buffer_slot((RinVkDevice)owner, dst_buffer);
    if (!source || !destination || !source->memory || !destination->memory ||
        (source->usage & RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0u ||
        (destination->usage & RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0u) {
        valid = 0;
        goto done;
    }
    memset(copies, 0, sizeof(copies));
    for (index = 0u; index < region_count; ++index) {
        uint64_t source_address;
        uint64_t destination_address;

        if (!checked_buffer_address(source, regions[index].srcOffset,
                                    regions[index].size, &source_address) ||
            !checked_buffer_address(destination, regions[index].dstOffset,
                                    regions[index].size,
                                    &destination_address)) {
            valid = 0;
            goto done;
        }
        if (regions[index].size == 0u) continue;
        copies[copy_count].source_allocation =
            source->memory->product_allocation;
        copies[copy_count].destination_allocation =
            destination->memory->product_allocation;
        copies[copy_count].source_gpu_address = source_address;
        copies[copy_count].destination_gpu_address = destination_address;
        copies[copy_count].size_bytes = regions[index].size;
        ++copy_count;
    }
    if (copy_count != 0u && rin_gpu_vulkan_command_buffer_record_copies(
                               &g_command_runtime, core, copies,
                               copy_count) != RIN_GPU_VULKAN_COMMAND_OK) {
        valid = 0;
    }

done:
    if (!valid) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
    }
}

static int image_layout_transfer_valid(uint32_t layout) {
    return layout == RIN_VK_IMAGE_LAYOUT_GENERAL ||
           layout == RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
           layout == RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
}

static int image_subresource_valid(const RinVkImageSlot* image,
                                   const RinVkImageSubresourceLayers* subresource,
                                   uint32_t width, uint32_t height) {
    return image && subresource &&
           subresource->aspectMask == RIN_VK_IMAGE_ASPECT_COLOR_BIT &&
           subresource->mipLevel == 0u && subresource->baseArrayLayer == 0u &&
           image->samples == RIN_VK_SAMPLE_COUNT_1_BIT &&
           subresource->layerCount == 1u && width == image->width &&
           height == image->height;
}

static int image_region_valid(const RinVkImageSlot* image,
                              const RinVkImageSubresourceLayers* subresource,
                              const RinVkOffset3D* offset,
                              const RinVkExtent3D* extent) {
    return image && offset && extent && offset->x == 0 && offset->y == 0 &&
           offset->z == 0 && extent->depth == 1u &&
           image_subresource_valid(image, subresource, extent->width,
                                     extent->height);
}

static int image_subresource_range_valid(
        const RinVkImageSlot* image,
        const RinVkImageSubresourceRange* range) {
    return image && range &&
           range->aspectMask == RIN_VK_IMAGE_ASPECT_COLOR_BIT &&
           range->baseMipLevel == 0u && range->levelCount == 1u &&
           range->baseArrayLayer == 0u && range->layerCount == 1u;
}

static int image_blit_region_valid(
        const RinVkImageSlot* image,
        const RinVkImageSubresourceLayers* subresource,
        const RinVkOffset3D offsets[2]) {
    return image && image->samples == RIN_VK_SAMPLE_COUNT_1_BIT &&
           subresource && offsets &&
           subresource->aspectMask == RIN_VK_IMAGE_ASPECT_COLOR_BIT &&
           subresource->mipLevel == 0u && subresource->baseArrayLayer == 0u &&
           subresource->layerCount == 1u && offsets[0].x == 0 &&
           offsets[0].y == 0 && offsets[0].z == 0 &&
           offsets[1].x == (int32_t)image->width &&
           offsets[1].y == (int32_t)image->height && offsets[1].z == 1;
}

static int image_resolve_region_valid(
        const RinVkImageSlot* image,
        const RinVkImageSubresourceLayers* subresource,
        const RinVkOffset3D* offset, const RinVkExtent3D* extent) {
    return image && subresource && offset && extent &&
           subresource->aspectMask == RIN_VK_IMAGE_ASPECT_COLOR_BIT &&
           subresource->mipLevel == 0u && subresource->baseArrayLayer == 0u &&
           subresource->layerCount == 1u && offset->x == 0 && offset->y == 0 &&
           offset->z == 0 && extent->width == image->width &&
           extent->height == image->height && extent->depth == 1u;
}

static int checked_image_address(const RinVkImageSlot* image, uint64_t offset,
                                 uint64_t size, uint64_t* address_out) {
    uint64_t address;
    if (!image || !image->memory || !address_out ||
        image->memory_generation != image->memory->generation ||
        image->memory->product_allocation == 0u ||
        image->memory->gpu_virtual_address == 0u ||
        offset > image->memory_size || size > image->memory_size - offset ||
        image->memory_offset > UINT64_MAX -
                                    image->memory->gpu_virtual_address)
        return 0;
    address = image->memory->gpu_virtual_address + image->memory_offset;
    if (offset > UINT64_MAX - address || size > UINT64_MAX - (address + offset))
        return 0;
    *address_out = address + offset;
    return 1;
}

static int command_owner_device(RinGpuVulkanCommandBufferV1* core,
                                struct RinVkDevice_T** owner_out) {
    uintptr_t owner_address = 0u;
    if (!owner_out || rin_gpu_vulkan_command_buffer_owner(
                          &g_command_runtime, core, &owner_address) !=
                          RIN_GPU_VULKAN_COMMAND_OK)
        return 0;
    *owner_out = device_slot((RinVkDevice)(void*)owner_address);
    return *owner_out != NULL;
}

static void record_transfer_ops(RinGpuVulkanCommandBufferV1* core,
                                const RinGpuVulkanTransferOpV2* operations,
                                uint32_t operation_count) {
    if (!core || !operations || operation_count == 0u ||
        rin_gpu_vulkan_command_buffer_record_transfer_ops(
            &g_command_runtime, core, operations, operation_count) !=
            RIN_GPU_VULKAN_COMMAND_OK)
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdCopyImage(
        RinVkCommandBuffer command_buffer, RinVkImage src_image,
        uint32_t src_image_layout, RinVkImage dst_image,
        uint32_t dst_image_layout, uint32_t region_count,
        const RinVkImageCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkImageSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint32_t index;
    int valid = 1;

    memset(operations, 0, sizeof(operations));
    if (region_count == 0u ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS || !regions ||
        !command_owner_device(core, &owner) ||
        !image_layout_transfer_valid(src_image_layout) ||
        !image_layout_transfer_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = image_slot(owner, dst_image);
    if (!source || !destination ||
        (source->usage & RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u ||
        (destination->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u)
        valid = 0;
    for (index = 0u; valid && index < region_count; ++index) {
        uint64_t source_address;
        uint64_t destination_address;
        if (!image_region_valid(source, &regions[index].srcSubresource,
                                &regions[index].srcOffset,
                                &regions[index].extent) ||
            !image_region_valid(destination, &regions[index].dstSubresource,
                                &regions[index].dstOffset,
                                &regions[index].extent) ||
            !checked_image_address(source, 0u, source->memory_size,
                                   &source_address) ||
            !checked_image_address(destination, 0u, destination->memory_size,
                                   &destination_address)) {
            valid = 0;
            break;
        }
        operations[index].type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_COPY;
        operations[index].source_allocation = source->memory->product_allocation;
        operations[index].destination_allocation =
            destination->memory->product_allocation;
        operations[index].source_gpu_address = source_address;
        operations[index].destination_gpu_address = destination_address;
        operations[index].size_bytes = source->memory_size;
        if (operations[index].size_bytes != destination->memory_size)
            valid = 0;
    }
done:
    if (valid) record_transfer_ops(core, operations, region_count);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdCopyBufferToImage(
        RinVkCommandBuffer command_buffer, RinVkBuffer src_buffer,
        RinVkImage dst_image, uint32_t dst_image_layout, uint32_t region_count,
        const RinVkBufferImageCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkBufferSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint32_t index;
    int valid = 1;
    memset(operations, 0, sizeof(operations));
    if (region_count == 0u ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS || !regions ||
        !command_owner_device(core, &owner) ||
        !image_layout_transfer_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = buffer_slot(owner, src_buffer);
    destination = image_slot(owner, dst_image);
    if (!source || !destination ||
        (source->usage & RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0u ||
        (destination->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u)
        valid = 0;
    for (index = 0u; valid && index < region_count; ++index) {
        uint64_t source_address;
        uint64_t destination_address;
        uint64_t size = destination->memory_size;
        if (regions[index].bufferRowLength != 0u ||
            regions[index].bufferImageHeight != 0u ||
            !image_region_valid(destination,
                                &regions[index].imageSubresource,
                                &regions[index].imageOffset,
                                &regions[index].imageExtent) ||
            !checked_buffer_address(source, regions[index].bufferOffset, size,
                                    &source_address) ||
            !checked_image_address(destination, 0u, size,
                                   &destination_address)) {
            valid = 0;
            break;
        }
        operations[index].type = RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_TO_IMAGE;
        operations[index].source_allocation = source->memory->product_allocation;
        operations[index].destination_allocation =
            destination->memory->product_allocation;
        operations[index].source_gpu_address = source_address;
        operations[index].destination_gpu_address = destination_address;
        operations[index].size_bytes = size;
    }
done:
    if (valid) record_transfer_ops(core, operations, region_count);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdCopyImageToBuffer(
        RinVkCommandBuffer command_buffer, RinVkImage src_image,
        uint32_t src_image_layout, RinVkBuffer dst_buffer,
        uint32_t region_count, const RinVkBufferImageCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkImageSlot* source;
    RinVkBufferSlot* destination;
    struct RinVkDevice_T* owner;
    uint32_t index;
    int valid = 1;
    memset(operations, 0, sizeof(operations));
    if (region_count == 0u ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS || !regions ||
        !command_owner_device(core, &owner) ||
        !image_layout_transfer_valid(src_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = buffer_slot(owner, dst_buffer);
    if (!source || !destination ||
        (source->usage & RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u ||
        (destination->usage & RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0u)
        valid = 0;
    for (index = 0u; valid && index < region_count; ++index) {
        uint64_t source_address;
        uint64_t destination_address;
        uint64_t size = source->memory_size;
        if (regions[index].bufferRowLength != 0u ||
            regions[index].bufferImageHeight != 0u ||
            !image_region_valid(source, &regions[index].imageSubresource,
                                &regions[index].imageOffset,
                                &regions[index].imageExtent) ||
            !checked_image_address(source, 0u, size, &source_address) ||
            !checked_buffer_address(destination, regions[index].bufferOffset,
                                    size, &destination_address)) {
            valid = 0;
            break;
        }
        operations[index].type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_TO_BUFFER;
        operations[index].source_allocation = source->memory->product_allocation;
        operations[index].destination_allocation =
            destination->memory->product_allocation;
        operations[index].source_gpu_address = source_address;
        operations[index].destination_gpu_address = destination_address;
        operations[index].size_bytes = size;
    }
done:
    if (valid) record_transfer_ops(core, operations, region_count);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdClearColorImage(
        RinVkCommandBuffer command_buffer, RinVkImage image_handle,
        uint32_t image_layout, const RinVkClearColorValue* color,
        uint32_t range_count, const RinVkImageSubresourceRange* ranges) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operation;
    RinVkImageSlot* image;
    struct RinVkDevice_T* owner;
    uint64_t destination_address;
    int valid = 1;

    memset(&operation, 0, sizeof(operation));
    if (!command_owner_device(core, &owner) || !color || range_count != 1u ||
        !ranges ||
        (image_layout != RIN_VK_IMAGE_LAYOUT_GENERAL &&
         image_layout != RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
        valid = 0;
        goto done;
    }
    image = image_slot(owner, image_handle);
    if (!image || (image->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u ||
        !image_subresource_range_valid(image, &ranges[0]) ||
        color->uint32[1] != 0u || color->uint32[2] != 0u ||
        color->uint32[3] != 0u ||
        !checked_image_address(image, 0u, image->memory_size,
                               &destination_address)) {
        valid = 0;
        goto done;
    }
    operation.type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR;
    operation.destination_allocation = image->memory->product_allocation;
    operation.destination_gpu_address = destination_address;
    operation.size_bytes = image->memory_size;
    operation.clear_value[0] = color->uint32[0];
done:
    if (valid) record_transfer_ops(core, &operation, 1u);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdBlitImage(
        RinVkCommandBuffer command_buffer, RinVkImage src_image,
        uint32_t src_image_layout, RinVkImage dst_image,
        uint32_t dst_image_layout, uint32_t region_count,
        const RinVkImageBlit* regions, uint32_t filter) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operation;
    RinVkImageSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint64_t source_address;
    uint64_t destination_address;
    int valid = 1;

    memset(&operation, 0, sizeof(operation));
    if (!command_owner_device(core, &owner) || region_count != 1u || !regions ||
        filter > RIN_VK_FILTER_LINEAR ||
        !image_layout_transfer_valid(src_image_layout) ||
        !image_layout_transfer_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = image_slot(owner, dst_image);
    if (!source || !destination || source == destination ||
        source->memory == destination->memory ||
        (source->usage & RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u ||
        (destination->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u ||
        !image_blit_region_valid(source, &regions[0].srcSubresource,
                                 regions[0].srcOffsets) ||
        !image_blit_region_valid(destination, &regions[0].dstSubresource,
                                 regions[0].dstOffsets) ||
        !checked_image_address(source, 0u, source->memory_size,
                               &source_address) ||
        !checked_image_address(destination, 0u, destination->memory_size,
                               &destination_address)) {
        valid = 0;
        goto done;
    }
    operation.type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BLIT;
    operation.source_allocation = source->memory->product_allocation;
    operation.destination_allocation = destination->memory->product_allocation;
    operation.source_gpu_address = source_address;
    operation.destination_gpu_address = destination_address;
    operation.size_bytes = destination->memory_size;
    operation.source_width = source->width;
    operation.source_height = source->height;
    operation.destination_width = destination->width;
    operation.destination_height = destination->height;
    operation.filter = filter;
done:
    if (valid) record_transfer_ops(core, &operation, 1u);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdResolveImage(
        RinVkCommandBuffer command_buffer, RinVkImage src_image,
        uint32_t src_image_layout, RinVkImage dst_image,
        uint32_t dst_image_layout, uint32_t region_count,
        const RinVkImageResolve* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operation;
    RinVkImageSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint64_t source_address;
    uint64_t destination_address;
    int valid = 1;

    memset(&operation, 0, sizeof(operation));
    if (!command_owner_device(core, &owner) || region_count != 1u || !regions ||
        !image_layout_transfer_valid(src_image_layout) ||
        !image_layout_transfer_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = image_slot(owner, dst_image);
    if (!source || !destination || source == destination ||
        source->memory == destination->memory ||
        (source->samples != RIN_VK_SAMPLE_COUNT_2_BIT &&
         source->samples != RIN_VK_SAMPLE_COUNT_4_BIT) ||
        destination->samples != RIN_VK_SAMPLE_COUNT_1_BIT ||
        (source->usage & RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u ||
        (destination->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u ||
        source->width != destination->width ||
        source->height != destination->height ||
        !image_resolve_region_valid(source, &regions[0].srcSubresource,
                                    &regions[0].srcOffset,
                                    &regions[0].extent) ||
        !image_resolve_region_valid(destination, &regions[0].dstSubresource,
                                    &regions[0].dstOffset,
                                    &regions[0].extent) ||
        !checked_image_address(source, 0u, source->memory_size,
                               &source_address) ||
        !checked_image_address(destination, 0u, destination->memory_size,
                               &destination_address)) {
        valid = 0;
        goto done;
    }
    operation.type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE;
    operation.source_allocation = source->memory->product_allocation;
    operation.destination_allocation = destination->memory->product_allocation;
    operation.source_gpu_address = source_address;
    operation.destination_gpu_address = destination_address;
    operation.size_bytes = destination->memory_size;
    operation.source_width = source->width;
    operation.source_height = source->height;
    operation.destination_width = destination->width;
    operation.destination_height = destination->height;
    operation.sample_count = source->samples;
done:
    if (valid) record_transfer_ops(core, &operation, 1u);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

static int snapshot_submission_packet(
    const struct RinVkDevice_T* device, uint32_t queue_family_index,
    RinGpuVulkanCommandBufferV1* const* command_buffers,
    uint32_t command_buffer_count, RinGpuVulkanTransferPacketV1* packet,
    RinVulkanProductResourceV1* resources, uint32_t* resource_count_out) {
    uint32_t buffer_index;
    uint32_t resource_count = 0u;

    if (!device || !command_buffers || command_buffer_count == 0u ||
        command_buffer_count > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS || !packet ||
        !resources || !resource_count_out) {
        return 0;
    }
    memset(packet, 0, sizeof(*packet));
    memset(resources, 0, sizeof(RinVulkanProductResourceV1) *
                             RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION);
    packet->struct_size = sizeof(*packet);
    packet->version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION;
    for (buffer_index = 0u; buffer_index < command_buffer_count;
         ++buffer_index) {
        const RinGpuVulkanCommandBufferV1* buffer = command_buffers[buffer_index];
        uint32_t copy_index;

        if (!buffer || buffer->owner != (uintptr_t)device || !buffer->pool ||
            buffer->pool->queue_family_index != queue_family_index ||
            buffer->copy_count >
                           RIN_GPU_VULKAN_COMMAND_MAX_COPIES ||
            buffer->copy_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_COPIES -
                                     packet->copy_count) {
            return 0;
        }
        for (copy_index = 0u; copy_index < buffer->copy_count; ++copy_index) {
            const RinGpuVulkanBufferCopyCommandV1* copy =
                &buffer->copies[copy_index];

            if (copy->source_allocation == 0u ||
                copy->destination_allocation == 0u ||
                copy->source_gpu_address == 0u ||
                copy->destination_gpu_address == 0u || copy->size_bytes == 0u ||
                !append_submission_resource(resources, &resource_count,
                                            copy->source_allocation,
                                            RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) ||
                !append_submission_resource(resources, &resource_count,
                                            copy->destination_allocation,
                                            RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE)) {
                return 0;
            }
            packet->copies[packet->copy_count++] = *copy;
        }
    }
    *resource_count_out = resource_count;
    return 1;
}

static int snapshot_submission_packet_v2(
    const struct RinVkDevice_T* device, uint32_t queue_family_index,
    RinGpuVulkanCommandBufferV1* const* command_buffers,
    uint32_t command_buffer_count, RinGpuVulkanTransferPacketV2* packet,
    RinVulkanProductResourceV1* resources, uint32_t* resource_count_out) {
    uint32_t buffer_index;
    uint32_t resource_count = 0u;
    if (!device || !command_buffers || command_buffer_count == 0u ||
        command_buffer_count > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS || !packet ||
        !resources || !resource_count_out)
        return 0;
    memset(packet, 0, sizeof(*packet));
    memset(resources, 0, sizeof(RinVulkanProductResourceV1) *
                             RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION);
    packet->struct_size = sizeof(*packet);
    packet->version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2;
    for (buffer_index = 0u; buffer_index < command_buffer_count;
         ++buffer_index) {
        const RinGpuVulkanCommandBufferV1* buffer = command_buffers[buffer_index];
        uint32_t copy_index;
        uint32_t operation_index;
        if (!buffer || buffer->owner != (uintptr_t)device || !buffer->pool ||
            buffer->pool->queue_family_index != queue_family_index ||
            buffer->copy_count > RIN_GPU_VULKAN_COMMAND_MAX_COPIES ||
            buffer->transfer_op_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS ||
            buffer->copy_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS ||
            buffer->transfer_op_count >
                RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS - buffer->copy_count ||
            packet->op_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS -
                                buffer->copy_count - buffer->transfer_op_count)
            return 0;
        for (copy_index = 0u; copy_index < buffer->copy_count; ++copy_index) {
            const RinGpuVulkanBufferCopyCommandV1* copy =
                &buffer->copies[copy_index];
            RinGpuVulkanTransferOpV2* operation =
                &packet->operations[packet->op_count++];
            if (!append_submission_resource(resources, &resource_count,
                                            copy->source_allocation,
                                            RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) ||
                !append_submission_resource(resources, &resource_count,
                                            copy->destination_allocation,
                                            RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE))
                return 0;
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_COPY;
            operation->source_allocation = copy->source_allocation;
            operation->destination_allocation = copy->destination_allocation;
            operation->source_gpu_address = copy->source_gpu_address;
            operation->destination_gpu_address = copy->destination_gpu_address;
            operation->size_bytes = copy->size_bytes;
        }
        for (operation_index = 0u;
             operation_index < buffer->transfer_op_count; ++operation_index) {
            const RinGpuVulkanTransferOpV2* source =
                &buffer->transfer_ops[operation_index];
            RinGpuVulkanTransferOpV2* operation =
                &packet->operations[packet->op_count++];
            if ((source->type != RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR &&
                 !append_submission_resource(
                     resources, &resource_count, source->source_allocation,
                     RIN_VULKAN_PRODUCT_MEMORY_GPU_READ)) ||
                !append_submission_resource(
                    resources, &resource_count, source->destination_allocation,
                    RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE))
                return 0;
            *operation = *source;
        }
    }
    if (packet->op_count == 0u) return 0;
    *resource_count_out = resource_count;
    return 1;
}

RinVkResult RIN_VKAPI_CALL vkQueueSubmit(
    RinVkQueue queue, uint32_t submit_count, const RinVkSubmitInfo* submits,
    uint64_t fence) {
    struct RinVkQueue_T* queue_slot_value = queue_slot(queue);
    struct RinVkDevice_T* device;
    RinVkSubmitInfo request;
    RinGpuVulkanCommandBufferV1*
        command_buffers[RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS];
    RinVulkanProductResourceV1
        resources[RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION];
    RinGpuVulkanTransferPacketV1 validation_packet;
    RinGpuVulkanTransferPacketV2 validation_packet_v2;
    RinVulkanProductSubmissionV1 submission;
    RinVulkanProductPlatformV1* product = NULL;
    RinVkSubmissionSlot* slot = NULL;
    RinVkSemaphore wait_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinVkSemaphore signal_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t wait_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t signal_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    const RinVkTimelineSemaphoreSubmitInfo* timeline_submit = NULL;
    RinVkResult result;
    uint32_t resource_count = 0u;
    uint32_t index;
    int product_result;
    int sync_locked = 0;
    int extended_packet = 0;

    if (!queue_slot_value) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (__atomic_exchange_n(&queue_slot_value->submit_lock, 1u,
                            __ATOMIC_ACQUIRE) != 0u) {
        return RIN_VK_NOT_READY;
    }
    device = queue_slot_value->device;
    result = maintain_device_submissions(device);
    if (result != RIN_VK_SUCCESS) goto done;
    if (__atomic_load_n(&device->descriptor_validation_error,
                        __ATOMIC_ACQUIRE) != 0u) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (submit_count > 1u ||
        (submit_count != 0u && !submits)) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    if (submit_count == 0u) {
        result = fence == 0u ? RIN_VK_SUCCESS
                             : RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    request = submits[0];
    if (request.sType != RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO ||
        request.waitSemaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        request.signalSemaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        (request.waitSemaphoreCount != 0u &&
         (!request.pWaitSemaphores || !request.pWaitDstStageMask)) ||
        (request.waitSemaphoreCount == 0u &&
         (request.pWaitSemaphores || request.pWaitDstStageMask)) ||
        (request.signalSemaphoreCount != 0u && !request.pSignalSemaphores) ||
        (request.signalSemaphoreCount == 0u && request.pSignalSemaphores) ||
        request.commandBufferCount == 0u ||
        request.commandBufferCount > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS ||
        !request.pCommandBuffers) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    if (request.pNext) {
        const RinVkTimelineSemaphoreSubmitInfo* candidate =
            (const RinVkTimelineSemaphoreSubmitInfo*)request.pNext;
        if (candidate->sType !=
                RIN_VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO ||
            candidate->pNext ||
            candidate->waitSemaphoreValueCount != request.waitSemaphoreCount ||
            candidate->signalSemaphoreValueCount !=
                request.signalSemaphoreCount ||
            (candidate->waitSemaphoreValueCount != 0u &&
             !candidate->pWaitSemaphoreValues) ||
            (candidate->signalSemaphoreValueCount != 0u &&
             !candidate->pSignalSemaphoreValues)) {
            result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
            goto done;
        }
        timeline_submit = candidate;
    }
    if (!sync_try_lock()) {
        result = RIN_VK_NOT_READY;
        goto done;
    }
    sync_locked = 1;
    memset(wait_semaphores, 0, sizeof(wait_semaphores));
    memset(signal_semaphores, 0, sizeof(signal_semaphores));
    memset(wait_values, 0, sizeof(wait_values));
    memset(signal_values, 0, sizeof(signal_values));
    for (index = 0u; index < request.waitSemaphoreCount; ++index) {
        wait_semaphores[index] = (RinVkSemaphore)
            request.pWaitSemaphores[index];
        if (semaphore_list_contains(wait_semaphores, index,
                                    wait_semaphores[index]) ||
            !semaphore_slot(device, wait_semaphores[index])) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
        if (timeline_submit)
            wait_values[index] = timeline_submit->pWaitSemaphoreValues[index];
    }
    for (index = 0u; index < request.signalSemaphoreCount; ++index) {
        signal_semaphores[index] = (RinVkSemaphore)
            request.pSignalSemaphores[index];
        if (semaphore_list_contains(signal_semaphores, index,
                                    signal_semaphores[index]) ||
            !semaphore_slot(device, signal_semaphores[index]) ||
            semaphore_list_contains(wait_semaphores,
                                    request.waitSemaphoreCount,
                                    signal_semaphores[index])) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
        if (timeline_submit)
            signal_values[index] =
                timeline_submit->pSignalSemaphoreValues[index];
    }
    if (fence != 0u) {
        RinVkFenceSlot* fence_value = fence_slot(device, (RinVkFence)fence);
        if (!fence_value ||
            __atomic_load_n(&fence_value->signaled, __ATOMIC_ACQUIRE) != 0u) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
        if (__atomic_load_n(&fence_value->pending, __ATOMIC_ACQUIRE) != 0u) {
            result = RIN_VK_NOT_READY;
            goto done;
        }
    }
    for (index = 0u; index < request.waitSemaphoreCount; ++index) {
        RinVkSemaphoreSlot* semaphore =
            semaphore_slot(device, wait_semaphores[index]);
        if ((semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE &&
             (!timeline_submit ||
              __atomic_load_n(&semaphore->value, __ATOMIC_ACQUIRE) <
                  wait_values[index])) ||
            (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY &&
             (timeline_submit ? wait_values[index] != 0u : 0) != 0u) ||
            __atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u ||
            (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY &&
             __atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) == 0u)) {
            result = RIN_VK_NOT_READY;
            goto done;
        }
    }
    for (index = 0u; index < request.signalSemaphoreCount; ++index) {
        RinVkSemaphoreSlot* semaphore =
            semaphore_slot(device, signal_semaphores[index]);
        if (__atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u) {
            result = RIN_VK_NOT_READY;
            goto done;
        }
        if ((semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY &&
             (timeline_submit ? signal_values[index] != 0u : 0) != 0u) ||
            (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY &&
             __atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) != 0u) ||
            (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE &&
             (!timeline_submit ||
              signal_values[index] <=
                  __atomic_load_n(&semaphore->value, __ATOMIC_ACQUIRE)))) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
    }
    for (index = 0u; index < request.commandBufferCount; ++index) {
        command_buffers[index] =
            (RinGpuVulkanCommandBufferV1*)(void*)request.pCommandBuffers[index];
        if (command_buffers[index] &&
            command_buffers[index]->transfer_op_count != 0u)
            extended_packet = 1;
    }
    if (rin_gpu_vulkan_command_buffers_validate_submit(
            &g_command_runtime, request.commandBufferCount,
            command_buffers) != RIN_GPU_VULKAN_COMMAND_OK ||
        (extended_packet
             ? !snapshot_submission_packet_v2(
                   device, queue_slot_value->queue_family_index, command_buffers,
                   request.commandBufferCount, &validation_packet_v2, resources,
                   &resource_count)
             : !snapshot_submission_packet(
                   device, queue_slot_value->queue_family_index, command_buffers,
                   request.commandBufferCount, &validation_packet, resources,
                   &resource_count))) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    slot = reserve_submission_slot();
    if (!slot) {
        result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        goto done;
    }
    slot->owner = device;
    slot->queue_id = queue_slot_value->queue_index;
    slot->command_buffer_count = request.commandBufferCount;
    memcpy(slot->command_buffers, command_buffers,
           sizeof(*command_buffers) * request.commandBufferCount);
    if ((extended_packet
             ? !snapshot_submission_packet_v2(
                   device, queue_slot_value->queue_family_index, command_buffers,
                   request.commandBufferCount, &slot->extended_packet, resources,
                   &resource_count)
             : !snapshot_submission_packet(
                   device, queue_slot_value->queue_family_index, command_buffers,
                   request.commandBufferCount, &slot->packet, resources,
                   &resource_count)) ||
        rin_gpu_vulkan_command_buffers_mark_submitted(
            &g_command_runtime, request.commandBufferCount,
            command_buffers) != RIN_GPU_VULKAN_COMMAND_OK) {
        clear_submission_slot(slot);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    product = acquire_product();
    if (!product || !product_matches_device(product, device)) {
        if (product) release_product();
        rin_gpu_vulkan_command_buffers_abort(
            &g_command_runtime, request.commandBufferCount, command_buffers);
        clear_submission_slot(slot);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    memset(&submission, 0, sizeof(submission));
    slot->packet_version = extended_packet
                               ? RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2
                               : RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION;
    product_result = product->prepare_submission(
        product->context, slot->queue_id,
        (uint64_t)(uintptr_t)(extended_packet
                                  ? (const void*)&slot->extended_packet
                                  : (const void*)&slot->packet),
        &submission);
    if (product_result == RIN_VULKAN_PRODUCT_OK) {
        product_result = product->submit(
            product->context, &submission,
            resource_count == 0u ? NULL : resources, resource_count);
    }
    release_product();
    if (product_result != RIN_VULKAN_PRODUCT_OK) {
        rin_gpu_vulkan_command_buffers_abort(
            &g_command_runtime, request.commandBufferCount, command_buffers);
        clear_submission_slot(slot);
        result = map_product_result(product_result);
        goto done;
    }
    slot->sequence = submission.sequence;
    slot->completion_value = submission.completion_value;
    slot->fence = (RinVkFence)fence;
    slot->wait_semaphore_count = request.waitSemaphoreCount;
    slot->signal_semaphore_count = request.signalSemaphoreCount;
    memcpy(slot->signal_semaphores, signal_semaphores,
           sizeof(RinVkSemaphore) * request.signalSemaphoreCount);
    memcpy(slot->signal_semaphore_values, signal_values,
           sizeof(uint64_t) * request.signalSemaphoreCount);
    for (index = 0u; index < request.waitSemaphoreCount; ++index) {
        RinVkSemaphoreSlot* semaphore =
            semaphore_slot(device, wait_semaphores[index]);
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY)
            __atomic_store_n(&semaphore->signaled, 0u, __ATOMIC_RELEASE);
    }
    if (fence != 0u) {
        RinVkFenceSlot* fence_value = fence_slot(device, (RinVkFence)fence);
        __atomic_store_n(&fence_value->pending, 1u, __ATOMIC_RELEASE);
    }
    for (index = 0u; index < request.signalSemaphoreCount; ++index) {
        RinVkSemaphoreSlot* semaphore =
            semaphore_slot(device, signal_semaphores[index]);
        __atomic_store_n(&semaphore->pending, 1u, __ATOMIC_RELEASE);
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE)
            semaphore->pending_value = signal_values[index];
    }
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    result = RIN_VK_SUCCESS;

done:
    if (sync_locked) sync_unlock();
    __atomic_store_n(&queue_slot_value->submit_lock, 0u, __ATOMIC_RELEASE);
    return result;
}

RinVkResult RIN_VKAPI_CALL vkAllocateMemory(
        RinVkDevice device, const RinVkMemoryAllocateInfo* allocate_info,
        const void* allocator, RinVkDeviceMemory* memory_out) {
    struct RinVkDevice_T* slot = device_slot(device);
    RinVkMemoryAllocateInfo request;
    RinVulkanProductAllocationDescV1 descriptor;
    RinVulkanProductAllocationInfoV1 information;
    RinVulkanProductPlatformV1* product;
    RinVkMemorySlot* memory;
    uint32_t slot_index;
    uint32_t properties;
    int result;
    (void)allocator;

    if (!memory_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *memory_out = 0u;
    if (!slot || !allocate_info) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    request = *allocate_info;
    if (request.sType != RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO ||
        request.pNext || request.allocationSize == 0u ||
        request.memoryTypeIndex >= slot->physical_profile.memory_type_count)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    properties =
        slot->physical_profile.memory_types[request.memoryTypeIndex]
            .property_flags;
    if ((properties & ~RIN_GPU_VK_MEMORY_KNOWN) != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memory = reserve_memory_slot(&slot_index);
    if (!memory) return RIN_VK_ERROR_OUT_OF_DEVICE_MEMORY;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.struct_size = sizeof(descriptor);
    descriptor.version = RIN_VULKAN_PRODUCT_MEMORY_VERSION;
    descriptor.heap = (properties & RIN_GPU_VK_MEMORY_DEVICE_LOCAL) != 0u
                          ? RIN_VULKAN_PRODUCT_MEMORY_HEAP_LOCAL
                          : RIN_VULKAN_PRODUCT_MEMORY_HEAP_SYSTEM;
    descriptor.flags = RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
                       RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE |
                       RIN_VULKAN_PRODUCT_MEMORY_ZEROED;
    if ((properties & RIN_GPU_VK_MEMORY_HOST_VISIBLE) != 0u)
        descriptor.flags |= RIN_VULKAN_PRODUCT_MEMORY_CPU_VISIBLE;
    descriptor.size_bytes = request.allocationSize;
    descriptor.alignment = RIN_VK_RESOURCE_ALIGNMENT;
    product = acquire_product();
    if (!product || !product_matches_device(product, slot)) {
        if (product) release_product();
        clear_memory_slot(memory);
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    result = product->allocate(product->context, &descriptor,
                               &memory->product_allocation);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        release_product();
        clear_memory_slot(memory);
        return map_product_result(result);
    }
    memset(&information, 0, sizeof(information));
    result = product->query_allocation(product->context,
                                       memory->product_allocation,
                                       &information);
    if (result != RIN_VULKAN_PRODUCT_OK ||
        information.struct_size != sizeof(information) ||
        information.version != RIN_VULKAN_PRODUCT_MEMORY_VERSION ||
        information.heap != descriptor.heap ||
        information.requested_size_bytes != request.allocationSize ||
        information.allocation_size_bytes < request.allocationSize ||
        (information.flags & descriptor.flags) != descriptor.flags ||
        information.state != RIN_VULKAN_PRODUCT_MEMORY_ALLOCATION_ACTIVE) {
        /* The allocation remains owned by this private slot for destruction
         * retry; returning an untracked handle on a protocol discrepancy
         * would make recovery impossible. */
        memory->owner = slot;
        memory->gpu_virtual_address = information.gpu_virtual_address;
        memory->requested_size = request.allocationSize;
        memory->memory_type_index = request.memoryTypeIndex;
        memory->bound_resource_count = 0u;
        __atomic_store_n(&memory->state, 1u, __ATOMIC_RELEASE);
        release_product();
        return result == RIN_VULKAN_PRODUCT_OK ? RIN_VK_ERROR_DEVICE_LOST
                                             : map_product_result(result);
    }
    release_product();
    memory->owner = slot;
    memory->gpu_virtual_address = information.gpu_virtual_address;
    memory->requested_size = information.requested_size_bytes;
    memory->memory_type_index = request.memoryTypeIndex;
    memory->bound_resource_count = 0u;
    __atomic_store_n(&memory->state, 1u, __ATOMIC_RELEASE);
    *memory_out = resource_handle(RIN_VK_MEMORY_TAG, slot_index,
                                  memory->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkFreeMemory(RinVkDevice device, RinVkDeviceMemory handle,
                                 const void* allocator) {
    RinVkMemorySlot* memory = memory_slot(device, handle);
    RinVulkanProductPlatformV1* product;
    (void)allocator;
    if (!memory || memory->bound_resource_count != 0u) return;
    product = acquire_product();
    if (!product || !product_matches_device(product, memory->owner)) {
        if (product) release_product();
        return;
    }
    if (product->destroy_allocation(product->context,
                                    memory->product_allocation) ==
        RIN_VULKAN_PRODUCT_OK)
        clear_memory_slot(memory);
    release_product();
}

RinVkResult RIN_VKAPI_CALL vkCreateBuffer(
        RinVkDevice device, const RinVkBufferCreateInfo* create_info,
        const void* allocator, RinVkBuffer* buffer_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkBufferCreateInfo request;
    RinVkBufferSlot* buffer;
    uint32_t index;
    (void)allocator;
    if (!buffer_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *buffer_out = 0u;
    if (!owner || !create_info) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    request = *create_info;
    if (request.sType != RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO ||
        request.pNext || request.flags != 0u || request.size == 0u ||
        request.size > owner->physical_profile.max_buffer_size ||
        request.usage == 0u ||
        (request.usage & ~RIN_VK_BUFFER_USAGE_KNOWN) != 0u ||
        request.sharingMode != RIN_VK_SHARING_MODE_EXCLUSIVE ||
        request.queueFamilyIndexCount != 0u ||
        request.pQueueFamilyIndices != NULL)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    buffer = reserve_buffer_slot(&index);
    if (!buffer) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    buffer->owner = owner;
    buffer->usage = request.usage;
    buffer->size = request.size;
    buffer->memory = NULL;
    buffer->memory_generation = 0u;
    buffer->memory_offset = 0u;
    __atomic_store_n(&buffer->state, 1u, __ATOMIC_RELEASE);
    *buffer_out = resource_handle(RIN_VK_BUFFER_TAG, index,
                                  buffer->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyBuffer(RinVkDevice device, RinVkBuffer handle,
                                    const void* allocator) {
    RinVkBufferSlot* buffer = buffer_slot(device, handle);
    (void)allocator;
    if (!buffer) return;
    if (buffer->memory &&
        __atomic_load_n(&buffer->memory->state, __ATOMIC_ACQUIRE) == 1u &&
        buffer->memory->generation == buffer->memory_generation &&
        buffer->memory->bound_resource_count != 0u)
        --buffer->memory->bound_resource_count;
    clear_buffer_slot(buffer);
}

void RIN_VKAPI_CALL vkGetBufferMemoryRequirements(
        RinVkDevice device, RinVkBuffer handle,
        RinVkMemoryRequirements* requirements) {
    RinVkBufferSlot* buffer;
    uint64_t size;
    if (!requirements) return;
    memset(requirements, 0, sizeof(*requirements));
    buffer = buffer_slot(device, handle);
    if (!buffer || buffer->size > UINT64_MAX -
                                   (RIN_VK_RESOURCE_ALIGNMENT - 1u))
        return;
    size = (buffer->size + RIN_VK_RESOURCE_ALIGNMENT - 1u) &
           ~(RIN_VK_RESOURCE_ALIGNMENT - 1u);
    requirements->size = size;
    requirements->alignment = RIN_VK_RESOURCE_ALIGNMENT;
    requirements->memoryTypeBits = memory_type_bits(buffer->owner);
}

RinVkResult RIN_VKAPI_CALL vkBindBufferMemory(
        RinVkDevice device, RinVkBuffer buffer_handle,
        RinVkDeviceMemory memory_handle, uint64_t memory_offset) {
    RinVkBufferSlot* buffer = buffer_slot(device, buffer_handle);
    RinVkMemorySlot* memory = memory_slot(device, memory_handle);
    if (!buffer || !memory || buffer->memory ||
        (memory_offset & (RIN_VK_RESOURCE_ALIGNMENT - 1u)) != 0u ||
        memory_offset > memory->requested_size ||
        buffer->size > memory->requested_size - memory_offset ||
        (memory_type_bits(buffer->owner) &
         (UINT32_C(1) << memory->memory_type_index)) == 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    buffer->memory = memory;
    buffer->memory_generation = memory->generation;
    buffer->memory_offset = memory_offset;
    ++memory->bound_resource_count;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateImage(
        RinVkDevice device, const RinVkImageCreateInfo* create_info,
        const void* allocator, RinVkImage* image_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkImageCreateInfo request;
    RinVkImageSlot* image;
    uint64_t memory_size;
    uint32_t index;
    (void)allocator;

    if (!image_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *image_out = 0u;
    if (!owner || !create_info) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    request = *create_info;
    if (request.sType != RIN_VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO ||
        request.pNext || request.flags != 0u ||
        request.imageType != RIN_VK_IMAGE_TYPE_2D ||
        request.format != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
         request.mipLevels != 1u || request.arrayLayers != 1u ||
         (request.samples != RIN_VK_SAMPLE_COUNT_1_BIT &&
          request.samples != RIN_VK_SAMPLE_COUNT_2_BIT &&
          request.samples != RIN_VK_SAMPLE_COUNT_4_BIT) ||
        request.tiling != RIN_VK_IMAGE_TILING_OPTIMAL ||
        request.usage == 0u ||
        (request.usage & ~RIN_VK_IMAGE_USAGE_KNOWN) != 0u ||
        request.sharingMode != RIN_VK_SHARING_MODE_EXCLUSIVE ||
        request.queueFamilyIndexCount != 0u ||
        request.pQueueFamilyIndices != NULL ||
        !image_memory_size(owner, &request, &memory_size))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    image = reserve_image_slot(&index);
    if (!image) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    image->owner = owner;
    image->memory = NULL;
    image->memory_generation = 0u;
    image->format = request.format;
    image->usage = request.usage;
    image->memory_size = memory_size;
    image->memory_offset = 0u;
    image->width = request.extent.width;
    image->height = request.extent.height;
    image->samples = request.samples;
    __atomic_store_n(&image->state, 1u, __ATOMIC_RELEASE);
    *image_out = resource_handle(RIN_VK_IMAGE_TAG, index,
                                  image->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyImage(RinVkDevice device, RinVkImage handle,
                                   const void* allocator) {
    RinVkImageSlot* image = image_slot(device, handle);
    (void)allocator;
    if (!image || image_has_views(image)) return;
    if (image->memory &&
        __atomic_load_n(&image->memory->state, __ATOMIC_ACQUIRE) == 1u &&
        image->memory->generation == image->memory_generation &&
        image->memory->bound_resource_count != 0u)
        --image->memory->bound_resource_count;
    clear_image_slot(image);
}

void RIN_VKAPI_CALL vkGetImageMemoryRequirements(
        RinVkDevice device, RinVkImage handle,
        RinVkMemoryRequirements* requirements) {
    RinVkImageSlot* image;
    if (!requirements) return;
    memset(requirements, 0, sizeof(*requirements));
    image = image_slot(device, handle);
    if (!image) return;
    requirements->size = image->memory_size;
    requirements->alignment = RIN_VK_RESOURCE_ALIGNMENT;
    requirements->memoryTypeBits = memory_type_bits(image->owner);
}

RinVkResult RIN_VKAPI_CALL vkBindImageMemory(
        RinVkDevice device, RinVkImage image_handle,
        RinVkDeviceMemory memory_handle, uint64_t memory_offset) {
    RinVkImageSlot* image = image_slot(device, image_handle);
    RinVkMemorySlot* memory = memory_slot(device, memory_handle);
    if (!image || !memory || image->memory ||
        (memory_offset & (RIN_VK_RESOURCE_ALIGNMENT - 1u)) != 0u ||
        memory_offset > memory->requested_size ||
        image->memory_size > memory->requested_size - memory_offset ||
        (memory_type_bits(image->owner) &
         (UINT32_C(1) << memory->memory_type_index)) == 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    image->memory = memory;
    image->memory_generation = memory->generation;
    image->memory_offset = memory_offset;
    ++memory->bound_resource_count;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateImageView(
        RinVkDevice device, const RinVkImageViewCreateInfo* create_info,
        const void* allocator, RinVkImageView* view_out) {
    RinVkImageViewSlot* view;
    RinVkImageSlot* image;
    uint32_t index;
    (void)allocator;
    if (!view_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *view_out = 0u;
    if (!device || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO ||
        create_info->pNext || create_info->flags != 0u ||
        (create_info->viewType != RIN_VK_IMAGE_VIEW_TYPE_2D &&
         create_info->viewType != RIN_VK_IMAGE_VIEW_TYPE_2D_ARRAY) ||
        create_info->format != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
        create_info->subresourceRange.aspectMask !=
            RIN_VK_IMAGE_ASPECT_COLOR_BIT ||
        create_info->subresourceRange.baseMipLevel != 0u ||
        create_info->subresourceRange.levelCount != 1u ||
        create_info->subresourceRange.baseArrayLayer != 0u ||
        create_info->subresourceRange.layerCount != 1u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    image = image_slot(device, create_info->image);
    if (!image || image->format != create_info->format)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    view = reserve_image_view_slot(device_slot(device), &index);
    if (!view) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    view->image = image;
    view->format = (uint32_t)create_info->format;
    view->aspect_mask = create_info->subresourceRange.aspectMask;
    __atomic_store_n(&view->state, 1u, __ATOMIC_RELEASE);
    *view_out = resource_handle(RIN_VK_IMAGE_VIEW_TAG, index,
                                view->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyImageView(RinVkDevice device, RinVkImageView view,
                                       const void* allocator) {
    RinVkImageViewSlot* slot;
    (void)allocator;
    slot = image_view_slot(device, view);
    if (slot) clear_image_view_slot(slot);
}

RinVkResult RIN_VKAPI_CALL vkCreateSampler(
        RinVkDevice device, const RinVkSamplerCreateInfo* create_info,
        const void* allocator, RinVkSampler* sampler_out) {
    RinVkSamplerSlot* sampler;
    uint32_t index;
    (void)allocator;
    if (!sampler_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *sampler_out = 0u;
    if (!device || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO ||
        create_info->pNext || create_info->flags != 0u ||
        create_info->magFilter > RIN_VK_FILTER_LINEAR ||
        create_info->minFilter > RIN_VK_FILTER_LINEAR ||
        create_info->mipmapMode > RIN_VK_FILTER_LINEAR ||
        create_info->addressModeU > 4u || create_info->addressModeV > 4u ||
        create_info->addressModeW > 4u || create_info->compareEnable > 1u ||
        create_info->compareOp > 8u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    sampler = reserve_sampler_slot(device_slot(device), &index);
    if (!sampler) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    sampler->mag_filter = create_info->magFilter;
    sampler->min_filter = create_info->minFilter;
    sampler->mipmap_mode = create_info->mipmapMode;
    sampler->address_mode_u = create_info->addressModeU;
    sampler->address_mode_v = create_info->addressModeV;
    sampler->address_mode_w = create_info->addressModeW;
    sampler->compare_enable = create_info->compareEnable;
    sampler->compare_op = create_info->compareOp;
    __atomic_store_n(&sampler->state, 1u, __ATOMIC_RELEASE);
    *sampler_out = resource_handle(RIN_VK_SAMPLER_TAG, index,
                                   sampler->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroySampler(RinVkDevice device, RinVkSampler sampler,
                                     const void* allocator) {
    RinVkSamplerSlot* slot;
    (void)allocator;
    slot = sampler_slot(device, sampler);
    if (slot) clear_sampler_slot(slot);
}

RinVkResult RIN_VKAPI_CALL vkCreateDescriptorSetLayout(
        RinVkDevice device, const RinVkDescriptorSetLayoutCreateInfo* info,
        const void* allocator, RinVkDescriptorSetLayout* layout_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinGpuVulkanDescriptorSetLayoutBindingV1 bindings[RIN_SHADER_MAX_RESOURCES];
    RinGpuVulkanDescriptorHandleV1 layout = 0u;
    uint32_t index;
    int result;
    (void)allocator;
    if (!layout_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *layout_out = 0u;
    if (!owner || !info ||
        info->sType != RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO ||
        info->pNext || info->flags != 0u || info->bindingCount == 0u ||
        info->bindingCount > RIN_SHADER_MAX_RESOURCES || !info->pBindings)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(bindings, 0, sizeof(bindings));
    for (index = 0u; index < info->bindingCount; ++index) {
        const RinVkDescriptorSetLayoutBinding* source = &info->pBindings[index];
        uint32_t type = descriptor_runtime_type(source->descriptorType);
        if (type == 0u || source->descriptorCount == 0u ||
            source->descriptorCount > RIN_SHADER_MAX_RESOURCES ||
            source->stageFlags == 0u || source->pImmutableSamplers)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        bindings[index].set = 0u;
        bindings[index].binding = source->binding;
        bindings[index].descriptor_type = type;
        bindings[index].descriptor_count = source->descriptorCount;
        bindings[index].stage_flags = source->stageFlags;
        bindings[index].reserved =
            (source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
             source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                ? RIN_GPU_VULKAN_DESCRIPTOR_BINDING_DYNAMIC
                : 0u;
    }
    result = rin_gpu_vulkan_descriptor_layout_create(
        &owner->descriptor_runtime, bindings, info->bindingCount, &layout);
    if (result != RIN_GPU_VULKAN_GRAPHICS_OK)
        return map_descriptor_result(result);
    *layout_out = (RinVkDescriptorSetLayout)layout;
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyDescriptorSetLayout(
        RinVkDevice device, RinVkDescriptorSetLayout layout,
        const void* allocator) {
    struct RinVkDevice_T* owner = device_slot(device);
    int result;
    (void)allocator;
    if (!owner || layout == 0u) return;
    result = rin_gpu_vulkan_descriptor_layout_destroy(
        &owner->descriptor_runtime,
        (RinGpuVulkanDescriptorHandleV1)layout);
    if (result != RIN_GPU_VULKAN_GRAPHICS_OK)
        __atomic_store_n(&owner->descriptor_validation_error, 1u,
                         __ATOMIC_RELEASE);
}

RinVkResult RIN_VKAPI_CALL vkCreateDescriptorPool(
        RinVkDevice device, const RinVkDescriptorPoolCreateInfo* info,
        const void* allocator, RinVkDescriptorPool* pool_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t limits[8];
    uint32_t index;
    uint32_t pool_index;
    RinGpuVulkanDescriptorHandleV1 pool = 0u;
    int result;
    (void)allocator;
    if (!pool_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *pool_out = 0u;
    if (!owner || !info ||
        info->sType != RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO ||
        info->pNext ||
        (info->flags & ~RIN_VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) !=
            0u || info->maxSets == 0u ||
        info->poolSizeCount == 0u || !info->pPoolSizes ||
        info->poolSizeCount > 8u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(limits, 0, sizeof(limits));
    for (index = 0u; index < info->poolSizeCount; ++index) {
        const RinVkDescriptorPoolSize* size = &info->pPoolSizes[index];
        uint32_t type = descriptor_runtime_type(size->type);
        if (type == 0u || size->descriptorCount == 0u || type >= 8u ||
            limits[type] != 0u ||
            UINT32_MAX - limits[type] < size->descriptorCount)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        limits[type] = size->descriptorCount;
    }
    (void)pool_index;
    result = rin_gpu_vulkan_descriptor_pool_create_v2(
        &owner->descriptor_runtime, info->maxSets, limits, 8u, &pool);
    if (result != RIN_GPU_VULKAN_GRAPHICS_OK)
        return map_descriptor_result(result);
    *pool_out = (RinVkDescriptorPool)pool;
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyDescriptorPool(RinVkDevice device,
                                            RinVkDescriptorPool pool,
                                            const void* allocator) {
    struct RinVkDevice_T* owner = device_slot(device);
    int result;
    (void)allocator;
    if (!owner || pool == 0u) return;
    result = rin_gpu_vulkan_descriptor_pool_destroy(
        &owner->descriptor_runtime, (RinGpuVulkanDescriptorHandleV1)pool);
    if (result != RIN_GPU_VULKAN_GRAPHICS_OK)
        __atomic_store_n(&owner->descriptor_validation_error, 1u,
                         __ATOMIC_RELEASE);
}

RinVkResult RIN_VKAPI_CALL vkAllocateDescriptorSets(
        RinVkDevice device, const RinVkDescriptorSetAllocateInfo* info,
        RinVkDescriptorSet* sets_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    if (!owner || !info || !sets_out ||
        info->sType != RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO ||
        info->pNext || info->descriptorPool == 0u ||
        info->descriptorSetCount == 0u ||
        info->descriptorSetCount > RIN_GPU_VULKAN_DESCRIPTOR_MAX_SETS ||
        !info->pSetLayouts)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < info->descriptorSetCount; ++index)
        sets_out[index] = 0u;
    for (index = 0u; index < info->descriptorSetCount; ++index) {
        RinGpuVulkanDescriptorHandleV1 set = 0u;
        int result = rin_gpu_vulkan_descriptor_set_allocate(
            &owner->descriptor_runtime,
            (RinGpuVulkanDescriptorHandleV1)info->descriptorPool,
            (RinGpuVulkanDescriptorHandleV1)info->pSetLayouts[index], &set);
        if (result != RIN_GPU_VULKAN_GRAPHICS_OK) {
            uint32_t cleanup;
            for (cleanup = 0u; cleanup < index; ++cleanup)
                (void)rin_gpu_vulkan_descriptor_set_free(
                    &owner->descriptor_runtime,
                    (RinGpuVulkanDescriptorHandleV1)sets_out[cleanup]);
            return map_descriptor_result(result);
        }
        sets_out[index] = (RinVkDescriptorSet)set;
    }
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkFreeDescriptorSets(
        RinVkDevice device, RinVkDescriptorPool pool, uint32_t set_count,
        const RinVkDescriptorSet* sets) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    RinVkResult result = RIN_VK_SUCCESS;
    if (!owner || pool == 0u || set_count == 0u || !sets)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < set_count; ++index) {
        int current = rin_gpu_vulkan_descriptor_set_free_from_pool(
            &owner->descriptor_runtime,
            (RinGpuVulkanDescriptorHandleV1)pool,
            (RinGpuVulkanDescriptorHandleV1)sets[index]);
        if (current != RIN_GPU_VULKAN_GRAPHICS_OK)
            result = map_descriptor_result(current);
    }
    return result;
}

void RIN_VKAPI_CALL vkUpdateDescriptorSets(
        RinVkDevice device, uint32_t write_count,
        const RinVkWriteDescriptorSet* writes, uint32_t copy_count,
        const void* copies) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    if (!owner || (write_count != 0u && !writes) || copy_count != 0u ||
        copies || write_count > RIN_SHADER_MAX_RESOURCES) {
        if (owner) __atomic_store_n(&owner->descriptor_validation_error, 1u,
                                    __ATOMIC_RELEASE);
        return;
    }
    for (index = 0u; index < write_count; ++index) {
        const RinVkWriteDescriptorSet* source = &writes[index];
        RinGpuVulkanDescriptorWriteV1 converted[RIN_SHADER_MAX_RESOURCES];
        uint32_t item;
        uint32_t type = descriptor_runtime_type(source->descriptorType);
        int invalid = source->sType != 0 || source->pNext ||
                      source->dstSet == 0u || source->descriptorCount == 0u ||
                      source->descriptorCount > RIN_SHADER_MAX_RESOURCES ||
                      type == 0u;
        if (source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_SAMPLER ||
            source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
            invalid = invalid || !source->pImageInfo;
        else if (source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                 source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                 source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                 source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            invalid = invalid || !source->pBufferInfo;
        else
            invalid = 1;
        if (invalid) {
            __atomic_store_n(&owner->descriptor_validation_error, 1u,
                             __ATOMIC_RELEASE);
            continue;
        }
        memset(converted, 0, sizeof(converted));
        for (item = 0u; item < source->descriptorCount; ++item) {
            RinGpuVulkanDescriptorWriteV1* destination = &converted[item];
            destination->set = 0u;
            destination->binding = source->dstBinding;
            destination->array_element = source->dstArrayElement + item;
            destination->resource_index = source->dstBinding;
            destination->descriptor_type = type;
            if (source->pBufferInfo) {
                const RinVkDescriptorBufferInfo* buffer_info =
                    &source->pBufferInfo[item];
                RinVkBufferSlot* buffer = buffer_slot(owner, buffer_info->buffer);
                if (!buffer || !buffer->memory || buffer_info->offset > buffer->size ||
                    buffer_info->range == 0u ||
                    buffer_info->range > buffer->size - buffer_info->offset) {
                    invalid = 1;
                    break;
                }
                destination->resource = (RinGpuHandle)buffer_info->buffer;
                destination->access =
                    type == RIN_GPU_VULKAN_DESCRIPTOR_UNIFORM_BUFFER
                        ? RIN_GPU_RESOURCE_READ
                        : RIN_GPU_RESOURCE_READ | RIN_GPU_RESOURCE_WRITE;
                destination->offset = buffer_info->offset;
                destination->size_bytes = buffer_info->range;
            } else {
                const RinVkDescriptorImageInfo* image_info =
                    &source->pImageInfo[item];
                if (source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_SAMPLER) {
                    if (!sampler_slot(owner, image_info->sampler)) {
                        invalid = 1;
                        break;
                    }
                    destination->resource =
                        (RinGpuHandle)image_info->sampler;
                } else {
                    RinVkImageViewSlot* view =
                        image_view_slot(owner, image_info->imageView);
                    if (!view || (image_info->imageLayout != RIN_VK_IMAGE_LAYOUT_GENERAL &&
                                  image_info->imageLayout != 5u)) {
                        invalid = 1;
                        break;
                    }
                    destination->resource = (RinGpuHandle)resource_handle(
                        RIN_VK_IMAGE_TAG,
                        (uint32_t)(view->image - &g_images[0]),
                        view->image->generation);
                    destination->access = RIN_GPU_RESOURCE_READ;
                }
            }
        }
        if (invalid || rin_gpu_vulkan_descriptor_set_update(
                            &owner->descriptor_runtime,
                            (RinGpuVulkanDescriptorHandleV1)source->dstSet,
                            converted, source->descriptorCount) !=
                        RIN_GPU_VULKAN_GRAPHICS_OK)
            __atomic_store_n(&owner->descriptor_validation_error, 1u,
                             __ATOMIC_RELEASE);
    }
}

RinVkResult RIN_VKAPI_CALL vkCreatePipelineLayout(
        RinVkDevice device, const RinVkPipelineLayoutCreateInfo* info,
        const void* allocator, RinVkPipelineLayout* layout_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkPipelineLayoutSlot* layout;
    uint32_t index;
    (void)allocator;
    if (!layout_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *layout_out = 0u;
    if (!owner || !info ||
        info->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO ||
        info->pNext || info->flags != 0u ||
        info->setLayoutCount > RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS ||
        (info->setLayoutCount != 0u && !info->pSetLayouts) ||
        info->pushConstantRangeCount != 0u || info->pPushConstantRanges)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    for (index = 0u; index < info->setLayoutCount; ++index)
        if (!rin_gpu_vulkan_descriptor_layout_is_valid(
                &owner->descriptor_runtime,
                (RinGpuVulkanDescriptorHandleV1)info->pSetLayouts[index]))
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
    layout = reserve_pipeline_layout_slot(owner, &index);
    if (!layout) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    layout->set_layout_count = info->setLayoutCount;
    if (info->setLayoutCount != 0u)
        memcpy(layout->set_layouts, info->pSetLayouts,
               sizeof(*info->pSetLayouts) * info->setLayoutCount);
    __atomic_store_n(&layout->state, 1u, __ATOMIC_RELEASE);
    *layout_out = resource_handle(RIN_VK_PIPELINE_LAYOUT_TAG, index,
                                  layout->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyPipelineLayout(RinVkDevice device,
                                            RinVkPipelineLayout handle,
                                            const void* allocator) {
    RinVkPipelineLayoutSlot* layout;
    (void)allocator;
    layout = pipeline_layout_slot(device, handle);
    if (layout) clear_pipeline_layout_slot(layout);
}

void RIN_VKAPI_CALL vkCmdBindDescriptorSets(
        RinVkCommandBuffer command_buffer, uint32_t pipeline_bind_point,
        RinVkPipelineLayout layout_handle, uint32_t first_set,
        uint32_t descriptor_set_count, const RinVkDescriptorSet* descriptor_sets,
        uint32_t dynamic_offset_count, const uint32_t* dynamic_offsets) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinVkPipelineLayoutSlot* layout;
    uintptr_t owner_address = 0u;
    struct RinVkDevice_T* owner;
    uint64_t set_handles[RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
    uint32_t dynamic_cursor = 0u;
    uint32_t index;
    int result;
    if (pipeline_bind_point > 1u || first_set > UINT32_MAX - descriptor_set_count ||
        descriptor_set_count == 0u ||
        descriptor_set_count > RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS ||
        !descriptor_sets || dynamic_offset_count >
            RIN_GPU_VULKAN_COMMAND_MAX_DYNAMIC_OFFSETS ||
        (dynamic_offset_count != 0u && !dynamic_offsets)) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    result = rin_gpu_vulkan_command_buffer_owner(
        &g_command_runtime, command, &owner_address);
    owner = result == RIN_GPU_VULKAN_COMMAND_OK
                ? device_slot((RinVkDevice)(void*)owner_address)
                : NULL;
    layout = owner ? pipeline_layout_slot(owner, layout_handle) : NULL;
    if (!owner || !layout || first_set > layout->set_layout_count ||
        descriptor_set_count > layout->set_layout_count - first_set) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    for (index = 0u; index < descriptor_set_count; ++index) {
        uint32_t required = 0u;
        result = rin_gpu_vulkan_descriptor_set_dynamic_offset_count(
            &owner->descriptor_runtime,
            (RinGpuVulkanDescriptorHandleV1)descriptor_sets[index], &required);
        if (result != RIN_GPU_VULKAN_GRAPHICS_OK ||
            !rin_gpu_vulkan_descriptor_set_matches_layout(
                &owner->descriptor_runtime,
                (RinGpuVulkanDescriptorHandleV1)descriptor_sets[index],
                (RinGpuVulkanDescriptorHandleV1)layout->set_layouts[first_set +
                                                                      index]) ||
            required > dynamic_offset_count - dynamic_cursor ||
            rin_gpu_vulkan_descriptor_set_validate_dynamic_offsets(
                &owner->descriptor_runtime,
                (RinGpuVulkanDescriptorHandleV1)descriptor_sets[index],
                dynamic_offsets ? dynamic_offsets + dynamic_cursor : NULL,
                required, &required) != RIN_GPU_VULKAN_GRAPHICS_OK) {
            rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                         command);
            return;
        }
        set_handles[index] = descriptor_sets[index];
        dynamic_cursor += required;
    }
    if (dynamic_cursor != dynamic_offset_count ||
        rin_gpu_vulkan_command_buffer_record_descriptor_bind(
            &g_command_runtime, command, first_set, set_handles,
            descriptor_set_count, dynamic_offsets, dynamic_offset_count) !=
            RIN_GPU_VULKAN_COMMAND_OK)
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
}

RinVkVoidFunction RIN_VKAPI_CALL vkGetDeviceProcAddr(
        RinVkDevice device, const char* name) {
    if (!device_slot(device) || !name) return NULL;
    if (name_equal(name, "vkGetDeviceProcAddr"))
        return (RinVkVoidFunction)vkGetDeviceProcAddr;
    if (name_equal(name, "vkDestroyDevice"))
        return (RinVkVoidFunction)vkDestroyDevice;
    if (name_equal(name, "vkGetDeviceQueue"))
        return (RinVkVoidFunction)vkGetDeviceQueue;
    if (name_equal(name, "vkDeviceWaitIdle"))
        return (RinVkVoidFunction)vkDeviceWaitIdle;
    if (name_equal(name, "vkQueueWaitIdle"))
        return (RinVkVoidFunction)vkQueueWaitIdle;
    if (name_equal(name, "vkCreateFence"))
        return (RinVkVoidFunction)vkCreateFence;
    if (name_equal(name, "vkDestroyFence"))
        return (RinVkVoidFunction)vkDestroyFence;
    if (name_equal(name, "vkResetFences"))
        return (RinVkVoidFunction)vkResetFences;
    if (name_equal(name, "vkGetFenceStatus"))
        return (RinVkVoidFunction)vkGetFenceStatus;
    if (name_equal(name, "vkWaitForFences"))
        return (RinVkVoidFunction)vkWaitForFences;
    if (name_equal(name, "vkCreateSemaphore"))
        return (RinVkVoidFunction)vkCreateSemaphore;
    if (name_equal(name, "vkDestroySemaphore"))
        return (RinVkVoidFunction)vkDestroySemaphore;
    if (name_equal(name, "vkGetSemaphoreCounterValue"))
        return (RinVkVoidFunction)vkGetSemaphoreCounterValue;
    if (name_equal(name, "vkSignalSemaphore"))
        return (RinVkVoidFunction)vkSignalSemaphore;
    if (name_equal(name, "vkWaitSemaphores"))
        return (RinVkVoidFunction)vkWaitSemaphores;
    if (name_equal(name, "vkQueueSubmit"))
        return (RinVkVoidFunction)vkQueueSubmit;
    if (name_equal(name, "vkCreateCommandPool"))
        return (RinVkVoidFunction)vkCreateCommandPool;
    if (name_equal(name, "vkDestroyCommandPool"))
        return (RinVkVoidFunction)vkDestroyCommandPool;
    if (name_equal(name, "vkResetCommandPool"))
        return (RinVkVoidFunction)vkResetCommandPool;
    if (name_equal(name, "vkAllocateCommandBuffers"))
        return (RinVkVoidFunction)vkAllocateCommandBuffers;
    if (name_equal(name, "vkFreeCommandBuffers"))
        return (RinVkVoidFunction)vkFreeCommandBuffers;
    if (name_equal(name, "vkBeginCommandBuffer"))
        return (RinVkVoidFunction)vkBeginCommandBuffer;
    if (name_equal(name, "vkEndCommandBuffer"))
        return (RinVkVoidFunction)vkEndCommandBuffer;
    if (name_equal(name, "vkResetCommandBuffer"))
        return (RinVkVoidFunction)vkResetCommandBuffer;
    if (name_equal(name, "vkCmdCopyBuffer"))
        return (RinVkVoidFunction)vkCmdCopyBuffer;
    if (name_equal(name, "vkCmdCopyImage"))
        return (RinVkVoidFunction)vkCmdCopyImage;
    if (name_equal(name, "vkCmdCopyBufferToImage"))
        return (RinVkVoidFunction)vkCmdCopyBufferToImage;
    if (name_equal(name, "vkCmdCopyImageToBuffer"))
        return (RinVkVoidFunction)vkCmdCopyImageToBuffer;
    if (name_equal(name, "vkCmdBlitImage"))
        return (RinVkVoidFunction)vkCmdBlitImage;
    if (name_equal(name, "vkCmdResolveImage"))
        return (RinVkVoidFunction)vkCmdResolveImage;
    if (name_equal(name, "vkCmdClearColorImage"))
        return (RinVkVoidFunction)vkCmdClearColorImage;
    if (name_equal(name, "vkAllocateMemory"))
        return (RinVkVoidFunction)vkAllocateMemory;
    if (name_equal(name, "vkFreeMemory"))
        return (RinVkVoidFunction)vkFreeMemory;
    if (name_equal(name, "vkCreateBuffer"))
        return (RinVkVoidFunction)vkCreateBuffer;
    if (name_equal(name, "vkDestroyBuffer"))
        return (RinVkVoidFunction)vkDestroyBuffer;
    if (name_equal(name, "vkGetBufferMemoryRequirements"))
        return (RinVkVoidFunction)vkGetBufferMemoryRequirements;
    if (name_equal(name, "vkBindBufferMemory"))
        return (RinVkVoidFunction)vkBindBufferMemory;
    if (name_equal(name, "vkCreateImage"))
        return (RinVkVoidFunction)vkCreateImage;
    if (name_equal(name, "vkDestroyImage"))
        return (RinVkVoidFunction)vkDestroyImage;
    if (name_equal(name, "vkGetImageMemoryRequirements"))
        return (RinVkVoidFunction)vkGetImageMemoryRequirements;
    if (name_equal(name, "vkBindImageMemory"))
        return (RinVkVoidFunction)vkBindImageMemory;
    if (name_equal(name, "vkCreateImageView"))
        return (RinVkVoidFunction)vkCreateImageView;
    if (name_equal(name, "vkDestroyImageView"))
        return (RinVkVoidFunction)vkDestroyImageView;
    if (name_equal(name, "vkCreateSampler"))
        return (RinVkVoidFunction)vkCreateSampler;
    if (name_equal(name, "vkDestroySampler"))
        return (RinVkVoidFunction)vkDestroySampler;
    if (name_equal(name, "vkCreateDescriptorSetLayout"))
        return (RinVkVoidFunction)vkCreateDescriptorSetLayout;
    if (name_equal(name, "vkDestroyDescriptorSetLayout"))
        return (RinVkVoidFunction)vkDestroyDescriptorSetLayout;
    if (name_equal(name, "vkCreateDescriptorPool"))
        return (RinVkVoidFunction)vkCreateDescriptorPool;
    if (name_equal(name, "vkDestroyDescriptorPool"))
        return (RinVkVoidFunction)vkDestroyDescriptorPool;
    if (name_equal(name, "vkAllocateDescriptorSets"))
        return (RinVkVoidFunction)vkAllocateDescriptorSets;
    if (name_equal(name, "vkFreeDescriptorSets"))
        return (RinVkVoidFunction)vkFreeDescriptorSets;
    if (name_equal(name, "vkUpdateDescriptorSets"))
        return (RinVkVoidFunction)vkUpdateDescriptorSets;
    if (name_equal(name, "vkCreatePipelineLayout"))
        return (RinVkVoidFunction)vkCreatePipelineLayout;
    if (name_equal(name, "vkDestroyPipelineLayout"))
        return (RinVkVoidFunction)vkDestroyPipelineLayout;
    if (name_equal(name, "vkCmdBindDescriptorSets"))
        return (RinVkVoidFunction)vkCmdBindDescriptorSets;
    return NULL;
}

RinVkVoidFunction RIN_VKAPI_CALL vkGetInstanceProcAddr(
        RinVkInstance instance, const char* name) {
    if (!name) return NULL;
    if (name_equal(name, "vkGetInstanceProcAddr"))
        return (RinVkVoidFunction)vkGetInstanceProcAddr;
    if (name_equal(name, "vkCreateInstance"))
        return (RinVkVoidFunction)vkCreateInstance;
    if (name_equal(name, "vkEnumerateInstanceVersion"))
        return (RinVkVoidFunction)vkEnumerateInstanceVersion;
    if (name_equal(name, "vkEnumerateInstanceExtensionProperties"))
        return (RinVkVoidFunction)vkEnumerateInstanceExtensionProperties;
    if (name_equal(name, "vkEnumerateDeviceExtensionProperties"))
        return (RinVkVoidFunction)vkEnumerateDeviceExtensionProperties;
    if (name_equal(name, "vkEnumerateInstanceLayerProperties"))
        return (RinVkVoidFunction)vkEnumerateInstanceLayerProperties;
    if (name_equal(name, "vk_icdNegotiateLoaderICDInterfaceVersion"))
        return (RinVkVoidFunction)
            vk_icdNegotiateLoaderICDInterfaceVersion;
    if (name_equal(name, "vk_icdGetPhysicalDeviceProcAddr"))
        return (RinVkVoidFunction)vk_icdGetPhysicalDeviceProcAddr;
    if (!instance_slot(instance)) return NULL;
    if (name_equal(name, "vkDestroyInstance"))
        return (RinVkVoidFunction)vkDestroyInstance;
    if (name_equal(name, "vkEnumeratePhysicalDevices"))
        return (RinVkVoidFunction)vkEnumeratePhysicalDevices;
    if (name_equal(name, "vkGetPhysicalDeviceFeatures"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceFeatures;
    if (name_equal(name, "vkGetPhysicalDeviceFeatures2"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceFeatures2;
    if (name_equal(name, "vkGetPhysicalDeviceProperties"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceProperties;
    if (name_equal(name, "vkGetPhysicalDeviceProperties2"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceProperties2;
    if (name_equal(name, "vkGetPhysicalDeviceQueueFamilyProperties"))
        return (RinVkVoidFunction)
            vkGetPhysicalDeviceQueueFamilyProperties;
    if (name_equal(name, "vkGetPhysicalDeviceMemoryProperties"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceMemoryProperties;
    if (name_equal(name, "vkEnumerateDeviceExtensionProperties"))
        return (RinVkVoidFunction)vkEnumerateDeviceExtensionProperties;
    if (name_equal(name, "vkCreateDevice"))
        return (RinVkVoidFunction)vkCreateDevice;
    if (name_equal(name, "vkGetDeviceProcAddr"))
        return (RinVkVoidFunction)vkGetDeviceProcAddr;
    return NULL;
}

RinVkVoidFunction RIN_VKAPI_CALL vk_icdGetInstanceProcAddr(
        RinVkInstance instance, const char* name) {
    return vkGetInstanceProcAddr(instance, name);
}

RinVkVoidFunction RIN_VKAPI_CALL vk_icdGetPhysicalDeviceProcAddr(
        RinVkInstance instance, const char* name) {
    if (!instance_slot(instance) || !name) return NULL;
    if (name_equal(name, "vkGetPhysicalDeviceFeatures"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceFeatures;
    if (name_equal(name, "vkGetPhysicalDeviceFeatures2"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceFeatures2;
    if (name_equal(name, "vkGetPhysicalDeviceProperties"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceProperties;
    if (name_equal(name, "vkGetPhysicalDeviceProperties2"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceProperties2;
    if (name_equal(name, "vkGetPhysicalDeviceQueueFamilyProperties"))
        return (RinVkVoidFunction)
            vkGetPhysicalDeviceQueueFamilyProperties;
    if (name_equal(name, "vkGetPhysicalDeviceMemoryProperties"))
        return (RinVkVoidFunction)vkGetPhysicalDeviceMemoryProperties;
    if (name_equal(name, "vkEnumerateDeviceExtensionProperties"))
        return (RinVkVoidFunction)vkEnumerateDeviceExtensionProperties;
    return NULL;
}
