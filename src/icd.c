/* SPDX-License-Identifier: MIT */

#include <rinvulkan/icd.h>
#include <rinvulkan/command_runtime.h>
#include <rinvulkan/descriptor_runtime.h>
#include <rinvulkan/graphics.h>

#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "atomic_compat.h"
#include "buffer_ownership.h"
#include "platform/time.h"
#include "sync2_scope.h"

#define RIN_VK_ICD_BINDING_TRANSITION UINTPTR_MAX
#define RIN_VK_ICD_CALL_RETRIES 4096u
#define RIN_VK_FEATURE_CHAIN_MAX 8u
#define RIN_VK_PROPERTY_CHAIN_MAX 8u
#define RIN_VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO 47
#define RIN_VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO 48
#define RIN_VK_MAX_BUFFERS 128u
#define RIN_VK_MAX_IMAGES 128u
#define RIN_VK_MAX_MEMORIES 64u
#define RIN_VK_MAX_DISPLAYS 128u
#define RIN_VK_MAX_DISPLAY_MODES 512u
#define RIN_VK_MAX_DISPLAY_SURFACES 64u
#define RIN_VK_MAX_SWAPCHAINS 32u
#define RIN_VK_MAX_SWAPCHAIN_IMAGES 8u
#define RIN_VK_MAX_WSI_PRESENTS \
    (RIN_VK_MAX_SWAPCHAINS * RIN_VK_MAX_SWAPCHAIN_IMAGES)
#define RIN_VK_SWAPCHAIN_IMAGE_AVAILABLE 0u
#define RIN_VK_SWAPCHAIN_IMAGE_ACQUIRED 1u
#define RIN_VK_SWAPCHAIN_IMAGE_PRESENT_PENDING 2u
#define RIN_VK_SWAPCHAIN_IMAGE_PRESENT_UNTRACKED 3u
#define RIN_VK_PRESENT_RECORD_ACTIVE 1u
#define RIN_VK_PRESENT_RECORD_RESERVED 2u
#define RIN_VK_PRESENT_RECORD_UNTRACKED 3u
#define RIN_VK_MAX_FENCES 128u
#define RIN_VK_MAX_SEMAPHORES 128u
#define RIN_VK_MAX_SUBMISSIONS 64u
#define RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS 4u
#define RIN_VK_SUBMISSION_ACTIVE 1u
#define RIN_VK_SUBMISSION_RESERVED 2u
#define RIN_VK_SUBMISSION_WAITING 3u
#define RIN_VK_MAX_IMAGE_VIEWS 128u
#define RIN_VK_MAX_SAMPLERS 128u
#define RIN_VK_MAX_PIPELINE_LAYOUTS 64u
#define RIN_VK_MAX_PIPELINE_CACHES 32u
#define RIN_VK_MAX_PIPELINES 64u
#define RIN_VK_MAX_SHADER_MODULES 64u
#define RIN_VK_MAX_QUERY_POOLS 32u
#define RIN_VK_MAX_EVENTS 128u
#define RIN_VK_MAX_DEBUG_MESSENGERS 32u
#define RIN_VK_MAX_DEBUG_OBJECTS 256u
#define RIN_VK_DEBUG_UTILS_MESSENGER_TAG UINT64_C(0x5255)
#define RIN_VK_DEBUG_SEVERITY_MASK \
    (RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | \
     RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | \
     RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | \
     RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
#define RIN_VK_DEBUG_TYPE_MASK \
    (RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | \
     RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | \
     RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
#define RIN_VK_QUERY_POOL_MAX_QUERIES 64u
#define RIN_VK_PIPELINE_CACHE_MAX_PAYLOAD 4096u
#define RIN_VK_RESOURCE_ALIGNMENT UINT64_C(2097152)
#define RIN_VK_BUFFER_TAG UINT64_C(0x5242)
#define RIN_VK_IMAGE_TAG UINT64_C(0x5249)
#define RIN_VK_MEMORY_TAG UINT64_C(0x524d)
#define RIN_VK_FENCE_TAG UINT64_C(0x5246)
#define RIN_VK_SEMAPHORE_TAG UINT64_C(0x5253)
#define RIN_VK_IMAGE_VIEW_TAG UINT64_C(0x5256)
#define RIN_VK_SAMPLER_TAG UINT64_C(0x5254)
#define RIN_VK_PIPELINE_LAYOUT_TAG UINT64_C(0x5250)
#define RIN_VK_PIPELINE_CACHE_TAG UINT64_C(0x5243)
#define RIN_VK_PIPELINE_TAG UINT64_C(0x5258)
#define RIN_VK_SHADER_MODULE_TAG UINT64_C(0x5248)
#define RIN_VK_QUERY_POOL_TAG UINT64_C(0x5251)
#define RIN_VK_EVENT_TAG UINT64_C(0x5245)
#define RIN_VK_DISPLAY_TAG UINT64_C(0x5244)
#define RIN_VK_DISPLAY_MODE_TAG UINT64_C(0x524f)
#define RIN_VK_SURFACE_TAG UINT64_C(0x5259)
#define RIN_VK_SWAPCHAIN_TAG UINT64_C(0x5257)
#define RIN_VK_PIPELINE_CACHE_MAGIC UINT32_C(0x52494e43)
#define RIN_VK_PIPELINE_CACHE_VERSION 1u
#define RIN_VK_PIPELINE_KIND_COMPUTE 1u
#define RIN_VK_PIPELINE_KIND_GRAPHICS 2u

typedef struct RinVkBaseFeatureStructure {
    RinVkStructureType sType;
    void* pNext;
} RinVkBaseFeatureStructure;

typedef struct RinVkFeatureChain {
    RinVkPhysicalDeviceVulkan12Features* vulkan12;
    RinVkPhysicalDeviceVulkan13Features* vulkan13;
    RinVkPhysicalDeviceSynchronization2Features* synchronization2;
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
static void sync_lock(void);
static void sync_unlock(void);
static void yield_thread(void);
static int finite_graphics_float(float value);
static int device_has_swapchains(const struct RinVkDevice_T* device);
static int instance_has_swapchains(const struct RinVkInstance_T* instance);
static int call_query_physical(
    RinGpuVulkanRuntimeV1* runtime, RinGpuVulkanHandle instance,
    RinGpuVulkanHandle physical,
    RinGpuVulkanPhysicalDeviceV2* profile);
static void wsi_cleanup_instance(struct RinVkInstance_T* instance);

struct RinVkPhysicalDevice_T {
    uintptr_t loader_magic;
    RinGpuVulkanHandle owner_instance;
    RinGpuVulkanHandle runtime_handle;
};

struct RinVkInstance_T {
    uintptr_t loader_magic;
    uint32_t state;
    uint32_t debug_utils_enabled;
    RinGpuVulkanHandle runtime_handle;
    struct RinVkPhysicalDevice_T
        physical_devices[RIN_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
};

struct RinVkQueue_T {
    uintptr_t loader_magic;
    struct RinVkDevice_T* device;
    uint32_t queue_family_index;
    uint32_t queue_index;
    uint32_t family_queue_index;
    volatile uint32_t submit_lock;
    uint32_t reserved;
};

typedef struct RinVkPendingPresent {
    uint32_t state;
    uint32_t presentation_display_id;
    uint32_t queue_index;
    uint32_t image_index;
    uint64_t display_cookie;
    uint64_t image_token;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t frame_id;
    uint64_t platform_token;
} RinVkPendingPresent;

struct RinVkDevice_T {
    uintptr_t loader_magic;
    uint32_t state;
    uint32_t reserved;
    RinGpuVulkanHandle runtime_handle;
    RinGpuVulkanHandle owner_instance;
    struct RinVkPhysicalDevice_T* physical_device;
    RinGpuVulkanDevicePlanV1 plan;
    RinGpuVulkanPhysicalDeviceV2 physical_profile;
    RinGpuVulkanDescriptorRuntimeV1 descriptor_runtime;
    volatile uint32_t descriptor_validation_error;
    uint32_t timeline_enabled;
    uint32_t synchronization2_enabled;
    uint32_t dynamic_rendering_enabled;
    uint32_t queue_count;
    uint32_t reserved_queue;
    uint64_t next_submission_order;
    volatile uint32_t wsi_lock;
    RinVkPendingPresent pending_presents[RIN_VK_MAX_WSI_PRESENTS];
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
    RinVkBufferOwnershipState ownership;
} RinVkBufferSlot;

typedef struct RinVkImageOwnershipState {
    uint32_t owner_queue_family;
    uint32_t transfer_pending;
    uint32_t transfer_source_family;
    uint32_t transfer_destination_family;
    uint32_t transfer_old_layout;
    uint32_t transfer_new_layout;
    uint32_t transfer_semaphore_count;
    uint32_t reserved;
    RinVkSemaphore transfer_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t transfer_semaphore_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
} RinVkImageOwnershipState;

typedef struct RinVkImageSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    struct RinVkSwapchainSlot* swapchain_owner;
    RinVkMemorySlot* memory;
    uint32_t memory_generation;
    int32_t format;
    uint32_t usage;
    uint64_t memory_size;
    uint64_t memory_offset;
    uint32_t width;
    uint32_t height;
    uint32_t samples;
    uint32_t current_layout;
    RinVkImageOwnershipState ownership;
} RinVkImageSlot;

typedef struct RinVkDisplaySlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkInstance_T* owner_instance;
    struct RinVkPhysicalDevice_T* owner_physical_device;
    uint64_t display_cookie;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t current_mode_cookie;
    uint32_t width;
    uint32_t height;
    uint32_t refresh_millihertz;
    uint32_t format;
    uint32_t physical_width_mm;
    uint32_t physical_height_mm;
    uint32_t flags;
    uint32_t plane_count;
    uint32_t mode_count;
    char display_name[RIN_VULKAN_WSI_DISPLAY_NAME_SIZE];
} RinVkDisplaySlot;

typedef struct RinVkDisplayModeSlot {
    uint32_t state;
    uint32_t generation;
    RinVkDisplaySlot* display;
    uint32_t display_generation;
    uint32_t width;
    uint32_t height;
    uint32_t refresh_millihertz;
    uint32_t format;
    uint64_t mode_cookie;
    uint64_t output_generation;
} RinVkDisplayModeSlot;

typedef struct RinVkDisplaySurfaceSlot {
    uint32_t state;
    uint32_t generation;
    uint32_t active_queries;
    uint32_t swapchain_count;
    struct RinVkInstance_T* owner_instance;
    RinVkDisplaySlot* display;
    uint32_t display_generation;
    RinVkDisplayModeSlot* mode;
    uint32_t mode_generation;
    uint32_t plane_index;
    uint32_t plane_stack_index;
    uint32_t transform;
    uint32_t alpha_mode;
    float global_alpha;
    RinVkExtent2D image_extent;
    uint64_t output_generation;
    uint64_t device_generation;
} RinVkDisplaySurfaceSlot;

typedef struct RinVkSwapchainSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    RinVkDisplaySurfaceSlot* surface;
    RinVkSurfaceKHR surface_handle;
    uint32_t surface_generation;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t mode_cookie;
    uint32_t image_count;
    uint32_t present_mode;
    int32_t image_format;
    int32_t image_color_space;
    RinVkExtent2D image_extent;
    uint32_t image_usage;
    uint32_t retired;
    uint32_t presentation_display_id;
    uint32_t next_image_index;
    uint64_t next_frame_id;
    uint32_t image_states[RIN_VK_MAX_SWAPCHAIN_IMAGES];
    uint64_t acquired_frame_ids[RIN_VK_MAX_SWAPCHAIN_IMAGES];
    uint64_t present_tokens[RIN_VK_MAX_SWAPCHAIN_IMAGES];
    RinVkImage images[RIN_VK_MAX_SWAPCHAIN_IMAGES];
    RinVkDeviceMemory memories[RIN_VK_MAX_SWAPCHAIN_IMAGES];
} RinVkSwapchainSlot;

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

typedef struct RinVkPipelineCacheSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint32_t payload_size;
    uint32_t reserved;
    uint8_t payload[RIN_VK_PIPELINE_CACHE_MAX_PAYLOAD];
} RinVkPipelineCacheSlot;

typedef struct RinVkShaderModuleSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    size_t code_size;
    uint32_t* code;
} RinVkShaderModuleSlot;

typedef struct RinVkPipelineSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint32_t kind;
    uint32_t set_layout_count;
    uint32_t descriptor_count;
    uint32_t shader_size;
    uint32_t fragment_shader_size;
    RinVkDescriptorSetLayout
        set_layouts[RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
    RinSpirvDescriptorV1 descriptors[RIN_SPIRV_MAX_RESOURCES];
    RinGpuGraphicsPipelineBackendDescV1 graphics_backend;
    RinVkViewport viewport;
    RinVkRect2D scissor;
    uint8_t* shader_ir;
    uint8_t* fragment_shader_ir;
} RinVkPipelineSlot;

typedef struct RinVkQueryValue {
    uint64_t values[9];
    uint64_t availability;
    uint32_t active;
    uint32_t pending;
} RinVkQueryValue;

typedef struct RinVkQueryPoolSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    uint32_t query_type;
    uint32_t query_count;
    uint32_t pipeline_statistics;
    uint32_t reserved;
    RinVkQueryValue queries[RIN_VK_QUERY_POOL_MAX_QUERIES];
} RinVkQueryPoolSlot;

typedef struct RinVkEventSlot {
    uint32_t state;
    uint32_t generation;
    struct RinVkDevice_T* owner;
    volatile uint32_t signaled;
    volatile uint32_t pending;
} RinVkEventSlot;

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
    volatile uint32_t waiter_count;
    volatile uint64_t value;
    uint64_t pending_value;
} RinVkSemaphoreSlot;

typedef struct RinVkDebugUtilsMessengerSlot {
    uint32_t state;
    uint32_t generation;
    uint32_t active_callbacks;
    uint32_t reserved;
    struct RinVkInstance_T* owner;
    uint32_t message_severity;
    uint32_t message_type;
    RinVkDebugUtilsMessengerCallbackEXT callback;
    void* user_data;
} RinVkDebugUtilsMessengerSlot;

typedef struct RinVkDebugUtilsObjectSlot {
    uint32_t state;
    uint32_t object_type;
    struct RinVkInstance_T* owner;
    uint64_t object_handle;
    char* name;
    uint64_t tag_name;
    size_t tag_size;
    void* tag;
} RinVkDebugUtilsObjectSlot;

typedef struct RinVkBufferOwnershipUpdate RinVkBufferOwnershipUpdate;

typedef struct RinVkBufferOwnershipUpdate RinVkBufferOwnershipUpdate;

typedef struct RinVkSubmissionSlot {
    uint32_t state;
    uint32_t command_buffer_count;
    struct RinVkDevice_T* owner;
    uint32_t queue_id;
    uint32_t reserved;
    uint64_t sequence;
    uint64_t completion_value;
    uint64_t order;
    RinVkFence fence;
    uint32_t wait_semaphore_count;
    uint32_t signal_semaphore_count;
    uint32_t resource_count;
    uint32_t waits_reserved;
    RinVkSemaphore wait_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t wait_semaphore_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinVkSemaphore signal_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t signal_semaphore_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinGpuVulkanCommandBufferV1*
        command_buffers[RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS];
    RinGpuVulkanTransferPacketV2 extended_packet;
    RinGpuVulkanTransferPacketV3 routed_packet;
    RinGpuVulkanComputePacketV1* compute_packet;
    size_t compute_packet_size;
    RinGpuVulkanGraphicsPacketV1* graphics_packet;
    size_t graphics_packet_size;
    RinVkBufferOwnershipUpdate* compute_buffer_ownership_updates;
    uint32_t compute_buffer_ownership_update_count;
    RinVulkanProductResourceV1
        resources[RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION];
} RinVkSubmissionSlot;

#define RIN_VK_COMMAND_RESOURCE_USE_BUFFER 1u
#define RIN_VK_COMMAND_RESOURCE_USE_IMAGE 2u
#define RIN_VK_COMMAND_RESOURCE_ACCESS_READ 1u
#define RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE 2u
#define RIN_VK_MAX_COMMAND_RESOURCE_USES \
    (RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS * 2u)
#define RIN_VK_MAX_SUBMISSION_RESOURCE_USES \
    (RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS * 2u)

typedef struct RinVkCommandResourceUse {
    uint32_t operation_index;
    uint32_t resource_kind;
    uint32_t access;
    uint32_t image_layout;
    uint64_t resource_handle;
    uint64_t offset;
    uint64_t size;
} RinVkCommandResourceUse;

typedef struct RinVkCommandResourceUseSlot {
    RinGpuVulkanCommandBufferV1* command_buffer;
    uint32_t use_count;
    RinVkCommandResourceUse uses[RIN_VK_MAX_COMMAND_RESOURCE_USES];
} RinVkCommandResourceUseSlot;

struct RinVkBufferOwnershipUpdate {
    RinVkBufferSlot* buffer;
    RinVkBufferOwnershipState state;
};

static uintptr_t g_runtime_binding;
static uint32_t g_active_calls;
static uintptr_t g_product_binding;
static uint32_t g_active_product_calls;
static uintptr_t g_wsi_binding;
static uint32_t g_active_wsi_calls;
static struct RinVkInstance_T g_instances[RIN_GPU_VULKAN_MAX_INSTANCES];
static struct RinVkDevice_T g_devices[RIN_GPU_VULKAN_MAX_DEVICES];
static RinGpuVulkanCommandRuntimeV1 g_command_runtime;
static RinVkMemorySlot g_memories[RIN_VK_MAX_MEMORIES];
static RinVkBufferSlot g_buffers[RIN_VK_MAX_BUFFERS];
static RinVkImageSlot g_images[RIN_VK_MAX_IMAGES];
static RinVkDisplaySlot g_displays[RIN_VK_MAX_DISPLAYS];
static RinVkDisplayModeSlot g_display_modes[RIN_VK_MAX_DISPLAY_MODES];
static RinVkDisplaySurfaceSlot
    g_display_surfaces[RIN_VK_MAX_DISPLAY_SURFACES];
static RinVkSwapchainSlot g_swapchains[RIN_VK_MAX_SWAPCHAINS];
static RinVkImageViewSlot g_image_views[RIN_VK_MAX_IMAGE_VIEWS];
static RinVkSamplerSlot g_samplers[RIN_VK_MAX_SAMPLERS];
static RinVkPipelineLayoutSlot g_pipeline_layouts[RIN_VK_MAX_PIPELINE_LAYOUTS];
static RinVkPipelineCacheSlot g_pipeline_caches[RIN_VK_MAX_PIPELINE_CACHES];
static RinVkPipelineSlot g_pipelines[RIN_VK_MAX_PIPELINES];
static RinVkShaderModuleSlot g_shader_modules[RIN_VK_MAX_SHADER_MODULES];
static RinVkQueryPoolSlot g_query_pools[RIN_VK_MAX_QUERY_POOLS];
static RinVkEventSlot g_events[RIN_VK_MAX_EVENTS];
static RinVkFenceSlot g_fences[RIN_VK_MAX_FENCES];
static RinVkSemaphoreSlot g_semaphores[RIN_VK_MAX_SEMAPHORES];
static RinVkDebugUtilsMessengerSlot
    g_debug_utils_messengers[RIN_VK_MAX_DEBUG_MESSENGERS];
static RinVkDebugUtilsObjectSlot
    g_debug_utils_objects[RIN_VK_MAX_DEBUG_OBJECTS];
static RinVkSubmissionSlot g_submissions[RIN_VK_MAX_SUBMISSIONS];
static RinVkCommandResourceUseSlot
    g_command_resource_uses[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];

static int device_has_swapchains(const struct RinVkDevice_T* device) {
    uint32_t index;
    if (!device) return 0;
    for (index = 0u; index < RIN_VK_MAX_SWAPCHAINS; ++index) {
        const RinVkSwapchainSlot* swapchain = &g_swapchains[index];
        const uint32_t state =
            __atomic_load_n(&swapchain->state, __ATOMIC_ACQUIRE);
        if ((state == 1u || state == 2u) && swapchain->owner == device)
            return 1;
    }
    return 0;
}

static int instance_has_swapchains(const struct RinVkInstance_T* instance) {
    uint32_t index;
    if (!instance) return 0;
    for (index = 0u; index < RIN_VK_MAX_SWAPCHAINS; ++index) {
        const RinVkSwapchainSlot* swapchain = &g_swapchains[index];
        const uint32_t state =
            __atomic_load_n(&swapchain->state, __ATOMIC_ACQUIRE);
        if ((state == 1u || state == 2u) && swapchain->surface &&
            swapchain->surface->owner_instance == instance)
            return 1;
    }
    return 0;
}

static RinVkCommandResourceUseSlot* command_resource_use_slot(
        RinGpuVulkanCommandBufferV1* command_buffer, int create) {
    RinVkCommandResourceUseSlot* free_slot = NULL;
    uint32_t index;
    if (!command_buffer) return NULL;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS; ++index) {
        RinVkCommandResourceUseSlot* slot = &g_command_resource_uses[index];
        if (slot->command_buffer == command_buffer) return slot;
        if (!slot->command_buffer && !free_slot) free_slot = slot;
    }
    if (!create || !free_slot) return NULL;
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->command_buffer = command_buffer;
    return free_slot;
}

static void clear_command_resource_uses(
        RinGpuVulkanCommandBufferV1* command_buffer) {
    RinVkCommandResourceUseSlot* slot =
        command_resource_use_slot(command_buffer, 0);
    if (slot) memset(slot, 0, sizeof(*slot));
}

static void clear_command_pool_resource_uses(
        RinGpuVulkanCommandPoolSlotV1* pool) {
    uint32_t index;
    if (!pool) return;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS; ++index) {
        RinVkCommandResourceUseSlot* slot = &g_command_resource_uses[index];
        if (slot->command_buffer && slot->command_buffer->pool == pool)
            memset(slot, 0, sizeof(*slot));
    }
}

static int append_command_resource_uses(
        RinGpuVulkanCommandBufferV1* command_buffer,
        const RinVkCommandResourceUse uses[], uint32_t use_count) {
    RinVkCommandResourceUseSlot* slot;
    uint32_t index;
    if (!command_buffer || (use_count != 0u && !uses)) return 0;
    if (use_count == 0u) return 1;
    slot = command_resource_use_slot(command_buffer, 1);
    if (!slot || use_count > RIN_VK_MAX_COMMAND_RESOURCE_USES -
                                slot->use_count)
        return 0;
    for (index = 0u; index < use_count; ++index) {
        const RinVkCommandResourceUse* use = &uses[index];
        if (use->operation_index >= command_buffer->transfer_op_count ||
            (use->resource_kind != RIN_VK_COMMAND_RESOURCE_USE_BUFFER &&
             use->resource_kind != RIN_VK_COMMAND_RESOURCE_USE_IMAGE) ||
            use->access == 0u ||
            (use->access & ~(RIN_VK_COMMAND_RESOURCE_ACCESS_READ |
                             RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE)) != 0u ||
            use->resource_handle == 0u || use->size == 0u)
            return 0;
    }
    memcpy(&slot->uses[slot->use_count], uses,
           sizeof(*uses) * use_count);
    slot->use_count += use_count;
    return 1;
}

static int snapshot_command_resource_uses(
        RinGpuVulkanCommandBufferV1* const command_buffers[],
        uint32_t command_buffer_count, uint32_t packet_op_count,
        RinVkCommandResourceUse uses[], uint32_t* use_count_out) {
    uint32_t use_count = 0u;
    uint32_t operation_base = 0u;
    uint32_t buffer_index;
    if (!command_buffers || !uses || !use_count_out ||
        command_buffer_count == 0u ||
        command_buffer_count > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS ||
        packet_op_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
        return 0;
    for (buffer_index = 0u; buffer_index < command_buffer_count;
         ++buffer_index) {
        RinGpuVulkanCommandBufferV1* command_buffer =
            command_buffers[buffer_index];
        RinVkCommandResourceUseSlot* slot;
        uint32_t use_index;
        if (!command_buffer ||
            command_buffer->copy_count > RIN_GPU_VULKAN_COMMAND_MAX_COPIES ||
            command_buffer->transfer_op_count >
                RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS ||
            command_buffer->copy_count > packet_op_count ||
            command_buffer->transfer_op_count >
                packet_op_count - command_buffer->copy_count ||
            operation_base > packet_op_count -
                                 command_buffer->copy_count -
                                 command_buffer->transfer_op_count)
            return 0;
        slot = command_resource_use_slot(command_buffer, 0);
        if (slot) {
            if (slot->use_count > RIN_VK_MAX_COMMAND_RESOURCE_USES ||
                use_count > RIN_VK_MAX_SUBMISSION_RESOURCE_USES -
                                slot->use_count)
                return 0;
            for (use_index = 0u; use_index < slot->use_count; ++use_index) {
                uses[use_count] = slot->uses[use_index];
                if (uses[use_count].operation_index >=
                    command_buffer->transfer_op_count)
                    return 0;
                uses[use_count].operation_index +=
                    operation_base + command_buffer->copy_count;
                ++use_count;
            }
        }
        operation_base += command_buffer->copy_count +
                          command_buffer->transfer_op_count;
    }
    if (operation_base != packet_op_count) return 0;
    *use_count_out = use_count;
    return 1;
}

static int all_zero(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t index;
    for (index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    return 1;
}

static int collect_feature_chain(void* first, int allow_unknown,
                                 int allow_loader_device_info,
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
        } else if (node->sType ==
                   RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES) {
            if (chain->synchronization2) return 0;
            chain->synchronization2 =
                (RinVkPhysicalDeviceSynchronization2Features*)node;
        } else if (allow_loader_device_info &&
                   node->sType ==
                       RIN_VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO) {
            /* Loader-private dispatch metadata is consumed by the loader and
             * is not a Vulkan feature request. */
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

static int requested_synchronization2_feature(
        const RinVkPhysicalDeviceSynchronization2Features* features,
        uint64_t* required) {
    RinVkPhysicalDeviceSynchronization2Features snapshot;
    if (!features) return 1;
    snapshot = *features;
    if (snapshot.sType !=
            RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES ||
        snapshot.synchronization2 > 1u)
        return 0;
    if (snapshot.synchronization2 != 0u)
        *required |= RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2;
    return 1;
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

static int extension_enabled(const RinVkDeviceCreateInfo* info,
                             const char* extension_name) {
    uint32_t index;
    if (!info || !extension_name || info->enabledExtensionCount == 0u)
        return 0;
    if (!info->ppEnabledExtensionNames) return 0;
    for (index = 0u; index < info->enabledExtensionCount; ++index) {
        if (!info->ppEnabledExtensionNames[index]) return 0;
        if (name_equal(info->ppEnabledExtensionNames[index],
                       extension_name))
            return 1;
    }
    return 0;
}

static int timeline_extension_enabled(const RinVkDeviceCreateInfo* info) {
    return extension_enabled(info, RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION);
}

static int synchronization2_extension_enabled(
        const RinVkDeviceCreateInfo* info) {
    return extension_enabled(info, RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION);
}

static int dynamic_rendering_extension_enabled(
        const RinVkDeviceCreateInfo* info) {
    return extension_enabled(info, RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION);
}

static int device_extensions_valid(const RinVkDeviceCreateInfo* info) {
    uint32_t index;
    uint32_t prior;
    if (!info || info->enabledExtensionCount > 3u) return 0;
    if (info->enabledExtensionCount == 0u)
        return info->ppEnabledExtensionNames == NULL;
    if (!info->ppEnabledExtensionNames) return 0;
    for (index = 0u; index < info->enabledExtensionCount; ++index) {
        const char* name = info->ppEnabledExtensionNames[index];
        if (!name ||
            (!name_equal(name, RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION) &&
             !name_equal(name, RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION) &&
             !name_equal(name, RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION)))
            return 0;
        for (prior = 0u; prior < index; ++prior) {
            if (name_equal(name, info->ppEnabledExtensionNames[prior]))
                return 0;
        }
    }
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

static RinVulkanWsiPlatformV1* acquire_wsi(void) {
    uint32_t attempt;
    for (attempt = 0u; attempt < RIN_VK_ICD_CALL_RETRIES; ++attempt) {
        uintptr_t binding =
            __atomic_load_n(&g_wsi_binding, __ATOMIC_ACQUIRE);
        if (binding == 0u || binding == RIN_VK_ICD_BINDING_TRANSITION)
            return NULL;
        __atomic_add_fetch(&g_active_wsi_calls, 1u, __ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_wsi_binding, __ATOMIC_ACQUIRE) == binding)
            return (RinVulkanWsiPlatformV1*)binding;
        __atomic_sub_fetch(&g_active_wsi_calls, 1u, __ATOMIC_RELEASE);
    }
    return NULL;
}

static void release_wsi(void) {
    __atomic_sub_fetch(&g_active_wsi_calls, 1u, __ATOMIC_RELEASE);
}

static RinVulkanWsiPlatformV2* acquire_wsi_v2(void) {
    RinVulkanWsiPlatformV1* platform = acquire_wsi();
    if (!platform) return NULL;
    if ((platform->version == RIN_VULKAN_WSI_PLATFORM_V2_VERSION &&
         platform->struct_size == sizeof(RinVulkanWsiPlatformV2)) ||
        (platform->version == RIN_VULKAN_WSI_PLATFORM_V3_VERSION &&
         platform->struct_size == sizeof(RinVulkanWsiPlatformV3)) ||
        (platform->version == RIN_VULKAN_WSI_PLATFORM_V4_VERSION &&
         platform->struct_size == sizeof(RinVulkanWsiPlatformV4)))
        return (RinVulkanWsiPlatformV2*)platform;
    release_wsi();
    return NULL;
}

static RinVulkanWsiPlatformV3* acquire_wsi_v3(void) {
    RinVulkanWsiPlatformV1* platform = acquire_wsi();
    if (!platform) return NULL;
    if ((platform->version == RIN_VULKAN_WSI_PLATFORM_V3_VERSION &&
         platform->struct_size == sizeof(RinVulkanWsiPlatformV3)) ||
        (platform->version == RIN_VULKAN_WSI_PLATFORM_V4_VERSION &&
         platform->struct_size == sizeof(RinVulkanWsiPlatformV4)))
        return (RinVulkanWsiPlatformV3*)platform;
    release_wsi();
    return NULL;
}

static RinVulkanWsiPlatformV4* acquire_wsi_v4(void) {
    RinVulkanWsiPlatformV1* platform = acquire_wsi();
    if (!platform) return NULL;
    if (platform->version != RIN_VULKAN_WSI_PLATFORM_V4_VERSION ||
        platform->struct_size != sizeof(RinVulkanWsiPlatformV4)) {
        release_wsi();
        return NULL;
    }
    return (RinVulkanWsiPlatformV4*)platform;
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

static RinVkDisplaySlot* display_slot_from_handle(
        struct RinVkPhysicalDevice_T* physical, RinVkDisplayKHR handle) {
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkDisplaySlot* slot;
    if (!physical || (handle >> 48u) != RIN_VK_DISPLAY_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_DISPLAYS ||
        generation == 0u)
        return NULL;
    slot = &g_displays[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation ||
        slot->owner_physical_device != physical)
        return NULL;
    return slot;
}

static RinVkDisplayModeSlot* display_mode_slot_from_handle(
        struct RinVkPhysicalDevice_T* physical,
        RinVkDisplayModeKHR handle) {
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkDisplayModeSlot* slot;
    if (!physical || (handle >> 48u) != RIN_VK_DISPLAY_MODE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_DISPLAY_MODES ||
        generation == 0u)
        return NULL;
    slot = &g_display_modes[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || !slot->display ||
        slot->display->owner_physical_device != physical ||
        slot->display_generation != slot->display->generation ||
        __atomic_load_n(&slot->display->state, __ATOMIC_ACQUIRE) != 1u)
        return NULL;
    return slot;
}

static RinVkDisplayModeSlot* display_mode_slot_from_handle_any(
        RinVkDisplayModeKHR handle) {
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkDisplayModeSlot* slot;
    if ((handle >> 48u) != RIN_VK_DISPLAY_MODE_TAG || index_field == 0u ||
        index_field > RIN_VK_MAX_DISPLAY_MODES || generation == 0u)
        return NULL;
    slot = &g_display_modes[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || !slot->display ||
        slot->display_generation != slot->display->generation ||
        __atomic_load_n(&slot->display->state, __ATOMIC_ACQUIRE) != 1u)
        return NULL;
    return slot;
}

static RinVkDisplaySlot* reserve_display_slot(uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_DISPLAYS; ++index) {
        RinVkDisplaySlot* slot = &g_displays[index];
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
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkDisplayModeSlot* reserve_display_mode_slot(
        uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_DISPLAY_MODES; ++index) {
        RinVkDisplayModeSlot* slot = &g_display_modes[index];
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
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static RinVkDisplaySurfaceSlot* reserve_display_surface_slot(
        uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_DISPLAY_SURFACES; ++index) {
        RinVkDisplaySurfaceSlot* slot = &g_display_surfaces[index];
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
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        *index_out = index;
        return slot;
    }
    return NULL;
}

static void clear_display_mode_slot(RinVkDisplayModeSlot* slot) {
    uint32_t generation;
    if (!slot) return;
    generation = slot->generation;
    memset(slot, 0, sizeof(*slot));
    slot->generation = generation;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_display_surface_slot(RinVkDisplaySurfaceSlot* slot) {
    uint32_t generation;
    if (!slot) return;
    if (__atomic_load_n(&slot->active_queries, __ATOMIC_ACQUIRE) != 0u ||
        __atomic_load_n(&slot->swapchain_count, __ATOMIC_ACQUIRE) != 0u)
        return;
    generation = slot->generation;
    memset(slot, 0, sizeof(*slot));
    slot->generation = generation;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_display_slot(RinVkDisplaySlot* slot) {
    uint32_t generation;
    if (!slot) return;
    generation = slot->generation;
    memset(slot, 0, sizeof(*slot));
    slot->generation = generation;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static int wsi_zero_words(const uint64_t* values, uint32_t count) {
    uint32_t index;
    if (!values) return 0;
    for (index = 0u; index < count; ++index)
        if (values[index] != 0u) return 0;
    return 1;
}

static int wsi_zero_u32_words(const uint32_t* values, uint32_t count) {
    uint32_t index;
    if (!values) return 0;
    for (index = 0u; index < count; ++index)
        if (values[index] != 0u) return 0;
    return 1;
}

static RinVkResult map_wsi_platform_result(int result) {
    switch (result) {
    case RIN_VULKAN_WSI_PLATFORM_OK:
        return RIN_VK_SUCCESS;
    case RIN_VULKAN_WSI_PLATFORM_INCOMPLETE:
        return RIN_VK_INCOMPLETE;
    case RIN_VULKAN_WSI_PLATFORM_NOT_READY:
        return RIN_VK_NOT_READY;
    case RIN_VULKAN_WSI_PLATFORM_OUT_OF_DATE:
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    case RIN_VULKAN_WSI_PLATFORM_DEVICE_LOST:
        return RIN_VK_ERROR_DEVICE_LOST;
    case RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED:
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    case RIN_VULKAN_WSI_PLATFORM_LIMIT:
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    case RIN_VULKAN_WSI_PLATFORM_INVALID_ARGUMENT:
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    default:
        return RIN_VK_ERROR_UNKNOWN;
    }
}

static int wsi_display_is_current_record(
        const RinVkDisplaySlot* slot,
        const RinVulkanWsiDisplayV1* displays, uint32_t display_count) {
    uint32_t index;
    if (!slot || !displays) return 0;
    for (index = 0u; index < display_count; ++index) {
        if (slot->display_cookie == displays[index].display_cookie &&
            slot->output_generation == displays[index].output_generation &&
            slot->device_generation == displays[index].device_generation)
            return 1;
    }
    return 0;
}

static void wsi_retire_stale_displays(
        struct RinVkInstance_T* instance,
        struct RinVkPhysicalDevice_T* physical,
        const RinVulkanWsiDisplayV1* displays, uint32_t display_count) {
    uint32_t index;
    if (!instance || !physical || !displays) return;
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_DISPLAY_MODES; ++index) {
        RinVkDisplayModeSlot* mode = &g_display_modes[index];
        RinVkDisplaySlot* display = mode->display;
        if (__atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) == 1u &&
            display && display->owner_instance == instance &&
            display->owner_physical_device == physical &&
            !wsi_display_is_current_record(display, displays, display_count))
            clear_display_mode_slot(mode);
    }
    for (index = 0u; index < RIN_VK_MAX_DISPLAYS; ++index) {
        RinVkDisplaySlot* display = &g_displays[index];
        if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) == 1u &&
            display->owner_instance == instance &&
            display->owner_physical_device == physical &&
            !wsi_display_is_current_record(display, displays, display_count))
            __atomic_store_n(&display->state, 3u, __ATOMIC_RELEASE);
    }
    sync_unlock();
}

static int wsi_display_record_valid(
        const RinVulkanWsiDisplayV1* display, uint64_t device_generation) {
    return display && display->struct_size == sizeof(*display) &&
           display->version == RIN_VULKAN_WSI_PLATFORM_VERSION &&
           display->display_cookie != 0u &&
           display->output_generation != 0u &&
           display->output_generation != UINT64_MAX &&
           display->device_generation == device_generation &&
           display->current_mode_cookie != 0u && display->width != 0u &&
           display->height != 0u && display->refresh_millihertz != 0u &&
           (display->format == RIN_VK_FORMAT_R8G8B8A8_UNORM) &&
           (display->flags & ~RIN_VULKAN_WSI_DISPLAY_FLAGS_KNOWN) == 0u &&
           display->plane_count != 0u && display->mode_count != 0u &&
           display->mode_count <= RIN_VULKAN_WSI_MAX_MODES &&
           memchr(display->display_name, '\0',
                  sizeof(display->display_name)) != NULL &&
           display->display_name[0] != '\0' &&
           wsi_zero_words(display->reserved, 2u);
}

static int wsi_mode_record_valid(const RinVulkanWsiModeV1* mode,
                                 const RinVkDisplaySlot* display) {
    return mode && display && mode->struct_size == sizeof(*mode) &&
           mode->version == RIN_VULKAN_WSI_PLATFORM_VERSION &&
           mode->display_cookie == display->display_cookie &&
           mode->mode_cookie != 0u &&
           mode->output_generation == display->output_generation &&
           mode->width != 0u && mode->height != 0u &&
           mode->refresh_millihertz != 0u &&
           mode->format == RIN_VK_FORMAT_R8G8B8A8_UNORM &&
           mode->flags == 0u && wsi_zero_words(mode->reserved, 1u);
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

static RinVkSwapchainSlot* swapchain_slot(
        RinVkDevice device, RinVkSwapchainKHR handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    const uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    const uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkSwapchainSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_SWAPCHAIN_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_SWAPCHAINS ||
        generation == 0u)
        return NULL;
    slot = &g_swapchains[index_field - 1u];
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

static RinVkPipelineCacheSlot* pipeline_cache_slot(
        RinVkDevice device, RinVkPipelineCache handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkPipelineCacheSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_PIPELINE_CACHE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_PIPELINE_CACHES ||
        generation == 0u)
        return NULL;
    slot = &g_pipeline_caches[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkPipelineSlot* pipeline_slot(RinVkDevice device,
                                       RinVkPipeline handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkPipelineSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_PIPELINE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_PIPELINES ||
        generation == 0u)
        return NULL;
    slot = &g_pipelines[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkShaderModuleSlot* shader_module_slot(
        RinVkDevice device, RinVkShaderModule handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkShaderModuleSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_SHADER_MODULE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_SHADER_MODULES ||
        generation == 0u)
        return NULL;
    slot = &g_shader_modules[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkQueryPoolSlot* query_pool_slot(
        RinVkDevice device, RinVkQueryPool handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkQueryPoolSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_QUERY_POOL_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_QUERY_POOLS ||
        generation == 0u)
        return NULL;
    slot = &g_query_pools[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static RinVkEventSlot* event_slot(RinVkDevice device, RinVkEvent handle) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkEventSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_EVENT_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_EVENTS ||
        generation == 0u)
        return NULL;
    slot = &g_events[index_field - 1u];
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
    __atomic_store_n(&slot->waiter_count, 0u, __ATOMIC_RELEASE);
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

static RinVkSwapchainSlot* reserve_swapchain_slot(uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_SWAPCHAINS; ++index) {
        RinVkSwapchainSlot* slot = &g_swapchains[index];
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
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
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

static RinVkPipelineCacheSlot* reserve_pipeline_cache_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_PIPELINE_CACHES; ++index) {
        RinVkPipelineCacheSlot* slot = &g_pipeline_caches[index];
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

static RinVkShaderModuleSlot* reserve_shader_module_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_SHADER_MODULES; ++index) {
        RinVkShaderModuleSlot* slot = &g_shader_modules[index];
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

static RinVkPipelineSlot* reserve_pipeline_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_PIPELINES; ++index) {
        RinVkPipelineSlot* slot = &g_pipelines[index];
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

static RinVkQueryPoolSlot* reserve_query_pool_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_QUERY_POOLS; ++index) {
        RinVkQueryPoolSlot* slot = &g_query_pools[index];
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

static RinVkEventSlot* reserve_event_slot(
        struct RinVkDevice_T* owner, uint32_t* index_out) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_EVENTS; ++index) {
        RinVkEventSlot* slot = &g_events[index];
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
    memset(&slot->ownership, 0, sizeof(slot->ownership));
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
           status.queue_count >= device->queue_count;
}

static uint32_t memory_type_bits(const struct RinVkDevice_T* device) {
    uint32_t count = device->physical_profile.memory_type_count;
    if (count == 0u || count > 31u) return 0u;
    return (UINT32_C(1) << count) - 1u;
}

static uint32_t image_format_bytes(int32_t format) {
    return format == RIN_VK_FORMAT_R8G8B8A8_UNORM ||
                   format == RIN_VK_FORMAT_D32_SFLOAT
               ? 4u
               : 0u;
}

static uint32_t image_format_aspects(int32_t format) {
    if (format == RIN_VK_FORMAT_R8G8B8A8_UNORM)
        return RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == RIN_VK_FORMAT_D32_SFLOAT)
        return RIN_VK_IMAGE_ASPECT_DEPTH_BIT;
    return 0u;
}

static int image_memory_size(const struct RinVkDevice_T* device,
                             const RinVkImageCreateInfo* request,
                             uint64_t* size_out) {
    uint64_t pixels;
    uint32_t bytes_per_pixel;

    if (!device || !request || !size_out ||
        (bytes_per_pixel = image_format_bytes(request->format)) == 0u ||
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
    if (pixels > UINT64_MAX / bytes_per_pixel ||
        pixels * bytes_per_pixel > UINT64_MAX / request->samples)
        return 0;
    *size_out = pixels * bytes_per_pixel * request->samples;
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

static struct RinVkInstance_T* debug_instance_for_device(
        const struct RinVkDevice_T* device) {
    uint32_t index;
    if (!device) return NULL;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        struct RinVkInstance_T* instance = &g_instances[index];
        if (__atomic_load_n(&instance->state, __ATOMIC_ACQUIRE) == 1u &&
            instance->runtime_handle == device->owner_instance)
            return instance;
    }
    return NULL;
}

static RinVkDebugUtilsMessengerSlot* debug_messenger_slot(
        struct RinVkInstance_T* owner, RinVkDebugUtilsMessengerEXT handle) {
    uint32_t index_field = (uint32_t)(handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(handle >> 16u);
    RinVkDebugUtilsMessengerSlot* slot;
    if (!owner || (handle >> 48u) != RIN_VK_DEBUG_UTILS_MESSENGER_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_DEBUG_MESSENGERS ||
        generation == 0u)
        return NULL;
    slot = &g_debug_utils_messengers[index_field - 1u];
    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
        slot->generation != generation || slot->owner != owner)
        return NULL;
    return slot;
}

static int debug_messenger_create_info_valid(
        const RinVkDebugUtilsMessengerCreateInfoEXT* create_info) {
    return create_info &&
           create_info->sType ==
               RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT &&
           create_info->pNext == NULL && create_info->flags == 0u &&
           create_info->messageSeverity != 0u &&
           (create_info->messageSeverity & ~RIN_VK_DEBUG_SEVERITY_MASK) == 0u &&
           create_info->messageType != 0u &&
           (create_info->messageType & ~RIN_VK_DEBUG_TYPE_MASK) == 0u &&
           create_info->pfnUserCallback != NULL;
}

static int collect_instance_create_chain(
        const void* first, int debug_utils_enabled,
        const RinVkDebugUtilsMessengerCreateInfoEXT** messenger_out) {
    const RinVkBaseFeatureStructure* node =
        (const RinVkBaseFeatureStructure*)first;
    const void* seen[RIN_VK_FEATURE_CHAIN_MAX];
    uint32_t count = 0u;
    uint32_t index;
    if (!messenger_out) return 0;
    *messenger_out = NULL;
    while (node && count < RIN_VK_FEATURE_CHAIN_MAX) {
        for (index = 0u; index < count; ++index) {
            if (seen[index] == node) return 0;
        }
        seen[count++] = node;
        if (node->sType ==
            RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT) {
            if (!debug_utils_enabled || *messenger_out) return 0;
            *messenger_out =
                (const RinVkDebugUtilsMessengerCreateInfoEXT*)node;
        } else if (node->sType !=
                   RIN_VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO) {
            return 0;
        }
        node = (const RinVkBaseFeatureStructure*)node->pNext;
    }
    return node == NULL &&
           (!*messenger_out ||
            debug_messenger_create_info_valid(*messenger_out));
}

static int debug_object_type_supported(uint32_t object_type) {
    switch (object_type) {
    case RIN_VK_OBJECT_TYPE_INSTANCE:
    case RIN_VK_OBJECT_TYPE_PHYSICAL_DEVICE:
    case RIN_VK_OBJECT_TYPE_DEVICE:
    case RIN_VK_OBJECT_TYPE_QUEUE:
    case RIN_VK_OBJECT_TYPE_SEMAPHORE:
    case RIN_VK_OBJECT_TYPE_COMMAND_BUFFER:
    case RIN_VK_OBJECT_TYPE_FENCE:
    case RIN_VK_OBJECT_TYPE_DEVICE_MEMORY:
    case RIN_VK_OBJECT_TYPE_BUFFER:
    case RIN_VK_OBJECT_TYPE_IMAGE:
    case RIN_VK_OBJECT_TYPE_EVENT:
    case RIN_VK_OBJECT_TYPE_QUERY_POOL:
    case RIN_VK_OBJECT_TYPE_IMAGE_VIEW:
    case RIN_VK_OBJECT_TYPE_SHADER_MODULE:
    case RIN_VK_OBJECT_TYPE_PIPELINE_CACHE:
    case RIN_VK_OBJECT_TYPE_PIPELINE_LAYOUT:
    case RIN_VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
    case RIN_VK_OBJECT_TYPE_SAMPLER:
    case RIN_VK_OBJECT_TYPE_DESCRIPTOR_POOL:
    case RIN_VK_OBJECT_TYPE_DESCRIPTOR_SET:
    case RIN_VK_OBJECT_TYPE_COMMAND_POOL:
    case RIN_VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER:
        return 1;
    default:
        return 0;
    }
}

static void debug_emit(
        struct RinVkInstance_T* instance, uint32_t severity,
        uint32_t types,
        const RinVkDebugUtilsMessengerCallbackDataEXT* callback_data) {
    struct CallbackSnapshot {
        RinVkDebugUtilsMessengerCallbackEXT callback;
        void* user_data;
        RinVkDebugUtilsMessengerSlot* slot;
        uint32_t generation;
    } callbacks[RIN_VK_MAX_DEBUG_MESSENGERS];
    uint32_t callback_count = 0u;
    uint32_t index;
    if (!instance || !instance->debug_utils_enabled || !callback_data ||
        severity == 0u || (severity & (severity - 1u)) != 0u ||
        (severity & RIN_VK_DEBUG_SEVERITY_MASK) == 0u || types == 0u ||
        (types & ~RIN_VK_DEBUG_TYPE_MASK) != 0u)
        return;
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_DEBUG_MESSENGERS; ++index) {
        RinVkDebugUtilsMessengerSlot* slot =
            &g_debug_utils_messengers[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
            slot->owner != instance ||
            (slot->message_severity & severity) == 0u ||
            (slot->message_type & types) == 0u)
            continue;
        callbacks[callback_count].callback = slot->callback;
        callbacks[callback_count].user_data = slot->user_data;
        callbacks[callback_count].slot = slot;
        callbacks[callback_count].generation = slot->generation;
        (void)__atomic_add_fetch(&slot->active_callbacks, 1u,
                                 __ATOMIC_ACQ_REL);
        ++callback_count;
    }
    sync_unlock();
    for (index = 0u; index < callback_count; ++index) {
        (void)callbacks[index].callback(severity, types, callback_data,
                                        callbacks[index].user_data);
        sync_lock();
        if (callbacks[index].slot->generation == callbacks[index].generation)
            (void)__atomic_sub_fetch(
                &callbacks[index].slot->active_callbacks, 1u,
                __ATOMIC_RELEASE);
        sync_unlock();
    }
}

static void debug_instance_create_report(
        const RinVkDebugUtilsMessengerCreateInfoEXT* create_info,
        uint32_t severity, const char* message_id, const char* message) {
    RinVkDebugUtilsMessengerCallbackDataEXT callback_data;
    if (!create_info || !create_info->pfnUserCallback ||
        (create_info->messageSeverity & severity) == 0u ||
        (create_info->messageType &
         RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) == 0u)
        return;
    memset(&callback_data, 0, sizeof(callback_data));
    callback_data.sType =
        RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callback_data.pMessageIdName = message_id;
    callback_data.pMessage = message;
    (void)create_info->pfnUserCallback(
        severity, RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        &callback_data, create_info->pUserData);
}

static RinVkDebugUtilsObjectSlot* debug_object_find(
        struct RinVkInstance_T* owner, uint32_t object_type,
        uint64_t object_handle, RinVkDebugUtilsObjectSlot** free_slot) {
    uint32_t index;
    if (free_slot) *free_slot = NULL;
    for (index = 0u; index < RIN_VK_MAX_DEBUG_OBJECTS; ++index) {
        RinVkDebugUtilsObjectSlot* slot = &g_debug_utils_objects[index];
        if (slot->state == 1u && slot->owner == owner &&
            slot->object_type == object_type &&
            slot->object_handle == object_handle)
            return slot;
        if (slot->state == 0u && free_slot && !*free_slot)
            *free_slot = slot;
    }
    return NULL;
}

static void debug_object_clear(struct RinVkInstance_T* owner,
                               uint32_t object_type,
                               uint64_t object_handle) {
    RinVkDebugUtilsObjectSlot* slot;
    if (!owner || object_handle == 0u) return;
    sync_lock();
    slot = debug_object_find(owner, object_type, object_handle, NULL);
    if (slot) {
        free(slot->name);
        free(slot->tag);
        memset(slot, 0, sizeof(*slot));
    }
    sync_unlock();
}

static void debug_utils_cleanup_instance(struct RinVkInstance_T* owner) {
    uint32_t wait_for_callbacks[RIN_VK_MAX_DEBUG_MESSENGERS] = {0u};
    uint32_t generations[RIN_VK_MAX_DEBUG_MESSENGERS] = {0u};
    uint32_t index;
    if (!owner) return;
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_DEBUG_MESSENGERS; ++index) {
        RinVkDebugUtilsMessengerSlot* slot =
            &g_debug_utils_messengers[index];
        if (slot->owner == owner) {
            wait_for_callbacks[index] = 1u;
            generations[index] = slot->generation;
            __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
        }
    }
    sync_unlock();
    for (index = 0u; index < RIN_VK_MAX_DEBUG_MESSENGERS; ++index) {
        RinVkDebugUtilsMessengerSlot* slot =
            &g_debug_utils_messengers[index];
        if (!wait_for_callbacks[index]) continue;
        while (__atomic_load_n(&slot->active_callbacks, __ATOMIC_ACQUIRE) !=
               0u)
            yield_thread();
    }
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_DEBUG_MESSENGERS; ++index) {
        RinVkDebugUtilsMessengerSlot* slot =
            &g_debug_utils_messengers[index];
        if (!wait_for_callbacks[index] || slot->owner != owner ||
            slot->generation != generations[index])
            continue;
        slot->owner = NULL;
        slot->message_severity = 0u;
        slot->message_type = 0u;
        slot->callback = NULL;
        slot->user_data = NULL;
        slot->reserved = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
    }
    for (index = 0u; index < RIN_VK_MAX_DEBUG_OBJECTS; ++index) {
        RinVkDebugUtilsObjectSlot* slot = &g_debug_utils_objects[index];
        if (slot->owner != owner) continue;
        free(slot->name);
        free(slot->tag);
        memset(slot, 0, sizeof(*slot));
    }
    sync_unlock();
}

static RinVkResult debug_object_update(
        struct RinVkInstance_T* owner, uint32_t object_type,
        uint64_t object_handle, const char* name, uint64_t tag_name,
        const void* tag, size_t tag_size, int update_name, int update_tag) {
    RinVkDebugUtilsObjectSlot* slot;
    RinVkDebugUtilsObjectSlot* free_slot;
    char* name_copy = NULL;
    void* tag_copy = NULL;
    size_t name_size = 0u;
    int remove_record;
    if (!owner || !debug_object_type_supported(object_type) ||
        object_handle == 0u || (name && !update_name) ||
        (tag_size != 0u && (!tag || !update_tag)))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (update_name && name && name[0] != '\0') {
        name_size = strlen(name);
        if (name_size == SIZE_MAX) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        name_copy = (char*)malloc(name_size + 1u);
        if (!name_copy) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        memcpy(name_copy, name, name_size + 1u);
    }
    if (update_tag && tag_size != 0u) {
        tag_copy = malloc(tag_size);
        if (!tag_copy) {
            free(name_copy);
            return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        memcpy(tag_copy, tag, tag_size);
    }
    sync_lock();
    slot = debug_object_find(owner, object_type, object_handle, &free_slot);
    if (!slot && (name_copy || tag_copy)) {
        if (!free_slot) {
            sync_unlock();
            free(name_copy);
            free(tag_copy);
            return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        slot = free_slot;
        memset(slot, 0, sizeof(*slot));
        slot->state = 1u;
        slot->owner = owner;
        slot->object_type = object_type;
        slot->object_handle = object_handle;
    }
    if (slot) {
        if (update_name) {
            free(slot->name);
            slot->name = name_copy;
            name_copy = NULL;
        }
        if (update_tag) {
            free(slot->tag);
            slot->tag = tag_copy;
            tag_copy = NULL;
            slot->tag_name = tag_size == 0u ? 0u : tag_name;
            slot->tag_size = tag_size;
        }
        remove_record = slot->name == NULL && slot->tag == NULL;
        if (remove_record) {
            memset(slot, 0, sizeof(*slot));
        }
    }
    sync_unlock();
    free(name_copy);
    free(tag_copy);
    return RIN_VK_SUCCESS;
}

static char* debug_tag_message(uint64_t tag_name, const void* tag,
                               size_t tag_size) {
    static const char hex_digits[] = "0123456789abcdef";
    char* message;
    size_t index;
    if (tag_size == 0u) {
        static const char cleared[] = "object tag cleared";
        message = (char*)malloc(sizeof(cleared));
        if (message) memcpy(message, cleared, sizeof(cleared));
        return message;
    }
    if (!tag || tag_size > (SIZE_MAX - 31u) / 2u) return NULL;
    message = (char*)malloc(31u + tag_size * 2u);
    if (!message) return NULL;
    memcpy(message, "tag=0x", 6u);
    for (index = 0u; index < 16u; ++index) {
        const uint32_t shift = (uint32_t)((15u - index) * 4u);
        message[6u + index] = hex_digits[(tag_name >> shift) & 0xfu];
    }
    memcpy(message + 22u, " data=0x", 8u);
    for (index = 0u; index < tag_size; ++index) {
        const uint8_t byte = ((const uint8_t*)tag)[index];
        message[30u + index * 2u] = hex_digits[byte >> 4u];
        message[31u + index * 2u] = hex_digits[byte & 0xfu];
    }
    message[30u + tag_size * 2u] = '\0';
    return message;
}

static void debug_utils_annotation(
        struct RinVkInstance_T* owner, const char* message_id,
        uint32_t object_type, uint64_t object_handle,
        const RinVkDebugUtilsLabelDataEXT* queue_label,
        const RinVkDebugUtilsLabelDataEXT* command_label,
        const char* message) {
    RinVkDebugUtilsMessengerCallbackDataEXT data;
    RinVkDebugUtilsObjectNameEXT object;
    char* object_name = NULL;
    memset(&data, 0, sizeof(data));
    data.sType = RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    data.pMessageIdName = message_id;
    data.pMessage = message;
    if (queue_label) {
        data.queueLabelCount = 1u;
        data.pQueueLabels = queue_label;
    }
    if (command_label) {
        data.cmdBufLabelCount = 1u;
        data.pCmdBufLabels = command_label;
    }
    if (object_handle != 0u) {
        object.objectType = (int32_t)object_type;
        object.objectHandle = object_handle;
        object.pObjectName = NULL;
        sync_lock();
        {
            RinVkDebugUtilsObjectSlot* slot = debug_object_find(
                owner, object_type, object_handle, NULL);
            if (slot && slot->name) {
                size_t name_size = strlen(slot->name);
                object_name = (char*)malloc(name_size + 1u);
                if (object_name)
                    memcpy(object_name, slot->name, name_size + 1u);
            }
        }
        data.objectCount = 1u;
        data.pObjects = &object;
        sync_unlock();
        object.pObjectName = object_name;
    }
    debug_emit(owner, RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
               RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &data);
    free(object_name);
}

static int submission_slots_active(void) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const uint32_t state = __atomic_load_n(&g_submissions[index].state,
                                                __ATOMIC_ACQUIRE);
        if (state == RIN_VK_SUBMISSION_ACTIVE ||
            state == RIN_VK_SUBMISSION_RESERVED ||
            state == RIN_VK_SUBMISSION_WAITING)
            return 1;
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
        if ((state == RIN_VK_SUBMISSION_ACTIVE ||
             state == RIN_VK_SUBMISSION_RESERVED ||
             state == RIN_VK_SUBMISSION_WAITING) &&
            slot->owner == device) {
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
        if (__atomic_compare_exchange_n(&slot->state, &expected,
                                        RIN_VK_SUBMISSION_RESERVED, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            memset(slot, 0, sizeof(*slot));
            __atomic_store_n(&slot->state, RIN_VK_SUBMISSION_RESERVED,
                             __ATOMIC_RELEASE);
            return slot;
        }
    }
    return NULL;
}

static void clear_submission_slot(RinVkSubmissionSlot* slot) {
    if (!slot) return;
    free(slot->compute_packet);
    free(slot->graphics_packet);
    free(slot->compute_buffer_ownership_updates);
    memset(slot, 0, sizeof(*slot));
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static int submission_waits_satisfied(const RinVkSubmissionSlot* slot) {
    uint32_t index;
    if (!slot || !slot->owner) return 0;
    for (index = 0u; index < slot->wait_semaphore_count; ++index) {
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)slot->owner, slot->wait_semaphores[index]);
        if (!semaphore) return 0;
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
            if (__atomic_load_n(&semaphore->value, __ATOMIC_ACQUIRE) <
                slot->wait_semaphore_values[index])
                return 0;
        } else if (__atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) !=
                       0u ||
                   __atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) ==
                       0u) {
            return 0;
        }
    }
    return 1;
}

static int queue_has_earlier_waiting_submission(
        const RinVkSubmissionSlot* candidate) {
    uint32_t index;
    if (!candidate || !candidate->owner) return 0;
    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* earlier = &g_submissions[index];
        if (__atomic_load_n(&earlier->state, __ATOMIC_ACQUIRE) ==
                RIN_VK_SUBMISSION_WAITING &&
            earlier->owner == candidate->owner &&
            earlier->queue_id == candidate->queue_id &&
            earlier->order < candidate->order)
            return 1;
    }
    return 0;
}

static int queue_has_earlier_uncompleted_submission(
        const RinVkSubmissionSlot* candidate) {
    uint32_t index;
    if (!candidate || !candidate->owner) return 0;
    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* earlier = &g_submissions[index];
        const uint32_t state = __atomic_load_n(&earlier->state,
                                                __ATOMIC_ACQUIRE);
        if ((state == RIN_VK_SUBMISSION_ACTIVE ||
             state == RIN_VK_SUBMISSION_RESERVED ||
             state == RIN_VK_SUBMISSION_WAITING) &&
            earlier != candidate && earlier->owner == candidate->owner &&
            earlier->queue_id == candidate->queue_id &&
            earlier->order < candidate->order)
            return 1;
    }
    return 0;
}

static void release_submission_wait_reservations(
        RinVkSubmissionSlot* slot) {
    uint32_t index;
    if (!slot || slot->waits_reserved == 0u) return;
    for (index = 0u; index < slot->wait_semaphore_count; ++index) {
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)slot->owner, slot->wait_semaphores[index]);
        if (semaphore && semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY) {
            const uint32_t waiter_count = __atomic_load_n(
                &semaphore->waiter_count, __ATOMIC_ACQUIRE);
            if (waiter_count != 0u)
                __atomic_store_n(&semaphore->waiter_count, waiter_count - 1u,
                                 __ATOMIC_RELEASE);
        }
    }
    slot->waits_reserved = 0u;
}

static void consume_submission_waits(RinVkSubmissionSlot* slot) {
    uint32_t index;
    if (!slot || !slot->owner) return;
    for (index = 0u; index < slot->wait_semaphore_count; ++index) {
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)slot->owner, slot->wait_semaphores[index]);
        if (!semaphore ||
            semaphore->type != RIN_VK_SEMAPHORE_TYPE_BINARY)
            continue;
        __atomic_store_n(&semaphore->signaled, 0u, __ATOMIC_RELEASE);
        if (slot->waits_reserved) {
            const uint32_t waiter_count = __atomic_load_n(
                &semaphore->waiter_count, __ATOMIC_ACQUIRE);
            if (waiter_count != 0u)
                __atomic_store_n(&semaphore->waiter_count, waiter_count - 1u,
                                 __ATOMIC_RELEASE);
        }
    }
    slot->waits_reserved = 0u;
}

static RinVkResult submit_slot_to_product(RinVkSubmissionSlot* slot) {
    RinVulkanProductPlatformV1* product;
    RinVulkanProductSubmissionV1 submission;
    int product_result;
    if (!slot || !slot->owner || !submission_waits_satisfied(slot))
        return RIN_VK_NOT_READY;
    if (slot->compute_packet &&
        (slot->compute_packet_size <
             offsetof(RinGpuVulkanComputePacketV1, shader_ir) ||
         slot->compute_packet_size != slot->compute_packet->struct_size))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (slot->graphics_packet &&
        (slot->graphics_packet_size <
             offsetof(RinGpuVulkanGraphicsPacketV1, shader_ir) ||
         slot->graphics_packet_size !=
             slot->graphics_packet->struct_size))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    product = acquire_product();
    if (!product || !product_matches_device(product, slot->owner)) {
        if (product) release_product();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    memset(&submission, 0, sizeof(submission));
    product_result = product->prepare_submission(
        product->context, slot->queue_id,
        (uint64_t)(uintptr_t)(slot->graphics_packet
                                  ? (const void*)slot->graphics_packet
                                  : (slot->compute_packet
                                         ? (const void*)slot->compute_packet
                                         : (const void*)&slot->routed_packet)),
        &submission);
    if (product_result == RIN_VULKAN_PRODUCT_OK)
        product_result = product->submit(
            product->context, &submission,
            slot->resource_count == 0u ? NULL : slot->resources,
            slot->resource_count);
    release_product();
    if (product_result != RIN_VULKAN_PRODUCT_OK)
        return map_product_result(product_result);
    slot->sequence = submission.sequence;
    slot->completion_value = submission.completion_value;
    if (slot->compute_packet || slot->graphics_packet) {
        uint32_t index;
        for (index = 0u;
             index < slot->compute_buffer_ownership_update_count; ++index) {
            RinVkBufferOwnershipUpdate* update =
                &slot->compute_buffer_ownership_updates[index];
            if (update->buffer) update->buffer->ownership = update->state;
        }
    }
    consume_submission_waits(slot);
    __atomic_store_n(&slot->state, RIN_VK_SUBMISSION_ACTIVE,
                     __ATOMIC_RELEASE);
    return RIN_VK_SUCCESS;
}

static void abort_device_submissions(struct RinVkDevice_T* device);
static void complete_submission_sync(const RinVkSubmissionSlot* submission);

static RinVkResult dispatch_ready_waiting_submissions(
        struct RinVkDevice_T* device) {
    for (;;) {
        RinVkSubmissionSlot* candidate = NULL;
        uint32_t index;
        for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
            RinVkSubmissionSlot* slot = &g_submissions[index];
            if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) !=
                    RIN_VK_SUBMISSION_WAITING ||
                slot->owner != device || !submission_waits_satisfied(slot))
                continue;
            if (slot->command_buffer_count == 0u) {
                if (queue_has_earlier_uncompleted_submission(slot)) continue;
            } else if (queue_has_earlier_waiting_submission(slot)) {
                continue;
            }
            if (!candidate || slot->order < candidate->order)
                candidate = slot;
        }
        if (!candidate) return RIN_VK_SUCCESS;
        if (candidate->command_buffer_count == 0u) {
            consume_submission_waits(candidate);
            complete_submission_sync(candidate);
            clear_submission_slot(candidate);
            continue;
        }
        {
            RinVkResult result = submit_slot_to_product(candidate);
            if (result == RIN_VK_NOT_READY)
                return RIN_VK_SUCCESS;
            if (result != RIN_VK_SUCCESS) {
                abort_device_submissions(device);
                return result;
            }
        }
    }
}

static int device_product_submissions_active(
        const struct RinVkDevice_T* device) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* slot = &g_submissions[index];
        const uint32_t state = __atomic_load_n(&slot->state,
                                                __ATOMIC_ACQUIRE);
        if ((state == RIN_VK_SUBMISSION_ACTIVE ||
             state == RIN_VK_SUBMISSION_RESERVED) &&
            slot->owner == device)
            return 1;
    }
    return 0;
}

static void clear_image_slot(RinVkImageSlot* slot) {
    slot->owner = NULL;
    slot->swapchain_owner = NULL;
    slot->memory = NULL;
    slot->memory_generation = 0u;
    slot->format = 0;
    slot->usage = 0u;
    slot->memory_size = 0u;
    slot->memory_offset = 0u;
    slot->width = 0u;
    slot->height = 0u;
    slot->current_layout = RIN_VK_IMAGE_LAYOUT_UNDEFINED;
    memset(&slot->ownership, 0, sizeof(slot->ownership));
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

static void clear_pipeline_cache_slot(RinVkPipelineCacheSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    slot->payload_size = 0u;
    slot->reserved = 0u;
    memset(slot->payload, 0, sizeof(slot->payload));
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_pipeline_slot(RinVkPipelineSlot* slot) {
    if (!slot) return;
    free(slot->shader_ir);
    free(slot->fragment_shader_ir);
    slot->owner = NULL;
    slot->kind = 0u;
    slot->set_layout_count = 0u;
    slot->descriptor_count = 0u;
    slot->shader_size = 0u;
    slot->fragment_shader_size = 0u;
    memset(slot->set_layouts, 0, sizeof(slot->set_layouts));
    memset(slot->descriptors, 0, sizeof(slot->descriptors));
    memset(&slot->graphics_backend, 0, sizeof(slot->graphics_backend));
    memset(&slot->viewport, 0, sizeof(slot->viewport));
    memset(&slot->scissor, 0, sizeof(slot->scissor));
    slot->shader_ir = NULL;
    slot->fragment_shader_ir = NULL;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_shader_module_slot(RinVkShaderModuleSlot* slot) {
    if (!slot) return;
    free(slot->code);
    slot->owner = NULL;
    slot->code_size = 0u;
    slot->code = NULL;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void cleanup_device_shader_modules(struct RinVkDevice_T* device) {
    uint32_t index;
    if (!device) return;
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_SHADER_MODULES; ++index) {
        RinVkShaderModuleSlot* slot = &g_shader_modules[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) == 1u &&
            slot->owner == device)
            clear_shader_module_slot(slot);
    }
    sync_unlock();
}

static void cleanup_device_pipelines(struct RinVkDevice_T* device) {
    uint32_t index;
    if (!device) return;
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_PIPELINES; ++index) {
        RinVkPipelineSlot* slot = &g_pipelines[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) == 1u &&
            slot->owner == device)
            clear_pipeline_slot(slot);
    }
    sync_unlock();
}

static void clear_query_pool_slot(RinVkQueryPoolSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    slot->query_type = 0u;
    slot->query_count = 0u;
    slot->pipeline_statistics = 0u;
    memset(slot->queries, 0, sizeof(slot->queries));
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void clear_event_slot(RinVkEventSlot* slot) {
    if (!slot) return;
    slot->owner = NULL;
    __atomic_store_n(&slot->signaled, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&slot->pending, 0u, __ATOMIC_RELEASE);
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
    for (index = 0u; index < RIN_VK_MAX_PIPELINE_CACHES; ++index)
        if ((__atomic_load_n(&g_pipeline_caches[index].state,
                             __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&g_pipeline_caches[index].state,
                             __ATOMIC_ACQUIRE) == 2u) &&
            g_pipeline_caches[index].owner == device)
            return 1;
    for (index = 0u; index < RIN_VK_MAX_QUERY_POOLS; ++index)
        if ((__atomic_load_n(&g_query_pools[index].state, __ATOMIC_ACQUIRE) ==
                 1u ||
             __atomic_load_n(&g_query_pools[index].state, __ATOMIC_ACQUIRE) ==
                 2u) &&
            g_query_pools[index].owner == device)
            return 1;
    for (index = 0u; index < RIN_VK_MAX_EVENTS; ++index)
        if ((__atomic_load_n(&g_events[index].state, __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&g_events[index].state, __ATOMIC_ACQUIRE) == 2u) &&
            g_events[index].owner == device)
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
    case RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_IMAGE;
    case RIN_VK_DESCRIPTOR_TYPE_SAMPLER:
        return RIN_GPU_VULKAN_DESCRIPTOR_SAMPLER;
    case RIN_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return RIN_GPU_VULKAN_DESCRIPTOR_COMBINED_IMAGE_SAMPLER;
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

static void yield_thread(void) {
    rinvulkan_platform_yield_thread();
}

static void sync_lock(void) {
    for (;;) {
        if (__atomic_exchange_n(&g_sync_lock, 1u, __ATOMIC_ACQUIRE) == 0u)
            return;
        do {
            yield_thread();
        } while (__atomic_load_n(&g_sync_lock, __ATOMIC_RELAXED) != 0u);
    }
}

static int wait_timeout_elapsed(uint64_t start_ns, uint64_t timeout_ns) {
    uint64_t now_ns;

    if (timeout_ns == UINT64_MAX) return 0;
    if (!rinvulkan_platform_monotonic_time_ns(&now_ns)) return -1;
    return now_ns < start_ns || now_ns - start_ns >= timeout_ns;
}

static int queue_submission_slots_active(
        const struct RinVkDevice_T* device, uint32_t queue_id) {
    uint32_t index;

    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* slot = &g_submissions[index];
        const uint32_t state = __atomic_load_n(&slot->state,
                                                __ATOMIC_ACQUIRE);
        if ((state == RIN_VK_SUBMISSION_ACTIVE ||
             state == RIN_VK_SUBMISSION_RESERVED ||
             state == RIN_VK_SUBMISSION_WAITING) &&
            slot->owner == device &&
            slot->queue_id == queue_id) {
            return 1;
        }
    }
    return 0;
}

static int timeline_pending_signal_range(
        const struct RinVkDevice_T* device, RinVkSemaphore semaphore_handle,
        const RinVkSubmissionSlot* excluded, uint64_t* minimum_out,
        uint64_t* maximum_out) {
    uint32_t index;
    int found = 0;
    uint64_t minimum = UINT64_MAX;
    uint64_t maximum = 0u;

    if (!device || semaphore_handle == 0u || !minimum_out || !maximum_out)
        return 0;
    for (index = 0u; index < RIN_VK_MAX_SUBMISSIONS; ++index) {
        const RinVkSubmissionSlot* submission = &g_submissions[index];
        const uint32_t state = __atomic_load_n(&submission->state,
                                                __ATOMIC_ACQUIRE);
        uint32_t signal_index;
        if ((state != RIN_VK_SUBMISSION_ACTIVE &&
             state != RIN_VK_SUBMISSION_RESERVED &&
             state != RIN_VK_SUBMISSION_WAITING) ||
            submission == excluded || submission->owner != device)
            continue;
        for (signal_index = 0u;
             signal_index < submission->signal_semaphore_count;
             ++signal_index) {
            uint64_t value;
            if (submission->signal_semaphores[signal_index] !=
                semaphore_handle)
                continue;
            value = submission->signal_semaphore_values[signal_index];
            if (!found || value < minimum) minimum = value;
            if (!found || value > maximum) maximum = value;
            found = 1;
        }
    }
    if (found) {
        *minimum_out = minimum;
        *maximum_out = maximum;
    }
    return found;
}

static void refresh_timeline_semaphore_pending(
        RinVkSemaphoreSlot* semaphore, struct RinVkDevice_T* device,
        RinVkSemaphore semaphore_handle,
        const RinVkSubmissionSlot* excluded) {
    uint64_t minimum;
    uint64_t maximum;
    if (!semaphore || semaphore->type != RIN_VK_SEMAPHORE_TYPE_TIMELINE)
        return;
    if (timeline_pending_signal_range(device, semaphore_handle, excluded,
                                      &minimum, &maximum)) {
        semaphore->pending_value = maximum;
        __atomic_store_n(&semaphore->pending, 1u, __ATOMIC_RELEASE);
    } else {
        semaphore->pending_value = 0u;
        __atomic_store_n(&semaphore->pending, 0u, __ATOMIC_RELEASE);
    }
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
        const RinVkSemaphore semaphore_handle =
            submission->signal_semaphores[index];
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)submission->owner,
            semaphore_handle);
        if (semaphore &&
            __atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u) {
            if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
                const uint64_t current = __atomic_load_n(
                    &semaphore->value, __ATOMIC_ACQUIRE);
                const uint64_t signal_value =
                    submission->signal_semaphore_values[index];
                if (signal_value > current)
                    __atomic_store_n(&semaphore->value, signal_value,
                                     __ATOMIC_RELEASE);
                refresh_timeline_semaphore_pending(
                    semaphore, submission->owner, semaphore_handle,
                    submission);
            } else {
                __atomic_store_n(&semaphore->pending, 0u, __ATOMIC_RELEASE);
                __atomic_store_n(&semaphore->signaled, 1u,
                                 __ATOMIC_RELEASE);
            }
        }
    }
}

static void complete_submission_query_events(
        const RinVkSubmissionSlot* submission) {
    uint32_t buffer_index;
    if (!submission || !submission->owner) return;
    for (buffer_index = 0u;
         buffer_index < submission->command_buffer_count; ++buffer_index) {
        const RinGpuVulkanCommandBufferV1* buffer =
            submission->command_buffers[buffer_index];
        uint32_t index;
        if (!buffer) continue;
        for (index = 0u; index < buffer->query_command_count; ++index) {
            const RinGpuVulkanQueryCommandV1* command =
                &buffer->query_commands[index];
            RinVkQueryPoolSlot* pool = query_pool_slot(
                (RinVkDevice)submission->owner,
                (RinVkQueryPool)command->query_pool);
            RinVkQueryValue* query;
            if (!pool || command->query >= pool->query_count) continue;
            query = &pool->queries[command->query];
            if (command->operation == RIN_GPU_VULKAN_QUERY_COMMAND_RESET) {
                memset(query, 0, sizeof(*query));
            } else if (command->operation ==
                       RIN_GPU_VULKAN_QUERY_COMMAND_TIMESTAMP) {
                if (query->active == 0u) {
                    query->values[0] = submission->sequence;
                    query->availability = 1u;
                }
            } else if (command->operation ==
                       RIN_GPU_VULKAN_QUERY_COMMAND_BEGIN) {
                query->active = 1u;
                query->availability = 0u;
            } else if (command->operation ==
                       RIN_GPU_VULKAN_QUERY_COMMAND_END) {
                if (query->active != 0u) {
                    query->active = 0u;
                    query->availability = 1u;
                }
            }
            query->pending = 0u;
        }
        for (index = 0u; index < buffer->event_command_count; ++index) {
            const RinGpuVulkanEventCommandV1* command =
                &buffer->event_commands[index];
            RinVkEventSlot* event = event_slot(
                (RinVkDevice)submission->owner, (RinVkEvent)command->event);
            if (!event) continue;
            if (command->operation == RIN_GPU_VULKAN_EVENT_COMMAND_SET)
                __atomic_store_n(&event->signaled, 1u, __ATOMIC_RELEASE);
            else if (command->operation == RIN_GPU_VULKAN_EVENT_COMMAND_RESET)
                __atomic_store_n(&event->signaled, 0u, __ATOMIC_RELEASE);
            __atomic_store_n(&event->pending, 0u, __ATOMIC_RELEASE);
        }
    }
}

static int validate_submission_query_events(
        struct RinVkDevice_T* device, uint32_t command_buffer_count,
        RinGpuVulkanCommandBufferV1* const* command_buffers) {
    RinVkQueryValue* query_values[64];
    RinVkEventSlot* event_slots[64];
    uint32_t query_count = 0u;
    uint32_t event_count = 0u;
    uint8_t query_active[64] = {0};
    uint8_t event_signaled[64] = {0};
    uint32_t buffer_index;
    if (!device || !command_buffers || command_buffer_count == 0u ||
        command_buffer_count > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS)
        return 0;
    for (buffer_index = 0u; buffer_index < command_buffer_count;
         ++buffer_index) {
        RinGpuVulkanCommandBufferV1* buffer = command_buffers[buffer_index];
        uint32_t index;
        if (!buffer) return 0;
        for (index = 0u; index < buffer->query_command_count; ++index) {
            const RinGpuVulkanQueryCommandV1* command =
                &buffer->query_commands[index];
            RinVkQueryPoolSlot* pool = query_pool_slot(
                (RinVkDevice)device, (RinVkQueryPool)command->query_pool);
            RinVkQueryValue* query;
            uint32_t query_index;
            if (!pool || command->query >= pool->query_count ||
                (command->flags != 0u &&
                 command->flags != RIN_VK_QUERY_CONTROL_PRECISE_BIT))
                return 0;
            query = &pool->queries[command->query];
            if (query->pending != 0u) return 0;
            for (query_index = 0u; query_index < query_count; ++query_index)
                if (query_values[query_index] == query)
                    break;
            if (query_index == query_count) {
                if (query_count >= 64u) return 0;
                query_values[query_count] = query;
                query_active[query_count] = query->active != 0u;
                ++query_count;
            }
            if (command->operation == RIN_GPU_VULKAN_QUERY_COMMAND_RESET) {
                if (query_active[query_index] != 0u) return 0;
                query_active[query_index] = 0u;
            } else if (command->operation ==
                       RIN_GPU_VULKAN_QUERY_COMMAND_TIMESTAMP) {
                if (pool->query_type != RIN_VK_QUERY_TYPE_TIMESTAMP ||
                    query_active[query_index] != 0u || command->flags != 0u)
                    return 0;
            } else if (command->operation ==
                       RIN_GPU_VULKAN_QUERY_COMMAND_BEGIN) {
                if (pool->query_type != RIN_VK_QUERY_TYPE_OCCLUSION ||
                    query_active[query_index] != 0u ||
                    (command->flags != 0u &&
                     pool->query_type != RIN_VK_QUERY_TYPE_OCCLUSION))
                    return 0;
                query_active[query_index] = 1u;
            } else if (command->operation ==
                       RIN_GPU_VULKAN_QUERY_COMMAND_END) {
                if (pool->query_type != RIN_VK_QUERY_TYPE_OCCLUSION ||
                    query_active[query_index] == 0u)
                    return 0;
                query_active[query_index] = 0u;
            } else {
                return 0;
            }
        }
        for (index = 0u; index < buffer->event_command_count; ++index) {
            const RinGpuVulkanEventCommandV1* command =
                &buffer->event_commands[index];
            RinVkEventSlot* event = event_slot(
                (RinVkDevice)device, (RinVkEvent)command->event);
            uint32_t event_index;
            if (!event || __atomic_load_n(&event->pending, __ATOMIC_ACQUIRE) != 0u)
                return 0;
            for (event_index = 0u; event_index < event_count; ++event_index)
                if (event_slots[event_index] == event)
                    break;
            if (event_index == event_count) {
                if (event_count >= 64u) return 0;
                event_slots[event_count] = event;
                event_signaled[event_count] =
                    (uint8_t)(__atomic_load_n(&event->signaled,
                                               __ATOMIC_ACQUIRE) != 0u);
                ++event_count;
            }
            if (command->operation == RIN_GPU_VULKAN_EVENT_COMMAND_SET)
                event_signaled[event_index] = 1u;
            else if (command->operation == RIN_GPU_VULKAN_EVENT_COMMAND_RESET)
                event_signaled[event_index] = 0u;
            else if (command->operation == RIN_GPU_VULKAN_EVENT_COMMAND_WAIT) {
                if (event_signaled[event_index] == 0u) return 0;
            } else {
                return 0;
            }
        }
    }
    for (uint32_t index = 0u; index < query_count; ++index)
        if (query_active[index] != 0u) return 0;
    return 1;
}

static void mark_submission_query_events(
        const RinVkSubmissionSlot* submission, uint32_t pending) {
    uint32_t buffer_index;
    if (!submission || !submission->owner) return;
    for (buffer_index = 0u;
         buffer_index < submission->command_buffer_count; ++buffer_index) {
        const RinGpuVulkanCommandBufferV1* buffer =
            submission->command_buffers[buffer_index];
        uint32_t index;
        if (!buffer) continue;
        for (index = 0u; index < buffer->query_command_count; ++index) {
            const RinGpuVulkanQueryCommandV1* command =
                &buffer->query_commands[index];
            RinVkQueryPoolSlot* pool = query_pool_slot(
                (RinVkDevice)submission->owner,
                (RinVkQueryPool)command->query_pool);
            if (pool && command->query < pool->query_count)
                pool->queries[command->query].pending = pending;
        }
        for (index = 0u; index < buffer->event_command_count; ++index) {
            RinVkEventSlot* event = event_slot(
                (RinVkDevice)submission->owner,
                (RinVkEvent)buffer->event_commands[index].event);
            if (event)
                __atomic_store_n(&event->pending, pending, __ATOMIC_RELEASE);
        }
    }
}

static void cancel_submission_sync(RinVkSubmissionSlot* submission) {
    uint32_t index;
    RinVkFenceSlot* fence;

    if (!submission || !submission->owner) return;
    release_submission_wait_reservations(submission);
    fence = fence_slot((RinVkDevice)submission->owner, submission->fence);
    if (fence) __atomic_store_n(&fence->pending, 0u, __ATOMIC_RELEASE);
    for (index = 0u; index < submission->signal_semaphore_count; ++index) {
        const RinVkSemaphore semaphore_handle =
            submission->signal_semaphores[index];
        RinVkSemaphoreSlot* semaphore = semaphore_slot(
            (RinVkDevice)submission->owner,
            semaphore_handle);
        if (!semaphore) continue;
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE)
            refresh_timeline_semaphore_pending(
                semaphore, submission->owner, semaphore_handle, submission);
        else
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
    for (index = 0u; index < RIN_VK_MAX_QUERY_POOLS; ++index) {
        RinVkQueryPoolSlot* pool = &g_query_pools[index];
        if ((__atomic_load_n(&pool->state, __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&pool->state, __ATOMIC_ACQUIRE) == 2u) &&
            pool->owner == device)
            clear_query_pool_slot(pool);
    }
    for (index = 0u; index < RIN_VK_MAX_EVENTS; ++index) {
        RinVkEventSlot* event = &g_events[index];
        if ((__atomic_load_n(&event->state, __ATOMIC_ACQUIRE) == 1u ||
             __atomic_load_n(&event->state, __ATOMIC_ACQUIRE) == 2u) &&
            event->owner == device)
            clear_event_slot(event);
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
        const uint32_t state = __atomic_load_n(&slot->state,
                                                __ATOMIC_ACQUIRE);
        if ((state != RIN_VK_SUBMISSION_ACTIVE &&
             state != RIN_VK_SUBMISSION_RESERVED &&
             state != RIN_VK_SUBMISSION_WAITING) ||
            slot->owner != device) {
            continue;
        }
        rin_gpu_vulkan_command_buffers_abort(
            &g_command_runtime, slot->command_buffer_count,
            slot->command_buffers);
        mark_submission_query_events(slot, 0u);
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
    {
        RinVkResult dispatch_result =
            dispatch_ready_waiting_submissions(device);
        if (dispatch_result != RIN_VK_SUCCESS) {
            sync_unlock();
            return dispatch_result;
        }
    }
    if (!submission_slots_active() ||
        !device_product_submissions_active(device)) {
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
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) !=
                RIN_VK_SUBMISSION_ACTIVE ||
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
        complete_submission_query_events(slot);
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
    {
        RinVkResult dispatch_result =
            dispatch_ready_waiting_submissions(device);
        if (dispatch_result != RIN_VK_SUCCESS) {
            sync_unlock();
            return dispatch_result;
        }
    }
    sync_unlock();
    return RIN_VK_SUCCESS;
}

static RinVkResult maintain_device_submissions_until_available(
        struct RinVkDevice_T* device) {
    RinVkResult result;
    do {
        result = maintain_device_submissions(device);
        if (result == RIN_VK_NOT_READY) yield_thread();
    } while (result == RIN_VK_NOT_READY);
    return result;
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
    if (__atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) != 0u ||
        __atomic_load_n(&g_wsi_binding, __ATOMIC_ACQUIRE) != 0u)
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
        __atomic_load_n(&g_wsi_binding, __ATOMIC_ACQUIRE) != 0u ||
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
    if (__atomic_load_n(&g_wsi_binding, __ATOMIC_ACQUIRE) != 0u) {
        __atomic_store_n(&g_product_binding, (uintptr_t)platform,
                         __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_BUSY;
    }
    if (__atomic_load_n(&g_active_product_calls, __ATOMIC_ACQUIRE) != 0u ||
        resource_slots_active() || submission_slots_active()) {
        __atomic_store_n(&g_product_binding, (uintptr_t)platform,
                         __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_BUSY;
    }
    __atomic_store_n(&g_product_binding, 0u, __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_bind_wsi_platform(
    RinVulkanWsiPlatformV1* platform) {
    uintptr_t expected = 0u;
    if (!platform || (uintptr_t)platform == RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        platform->struct_size != sizeof(*platform) ||
        platform->version != RIN_VULKAN_WSI_PLATFORM_VERSION ||
        !platform->context || !platform->query_displays ||
        !platform->query_modes || !platform->present ||
        !platform->poll_present || !platform->cancel_present ||
        !wsi_zero_words(platform->reserved, 4u))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&g_wsi_binding, &expected,
                                     (uintptr_t)platform, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_bind_wsi_platform_v2(
    RinVulkanWsiPlatformV2* platform) {
    uintptr_t expected = 0u;
    if (!platform || (uintptr_t)platform == RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        platform->struct_size != sizeof(*platform) ||
        platform->version != RIN_VULKAN_WSI_PLATFORM_V2_VERSION ||
        !platform->context || !platform->query_displays ||
        !platform->query_modes || !platform->present ||
        !platform->poll_present || !platform->cancel_present ||
        !platform->query_planes ||
        !platform->query_plane_supported_displays ||
        !platform->query_plane_capabilities ||
        !wsi_zero_words(platform->reserved, 4u) ||
        !wsi_zero_words(platform->reserved_v2, 4u))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&g_wsi_binding, &expected,
                                     (uintptr_t)platform, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_bind_wsi_platform_v3(
    RinVulkanWsiPlatformV3* platform) {
    uintptr_t expected = 0u;
    if (!platform || (uintptr_t)platform == RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        platform->struct_size != sizeof(*platform) ||
        platform->version != RIN_VULKAN_WSI_PLATFORM_V3_VERSION ||
        !platform->context || !platform->query_displays ||
        !platform->query_modes || !platform->present ||
        !platform->poll_present || !platform->cancel_present ||
        !platform->query_planes ||
        !platform->query_plane_supported_displays ||
        !platform->query_plane_capabilities ||
        !platform->query_surface_support ||
        !wsi_zero_words(platform->reserved, 4u) ||
        !wsi_zero_words(platform->reserved_v2, 4u) ||
        !wsi_zero_words(platform->reserved_v3, 4u))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&g_wsi_binding, &expected,
                                     (uintptr_t)platform, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_bind_wsi_platform_v4(
    RinVulkanWsiPlatformV4* platform) {
    uintptr_t expected = 0u;
    if (!platform || (uintptr_t)platform == RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_runtime_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&g_product_binding, __ATOMIC_ACQUIRE) ==
            RIN_VK_ICD_BINDING_TRANSITION ||
        platform->struct_size != sizeof(*platform) ||
        platform->version != RIN_VULKAN_WSI_PLATFORM_V4_VERSION ||
        !platform->context || !platform->query_displays ||
        !platform->query_modes || !platform->present ||
        !platform->poll_present || !platform->cancel_present ||
        !platform->query_planes ||
        !platform->query_plane_supported_displays ||
        !platform->query_plane_capabilities ||
        !platform->query_surface_support ||
        !platform->query_surface_properties ||
        !wsi_zero_words(platform->reserved, 4u) ||
        !wsi_zero_words(platform->reserved_v2, 4u) ||
        !wsi_zero_words(platform->reserved_v3, 4u) ||
        !wsi_zero_words(platform->reserved_v4, 4u))
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    if (!__atomic_compare_exchange_n(&g_wsi_binding, &expected,
                                     (uintptr_t)platform, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_RELAXED))
        return RIN_GPU_VULKAN_BUSY;
    return RIN_GPU_VULKAN_OK;
}

static int unbind_wsi_platform(void* platform) {
    uintptr_t expected;
    uint32_t index;
    if (!platform || (uintptr_t)platform == RIN_VK_ICD_BINDING_TRANSITION)
        return RIN_GPU_VULKAN_INVALID_ARGUMENT;
    expected = (uintptr_t)platform;
    if (!__atomic_compare_exchange_n(
            &g_wsi_binding, &expected, RIN_VK_ICD_BINDING_TRANSITION, 0,
            __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        return expected == 0u ? RIN_GPU_VULKAN_NOT_INITIALIZED
                              : RIN_GPU_VULKAN_BUSY;
    if (__atomic_load_n(&g_active_wsi_calls, __ATOMIC_ACQUIRE) != 0u) {
        __atomic_store_n(&g_wsi_binding, (uintptr_t)platform,
                         __ATOMIC_RELEASE);
        return RIN_GPU_VULKAN_BUSY;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_INSTANCES; ++index) {
        if (__atomic_load_n(&g_instances[index].state,
                            __ATOMIC_ACQUIRE) != 0u) {
            __atomic_store_n(&g_wsi_binding, (uintptr_t)platform,
                             __ATOMIC_RELEASE);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    for (index = 0u; index < RIN_VK_MAX_DISPLAYS; ++index) {
        const uint32_t state =
            __atomic_load_n(&g_displays[index].state, __ATOMIC_ACQUIRE);
        if (state == 1u || state == 2u) {
            __atomic_store_n(&g_wsi_binding, (uintptr_t)platform,
                             __ATOMIC_RELEASE);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    for (index = 0u; index < RIN_VK_MAX_DISPLAY_MODES; ++index) {
        const uint32_t state = __atomic_load_n(
            &g_display_modes[index].state, __ATOMIC_ACQUIRE);
        if (state == 1u || state == 2u) {
            __atomic_store_n(&g_wsi_binding, (uintptr_t)platform,
                             __ATOMIC_RELEASE);
            return RIN_GPU_VULKAN_BUSY;
        }
    }
    __atomic_store_n(&g_wsi_binding, 0u, __ATOMIC_RELEASE);
    return RIN_GPU_VULKAN_OK;
}

int rin_gpu_vulkan_icd_unbind_wsi_platform(
    RinVulkanWsiPlatformV1* platform) {
    return unbind_wsi_platform(platform);
}

int rin_gpu_vulkan_icd_unbind_wsi_platform_v2(
    RinVulkanWsiPlatformV2* platform) {
    return unbind_wsi_platform(platform);
}

int rin_gpu_vulkan_icd_unbind_wsi_platform_v3(
    RinVulkanWsiPlatformV3* platform) {
    return unbind_wsi_platform(platform);
}

int rin_gpu_vulkan_icd_unbind_wsi_platform_v4(
    RinVulkanWsiPlatformV4* platform) {
    return unbind_wsi_platform(platform);
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
    RinVkExtensionProperties extension;
    uint32_t capacity;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (layer_name) {
        *property_count = 0u;
        return RIN_VK_ERROR_LAYER_NOT_PRESENT;
    }
    capacity = *property_count;
    if (!properties) {
        *property_count = 1u;
        return RIN_VK_SUCCESS;
    }
    *property_count = capacity == 0u ? 0u : 1u;
    if (capacity == 0u) return RIN_VK_INCOMPLETE;
    memset(&extension, 0, sizeof(extension));
    memcpy(extension.extensionName, RIN_VK_EXT_DEBUG_UTILS_EXTENSION,
           sizeof(RIN_VK_EXT_DEBUG_UTILS_EXTENSION));
    extension.specVersion = RIN_VK_DEBUG_UTILS_SPEC_VERSION;
    properties[0] = extension;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
        RinVkInstance instance,
        const RinVkDebugUtilsMessengerCreateInfoEXT* create_info,
        const void* allocator, RinVkDebugUtilsMessengerEXT* messenger_out) {
    struct RinVkInstance_T* owner = instance_slot(instance);
    uint32_t index;
    (void)allocator;
    if (!messenger_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *messenger_out = 0u;
    if (!owner || !owner->debug_utils_enabled)
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (!debug_messenger_create_info_valid(create_info))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_DEBUG_MESSENGERS; ++index) {
        RinVkDebugUtilsMessengerSlot* slot =
            &g_debug_utils_messengers[index];
        if (slot->state != 0u) continue;
        if (slot->generation == UINT32_MAX) {
            slot->state = 3u;
            continue;
        }
        ++slot->generation;
        if (slot->generation == 0u) {
            slot->state = 3u;
            continue;
        }
        slot->active_callbacks = 0u;
        slot->reserved = 0u;
        slot->owner = owner;
        slot->message_severity = create_info->messageSeverity;
        slot->message_type = create_info->messageType;
        slot->callback = create_info->pfnUserCallback;
        slot->user_data = create_info->pUserData;
        slot->state = 1u;
        *messenger_out = resource_handle(
            RIN_VK_DEBUG_UTILS_MESSENGER_TAG, index, slot->generation);
        break;
    }
    sync_unlock();
    return *messenger_out != 0u ? RIN_VK_SUCCESS
                                : RIN_VK_ERROR_TOO_MANY_OBJECTS;
}

void RIN_VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
        RinVkInstance instance, RinVkDebugUtilsMessengerEXT messenger,
        const void* allocator) {
    struct RinVkInstance_T* owner = instance_slot(instance);
    RinVkDebugUtilsMessengerSlot* slot;
    uint32_t generation;
    (void)allocator;
    if (!owner || messenger == 0u) return;
    sync_lock();
    slot = debug_messenger_slot(owner, messenger);
    if (slot) {
        generation = slot->generation;
        __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
    }
    sync_unlock();
    if (!slot) return;
    while (__atomic_load_n(&slot->active_callbacks, __ATOMIC_ACQUIRE) != 0u)
        yield_thread();
    sync_lock();
    if (slot->generation == generation && slot->state == 2u) {
        slot->owner = NULL;
        slot->message_severity = 0u;
        slot->message_type = 0u;
        slot->callback = NULL;
        slot->user_data = NULL;
        slot->reserved = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
    }
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkSetDebugUtilsObjectNameEXT(
        RinVkDevice device,
        const RinVkDebugUtilsObjectNameInfoEXT* name_info) {
    struct RinVkDevice_T* device_value = device_slot(device);
    struct RinVkInstance_T* owner = debug_instance_for_device(device_value);
    RinVkResult result;
    if (!device_value || !owner || !owner->debug_utils_enabled)
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (!name_info ||
        name_info->sType !=
            RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT ||
        name_info->pNext || name_info->objectHandle == 0u ||
        !debug_object_type_supported((uint32_t)name_info->objectType))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = debug_object_update(owner, (uint32_t)name_info->objectType,
                                 name_info->objectHandle,
                                 name_info->pObjectName, 0u, NULL, 0u,
                                 1, 0);
    if (result != RIN_VK_SUCCESS) return result;
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.ObjectName",
                           (uint32_t)name_info->objectType,
                           name_info->objectHandle, NULL, NULL,
                           name_info->pObjectName ? name_info->pObjectName
                                                  : "object name cleared");
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkSetDebugUtilsObjectTagEXT(
        RinVkDevice device,
        const RinVkDebugUtilsObjectTagInfoEXT* tag_info) {
    struct RinVkDevice_T* device_value = device_slot(device);
    struct RinVkInstance_T* owner = debug_instance_for_device(device_value);
    char* tag_message;
    RinVkResult result;
    if (!device_value || !owner || !owner->debug_utils_enabled)
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (!tag_info ||
        tag_info->sType !=
            RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT ||
        tag_info->pNext || tag_info->objectHandle == 0u ||
        !debug_object_type_supported((uint32_t)tag_info->objectType) ||
        (tag_info->tagSize != 0u && !tag_info->pTag))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    tag_message = debug_tag_message(tag_info->tagName, tag_info->pTag,
                                    tag_info->tagSize);
    if (!tag_message) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    result = debug_object_update(owner, (uint32_t)tag_info->objectType,
                                 tag_info->objectHandle, NULL,
                                 tag_info->tagName, tag_info->pTag,
                                 tag_info->tagSize, 0, 1);
    if (result != RIN_VK_SUCCESS) {
        free(tag_message);
        return result;
    }
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.ObjectTag",
                           (uint32_t)tag_info->objectType,
                           tag_info->objectHandle, NULL, NULL,
                           tag_message);
    free(tag_message);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkSubmitDebugUtilsMessageEXT(
        RinVkInstance instance, uint32_t message_severity,
        uint32_t message_types,
        const RinVkDebugUtilsMessengerCallbackDataEXT* callback_data) {
    struct RinVkInstance_T* owner = instance_slot(instance);
    if (!owner || !owner->debug_utils_enabled || !callback_data ||
        callback_data->sType !=
            RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT ||
        callback_data->pNext || !callback_data->pMessage ||
        (callback_data->queueLabelCount && !callback_data->pQueueLabels) ||
        (callback_data->cmdBufLabelCount && !callback_data->pCmdBufLabels) ||
        (callback_data->objectCount && !callback_data->pObjects) ||
        message_severity == 0u ||
        (message_severity & (message_severity - 1u)) != 0u ||
        (message_severity & ~RIN_VK_DEBUG_SEVERITY_MASK) != 0u ||
        message_types == 0u || (message_types & ~RIN_VK_DEBUG_TYPE_MASK) != 0u)
        return;
    debug_emit(owner, message_severity, message_types, callback_data);
}

static RinVkDebugUtilsLabelDataEXT debug_label_data(
        const RinVkDebugUtilsLabelEXT* label_info) {
    RinVkDebugUtilsLabelDataEXT label;
    uint32_t index;
    label.pLabelName = label_info->pLabelName;
    for (index = 0u; index < 4u; ++index)
        label.color[index] = label_info->color[index];
    return label;
}

static int debug_label_valid(const RinVkDebugUtilsLabelEXT* label_info) {
    uint32_t index;
    if (!label_info ||
        label_info->sType != RIN_VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT ||
        label_info->pNext || !label_info->pLabelName)
        return 0;
    for (index = 0u; index < 4u; ++index)
        if (!(label_info->color[index] >= 0.0f &&
              label_info->color[index] <= 1.0f))
            return 0;
    return 1;
}

static RinGpuVulkanCommandBufferV1* debug_command_buffer(
        RinVkCommandBuffer command_buffer,
        struct RinVkDevice_T** device_out) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* device;
    if (!core || core->loader_magic != RIN_VK_ICD_LOADER_MAGIC ||
        core->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING)
        return NULL;
    device = device_slot((RinVkDevice)core->owner);
    if (!device) return NULL;
    if (device_out) *device_out = device;
    return core;
}

void RIN_VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(
        RinVkCommandBuffer command_buffer,
        const RinVkDebugUtilsLabelEXT* label_info) {
    struct RinVkDevice_T* device = NULL;
    RinGpuVulkanCommandBufferV1* core =
        debug_command_buffer(command_buffer, &device);
    RinVkDebugUtilsLabelDataEXT label;
    struct RinVkInstance_T* owner;
    if (!core || !debug_label_valid(label_info) ||
        !(owner = debug_instance_for_device(device)) ||
        !owner->debug_utils_enabled)
        return;
    label = debug_label_data(label_info);
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.CommandLabelBegin",
                           RIN_VK_OBJECT_TYPE_COMMAND_BUFFER,
                           (uint64_t)(uintptr_t)command_buffer,
                           NULL, &label, label_info->pLabelName);
}

void RIN_VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(
        RinVkCommandBuffer command_buffer) {
    struct RinVkDevice_T* device = NULL;
    RinGpuVulkanCommandBufferV1* core =
        debug_command_buffer(command_buffer, &device);
    struct RinVkInstance_T* owner;
    if (!core || !(owner = debug_instance_for_device(device)) ||
        !owner->debug_utils_enabled)
        return;
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.CommandLabelEnd",
                           RIN_VK_OBJECT_TYPE_COMMAND_BUFFER,
                           (uint64_t)(uintptr_t)command_buffer,
                           NULL, NULL, "command label end");
}

void RIN_VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(
        RinVkCommandBuffer command_buffer,
        const RinVkDebugUtilsLabelEXT* label_info) {
    struct RinVkDevice_T* device = NULL;
    RinGpuVulkanCommandBufferV1* core =
        debug_command_buffer(command_buffer, &device);
    RinVkDebugUtilsLabelDataEXT label;
    struct RinVkInstance_T* owner;
    if (!core || !debug_label_valid(label_info) ||
        !(owner = debug_instance_for_device(device)) ||
        !owner->debug_utils_enabled)
        return;
    label = debug_label_data(label_info);
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.CommandLabelInsert",
                           RIN_VK_OBJECT_TYPE_COMMAND_BUFFER,
                           (uint64_t)(uintptr_t)command_buffer,
                           NULL, &label, label_info->pLabelName);
}

void RIN_VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(
        RinVkQueue queue, const RinVkDebugUtilsLabelEXT* label_info) {
    struct RinVkQueue_T* queue_value = queue_slot(queue);
    struct RinVkInstance_T* owner;
    RinVkDebugUtilsLabelDataEXT label;
    if (!queue_value || !debug_label_valid(label_info) ||
        !(owner = debug_instance_for_device(queue_value->device)) ||
        !owner->debug_utils_enabled)
        return;
    label = debug_label_data(label_info);
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.QueueLabelBegin",
                           RIN_VK_OBJECT_TYPE_QUEUE,
                           (uint64_t)(uintptr_t)queue,
                           &label, NULL, label_info->pLabelName);
}

void RIN_VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(RinVkQueue queue) {
    struct RinVkQueue_T* queue_value = queue_slot(queue);
    struct RinVkInstance_T* owner;
    if (!queue_value ||
        !(owner = debug_instance_for_device(queue_value->device)) ||
        !owner->debug_utils_enabled)
        return;
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.QueueLabelEnd",
                           RIN_VK_OBJECT_TYPE_QUEUE,
                           (uint64_t)(uintptr_t)queue,
                           NULL, NULL, "queue label end");
}

void RIN_VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(
        RinVkQueue queue, const RinVkDebugUtilsLabelEXT* label_info) {
    struct RinVkQueue_T* queue_value = queue_slot(queue);
    struct RinVkInstance_T* owner;
    RinVkDebugUtilsLabelDataEXT label;
    if (!queue_value || !debug_label_valid(label_info) ||
        !(owner = debug_instance_for_device(queue_value->device)) ||
        !owner->debug_utils_enabled)
        return;
    label = debug_label_data(label_info);
    debug_utils_annotation(owner, "RinVulkan.DebugUtils.QueueLabelInsert",
                           RIN_VK_OBJECT_TYPE_QUEUE,
                           (uint64_t)(uintptr_t)queue,
                           &label, NULL, label_info->pLabelName);
}

RinVkResult RIN_VKAPI_CALL vkEnumerateDeviceExtensionProperties(
        RinVkPhysicalDevice physical_device, const char* layer_name,
        uint32_t* property_count, RinVkExtensionProperties* properties) {
    RinVkExtensionProperties extensions[3];
    uint32_t capacity;
    uint32_t available = 3u;
    uint32_t count;
    uint32_t index;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!physical_slot(physical_device, NULL))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (layer_name) {
        *property_count = 0u;
        return RIN_VK_ERROR_LAYER_NOT_PRESENT;
    }
    capacity = *property_count;
    if (!properties) {
        *property_count = available;
        return RIN_VK_SUCCESS;
    }
    count = capacity < available ? capacity : available;
    *property_count = count;
    if (count == 0u) return RIN_VK_INCOMPLETE;
    memset(extensions, 0, sizeof(extensions));
    memcpy(extensions[0].extensionName,
           RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION,
           sizeof(RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION));
    extensions[0].specVersion = 2u;
    memcpy(extensions[1].extensionName,
           RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION,
           sizeof(RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION));
    extensions[1].specVersion = 1u;
    memcpy(extensions[2].extensionName,
           RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION,
           sizeof(RIN_VK_KHR_DYNAMIC_RENDERING_EXTENSION));
    extensions[2].specVersion = 1u;
    for (index = 0u; index < count; ++index) properties[index] = extensions[index];
    return count < available ? RIN_VK_INCOMPLETE : RIN_VK_SUCCESS;
}

static int display_metadata_equal(const RinVkDisplaySlot* slot,
                                  const RinVulkanWsiDisplayV1* display) {
    return slot && display && slot->width == display->width &&
           slot->height == display->height &&
           slot->refresh_millihertz == display->refresh_millihertz &&
           slot->format == display->format &&
           slot->physical_width_mm == display->physical_width_mm &&
           slot->physical_height_mm == display->physical_height_mm &&
           slot->flags == display->flags &&
           slot->plane_count == display->plane_count &&
           slot->mode_count == display->mode_count &&
           slot->current_mode_cookie == display->current_mode_cookie &&
           strcmp(slot->display_name, display->display_name) == 0;
}

static RinVkDisplaySlot* cache_display_locked(
        struct RinVkInstance_T* instance,
        struct RinVkPhysicalDevice_T* physical,
        const RinVulkanWsiDisplayV1* source, uint32_t* index_out,
        int* created_out, int* result_out) {
    uint32_t index;
    size_t name_length;
    RinVkDisplaySlot* slot;
    if (created_out) *created_out = 0;
    if (result_out) *result_out = RIN_GPU_VULKAN_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_VK_MAX_DISPLAYS; ++index) {
        slot = &g_displays[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
            slot->owner_instance != instance ||
            slot->owner_physical_device != physical ||
            slot->display_cookie != source->display_cookie ||
            slot->output_generation != source->output_generation ||
            slot->device_generation != source->device_generation)
            continue;
        if (!display_metadata_equal(slot, source)) return NULL;
        *index_out = index;
        if (result_out) *result_out = RIN_GPU_VULKAN_OK;
        return slot;
    }
    slot = reserve_display_slot(&index);
    if (!slot) {
        if (result_out) *result_out = RIN_GPU_VULKAN_LIMIT;
        return NULL;
    }
    slot->owner_instance = instance;
    slot->owner_physical_device = physical;
    slot->display_cookie = source->display_cookie;
    slot->output_generation = source->output_generation;
    slot->device_generation = source->device_generation;
    slot->current_mode_cookie = source->current_mode_cookie;
    slot->width = source->width;
    slot->height = source->height;
    slot->refresh_millihertz = source->refresh_millihertz;
    slot->format = source->format;
    slot->physical_width_mm = source->physical_width_mm;
    slot->physical_height_mm = source->physical_height_mm;
    slot->flags = source->flags;
    slot->plane_count = source->plane_count;
    slot->mode_count = source->mode_count;
    name_length = (size_t)((const char*)memchr(
        source->display_name, '\0', sizeof(source->display_name)) -
                           source->display_name);
    memcpy(slot->display_name, source->display_name, name_length);
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *index_out = index;
    if (created_out) *created_out = 1;
    if (result_out) *result_out = RIN_GPU_VULKAN_OK;
    return slot;
}

static int display_mode_metadata_equal(
        const RinVkDisplayModeSlot* slot, const RinVulkanWsiModeV1* source) {
    return slot && source && slot->width == source->width &&
           slot->height == source->height &&
           slot->refresh_millihertz == source->refresh_millihertz &&
           slot->format == source->format;
}

static RinVkDisplayModeSlot* cache_display_mode_locked(
        RinVkDisplaySlot* display, const RinVulkanWsiModeV1* source,
        uint32_t* index_out, int* created_out, int* result_out) {
    uint32_t index;
    RinVkDisplayModeSlot* slot;
    if (created_out) *created_out = 0;
    if (result_out) *result_out = RIN_GPU_VULKAN_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_VK_MAX_DISPLAY_MODES; ++index) {
        slot = &g_display_modes[index];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 1u ||
            slot->display != display ||
            slot->display_generation != display->generation ||
            slot->mode_cookie != source->mode_cookie ||
            slot->output_generation != source->output_generation)
            continue;
        if (!display_mode_metadata_equal(slot, source)) return NULL;
        *index_out = index;
        if (result_out) *result_out = RIN_GPU_VULKAN_OK;
        return slot;
    }
    slot = reserve_display_mode_slot(&index);
    if (!slot) {
        if (result_out) *result_out = RIN_GPU_VULKAN_LIMIT;
        return NULL;
    }
    slot->display = display;
    slot->display_generation = display->generation;
    slot->mode_cookie = source->mode_cookie;
    slot->output_generation = source->output_generation;
    slot->width = source->width;
    slot->height = source->height;
    slot->refresh_millihertz = source->refresh_millihertz;
    slot->format = source->format;
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *index_out = index;
    if (created_out) *created_out = 1;
    if (result_out) *result_out = RIN_GPU_VULKAN_OK;
    return slot;
}

static int wsi_query_display_records(
        struct RinVkPhysicalDevice_T* physical,
        struct RinVkInstance_T** instance_out,
        RinVulkanWsiDisplayV1 displays[RIN_VULKAN_WSI_MAX_DISPLAYS],
        uint32_t* count_out, RinVkResult* result_out) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanRuntimeV1* runtime;
    RinVulkanWsiPlatformV1* platform;
    struct RinVkInstance_T* instance = NULL;
    uint32_t count = 0u;
    int result;
    if (!physical_slot((RinVkPhysicalDevice)physical, &instance)) {
        *result_out = RIN_VK_ERROR_INITIALIZATION_FAILED;
        return 0;
    }
    runtime = acquire_runtime();
    if (!runtime) {
        *result_out = RIN_VK_ERROR_INITIALIZATION_FAILED;
        return 0;
    }
    memset(&profile, 0, sizeof(profile));
    result = call_query_physical(runtime, physical->owner_instance,
                                 physical->runtime_handle, &profile);
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK ||
        profile.device_epoch == 0u) {
        *result_out = result == RIN_GPU_VULKAN_OK
                          ? RIN_VK_ERROR_DEVICE_LOST
                          : map_result(result);
        return 0;
    }
    platform = acquire_wsi();
    if (!platform) {
        *result_out = RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
        return 0;
    }
    memset(displays, 0,
           sizeof(RinVulkanWsiDisplayV1) * RIN_VULKAN_WSI_MAX_DISPLAYS);
    result = platform->query_displays(
        platform->context, profile.device_epoch,
        RIN_VULKAN_WSI_MAX_DISPLAYS, &count, displays);
    release_wsi();
    if (count > RIN_VULKAN_WSI_MAX_DISPLAYS) {
        *result_out = RIN_VK_ERROR_DEVICE_LOST;
        return 0;
    }
    if (result != RIN_VULKAN_WSI_PLATFORM_OK &&
        result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE) {
        *result_out = map_wsi_platform_result(result);
        return 0;
    }
    for (uint32_t index = 0u; index < count; ++index) {
        if (!wsi_display_record_valid(&displays[index],
                                      profile.device_epoch)) {
            *result_out = RIN_VK_ERROR_DEVICE_LOST;
            return 0;
        }
        for (uint32_t prior = 0u; prior < index; ++prior) {
            if (displays[prior].display_cookie ==
                displays[index].display_cookie) {
                *result_out = RIN_VK_ERROR_DEVICE_LOST;
                return 0;
            }
        }
    }
    *count_out = count;
    if (instance_out) *instance_out = instance;
    *result_out = map_wsi_platform_result(result);
    return 1;
}

static int wsi_plane_record_valid(
        const RinVulkanWsiDisplayPlaneV2* plane, uint32_t expected_index,
        uint64_t device_generation) {
    if (!plane || plane->struct_size != sizeof(*plane) ||
        plane->version != RIN_VULKAN_WSI_PLATFORM_V2_VERSION ||
        plane->plane_index != expected_index ||
        plane->device_generation != device_generation ||
        plane->flags != 0u || plane->reserved0 != 0u ||
        !wsi_zero_words(plane->reserved, 2u))
        return 0;
    if (plane->current_display_cookie == 0u)
        return plane->output_generation == 0u;
    return plane->output_generation != 0u &&
           plane->output_generation != UINT64_MAX;
}

static int wsi_query_plane_records(
        struct RinVkPhysicalDevice_T* physical,
        struct RinVkInstance_T** instance_out,
        RinVulkanWsiDisplayPlaneV2 planes[RIN_VULKAN_WSI_MAX_PLANES],
        uint32_t* count_out, uint64_t* device_generation_out,
        RinVkResult* result_out) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanRuntimeV1* runtime;
    RinVulkanWsiPlatformV2* platform;
    struct RinVkInstance_T* instance = NULL;
    uint32_t count = 0u;
    uint32_t index;
    int result;
    if (!physical_slot((RinVkPhysicalDevice)physical, &instance)) {
        *result_out = RIN_VK_ERROR_INITIALIZATION_FAILED;
        return 0;
    }
    runtime = acquire_runtime();
    if (!runtime) {
        *result_out = RIN_VK_ERROR_INITIALIZATION_FAILED;
        return 0;
    }
    memset(&profile, 0, sizeof(profile));
    result = call_query_physical(runtime, physical->owner_instance,
                                 physical->runtime_handle, &profile);
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK || profile.device_epoch == 0u) {
        *result_out = result == RIN_GPU_VULKAN_OK
                          ? RIN_VK_ERROR_DEVICE_LOST
                          : map_result(result);
        return 0;
    }
    platform = acquire_wsi_v2();
    if (!platform) {
        *result_out = RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
        return 0;
    }
    memset(planes, 0,
           sizeof(RinVulkanWsiDisplayPlaneV2) *
               RIN_VULKAN_WSI_MAX_PLANES);
    result = platform->query_planes(
        platform->context, profile.device_epoch,
        RIN_VULKAN_WSI_MAX_PLANES, &count, planes);
    release_wsi();
    if (count > RIN_VULKAN_WSI_MAX_PLANES) {
        *result_out = RIN_VK_ERROR_DEVICE_LOST;
        return 0;
    }
    if (result != RIN_VULKAN_WSI_PLATFORM_OK &&
        result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE) {
        *result_out = map_wsi_platform_result(result);
        return 0;
    }
    for (index = 0u; index < count; ++index) {
        if (!wsi_plane_record_valid(&planes[index], index,
                                    profile.device_epoch) ||
            (planes[index].current_display_cookie != 0u &&
             planes[index].current_stack_index >= count)) {
            *result_out = RIN_VK_ERROR_DEVICE_LOST;
            return 0;
        }
    }
    *count_out = count;
    *device_generation_out = profile.device_epoch;
    if (instance_out) *instance_out = instance;
    *result_out = map_wsi_platform_result(result);
    return 1;
}

static const RinVulkanWsiDisplayV1* wsi_find_display_record(
        const RinVulkanWsiDisplayV1* displays, uint32_t display_count,
        uint64_t display_cookie, uint64_t output_generation,
        uint64_t device_generation) {
    uint32_t index;
    if (!displays || display_cookie == 0u) return NULL;
    for (index = 0u; index < display_count; ++index) {
        if (displays[index].display_cookie == display_cookie &&
            displays[index].output_generation == output_generation &&
            displays[index].device_generation == device_generation)
            return &displays[index];
    }
    return NULL;
}

static RinVkResult wsi_cache_display_handle(
        struct RinVkInstance_T* instance,
        struct RinVkPhysicalDevice_T* physical,
        const RinVulkanWsiDisplayV1* source, RinVkDisplayKHR* handle_out) {
    RinVkDisplaySlot* slot;
    uint32_t slot_index = 0u;
    int cache_result = RIN_GPU_VULKAN_OK;
    if (!instance || !physical || !source || !handle_out)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    sync_lock();
    slot = cache_display_locked(instance, physical, source, &slot_index, NULL,
                                &cache_result);
    if (slot)
        *handle_out = resource_handle(RIN_VK_DISPLAY_TAG, slot_index,
                                      slot->generation);
    sync_unlock();
    if (slot) return RIN_VK_SUCCESS;
    return cache_result == RIN_GPU_VULKAN_LIMIT
               ? RIN_VK_ERROR_TOO_MANY_OBJECTS
               : RIN_VK_ERROR_DEVICE_LOST;
}

RinVkResult RIN_VKAPI_CALL vkGetPhysicalDeviceDisplayPropertiesKHR(
        RinVkPhysicalDevice physical_device, uint32_t* property_count,
        RinVkDisplayPropertiesKHR* properties) {
    RinVulkanWsiDisplayV1 displays[RIN_VULKAN_WSI_MAX_DISPLAYS];
    RinVkDisplayPropertiesKHR converted[RIN_VULKAN_WSI_MAX_DISPLAYS];
    RinVkDisplaySlot* newly_created[RIN_VULKAN_WSI_MAX_DISPLAYS];
    struct RinVkPhysicalDevice_T* physical;
    struct RinVkInstance_T* instance = NULL;
    RinVkResult provider_result = RIN_VK_SUCCESS;
    uint32_t available = 0u;
    uint32_t capacity;
    uint32_t written;
    uint32_t created_count = 0u;
    uint32_t index;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *property_count;
    *property_count = 0u;
    physical = physical_slot(physical_device, NULL);
    if (!physical ||
        !wsi_query_display_records(physical, &instance, displays,
                                   &available, &provider_result))
        return provider_result;
    if (provider_result == RIN_VK_SUCCESS)
        wsi_retire_stale_displays(instance, physical, displays, available);
    if (!properties) {
        *property_count = available;
        return provider_result;
    }
    written = capacity < available ? capacity : available;
    if (written == 0u) return available != 0u ? RIN_VK_INCOMPLETE
                                              : provider_result;
    memset(converted, 0, sizeof(converted));
    sync_lock();
    for (index = 0u; index < written; ++index) {
        uint32_t slot_index = 0u;
        int created = 0;
        int cache_result = RIN_GPU_VULKAN_OK;
        RinVkDisplaySlot* slot = cache_display_locked(
            instance, physical, &displays[index], &slot_index, &created,
            &cache_result);
        if (!slot) {
            provider_result = cache_result == RIN_GPU_VULKAN_LIMIT
                                  ? RIN_VK_ERROR_TOO_MANY_OBJECTS
                                  : RIN_VK_ERROR_DEVICE_LOST;
            break;
        }
        if (created) newly_created[created_count++] = slot;
        converted[index].display = resource_handle(
            RIN_VK_DISPLAY_TAG, slot_index, slot->generation);
        converted[index].displayName = slot->display_name;
        converted[index].physicalDimensions.width = slot->physical_width_mm;
        converted[index].physicalDimensions.height = slot->physical_height_mm;
        converted[index].physicalResolution.width = slot->width;
        converted[index].physicalResolution.height = slot->height;
        converted[index].supportedTransforms =
            (slot->flags & RIN_VULKAN_WSI_DISPLAY_TRANSFORM_IDENTITY) != 0u
                ? 1u
                : 0u;
        converted[index].planeReorderPossible =
            (slot->flags & RIN_VULKAN_WSI_DISPLAY_PLANE_REORDER) != 0u;
        converted[index].persistentContent =
            (slot->flags & RIN_VULKAN_WSI_DISPLAY_PERSISTENT_CONTENT) != 0u;
    }
    if (provider_result < 0) {
        while (created_count != 0u)
            clear_display_slot(newly_created[--created_count]);
        sync_unlock();
        return provider_result;
    }
    memcpy(properties, converted, sizeof(converted[0]) * written);
    sync_unlock();
    *property_count = written;
    if (written < available || provider_result == RIN_VK_INCOMPLETE)
        return RIN_VK_INCOMPLETE;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkGetDisplayModePropertiesKHR(
        RinVkPhysicalDevice physical_device, RinVkDisplayKHR display_handle,
        uint32_t* property_count,
        RinVkDisplayModePropertiesKHR* properties) {
    RinVulkanWsiModeV1 modes[RIN_VULKAN_WSI_MAX_MODES];
    RinVkDisplayModePropertiesKHR converted[RIN_VULKAN_WSI_MAX_MODES];
    RinVkDisplayModeSlot* newly_created[RIN_VULKAN_WSI_MAX_MODES];
    struct RinVkPhysicalDevice_T* physical;
    RinVkDisplaySlot* display;
    RinVulkanWsiPlatformV1* platform;
    uint32_t capacity;
    uint32_t available = 0u;
    uint32_t written;
    uint32_t created_count = 0u;
    uint32_t index;
    int callback_result;
    RinVkResult provider_result = RIN_VK_SUCCESS;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *property_count;
    *property_count = 0u;
    physical = physical_slot(physical_device, NULL);
    if (!physical) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    display = display_slot_from_handle(physical, display_handle);
    if (!display) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    platform = acquire_wsi();
    if (!platform) return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    memset(modes, 0, sizeof(modes));
    callback_result = platform->query_modes(
        platform->context, display->device_generation,
        display->display_cookie, display->output_generation,
        RIN_VULKAN_WSI_MAX_MODES, &available, modes);
    release_wsi();
    if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    if (available > RIN_VULKAN_WSI_MAX_MODES)
        return RIN_VK_ERROR_DEVICE_LOST;
    if (callback_result != RIN_VULKAN_WSI_PLATFORM_OK &&
        callback_result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return map_wsi_platform_result(callback_result);
    if (available != display->mode_count)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    {
        int current_mode_found = 0;
        for (index = 0u; index < available; ++index) {
            if (!wsi_mode_record_valid(&modes[index], display))
                return RIN_VK_ERROR_DEVICE_LOST;
            for (uint32_t prior = 0u; prior < index; ++prior) {
                if (modes[prior].mode_cookie == modes[index].mode_cookie)
                    return RIN_VK_ERROR_DEVICE_LOST;
            }
            if (modes[index].mode_cookie == display->current_mode_cookie) {
                if (modes[index].width != display->width ||
                    modes[index].height != display->height ||
                    modes[index].refresh_millihertz !=
                        display->refresh_millihertz ||
                    modes[index].format != display->format)
                    return RIN_VK_ERROR_DEVICE_LOST;
                current_mode_found = 1;
            }
        }
        if (!current_mode_found) return RIN_VK_ERROR_DEVICE_LOST;
    }
    if (!properties) {
        *property_count = available;
        return callback_result == RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
                   ? RIN_VK_INCOMPLETE
                   : RIN_VK_SUCCESS;
    }
    written = capacity < available ? capacity : available;
    if (written == 0u)
        return available != 0u ? RIN_VK_INCOMPLETE : RIN_VK_SUCCESS;
    memset(converted, 0, sizeof(converted));
    sync_lock();
    if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u) {
        sync_unlock();
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    }
    for (index = 0u; index < written; ++index) {
        uint32_t slot_index = 0u;
        int created = 0;
        int cache_result = RIN_GPU_VULKAN_OK;
        RinVkDisplayModeSlot* slot = cache_display_mode_locked(
            display, &modes[index], &slot_index, &created, &cache_result);
        if (!slot) {
            provider_result = cache_result == RIN_GPU_VULKAN_LIMIT
                                  ? RIN_VK_ERROR_TOO_MANY_OBJECTS
                                  : RIN_VK_ERROR_DEVICE_LOST;
            break;
        }
        if (created) newly_created[created_count++] = slot;
        converted[index].displayMode = resource_handle(
            RIN_VK_DISPLAY_MODE_TAG, slot_index, slot->generation);
        converted[index].parameters.visibleRegion.width = slot->width;
        converted[index].parameters.visibleRegion.height = slot->height;
        converted[index].parameters.refreshRate =
            slot->refresh_millihertz;
    }
    if (provider_result < 0) {
        while (created_count != 0u)
            clear_display_mode_slot(newly_created[--created_count]);
        sync_unlock();
        return provider_result;
    }
    memcpy(properties, converted, sizeof(converted[0]) * written);
    sync_unlock();
    *property_count = written;
    if (written < available ||
        callback_result == RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return RIN_VK_INCOMPLETE;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateDisplayModeKHR(
        RinVkPhysicalDevice physical_device, RinVkDisplayKHR display_handle,
        const RinVkDisplayModeCreateInfoKHR* create_info,
        const void* allocator, RinVkDisplayModeKHR* mode_out) {
    RinVkDisplayModePropertiesKHR modes[RIN_VULKAN_WSI_MAX_MODES];
    struct RinVkPhysicalDevice_T* physical;
    uint32_t mode_count = RIN_VULKAN_WSI_MAX_MODES;
    uint32_t index;
    RinVkResult result;
    (void)allocator;
    if (!mode_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *mode_out = 0u;
    if (!create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR ||
        create_info->pNext != NULL || create_info->flags != 0u ||
        create_info->parameters.visibleRegion.width == 0u ||
        create_info->parameters.visibleRegion.height == 0u ||
        create_info->parameters.refreshRate == 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    physical = physical_slot(physical_device, NULL);
    if (!physical || !display_slot_from_handle(physical, display_handle))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = vkGetDisplayModePropertiesKHR(
        physical_device, display_handle, &mode_count, modes);
    if (result != RIN_VK_SUCCESS)
        return result == RIN_VK_INCOMPLETE
                   ? RIN_VK_ERROR_OUT_OF_DATE_KHR
                   : result;
    for (index = 0u; index < mode_count; ++index) {
        if (modes[index].parameters.visibleRegion.width ==
                create_info->parameters.visibleRegion.width &&
            modes[index].parameters.visibleRegion.height ==
                create_info->parameters.visibleRegion.height &&
            modes[index].parameters.refreshRate ==
                create_info->parameters.refreshRate) {
            *mode_out = modes[index].displayMode;
            return RIN_VK_SUCCESS;
        }
    }
    return RIN_VK_ERROR_INITIALIZATION_FAILED;
}

RinVkResult RIN_VKAPI_CALL vkCreateDisplayPlaneSurfaceKHR(
        RinVkInstance instance_handle,
        const RinVkDisplaySurfaceCreateInfoKHR* create_info,
        const void* allocator, RinVkSurfaceKHR* surface_out) {
    RinVkDisplayPlanePropertiesKHR plane_properties[RIN_VULKAN_WSI_MAX_PLANES];
    RinVkDisplayKHR supported_displays[RIN_VULKAN_WSI_MAX_DISPLAYS];
    RinVkDisplayPlaneCapabilitiesKHR capabilities;
    RinVkPhysicalDeviceProperties physical_properties;
    struct RinVkInstance_T* instance;
    struct RinVkInstance_T* mode_instance = NULL;
    struct RinVkPhysicalDevice_T* physical;
    RinVkDisplayModeSlot* mode;
    RinVkDisplaySlot* display;
    RinVkDisplaySurfaceSlot* surface;
    uint32_t plane_count = RIN_VULKAN_WSI_MAX_PLANES;
    uint32_t supported_count = RIN_VULKAN_WSI_MAX_DISPLAYS;
    uint32_t slot_index = 0u;
    uint32_t index;
    RinVkResult result;
    (void)allocator;
    if (!surface_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *surface_out = 0u;
    instance = instance_slot(instance_handle);
    if (!instance || !create_info ||
        create_info->sType !=
            RIN_VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR ||
        create_info->flags != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->pNext != NULL)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    mode = display_mode_slot_from_handle_any(create_info->displayMode);
    if (!mode) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    display = mode->display;
    physical = display->owner_physical_device;
    if (display->owner_instance != instance ||
        !physical_slot((RinVkPhysicalDevice)physical, &mode_instance) ||
        mode_instance != instance)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->transform !=
        RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if (create_info->alphaMode == 0u ||
        (create_info->alphaMode &
         (create_info->alphaMode - 1u)) != 0u ||
        (create_info->alphaMode &
         ~RIN_VK_DISPLAY_PLANE_ALPHA_KNOWN_BITS_KHR) != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->alphaMode == RIN_VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR &&
        (!finite_graphics_float(create_info->globalAlpha) ||
         create_info->globalAlpha < 0.0f ||
         create_info->globalAlpha > 1.0f))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if ((display->flags & RIN_VULKAN_WSI_DISPLAY_TRANSFORM_IDENTITY) == 0u)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if (display->device_generation == 0u ||
        mode->output_generation != display->output_generation ||
        mode->display_generation != display->generation)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;

    result = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
        (RinVkPhysicalDevice)physical, &plane_count, plane_properties);
    if (result == RIN_VK_INCOMPLETE)
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    if (result != RIN_VK_SUCCESS) return result;
    if (create_info->planeIndex >= plane_count)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if ((display->flags & RIN_VULKAN_WSI_DISPLAY_PLANE_REORDER) != 0u) {
        if (create_info->planeStackIndex >= plane_count)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
    } else if (create_info->planeStackIndex !=
               plane_properties[create_info->planeIndex].currentStackIndex) {
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }

    result = vkGetDisplayPlaneSupportedDisplaysKHR(
        (RinVkPhysicalDevice)physical, create_info->planeIndex,
        &supported_count, supported_displays);
    if (result == RIN_VK_INCOMPLETE)
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    if (result != RIN_VK_SUCCESS) return result;
    for (index = 0u; index < supported_count; ++index) {
        if (display_slot_from_handle(physical, supported_displays[index]) ==
            display)
            break;
    }
    if (index == supported_count) {
        if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u ||
            __atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) != 1u ||
            mode->output_generation != display->output_generation)
            return RIN_VK_ERROR_OUT_OF_DATE_KHR;
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }

    result = vkGetDisplayPlaneCapabilitiesKHR(
        (RinVkPhysicalDevice)physical, create_info->displayMode,
        create_info->planeIndex, &capabilities);
    if (result != RIN_VK_SUCCESS) return result;
    if ((capabilities.supportedAlpha & create_info->alphaMode) == 0u)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    vkGetPhysicalDeviceProperties((RinVkPhysicalDevice)physical,
                                  &physical_properties);
    if (physical_properties.limits.maxImageDimension2D == 0u)
        return RIN_VK_ERROR_DEVICE_LOST;
    if (create_info->imageExtent.width >
            physical_properties.limits.maxImageDimension2D ||
        create_info->imageExtent.height >
            physical_properties.limits.maxImageDimension2D)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;

    if (__atomic_load_n(&instance->state, __ATOMIC_ACQUIRE) != 1u ||
        __atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u ||
        __atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) != 1u ||
        mode->display != display ||
        mode->generation !=
            (uint32_t)(create_info->displayMode >> 16u) ||
        mode->display_generation != display->generation ||
        mode->output_generation != display->output_generation ||
        mode->output_generation == 0u)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    surface = reserve_display_surface_slot(&slot_index);
    if (!surface) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    surface->owner_instance = instance;
    surface->display = display;
    surface->display_generation = display->generation;
    surface->mode = mode;
    surface->mode_generation = mode->generation;
    surface->plane_index = create_info->planeIndex;
    surface->plane_stack_index = create_info->planeStackIndex;
    surface->transform = create_info->transform;
    surface->alpha_mode = create_info->alphaMode;
    surface->global_alpha = create_info->globalAlpha;
    surface->image_extent = create_info->imageExtent;
    surface->output_generation = display->output_generation;
    surface->device_generation = display->device_generation;
    if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u ||
        __atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) != 1u ||
        surface->display_generation != display->generation ||
        surface->mode_generation != mode->generation ||
        surface->output_generation != mode->output_generation) {
        clear_display_surface_slot(surface);
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    }
    __atomic_store_n(&surface->state, 1u, __ATOMIC_RELEASE);
    *surface_out = resource_handle(RIN_VK_SURFACE_TAG, slot_index,
                                   surface->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroySurfaceKHR(RinVkInstance instance_handle,
                                        RinVkSurfaceKHR surface_handle,
                                        const void* allocator) {
    struct RinVkInstance_T* instance = instance_slot(instance_handle);
    uint32_t index_field = (uint32_t)(surface_handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(surface_handle >> 16u);
    RinVkDisplaySurfaceSlot* surface;
    (void)allocator;
    if (!instance || (surface_handle >> 48u) != RIN_VK_SURFACE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_DISPLAY_SURFACES ||
        generation == 0u)
        return;
    surface = &g_display_surfaces[index_field - 1u];
    sync_lock();
    if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 1u &&
        surface->generation == generation &&
        surface->owner_instance == instance &&
        __atomic_load_n(&surface->swapchain_count, __ATOMIC_ACQUIRE) == 0u) {
        __atomic_store_n(&surface->state, 2u, __ATOMIC_RELEASE);
        if (__atomic_load_n(&surface->active_queries, __ATOMIC_ACQUIRE) == 0u)
            clear_display_surface_slot(surface);
    }
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
        RinVkPhysicalDevice physical_device, uint32_t* property_count,
        RinVkDisplayPlanePropertiesKHR* properties) {
    RinVulkanWsiDisplayPlaneV2 planes[RIN_VULKAN_WSI_MAX_PLANES];
    RinVulkanWsiDisplayV1 displays[RIN_VULKAN_WSI_MAX_DISPLAYS];
    RinVkDisplayPlanePropertiesKHR converted[RIN_VULKAN_WSI_MAX_PLANES];
    struct RinVkPhysicalDevice_T* physical;
    struct RinVkInstance_T* instance = NULL;
    RinVkResult plane_result = RIN_VK_SUCCESS;
    RinVkResult display_result = RIN_VK_SUCCESS;
    uint32_t available = 0u;
    uint32_t display_count = 0u;
    uint32_t capacity;
    uint32_t written;
    uint32_t index;
    uint64_t device_generation = 0u;
    int needs_displays = 0;
    if (!property_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *property_count;
    *property_count = 0u;
    physical = physical_slot(physical_device, NULL);
    if (!physical || !wsi_query_plane_records(
                         physical, &instance, planes, &available,
                         &device_generation, &plane_result))
        return plane_result;
    if (!properties) {
        *property_count = available;
        return plane_result;
    }
    written = capacity < available ? capacity : available;
    if (written == 0u)
        return available != 0u ? RIN_VK_INCOMPLETE : plane_result;
    for (index = 0u; index < written; ++index)
        needs_displays |= planes[index].current_display_cookie != 0u;
    if (needs_displays) {
        if (!wsi_query_display_records(physical, NULL, displays,
                                       &display_count, &display_result))
            return display_result;
        if (display_result == RIN_VK_SUCCESS)
            wsi_retire_stale_displays(instance, physical, displays,
                                      display_count);
    }
    memset(converted, 0, sizeof(converted));
    for (index = 0u; index < written; ++index) {
        const RinVulkanWsiDisplayPlaneV2* plane = &planes[index];
        converted[index].currentStackIndex = plane->current_stack_index;
        if (plane->current_display_cookie != 0u) {
            const RinVulkanWsiDisplayV1* display = wsi_find_display_record(
                displays, display_count, plane->current_display_cookie,
                plane->output_generation, device_generation);
            RinVkResult handle_result;
            if (!display) return RIN_VK_ERROR_OUT_OF_DATE_KHR;
            handle_result = wsi_cache_display_handle(
                instance, physical, display,
                &converted[index].currentDisplay);
            if (handle_result != RIN_VK_SUCCESS) return handle_result;
        }
    }
    memcpy(properties, converted, sizeof(converted[0]) * written);
    *property_count = written;
    if (written < available || plane_result == RIN_VK_INCOMPLETE ||
        display_result == RIN_VK_INCOMPLETE)
        return RIN_VK_INCOMPLETE;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkGetDisplayPlaneSupportedDisplaysKHR(
        RinVkPhysicalDevice physical_device, uint32_t plane_index,
        uint32_t* display_count, RinVkDisplayKHR* displays) {
    RinVulkanWsiDisplayPlaneV2 planes[RIN_VULKAN_WSI_MAX_PLANES];
    RinVulkanWsiDisplayV1 display_records[RIN_VULKAN_WSI_MAX_DISPLAYS];
    RinVkDisplayKHR converted[RIN_VULKAN_WSI_MAX_DISPLAYS];
    uint64_t display_cookies[RIN_VULKAN_WSI_MAX_DISPLAYS];
    struct RinVkPhysicalDevice_T* physical;
    struct RinVkInstance_T* instance = NULL;
    RinVulkanWsiPlatformV2* platform;
    RinVkResult plane_result = RIN_VK_SUCCESS;
    RinVkResult records_result = RIN_VK_SUCCESS;
    uint32_t plane_count = 0u;
    uint32_t available = 0u;
    uint32_t record_count = 0u;
    uint32_t capacity;
    uint32_t written;
    uint32_t index;
    uint64_t device_generation = 0u;
    int callback_result;
    if (!display_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *display_count;
    *display_count = 0u;
    physical = physical_slot(physical_device, NULL);
    if (!physical || !wsi_query_plane_records(
                         physical, &instance, planes, &plane_count,
                         &device_generation, &plane_result))
        return plane_result;
    if (plane_index >= plane_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    platform = acquire_wsi_v2();
    if (!platform) return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    memset(display_cookies, 0, sizeof(display_cookies));
    callback_result = platform->query_plane_supported_displays(
        platform->context, device_generation, plane_index,
        RIN_VULKAN_WSI_MAX_DISPLAYS, &available, display_cookies);
    release_wsi();
    if (available > RIN_VULKAN_WSI_MAX_DISPLAYS)
        return RIN_VK_ERROR_DEVICE_LOST;
    if (callback_result != RIN_VULKAN_WSI_PLATFORM_OK &&
        callback_result != RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return map_wsi_platform_result(callback_result);
    for (index = 0u; index < available; ++index) {
        if (display_cookies[index] == 0u)
            return RIN_VK_ERROR_DEVICE_LOST;
        for (uint32_t prior = 0u; prior < index; ++prior) {
            if (display_cookies[prior] == display_cookies[index])
                return RIN_VK_ERROR_DEVICE_LOST;
        }
    }
    if (!displays) {
        *display_count = available;
        return callback_result == RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
                   ? RIN_VK_INCOMPLETE
                   : RIN_VK_SUCCESS;
    }
    written = capacity < available ? capacity : available;
    if (written == 0u)
        return available != 0u ? RIN_VK_INCOMPLETE : RIN_VK_SUCCESS;
    if (!wsi_query_display_records(physical, NULL, display_records,
                                   &record_count, &records_result))
        return records_result;
    if (records_result == RIN_VK_SUCCESS)
        wsi_retire_stale_displays(instance, physical, display_records,
                                  record_count);
    memset(converted, 0, sizeof(converted));
    for (index = 0u; index < written; ++index) {
        const RinVulkanWsiDisplayV1* record = NULL;
        uint32_t record_index;
        for (record_index = 0u; record_index < record_count;
             ++record_index) {
            if (display_records[record_index].display_cookie ==
                display_cookies[index]) {
                record = &display_records[record_index];
                break;
            }
        }
        if (!record || record->device_generation != device_generation)
            return RIN_VK_ERROR_OUT_OF_DATE_KHR;
        {
            RinVkResult handle_result = wsi_cache_display_handle(
                instance, physical, record, &converted[index]);
            if (handle_result != RIN_VK_SUCCESS) return handle_result;
        }
    }
    memcpy(displays, converted, sizeof(converted[0]) * written);
    *display_count = written;
    if (written < available ||
        callback_result == RIN_VULKAN_WSI_PLATFORM_INCOMPLETE ||
        records_result == RIN_VK_INCOMPLETE)
        return RIN_VK_INCOMPLETE;
    return RIN_VK_SUCCESS;
}

static int wsi_plane_capabilities_valid(
        const RinVulkanWsiPlaneCapabilitiesV2* capabilities) {
    return capabilities && capabilities->supported_alpha != 0u &&
           (capabilities->supported_alpha &
            ~RIN_VK_DISPLAY_PLANE_ALPHA_KNOWN_BITS_KHR) == 0u &&
           capabilities->min_src_x <= capabilities->max_src_x &&
           capabilities->min_src_y <= capabilities->max_src_y &&
           capabilities->min_src_width != 0u &&
           capabilities->min_src_height != 0u &&
           capabilities->min_src_width <= capabilities->max_src_width &&
           capabilities->min_src_height <= capabilities->max_src_height &&
           capabilities->min_dst_x <= capabilities->max_dst_x &&
           capabilities->min_dst_y <= capabilities->max_dst_y &&
           capabilities->min_dst_width != 0u &&
           capabilities->min_dst_height != 0u &&
           capabilities->min_dst_width <= capabilities->max_dst_width &&
           capabilities->min_dst_height <= capabilities->max_dst_height;
}

RinVkResult RIN_VKAPI_CALL vkGetDisplayPlaneCapabilitiesKHR(
        RinVkPhysicalDevice physical_device, RinVkDisplayModeKHR mode_handle,
        uint32_t plane_index,
        RinVkDisplayPlaneCapabilitiesKHR* capabilities_out) {
    RinVulkanWsiDisplayPlaneV2 planes[RIN_VULKAN_WSI_MAX_PLANES];
    RinVulkanWsiPlaneCapabilitiesV2 source;
    RinVkDisplayPlaneCapabilitiesKHR converted;
    struct RinVkPhysicalDevice_T* physical;
    RinVkDisplayModeSlot* mode;
    RinVkDisplaySlot* display;
    RinVulkanWsiPlatformV2* platform;
    RinVkResult plane_result = RIN_VK_SUCCESS;
    uint32_t plane_count = 0u;
    uint64_t device_generation = 0u;
    int callback_result;
    if (!capabilities_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    physical = physical_slot(physical_device, NULL);
    if (!physical || !wsi_query_plane_records(
                         physical, NULL, planes, &plane_count,
                         &device_generation, &plane_result))
        return plane_result;
    if (plane_result != RIN_VK_SUCCESS)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    if (plane_index >= plane_count)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    mode = display_mode_slot_from_handle(physical, mode_handle);
    if (!mode) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    display = mode->display;
    if (display->device_generation != device_generation ||
        mode->output_generation != display->output_generation)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    platform = acquire_wsi_v2();
    if (!platform) return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    memset(&source, 0, sizeof(source));
    callback_result = platform->query_plane_capabilities(
        platform->context, device_generation, display->display_cookie,
        display->output_generation, mode->mode_cookie, plane_index, &source);
    release_wsi();
    if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u ||
        __atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) != 1u)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    if (callback_result != RIN_VULKAN_WSI_PLATFORM_OK)
        return callback_result == RIN_VULKAN_WSI_PLATFORM_INCOMPLETE
                   ? RIN_VK_ERROR_DEVICE_LOST
                   : map_wsi_platform_result(callback_result);
    if (!wsi_plane_capabilities_valid(&source))
        return RIN_VK_ERROR_DEVICE_LOST;
    memset(&converted, 0, sizeof(converted));
    converted.supportedAlpha = source.supported_alpha;
    converted.minSrcPosition.x = source.min_src_x;
    converted.minSrcPosition.y = source.min_src_y;
    converted.maxSrcPosition.x = source.max_src_x;
    converted.maxSrcPosition.y = source.max_src_y;
    converted.minSrcExtent.width = source.min_src_width;
    converted.minSrcExtent.height = source.min_src_height;
    converted.maxSrcExtent.width = source.max_src_width;
    converted.maxSrcExtent.height = source.max_src_height;
    converted.minDstPosition.x = source.min_dst_x;
    converted.minDstPosition.y = source.min_dst_y;
    converted.maxDstPosition.x = source.max_dst_x;
    converted.maxDstPosition.y = source.max_dst_y;
    converted.minDstExtent.width = source.min_dst_width;
    converted.minDstExtent.height = source.min_dst_height;
    converted.maxDstExtent.width = source.max_dst_width;
    converted.maxDstExtent.height = source.max_dst_height;
    *capabilities_out = converted;
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
    const RinVkDebugUtilsMessengerCreateInfoEXT* create_messenger_info = NULL;
    uint32_t index;
    uint32_t expected;
    uint32_t debug_utils_enabled = 0u;
    int result;
    (void)allocator;

    if (!instance_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *instance_out = NULL;
    if (!create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->flags != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    /* Instance layers are owned and dispatched by the Vulkan loader.  The
     * enabled layer names may remain in the ICD call; do not resolve them. */
    if (create_info->enabledExtensionCount > 1u ||
        (create_info->enabledExtensionCount != 0u &&
         !create_info->ppEnabledExtensionNames))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (create_info->enabledExtensionCount == 1u) {
        if (!create_info->ppEnabledExtensionNames[0] ||
            !name_equal(create_info->ppEnabledExtensionNames[0],
                        RIN_VK_EXT_DEBUG_UTILS_EXTENSION))
            return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
        debug_utils_enabled = 1u;
    }
    if (!collect_instance_create_chain(create_info->pNext,
                                       debug_utils_enabled,
                                       &create_messenger_info))
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
    if (!runtime) {
        debug_instance_create_report(
            create_messenger_info,
            RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            "RinVulkan.InstanceCreateFailed",
            "Vulkan instance runtime is not bound");
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
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
    slot->debug_utils_enabled = debug_utils_enabled;
    slot->runtime_handle = 0u;
    result = call_create_instance(runtime, &request, &slot->runtime_handle);
    release_runtime();
    if (result != RIN_GPU_VULKAN_OK) {
        debug_instance_create_report(
            create_messenger_info,
            RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            "RinVulkan.InstanceCreateFailed",
            "Vulkan instance creation failed");
        slot->loader_magic = 0u;
        slot->debug_utils_enabled = 0u;
        slot->runtime_handle = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return map_result(result);
    }
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *instance_out = slot;
    if (create_messenger_info &&
        (create_messenger_info->messageSeverity &
         RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u &&
        (create_messenger_info->messageType &
         RIN_VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u) {
        debug_instance_create_report(
            create_messenger_info,
            RIN_VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
            "RinVulkan.InstanceCreateSucceeded", "Vulkan instance created");
    }
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyInstance(RinVkInstance instance,
                                      const void* allocator) {
    struct RinVkInstance_T* slot = instance_slot(instance);
    RinGpuVulkanRuntimeV1* runtime;
    uint32_t expected = 1u;
    int result;
    (void)allocator;
    if (!slot || instance_has_swapchains(slot) ||
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
    wsi_cleanup_instance(slot);
    debug_utils_cleanup_instance(slot);
    memset(slot->physical_devices, 0, sizeof(slot->physical_devices));
    slot->runtime_handle = 0u;
    slot->loader_magic = 0u;
    slot->debug_utils_enabled = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static void wsi_cleanup_instance(struct RinVkInstance_T* instance) {
    uint32_t index;
    uint32_t active;
    if (!instance) return;
    do {
        active = 0u;
        sync_lock();
        for (index = 0u; index < RIN_VK_MAX_DISPLAY_SURFACES; ++index) {
            RinVkDisplaySurfaceSlot* surface = &g_display_surfaces[index];
            if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) != 0u &&
                surface->owner_instance == instance) {
                __atomic_store_n(&surface->state, 2u, __ATOMIC_RELEASE);
                if (__atomic_load_n(&surface->active_queries,
                                    __ATOMIC_ACQUIRE) == 0u)
                    clear_display_surface_slot(surface);
                else
                    ++active;
            }
        }
        sync_unlock();
        if (active != 0u) yield_thread();
    } while (active != 0u);
    sync_lock();
    for (index = 0u; index < RIN_VK_MAX_DISPLAY_MODES; ++index) {
        RinVkDisplayModeSlot* mode = &g_display_modes[index];
        if (__atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) == 1u &&
            mode->display && mode->display->owner_instance == instance)
            clear_display_mode_slot(mode);
    }
    for (index = 0u; index < RIN_VK_MAX_DISPLAYS; ++index) {
        RinVkDisplaySlot* display = &g_displays[index];
        const uint32_t state =
            __atomic_load_n(&display->state, __ATOMIC_ACQUIRE);
        if ((state == 1u || state == 3u) &&
            display->owner_instance == instance)
            clear_display_slot(display);
    }
    sync_unlock();
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

RinVkResult RIN_VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(
        RinVkPhysicalDevice physical_device, uint32_t queue_family_index,
        RinVkSurfaceKHR surface_handle, uint32_t* supported_out) {
    struct RinVkInstance_T* instance = NULL;
    struct RinVkPhysicalDevice_T* physical;
    RinVkDisplaySurfaceSlot* surface;
    RinVkDisplaySlot* display;
    RinVkDisplayModeSlot* mode;
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinVulkanWsiPlatformV3* platform;
    uint32_t index_field = (uint32_t)(surface_handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(surface_handle >> 16u);
    uint32_t plane_index;
    uint32_t queue_flags;
    uint32_t supported = 0u;
    uint64_t display_cookie;
    uint64_t mode_cookie;
    uint64_t output_generation;
    uint64_t device_generation;
    int result;
    RinVkResult surface_result = RIN_VK_SUCCESS;

    if (!supported_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *supported_out = 0u;
    physical = physical_slot(physical_device, &instance);
    if (!physical || (surface_handle >> 48u) != RIN_VK_SURFACE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_DISPLAY_SURFACES ||
        generation == 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = get_physical_profile(physical_device, &profile);
    if (result != RIN_GPU_VULKAN_OK) return map_result(result);
    if (profile.device_epoch == 0u) return RIN_VK_ERROR_DEVICE_LOST;
    if (queue_family_index >= profile.queue_family_count)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    queue_flags = profile.queue_families[queue_family_index].flags &
        (RIN_GPU_VK_QUEUE_GRAPHICS | RIN_GPU_VK_QUEUE_COMPUTE |
         RIN_GPU_VK_QUEUE_TRANSFER);
    if (profile.queue_families[queue_family_index].queue_count == 0u ||
        queue_flags == 0u)
        return RIN_VK_ERROR_DEVICE_LOST;

    surface = &g_display_surfaces[index_field - 1u];
    sync_lock();
    if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) != 1u ||
        surface->generation != generation ||
        surface->owner_instance != instance || !surface->display ||
        !surface->mode || surface->display->owner_instance != instance ||
        surface->display->owner_physical_device != physical ||
        surface->mode->display != surface->display ||
        surface->mode->display_generation != surface->display_generation ||
        surface->mode_generation != surface->mode->generation) {
        sync_unlock();
        return RIN_VK_ERROR_SURFACE_LOST_KHR;
    }
    if (__atomic_load_n(&surface->display->state, __ATOMIC_ACQUIRE) != 1u ||
        __atomic_load_n(&surface->mode->state, __ATOMIC_ACQUIRE) != 1u ||
        surface->display_generation != surface->display->generation ||
        surface->output_generation != surface->display->output_generation ||
        surface->mode->output_generation != surface->output_generation) {
        sync_unlock();
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    }
    if (surface->device_generation != profile.device_epoch ||
        surface->display->device_generation != profile.device_epoch) {
        sync_unlock();
        return RIN_VK_ERROR_DEVICE_LOST;
    }
    if (__atomic_load_n(&surface->active_queries, __ATOMIC_ACQUIRE) ==
        UINT32_MAX) {
        sync_unlock();
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    }
    __atomic_add_fetch(&surface->active_queries, 1u, __ATOMIC_ACQUIRE);
    display = surface->display;
    mode = surface->mode;
    plane_index = surface->plane_index;
    display_cookie = display->display_cookie;
    mode_cookie = mode->mode_cookie;
    output_generation = surface->output_generation;
    device_generation = surface->device_generation;
    sync_unlock();

    platform = acquire_wsi_v3();
    if (!platform) {
        result = RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    } else {
        result = platform->query_surface_support(
            platform->context, device_generation, display_cookie,
            output_generation, mode_cookie, plane_index,
            queue_family_index, queue_flags,
            profile.queue_families[queue_family_index].queue_count,
            &supported);
        release_wsi();
    }

    /* The provider call may race surface destruction, output hotplug, or
     * device reset. Retire the reference before any owner records can be
     * reclaimed, and publish only a result for the original generations. */
    sync_lock();
    if (surface->generation != generation || surface->display != display ||
        surface->mode != mode ||
        __atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 0u ||
        __atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 3u) {
        surface_result = RIN_VK_ERROR_SURFACE_LOST_KHR;
    } else if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) != 1u) {
        surface_result = RIN_VK_ERROR_SURFACE_LOST_KHR;
    } else if (__atomic_load_n(&display->state, __ATOMIC_ACQUIRE) != 1u ||
               __atomic_load_n(&mode->state, __ATOMIC_ACQUIRE) != 1u ||
               surface->output_generation != output_generation ||
               display->output_generation != output_generation ||
               mode->output_generation != output_generation ||
               surface->device_generation != device_generation ||
               display->device_generation != device_generation) {
        surface_result = RIN_VK_ERROR_OUT_OF_DATE_KHR;
    }
    __atomic_sub_fetch(&surface->active_queries, 1u, __ATOMIC_RELEASE);
    if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 2u &&
        __atomic_load_n(&surface->active_queries, __ATOMIC_ACQUIRE) == 0u)
        clear_display_surface_slot(surface);
    sync_unlock();
    if (surface_result != RIN_VK_SUCCESS) return surface_result;
    if (result != RIN_VULKAN_WSI_PLATFORM_OK)
        return map_wsi_platform_result(result);
    if (supported > 1u) return RIN_VK_ERROR_DEVICE_LOST;
    *supported_out = supported;
    return RIN_VK_SUCCESS;
}

typedef struct RinVkSurfaceQueryLease {
    RinVkDisplaySurfaceSlot* surface;
    RinVkDisplaySlot* display;
    RinVkDisplayModeSlot* mode;
    uint32_t surface_generation;
    uint32_t plane_index;
    uint32_t display_generation;
    uint32_t mode_generation;
    uint64_t display_cookie;
    uint64_t mode_cookie;
    uint64_t output_generation;
    uint64_t device_generation;
} RinVkSurfaceQueryLease;

static RinVkResult begin_surface_query(
        RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface_handle,
        RinGpuVulkanPhysicalDeviceV2* profile_out,
        RinVkSurfaceQueryLease* lease) {
    struct RinVkInstance_T* instance = NULL;
    struct RinVkPhysicalDevice_T* physical;
    uint32_t index_field = (uint32_t)(surface_handle & UINT64_C(0xffff));
    uint32_t generation = (uint32_t)(surface_handle >> 16u);
    int result;

    if (!profile_out || !lease) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(profile_out, 0, sizeof(*profile_out));
    memset(lease, 0, sizeof(*lease));
    physical = physical_slot(physical_device, &instance);
    if (!physical || (surface_handle >> 48u) != RIN_VK_SURFACE_TAG ||
        index_field == 0u || index_field > RIN_VK_MAX_DISPLAY_SURFACES ||
        generation == 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = get_physical_profile(physical_device, profile_out);
    if (result != RIN_GPU_VULKAN_OK) return map_result(result);
    if (profile_out->device_epoch == 0u)
        return RIN_VK_ERROR_DEVICE_LOST;

    lease->surface = &g_display_surfaces[index_field - 1u];
    sync_lock();
    if (__atomic_load_n(&lease->surface->state, __ATOMIC_ACQUIRE) != 1u ||
        lease->surface->generation != generation ||
        lease->surface->owner_instance != instance ||
        !lease->surface->display || !lease->surface->mode ||
        lease->surface->display->owner_instance != instance ||
        lease->surface->display->owner_physical_device != physical ||
        lease->surface->mode->display != lease->surface->display ||
        lease->surface->display_generation !=
            lease->surface->display->generation ||
        lease->surface->mode_generation != lease->surface->mode->generation ||
        lease->surface->mode->display_generation !=
            lease->surface->display_generation) {
        sync_unlock();
        return RIN_VK_ERROR_SURFACE_LOST_KHR;
    }
    if (__atomic_load_n(&lease->surface->display->state,
                        __ATOMIC_ACQUIRE) != 1u ||
        __atomic_load_n(&lease->surface->mode->state,
                        __ATOMIC_ACQUIRE) != 1u ||
        lease->surface->output_generation !=
            lease->surface->display->output_generation ||
        lease->surface->mode->output_generation !=
            lease->surface->output_generation) {
        sync_unlock();
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    }
    if (lease->surface->device_generation != profile_out->device_epoch ||
        lease->surface->display->device_generation !=
            profile_out->device_epoch)
    {
        sync_unlock();
        return RIN_VK_ERROR_DEVICE_LOST;
    }
    if (__atomic_load_n(&lease->surface->active_queries, __ATOMIC_ACQUIRE) ==
        UINT32_MAX) {
        sync_unlock();
        return RIN_VK_ERROR_TOO_MANY_OBJECTS;
    }
    __atomic_add_fetch(&lease->surface->active_queries, 1u,
                       __ATOMIC_ACQUIRE);
    lease->display = lease->surface->display;
    lease->mode = lease->surface->mode;
    lease->surface_generation = lease->surface->generation;
    lease->display_generation = lease->surface->display_generation;
    lease->mode_generation = lease->surface->mode_generation;
    lease->plane_index = lease->surface->plane_index;
    lease->display_cookie = lease->display->display_cookie;
    lease->mode_cookie = lease->mode->mode_cookie;
    lease->output_generation = lease->surface->output_generation;
    lease->device_generation = lease->surface->device_generation;
    sync_unlock();
    return RIN_VK_SUCCESS;
}

static RinVkResult end_surface_query(RinVkSurfaceQueryLease* lease) {
    RinVkResult result = RIN_VK_SUCCESS;
    RinVkDisplaySurfaceSlot* surface;
    if (!lease || !lease->surface || !lease->display || !lease->mode)
        return RIN_VK_ERROR_DEVICE_LOST;
    surface = lease->surface;
    sync_lock();
    if (surface->generation != lease->surface_generation ||
        surface->display != lease->display || surface->mode != lease->mode ||
        __atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) != 1u) {
        result = RIN_VK_ERROR_SURFACE_LOST_KHR;
    } else if (__atomic_load_n(&lease->display->state, __ATOMIC_ACQUIRE) !=
                   1u ||
               __atomic_load_n(&lease->mode->state, __ATOMIC_ACQUIRE) != 1u ||
               surface->display_generation != lease->display_generation ||
               lease->display->generation != lease->display_generation ||
               surface->mode_generation != lease->mode_generation ||
               lease->mode->generation != lease->mode_generation ||
               surface->output_generation != lease->output_generation ||
               lease->display->output_generation != lease->output_generation ||
               lease->mode->output_generation != lease->output_generation ||
               surface->device_generation != lease->device_generation ||
               lease->display->device_generation !=
                   lease->device_generation) {
        result = RIN_VK_ERROR_OUT_OF_DATE_KHR;
    }
    __atomic_sub_fetch(&surface->active_queries, 1u, __ATOMIC_RELEASE);
    if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 2u &&
        __atomic_load_n(&surface->active_queries, __ATOMIC_ACQUIRE) == 0u)
        clear_display_surface_slot(surface);
    sync_unlock();
    return result;
}

static int surface_properties_valid(
        const RinVulkanWsiSurfacePropertiesV4* properties,
        const RinGpuVulkanPhysicalDeviceV2* profile) {
    uint32_t index;
    uint32_t prior;
    const uint32_t transform_mask =
        RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if (!properties || !profile ||
        properties->struct_size != sizeof(*properties) ||
        properties->version != RIN_VULKAN_WSI_PLATFORM_V4_VERSION ||
        !wsi_zero_u32_words(properties->reserved0, 2u) ||
        !wsi_zero_u32_words(properties->reserved, 4u) ||
        properties->min_image_count == 0u ||
        properties->min_image_count > RIN_VK_MAX_SWAPCHAIN_IMAGES ||
        (properties->max_image_count != 0u &&
         properties->max_image_count < properties->min_image_count) ||
        properties->min_image_extent_width == 0u ||
        properties->min_image_extent_height == 0u ||
        properties->max_image_extent_width <
            properties->min_image_extent_width ||
        properties->max_image_extent_height <
            properties->min_image_extent_height ||
        properties->max_image_extent_width > profile->max_image_dimension_2d ||
        properties->max_image_extent_height >
            profile->max_image_dimension_2d ||
        properties->max_image_array_layers == 0u ||
        properties->supported_transforms == 0u ||
        (properties->supported_transforms & ~transform_mask) != 0u ||
        properties->current_transform !=
            RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ||
        (properties->supported_transforms & properties->current_transform) ==
            0u ||
        properties->supported_composite_alpha == 0u ||
        (properties->supported_composite_alpha &
         ~RIN_VK_COMPOSITE_ALPHA_KNOWN_BITS_KHR) != 0u ||
        properties->supported_usage_flags == 0u ||
        (properties->supported_usage_flags & ~RIN_VK_IMAGE_USAGE_KNOWN) !=
            0u ||
        properties->format_count == 0u ||
        properties->format_count > RIN_VULKAN_WSI_MAX_SURFACE_FORMATS ||
        properties->present_mode_count == 0u ||
        properties->present_mode_count >
            RIN_VULKAN_WSI_MAX_SURFACE_PRESENT_MODES)
        return 0;
    if ((properties->current_extent_width == UINT32_MAX) !=
        (properties->current_extent_height == UINT32_MAX))
        return 0;
    if (properties->current_extent_width != UINT32_MAX &&
        (properties->current_extent_width <
             properties->min_image_extent_width ||
         properties->current_extent_width >
             properties->max_image_extent_width ||
         properties->current_extent_height <
             properties->min_image_extent_height ||
         properties->current_extent_height >
             properties->max_image_extent_height))
        return 0;
    for (index = 0u; index < properties->format_count; ++index) {
        const RinVulkanWsiSurfaceFormatV4* format =
            &properties->formats[index];
        if (format->format != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
            format->color_space != RIN_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return 0;
        for (prior = 0u; prior < index; ++prior)
            if (properties->formats[prior].format == format->format &&
                properties->formats[prior].color_space == format->color_space)
                return 0;
    }
    for (index = 0u; index < properties->present_mode_count; ++index) {
        const int32_t mode = properties->present_modes[index];
        if (mode < RIN_VK_PRESENT_MODE_IMMEDIATE_KHR ||
            mode > RIN_VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            return 0;
        for (prior = 0u; prior < index; ++prior)
            if (properties->present_modes[prior] == mode) return 0;
    }
    return 1;
}

static RinVkResult query_surface_properties_v4(
        RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface_handle,
        RinVulkanWsiSurfacePropertiesV4* properties_out) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinVkSurfaceQueryLease lease;
    RinVulkanWsiPlatformV4* platform;
    RinVulkanWsiSurfacePropertiesV4 properties;
    RinVkResult result;
    int platform_result;
    if (!properties_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(properties_out, 0, sizeof(*properties_out));
    result = begin_surface_query(physical_device, surface_handle,
                                 &profile, &lease);
    if (result != RIN_VK_SUCCESS) return result;
    memset(&properties, 0, sizeof(properties));
    platform = acquire_wsi_v4();
    if (!platform) {
        platform_result = RIN_VULKAN_WSI_PLATFORM_UNSUPPORTED;
    } else {
        platform_result = platform->query_surface_properties(
            platform->context, lease.device_generation, lease.display_cookie,
            lease.output_generation, lease.mode_cookie, lease.plane_index,
            &properties);
        release_wsi();
    }
    result = end_surface_query(&lease);
    if (result != RIN_VK_SUCCESS) return result;
    if (platform_result == RIN_VULKAN_WSI_PLATFORM_INCOMPLETE)
        return RIN_VK_ERROR_DEVICE_LOST;
    if (platform_result != RIN_VULKAN_WSI_PLATFORM_OK)
        return map_wsi_platform_result(platform_result);
    if (!surface_properties_valid(&properties, &profile))
        return RIN_VK_ERROR_DEVICE_LOST;
    *properties_out = properties;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface,
        RinVkSurfaceCapabilitiesKHR* capabilities_out) {
    RinVulkanWsiSurfacePropertiesV4 properties;
    RinVkResult result;
    if (!capabilities_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(capabilities_out, 0, sizeof(*capabilities_out));
    result = query_surface_properties_v4(physical_device, surface,
                                         &properties);
    if (result != RIN_VK_SUCCESS) return result;
    capabilities_out->minImageCount = properties.min_image_count;
    capabilities_out->maxImageCount =
        properties.max_image_count == 0u ||
                properties.max_image_count > RIN_VK_MAX_SWAPCHAIN_IMAGES
            ? RIN_VK_MAX_SWAPCHAIN_IMAGES
            : properties.max_image_count;
    capabilities_out->currentExtent.width =
        properties.current_extent_width;
    capabilities_out->currentExtent.height =
        properties.current_extent_height;
    capabilities_out->minImageExtent.width =
        properties.min_image_extent_width;
    capabilities_out->minImageExtent.height =
        properties.min_image_extent_height;
    capabilities_out->maxImageExtent.width =
        properties.max_image_extent_width;
    capabilities_out->maxImageExtent.height =
        properties.max_image_extent_height;
    capabilities_out->maxImageArrayLayers =
        properties.max_image_array_layers;
    capabilities_out->supportedTransforms = properties.supported_transforms;
    capabilities_out->currentTransform = properties.current_transform;
    capabilities_out->supportedCompositeAlpha =
        properties.supported_composite_alpha;
    capabilities_out->supportedUsageFlags = properties.supported_usage_flags;
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
        RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface,
        uint32_t* format_count, RinVkSurfaceFormatKHR* formats) {
    RinVulkanWsiSurfacePropertiesV4 properties;
    uint32_t capacity;
    uint32_t count;
    uint32_t index;
    RinVkResult result;
    if (!format_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *format_count;
    *format_count = 0u;
    if (capacity != 0u && !formats)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = query_surface_properties_v4(physical_device, surface,
                                         &properties);
    if (result != RIN_VK_SUCCESS) return result;
    if (!formats) {
        *format_count = properties.format_count;
        return RIN_VK_SUCCESS;
    }
    count = capacity < properties.format_count
                ? capacity
                : properties.format_count;
    for (index = 0u; index < count; ++index) {
        formats[index].format = properties.formats[index].format;
        formats[index].colorSpace = properties.formats[index].color_space;
    }
    *format_count = count;
    return count < properties.format_count ? RIN_VK_INCOMPLETE
                                           : RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
        RinVkPhysicalDevice physical_device, RinVkSurfaceKHR surface,
        uint32_t* present_mode_count, RinVkPresentModeKHR* present_modes) {
    RinVulkanWsiSurfacePropertiesV4 properties;
    uint32_t capacity;
    uint32_t count;
    uint32_t index;
    RinVkResult result;
    if (!present_mode_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *present_mode_count;
    *present_mode_count = 0u;
    if (capacity != 0u && !present_modes)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = query_surface_properties_v4(physical_device, surface,
                                         &properties);
    if (result != RIN_VK_SUCCESS) return result;
    if (!present_modes) {
        *present_mode_count = properties.present_mode_count;
        return RIN_VK_SUCCESS;
    }
    count = capacity < properties.present_mode_count
                ? capacity
                : properties.present_mode_count;
    for (index = 0u; index < count; ++index)
        present_modes[index] = properties.present_modes[index];
    *present_mode_count = count;
    return count < properties.present_mode_count ? RIN_VK_INCOMPLETE
                                                 : RIN_VK_SUCCESS;
}

static void clear_swapchain_slot(RinVkSwapchainSlot* slot) {
    uint32_t generation;
    if (!slot) return;
    generation = slot->generation;
    memset(slot, 0, sizeof(*slot));
    slot->generation = generation;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

static int swapchain_surface_current(const RinVkSwapchainSlot* swapchain) {
    const RinVkDisplaySurfaceSlot* surface;
    if (!swapchain || !swapchain->surface) return 0;
    surface = swapchain->surface;
    return __atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 1u &&
           surface->generation == swapchain->surface_generation &&
           surface->display && surface->mode &&
           __atomic_load_n(&surface->display->state, __ATOMIC_ACQUIRE) == 1u &&
           __atomic_load_n(&surface->mode->state, __ATOMIC_ACQUIRE) == 1u &&
           surface->display_generation == surface->display->generation &&
           surface->mode_generation == surface->mode->generation &&
           surface->output_generation == swapchain->output_generation &&
           surface->device_generation == swapchain->device_generation &&
           surface->display->output_generation == swapchain->output_generation &&
           surface->mode->output_generation == swapchain->output_generation &&
           surface->display->device_generation == swapchain->device_generation;
}

static int surface_supports_format(
        const RinVulkanWsiSurfacePropertiesV4* properties,
        int32_t format, int32_t color_space) {
    uint32_t index;
    for (index = 0u; index < properties->format_count; ++index)
        if (properties->formats[index].format == format &&
            properties->formats[index].color_space == color_space)
            return 1;
    return 0;
}

static int surface_supports_present_mode(
        const RinVulkanWsiSurfacePropertiesV4* properties, uint32_t mode) {
    uint32_t index;
    for (index = 0u; index < properties->present_mode_count; ++index)
        if (properties->present_modes[index] == (int32_t)mode) return 1;
    return 0;
}

static void device_wsi_lock(struct RinVkDevice_T* device) {
    while (__atomic_exchange_n(&device->wsi_lock, 1u, __ATOMIC_ACQUIRE) != 0u)
        yield_thread();
}

static void device_wsi_unlock(struct RinVkDevice_T* device) {
    __atomic_store_n(&device->wsi_lock, 0u, __ATOMIC_RELEASE);
}

static RinVkSwapchainSlot* swapchain_for_presentation(
        struct RinVkDevice_T* device, uint32_t presentation_display_id) {
    uint32_t index;
    if (!device || presentation_display_id == 0u) return NULL;
    for (index = 0u; index < RIN_VK_MAX_SWAPCHAINS; ++index) {
        RinVkSwapchainSlot* swapchain = &g_swapchains[index];
        if (__atomic_load_n(&swapchain->state, __ATOMIC_ACQUIRE) != 0u &&
            swapchain->owner == device &&
            swapchain->presentation_display_id == presentation_display_id)
            return swapchain;
    }
    return NULL;
}

static RinVkPendingPresent* reserve_pending_present(
        struct RinVkDevice_T* device) {
    uint32_t index;
    for (index = 0u; index < RIN_VK_MAX_WSI_PRESENTS; ++index) {
        RinVkPendingPresent* pending = &device->pending_presents[index];
        if (pending->state == 0u) {
            memset(pending, 0, sizeof(*pending));
            pending->state = RIN_VK_PRESENT_RECORD_RESERVED;
            return pending;
        }
    }
    return NULL;
}

static RinVkResult poll_pending_present(
        struct RinVkDevice_T* device, RinVkPendingPresent* pending) {
    RinVulkanWsiPresentStatusV1 status;
    RinVulkanWsiPlatformV4* platform;
    RinVkSwapchainSlot* swapchain;
    RinVkImageSlot* image;
    int result;
    if (!device || !pending ||
        (pending->state != RIN_VK_PRESENT_RECORD_ACTIVE &&
         pending->state != RIN_VK_PRESENT_RECORD_UNTRACKED) ||
        pending->platform_token == 0u)
        return RIN_VK_ERROR_DEVICE_LOST;
    memset(&status, 0, sizeof(status));
    status.struct_size = sizeof(status);
    status.version = RIN_VULKAN_WSI_PLATFORM_VERSION;
    platform = acquire_wsi_v4();
    if (!platform) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = platform->poll_present(platform->context,
                                    pending->display_cookie,
                                    pending->platform_token, &status);
    release_wsi();
    if (result == RIN_VULKAN_WSI_PLATFORM_NOT_READY)
        return RIN_VK_NOT_READY;
    if (result != RIN_VULKAN_WSI_PLATFORM_OK)
        return map_wsi_platform_result(result);
    if (status.struct_size != sizeof(status) ||
        status.version != RIN_VULKAN_WSI_PLATFORM_VERSION ||
        status.reserved0 != 0u || !wsi_zero_words(status.reserved, 2u))
        return RIN_VK_ERROR_DEVICE_LOST;
    if (status.output_generation != pending->output_generation ||
        status.device_generation != pending->device_generation)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    if (status.state == RIN_VULKAN_WSI_PRESENT_PENDING)
        return RIN_VK_NOT_READY;
    if (status.state != RIN_VULKAN_WSI_PRESENT_COMPLETE)
        return RIN_VK_ERROR_DEVICE_LOST;

    swapchain = swapchain_for_presentation(
        device, pending->presentation_display_id);
    if (!swapchain || pending->image_index >= swapchain->image_count ||
        swapchain->output_generation != pending->output_generation ||
        swapchain->device_generation != pending->device_generation ||
        swapchain->acquired_frame_ids[pending->image_index] !=
            pending->frame_id ||
        swapchain->present_tokens[pending->image_index] !=
            pending->platform_token ||
        (swapchain->image_states[pending->image_index] !=
             RIN_VK_SWAPCHAIN_IMAGE_PRESENT_PENDING &&
         swapchain->image_states[pending->image_index] !=
             RIN_VK_SWAPCHAIN_IMAGE_PRESENT_UNTRACKED))
        return RIN_VK_ERROR_DEVICE_LOST;
    image = image_slot((RinVkDevice)device,
                       swapchain->images[pending->image_index]);
    if (!image || !image->memory ||
        image->memory->product_allocation != pending->image_token ||
        image->memory_generation != image->memory->generation)
        return RIN_VK_ERROR_DEVICE_LOST;

    swapchain->image_states[pending->image_index] =
        RIN_VK_SWAPCHAIN_IMAGE_AVAILABLE;
    swapchain->acquired_frame_ids[pending->image_index] = 0u;
    swapchain->present_tokens[pending->image_index] = 0u;
    memset(pending, 0, sizeof(*pending));
    return RIN_VK_SUCCESS;
}

static RinVkResult poll_device_presentations_locked(
        struct RinVkDevice_T* device, uint32_t queue_index) {
    uint32_t index;
    RinVkResult result = RIN_VK_SUCCESS;
    for (index = 0u; index < RIN_VK_MAX_WSI_PRESENTS; ++index) {
        RinVkPendingPresent* pending = &device->pending_presents[index];
        RinVkResult poll_result;
        if (pending->state != RIN_VK_PRESENT_RECORD_ACTIVE &&
            pending->state != RIN_VK_PRESENT_RECORD_UNTRACKED)
            continue;
        if (queue_index != UINT32_MAX &&
            pending->queue_index != queue_index)
            continue;
        poll_result = poll_pending_present(device, pending);
        if (poll_result == RIN_VK_NOT_READY)
            result = RIN_VK_NOT_READY;
        else if (poll_result != RIN_VK_SUCCESS)
            return poll_result;
    }
    return result;
}

static RinVkResult poll_device_presentations(
        struct RinVkDevice_T* device, uint32_t queue_index) {
    RinVkResult result;
    if (!device) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    device_wsi_lock(device);
    result = poll_device_presentations_locked(device, queue_index);
    device_wsi_unlock(device);
    return result;
}

static RinVkResult poll_swapchain_presentations_locked(
        struct RinVkDevice_T* device, uint32_t presentation_display_id) {
    uint32_t index;
    RinVkResult result = RIN_VK_SUCCESS;
    for (index = 0u; index < RIN_VK_MAX_WSI_PRESENTS; ++index) {
        RinVkPendingPresent* pending = &device->pending_presents[index];
        RinVkResult poll_result;
        if ((pending->state != RIN_VK_PRESENT_RECORD_ACTIVE &&
             pending->state != RIN_VK_PRESENT_RECORD_UNTRACKED) ||
            pending->presentation_display_id != presentation_display_id)
            continue;
        poll_result = poll_pending_present(device, pending);
        if (poll_result == RIN_VK_NOT_READY)
            result = RIN_VK_NOT_READY;
        else if (poll_result != RIN_VK_SUCCESS)
            return poll_result;
    }
    return result;
}

static RinVkResult wait_present_semaphores(
        struct RinVkDevice_T* device, uint32_t semaphore_count,
        const RinVkSemaphore* semaphores) {
    uint32_t index;
    if (!device || semaphore_count > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        (semaphore_count != 0u && !semaphores))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < semaphore_count; ++index) {
        uint32_t prior;
        for (prior = 0u; prior < index; ++prior)
            if (semaphores[prior] == semaphores[index])
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    for (;;) {
        uint32_t ready_count = 0u;
        RinVkResult result = maintain_device_submissions(device);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
            return result;
        if (result == RIN_VK_NOT_READY) {
            yield_thread();
            continue;
        }
        sync_lock();
        for (index = 0u; index < semaphore_count; ++index) {
            RinVkSemaphoreSlot* semaphore = semaphore_slot(
                (RinVkDevice)device, semaphores[index]);
            if (!semaphore ||
                semaphore->type != RIN_VK_SEMAPHORE_TYPE_BINARY) {
                sync_unlock();
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
            }
            if (__atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) != 0u &&
                __atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) == 0u)
                ++ready_count;
        }
        if (ready_count == semaphore_count) {
            for (index = 0u; index < semaphore_count; ++index) {
                RinVkSemaphoreSlot* semaphore = semaphore_slot(
                    (RinVkDevice)device, semaphores[index]);
                __atomic_store_n(&semaphore->signaled, 0u, __ATOMIC_RELEASE);
            }
            sync_unlock();
            return RIN_VK_SUCCESS;
        }
        sync_unlock();
        yield_thread();
    }
}

static int cancel_swapchain_presentations_locked(
        struct RinVkDevice_T* device, uint32_t presentation_display_id) {
    uint32_t index;
    RinVkSwapchainSlot* swapchain;
    if (!device || presentation_display_id == 0u) return 0;
    swapchain = swapchain_for_presentation(device, presentation_display_id);
    if (!swapchain) return 0;
    for (index = 0u; index < RIN_VK_MAX_WSI_PRESENTS; ++index) {
        RinVkPendingPresent* pending = &device->pending_presents[index];
        RinVulkanWsiPlatformV4* platform;
        RinVkImageSlot* image;
        int result;
        if (pending->state == 0u ||
            pending->presentation_display_id != presentation_display_id)
            continue;
        if ((pending->state != RIN_VK_PRESENT_RECORD_ACTIVE &&
             pending->state != RIN_VK_PRESENT_RECORD_UNTRACKED) ||
            pending->platform_token == 0u ||
            pending->image_index >= swapchain->image_count ||
            swapchain->acquired_frame_ids[pending->image_index] !=
                pending->frame_id ||
            swapchain->present_tokens[pending->image_index] !=
                pending->platform_token)
            return 0;
        image = image_slot((RinVkDevice)device,
                           swapchain->images[pending->image_index]);
        if (!image || !image->memory ||
            image->memory->product_allocation != pending->image_token ||
            image->memory_generation != image->memory->generation)
            return 0;
        platform = acquire_wsi_v4();
        if (!platform) return 0;
        result = platform->cancel_present(platform->context,
                                          pending->display_cookie,
                                          pending->platform_token);
        release_wsi();
        if (result != RIN_VULKAN_WSI_PLATFORM_OK) return 0;
        swapchain->image_states[pending->image_index] =
            RIN_VK_SWAPCHAIN_IMAGE_AVAILABLE;
        swapchain->acquired_frame_ids[pending->image_index] = 0u;
        swapchain->present_tokens[pending->image_index] = 0u;
        memset(pending, 0, sizeof(*pending));
    }
    return 1;
}

static RinVkResult wait_queue_submissions_idle(
        struct RinVkQueue_T* queue) {
    for (;;) {
        RinVkResult result = maintain_device_submissions(queue->device);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
            return result;
        if (result == RIN_VK_SUCCESS &&
            !queue_submission_slots_active(queue->device,
                                          queue->queue_index))
            return RIN_VK_SUCCESS;
        yield_thread();
    }
}

static RinVkResult swapchain_create_image(
        struct RinVkDevice_T* device, RinVkSwapchainSlot* swapchain,
        uint32_t image_index) {
    RinVkImageCreateInfo image_info;
    RinVkMemoryRequirements requirements;
    RinVkMemoryAllocateInfo allocation_info;
    RinVkImage image = 0u;
    RinVkDeviceMemory memory = 0u;
    uint32_t memory_type = UINT32_MAX;
    uint32_t index;
    RinVkResult result;

    memset(&image_info, 0, sizeof(image_info));
    image_info.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = RIN_VK_IMAGE_TYPE_2D;
    image_info.format = swapchain->image_format;
    image_info.extent.width = swapchain->image_extent.width;
    image_info.extent.height = swapchain->image_extent.height;
    image_info.extent.depth = 1u;
    image_info.mipLevels = 1u;
    image_info.arrayLayers = 1u;
    image_info.samples = RIN_VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = RIN_VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = swapchain->image_usage;
    image_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateImage((RinVkDevice)device, &image_info, NULL, &image);
    if (result != RIN_VK_SUCCESS) return result;
    swapchain->images[image_index] = image;
    ++swapchain->image_count;
    vkGetImageMemoryRequirements((RinVkDevice)device, image, &requirements);
    for (index = 0u; index < device->physical_profile.memory_type_count;
         ++index) {
        if ((requirements.memoryTypeBits & (UINT32_C(1) << index)) != 0u &&
            (device->physical_profile.memory_types[index].property_flags &
             RIN_GPU_VK_MEMORY_DEVICE_LOCAL) != 0u) {
            memory_type = index;
            break;
        }
    }
    if (memory_type == UINT32_MAX) {
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }
    memset(&allocation_info, 0, sizeof(allocation_info));
    allocation_info.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = requirements.size;
    allocation_info.memoryTypeIndex = memory_type;
    result = vkAllocateMemory((RinVkDevice)device, &allocation_info, NULL,
                              &memory);
    if (result != RIN_VK_SUCCESS) return result;
    swapchain->memories[image_index] = memory;
    result = vkBindImageMemory((RinVkDevice)device, image, memory, 0u);
    if (result != RIN_VK_SUCCESS) return result;
    {
        RinVkImageSlot* record = image_slot((RinVkDevice)device, image);
        if (!record)
            return RIN_VK_ERROR_DEVICE_LOST;
        record->swapchain_owner = swapchain;
    }
    return RIN_VK_SUCCESS;
}

static int swapchain_destroy_images(RinVkDevice device,
                                    RinVkSwapchainSlot* swapchain) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint32_t index;
    if (!owner || device_submission_slots_active(owner)) return 0;
    for (index = 0u; index < swapchain->image_count; ++index) {
        RinVkImageSlot* image = image_slot(device, swapchain->images[index]);
        if (image && image_has_views(image)) return 0;
    }
    if (swapchain->presentation_display_id != 0u) {
        device_wsi_lock(owner);
        if (!cancel_swapchain_presentations_locked(
                owner, swapchain->presentation_display_id)) {
            device_wsi_unlock(owner);
            return 0;
        }
        device_wsi_unlock(owner);
    }
    for (index = 0u; index < swapchain->image_count; ++index) {
        if (swapchain->image_states[index] ==
                RIN_VK_SWAPCHAIN_IMAGE_PRESENT_PENDING ||
            swapchain->image_states[index] ==
                RIN_VK_SWAPCHAIN_IMAGE_PRESENT_UNTRACKED)
            return 0;
    }
    for (index = 0u; index < swapchain->image_count; ++index) {
        RinVkImageSlot* image = image_slot(device, swapchain->images[index]);
        if (image) {
            image->swapchain_owner = NULL;
            vkDestroyImage(device, swapchain->images[index], NULL);
            if (image_slot(device, swapchain->images[index])) return 0;
            swapchain->images[index] = 0u;
        }
        if (swapchain->memories[index] != 0u) {
            vkFreeMemory(device, swapchain->memories[index], NULL);
            if (memory_slot(device, swapchain->memories[index])) return 0;
            swapchain->memories[index] = 0u;
        }
    }
    swapchain->image_count = 0u;
    return 1;
}

RinVkResult RIN_VKAPI_CALL vkCreateSwapchainKHR(
        RinVkDevice device_handle,
        const RinVkSwapchainCreateInfoKHR* create_info,
        const void* allocator, RinVkSwapchainKHR* swapchain_out) {
    struct RinVkDevice_T* device = device_slot(device_handle);
    RinVulkanWsiSurfacePropertiesV4 properties;
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinVkSurfaceQueryLease lease;
    RinVkSwapchainSlot* swapchain = NULL;
    RinVkSwapchainSlot* old_swapchain = NULL;
    uint32_t slot_index = 0u;
    uint32_t index;
    uint32_t supported_queue = 0u;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;
    int surface_counted = 0;
    (void)allocator;

    if (!swapchain_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *swapchain_out = 0u;
    memset(&lease, 0, sizeof(lease));
    if (!device || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR ||
        create_info->pNext || create_info->flags != 0u ||
        create_info->minImageCount == 0u ||
        create_info->minImageCount > RIN_VK_MAX_SWAPCHAIN_IMAGES ||
        create_info->imageArrayLayers != 1u || create_info->imageUsage == 0u ||
        (create_info->imageUsage & ~RIN_VK_IMAGE_USAGE_KNOWN) != 0u ||
        create_info->imageSharingMode != RIN_VK_SHARING_MODE_EXCLUSIVE ||
        create_info->queueFamilyIndexCount != 0u ||
        create_info->pQueueFamilyIndices || create_info->clipped > 1u ||
        create_info->presentMode > RIN_VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;

    result = begin_surface_query((RinVkPhysicalDevice)device->physical_device,
                                 create_info->surface, &profile, &lease);
    if (result != RIN_VK_SUCCESS) return result;
    if (lease.device_generation != device->physical_profile.device_epoch ||
        lease.device_generation != device->plan.device_epoch) {
        result = RIN_VK_ERROR_DEVICE_LOST;
        goto done;
    }
    result = query_surface_properties_v4(
        (RinVkPhysicalDevice)device->physical_device, create_info->surface,
        &properties);
    if (result != RIN_VK_SUCCESS) goto done;
    if (create_info->minImageCount < properties.min_image_count ||
        (properties.max_image_count != 0u &&
         create_info->minImageCount > properties.max_image_count) ||
        (properties.supported_usage_flags & create_info->imageUsage) !=
            create_info->imageUsage ||
        create_info->imageExtent.width == 0u ||
        create_info->imageExtent.height == 0u ||
        create_info->imageExtent.width < properties.min_image_extent_width ||
        create_info->imageExtent.width > properties.max_image_extent_width ||
        create_info->imageExtent.height < properties.min_image_extent_height ||
        create_info->imageExtent.height > properties.max_image_extent_height ||
        (properties.current_extent_width != UINT32_MAX &&
         (create_info->imageExtent.width != properties.current_extent_width ||
          create_info->imageExtent.height != properties.current_extent_height)) ||
        create_info->preTransform != properties.current_transform ||
        (properties.supported_transforms & create_info->preTransform) == 0u ||
        create_info->preTransform != RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ||
        create_info->compositeAlpha == 0u ||
        (create_info->compositeAlpha &
         (create_info->compositeAlpha - 1u)) != 0u ||
        (properties.supported_composite_alpha & create_info->compositeAlpha) ==
            0u ||
        !surface_supports_format(&properties, create_info->imageFormat,
                                 create_info->imageColorSpace) ||
        !surface_supports_present_mode(&properties,
                                       create_info->presentMode)) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    for (index = 0u; index < device->queue_count; ++index) {
        struct RinVkQueue_T* queue = &device->queues[index];
        uint32_t supported = 0u;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(
            (RinVkPhysicalDevice)device->physical_device,
            queue->queue_family_index, create_info->surface, &supported);
        if (result != RIN_VK_SUCCESS) goto done;
        if (supported) supported_queue = 1u;
    }
    if (!supported_queue) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    if (create_info->oldSwapchain != 0u) {
        old_swapchain = swapchain_slot(device_handle,
                                       create_info->oldSwapchain);
        if (!old_swapchain || old_swapchain->surface != lease.surface ||
            old_swapchain->surface_generation != lease.surface_generation) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
    }
    swapchain = reserve_swapchain_slot(&slot_index);
    if (!swapchain) {
        result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        goto done;
    }
    swapchain->owner = device;
    swapchain->surface = lease.surface;
    swapchain->surface_handle = create_info->surface;
    swapchain->surface_generation = lease.surface_generation;
    swapchain->output_generation = lease.output_generation;
    swapchain->device_generation = lease.device_generation;
    swapchain->mode_cookie = lease.mode_cookie;
    swapchain->present_mode = create_info->presentMode;
    swapchain->image_format = create_info->imageFormat;
    swapchain->image_color_space = create_info->imageColorSpace;
    swapchain->image_extent = create_info->imageExtent;
    swapchain->image_usage = create_info->imageUsage;
    swapchain->presentation_display_id = slot_index + 1u;
    swapchain->next_frame_id = 1u;
    for (index = 0u; index < create_info->minImageCount; ++index) {
        result = swapchain_create_image(device, swapchain, index);
        if (result != RIN_VK_SUCCESS) goto rollback;
    }
    sync_lock();
    if (__atomic_load_n(&lease.surface->state, __ATOMIC_ACQUIRE) != 1u ||
        lease.surface->generation != lease.surface_generation ||
        lease.surface->swapchain_count == UINT32_MAX) {
        sync_unlock();
        result = RIN_VK_ERROR_OUT_OF_DATE_KHR;
        goto rollback;
    }
    __atomic_add_fetch(&lease.surface->swapchain_count, 1u, __ATOMIC_RELEASE);
    surface_counted = 1;
    sync_unlock();
    result = end_surface_query(&lease);
    memset(&lease, 0, sizeof(lease));
    if (result != RIN_VK_SUCCESS) goto rollback;
    __atomic_store_n(&swapchain->state, 1u, __ATOMIC_RELEASE);
    if (old_swapchain)
        __atomic_store_n(&old_swapchain->retired, 1u, __ATOMIC_RELEASE);
    *swapchain_out = resource_handle(RIN_VK_SWAPCHAIN_TAG, slot_index,
                                     swapchain->generation);
    return RIN_VK_SUCCESS;

rollback:
    if (surface_counted && swapchain && swapchain->surface) {
        RinVkDisplaySurfaceSlot* surface = swapchain->surface;
        __atomic_sub_fetch(&surface->swapchain_count, 1u, __ATOMIC_RELEASE);
        surface_counted = 0;
        if (__atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 2u &&
            __atomic_load_n(&surface->active_queries, __ATOMIC_ACQUIRE) == 0u)
            clear_display_surface_slot(surface);
    }
    if (swapchain) {
        /* Failed destruction leaves backing allocations tracked by the device;
         * device cleanup retries them through the product allocation owner. */
        (void)swapchain_destroy_images(device_handle, swapchain);
        clear_swapchain_slot(swapchain);
    }
done:
    if (lease.surface) {
        const RinVkResult lease_result = end_surface_query(&lease);
        memset(&lease, 0, sizeof(lease));
        if (result == RIN_VK_SUCCESS) result = lease_result;
    }
    if (surface_counted && swapchain) {
        __atomic_sub_fetch(&swapchain->surface->swapchain_count, 1u,
                           __ATOMIC_RELEASE);
        if (__atomic_load_n(&swapchain->surface->state, __ATOMIC_ACQUIRE) ==
                2u &&
            __atomic_load_n(&swapchain->surface->active_queries,
                            __ATOMIC_ACQUIRE) == 0u)
            clear_display_surface_slot(swapchain->surface);
    }
    return result;
}

void RIN_VKAPI_CALL vkDestroySwapchainKHR(
        RinVkDevice device, RinVkSwapchainKHR swapchain_handle,
        const void* allocator) {
    RinVkSwapchainSlot* swapchain = swapchain_slot(device, swapchain_handle);
    RinVkDisplaySurfaceSlot* surface;
    (void)allocator;
    if (!swapchain || !swapchain_destroy_images(device, swapchain)) return;
    surface = swapchain->surface;
    clear_swapchain_slot(swapchain);
    if (surface &&
        __atomic_sub_fetch(&surface->swapchain_count, 1u, __ATOMIC_RELEASE) ==
            0u &&
        __atomic_load_n(&surface->state, __ATOMIC_ACQUIRE) == 2u &&
        __atomic_load_n(&surface->active_queries, __ATOMIC_ACQUIRE) == 0u)
        clear_display_surface_slot(surface);
}

RinVkResult RIN_VKAPI_CALL vkGetSwapchainImagesKHR(
        RinVkDevice device, RinVkSwapchainKHR swapchain_handle,
        uint32_t* image_count, RinVkImage* images) {
    RinVkSwapchainSlot* swapchain;
    uint32_t capacity;
    uint32_t count;
    uint32_t index;
    if (!image_count) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    capacity = *image_count;
    *image_count = 0u;
    if (capacity != 0u && !images)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    swapchain = swapchain_slot(device, swapchain_handle);
    if (!swapchain) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (!swapchain_surface_current(swapchain))
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    if (!images) {
        *image_count = swapchain->image_count;
        return RIN_VK_SUCCESS;
    }
    count = capacity < swapchain->image_count ? capacity : swapchain->image_count;
    for (index = 0u; index < count; ++index)
        images[index] = swapchain->images[index];
    *image_count = count;
    return count < swapchain->image_count ? RIN_VK_INCOMPLETE : RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkAcquireNextImageKHR(
        RinVkDevice device_handle, RinVkSwapchainKHR swapchain_handle,
        uint64_t timeout, RinVkSemaphore semaphore_handle,
        RinVkFence fence_handle, uint32_t* image_index_out) {
    struct RinVkDevice_T* device = device_slot(device_handle);
    RinVkSwapchainSlot* swapchain;
    uint64_t start_ns = 0u;
    if (!image_index_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *image_index_out = UINT32_MAX;
    if (!device || (semaphore_handle == 0u && fence_handle == 0u))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    swapchain = swapchain_slot(device_handle, swapchain_handle);
    if (!swapchain || swapchain->owner != device ||
        swapchain->presentation_display_id == 0u ||
        __atomic_load_n(&swapchain->retired, __ATOMIC_ACQUIRE) != 0u)
        return RIN_VK_ERROR_OUT_OF_DATE_KHR;
    if (swapchain->image_count == 0u)
        return RIN_VK_ERROR_DEVICE_LOST;
    if (timeout != 0u && timeout != UINT64_MAX &&
        !rinvulkan_platform_monotonic_time_ns(&start_ns))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;

    for (;;) {
        RinVkSurfaceCapabilitiesKHR capabilities;
        RinVkResult result;
        uint32_t image_index = UINT32_MAX;
        uint64_t frame_id = 0u;
        uint32_t offset;

        if (__atomic_load_n(&swapchain->retired, __ATOMIC_ACQUIRE) != 0u ||
            !swapchain_surface_current(swapchain))
            return RIN_VK_ERROR_OUT_OF_DATE_KHR;
        result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            (RinVkPhysicalDevice)device->physical_device,
            swapchain->surface_handle, &capabilities);
        if (result != RIN_VK_SUCCESS) return result;

        sync_lock();
        if (semaphore_handle != 0u) {
            RinVkSemaphoreSlot* semaphore = semaphore_slot(
                device_handle, semaphore_handle);
            if (!semaphore ||
                semaphore->type != RIN_VK_SEMAPHORE_TYPE_BINARY ||
                __atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) != 0u ||
                __atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u) {
                sync_unlock();
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
            }
        }
        if (fence_handle != 0u) {
            RinVkFenceSlot* fence = fence_slot(device_handle, fence_handle);
            if (!fence ||
                __atomic_load_n(&fence->signaled, __ATOMIC_ACQUIRE) != 0u ||
                __atomic_load_n(&fence->pending, __ATOMIC_ACQUIRE) != 0u) {
                sync_unlock();
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
            }
        }
        sync_unlock();

        device_wsi_lock(device);
        result = poll_swapchain_presentations_locked(
            device, swapchain->presentation_display_id);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY) {
            device_wsi_unlock(device);
            return result;
        }
        if (__atomic_load_n(&swapchain->retired, __ATOMIC_ACQUIRE) != 0u ||
            !swapchain_surface_current(swapchain)) {
            device_wsi_unlock(device);
            return RIN_VK_ERROR_OUT_OF_DATE_KHR;
        }
        for (offset = 0u; offset < swapchain->image_count; ++offset) {
            const uint32_t candidate =
                (swapchain->next_image_index + offset) %
                swapchain->image_count;
            if (swapchain->image_states[candidate] ==
                RIN_VK_SWAPCHAIN_IMAGE_AVAILABLE) {
                image_index = candidate;
                break;
            }
        }
        if (image_index != UINT32_MAX) {
            RinVkImageSlot* image = image_slot(
                device_handle, swapchain->images[image_index]);
            if (!image || !image->memory ||
                image->memory->product_allocation == 0u ||
                image->memory_generation != image->memory->generation ||
                image->owner != device ||
                image->swapchain_owner != swapchain ||
                swapchain->next_frame_id == 0u) {
                device_wsi_unlock(device);
                return RIN_VK_ERROR_DEVICE_LOST;
            }
            frame_id = swapchain->next_frame_id++;
            swapchain->next_image_index =
                (image_index + 1u) % swapchain->image_count;
            swapchain->image_states[image_index] =
                RIN_VK_SWAPCHAIN_IMAGE_ACQUIRED;
            swapchain->acquired_frame_ids[image_index] = frame_id;
            swapchain->present_tokens[image_index] = 0u;
            device_wsi_unlock(device);

            sync_lock();
            if (semaphore_handle != 0u) {
                RinVkSemaphoreSlot* semaphore = semaphore_slot(
                    device_handle, semaphore_handle);
                if (!semaphore ||
                    semaphore->type != RIN_VK_SEMAPHORE_TYPE_BINARY ||
                    __atomic_load_n(&semaphore->signaled,
                                    __ATOMIC_ACQUIRE) != 0u ||
                    __atomic_load_n(&semaphore->pending,
                                    __ATOMIC_ACQUIRE) != 0u) {
                    sync_unlock();
                    device_wsi_lock(device);
                    if (swapchain->image_states[image_index] ==
                            RIN_VK_SWAPCHAIN_IMAGE_ACQUIRED &&
                        swapchain->acquired_frame_ids[image_index] == frame_id) {
                        swapchain->image_states[image_index] =
                            RIN_VK_SWAPCHAIN_IMAGE_AVAILABLE;
                        swapchain->acquired_frame_ids[image_index] = 0u;
                    }
                    device_wsi_unlock(device);
                    return RIN_VK_ERROR_INITIALIZATION_FAILED;
                }
                __atomic_store_n(&semaphore->signaled, 1u, __ATOMIC_RELEASE);
            }
            if (fence_handle != 0u) {
                RinVkFenceSlot* fence = fence_slot(device_handle, fence_handle);
                if (!fence ||
                    __atomic_load_n(&fence->signaled, __ATOMIC_ACQUIRE) != 0u ||
                    __atomic_load_n(&fence->pending, __ATOMIC_ACQUIRE) != 0u) {
                    sync_unlock();
                    device_wsi_lock(device);
                    if (swapchain->image_states[image_index] ==
                            RIN_VK_SWAPCHAIN_IMAGE_ACQUIRED &&
                        swapchain->acquired_frame_ids[image_index] == frame_id) {
                        swapchain->image_states[image_index] =
                            RIN_VK_SWAPCHAIN_IMAGE_AVAILABLE;
                        swapchain->acquired_frame_ids[image_index] = 0u;
                    }
                    device_wsi_unlock(device);
                    return RIN_VK_ERROR_INITIALIZATION_FAILED;
                }
                __atomic_store_n(&fence->signaled, 1u, __ATOMIC_RELEASE);
            }
            sync_unlock();
            *image_index_out = image_index;
            return RIN_VK_SUCCESS;
        }
        device_wsi_unlock(device);
        if (timeout == 0u) return RIN_VK_NOT_READY;
        if (timeout != UINT64_MAX) {
            const int elapsed = wait_timeout_elapsed(start_ns, timeout);
            if (elapsed < 0) return RIN_VK_ERROR_INITIALIZATION_FAILED;
            if (elapsed != 0) return RIN_VK_TIMEOUT;
        }
        yield_thread();
    }
}

RinVkResult RIN_VKAPI_CALL vkQueuePresentKHR(
        RinVkQueue queue_handle, const RinVkPresentInfoKHR* present_info) {
    struct RinVkQueue_T* queue = queue_slot(queue_handle);
    struct RinVkDevice_T* device;
    RinVkSwapchainSlot* swapchains[RIN_VK_MAX_SWAPCHAINS];
    uint32_t index;
    RinVkResult overall = RIN_VK_SUCCESS;
    RinVkResult validation_result = RIN_VK_SUCCESS;
    if (!queue || !present_info ||
        present_info->sType != RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR ||
        present_info->pNext ||
        present_info->waitSemaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        (present_info->waitSemaphoreCount != 0u &&
         !present_info->pWaitSemaphores) ||
        present_info->swapchainCount == 0u ||
        present_info->swapchainCount > RIN_VK_MAX_SWAPCHAINS ||
        !present_info->pSwapchains || !present_info->pImageIndices)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    device = queue->device;
    if (present_info->pResults)
        for (index = 0u; index < present_info->swapchainCount; ++index)
            present_info->pResults[index] = RIN_VK_SUCCESS;

    for (index = 0u; index < present_info->swapchainCount; ++index) {
        uint32_t prior;
        uint32_t supported = 0u;
        uint32_t image_index = present_info->pImageIndices[index];
        swapchains[index] = swapchain_slot(
            (RinVkDevice)device, present_info->pSwapchains[index]);
        if (!swapchains[index] || swapchains[index]->owner != device) {
            validation_result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto validation_failed;
        }
        for (prior = 0u; prior < index; ++prior)
            if (present_info->pSwapchains[prior] ==
                present_info->pSwapchains[index]) {
                validation_result = RIN_VK_ERROR_INITIALIZATION_FAILED;
                goto validation_failed;
            }
        if (__atomic_load_n(&swapchains[index]->retired,
                            __ATOMIC_ACQUIRE) != 0u ||
            !swapchain_surface_current(swapchains[index])) {
            validation_result = RIN_VK_ERROR_OUT_OF_DATE_KHR;
            goto validation_failed;
        }
        if (image_index >= swapchains[index]->image_count ||
            swapchains[index]->image_states[image_index] !=
                RIN_VK_SWAPCHAIN_IMAGE_ACQUIRED ||
            swapchains[index]->acquired_frame_ids[image_index] == 0u ||
            swapchains[index]->presentation_display_id == 0u) {
            validation_result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto validation_failed;
        }
        validation_result = vkGetPhysicalDeviceSurfaceSupportKHR(
            (RinVkPhysicalDevice)device->physical_device,
            queue->queue_family_index, swapchains[index]->surface_handle,
            &supported);
        if (validation_result != RIN_VK_SUCCESS) goto validation_failed;
        if (supported == 0u) {
            validation_result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
            goto validation_failed;
        }
        {
            RinVkSurfaceCapabilitiesKHR capabilities;
            validation_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                (RinVkPhysicalDevice)device->physical_device,
                swapchains[index]->surface_handle, &capabilities);
        }
        if (validation_result != RIN_VK_SUCCESS) goto validation_failed;
    }

    {
        RinVkResult result = wait_queue_submissions_idle(queue);
        if (result != RIN_VK_SUCCESS) {
            overall = result;
            goto validation_failed;
        }
    }
    {
        RinVkResult result = wait_present_semaphores(
            device, present_info->waitSemaphoreCount,
            present_info->pWaitSemaphores);
        if (result != RIN_VK_SUCCESS) {
            overall = result;
            goto validation_failed;
        }
    }

    for (index = 0u; index < present_info->swapchainCount; ++index) {
        RinVkSwapchainSlot* swapchain = swapchains[index];
        RinVkPendingPresent* pending;
        RinVkImageSlot* image;
        RinVulkanWsiPresentRequestV1 request;
        RinVulkanWsiPlatformV4* platform;
        const uint32_t image_index = present_info->pImageIndices[index];
        uint64_t platform_token = 0u;
        int platform_result;
        RinVkResult result;

        device_wsi_lock(device);
        if (__atomic_load_n(&swapchain->retired, __ATOMIC_ACQUIRE) != 0u ||
            !swapchain_surface_current(swapchain) ||
            image_index >= swapchain->image_count ||
            swapchain->image_states[image_index] !=
                RIN_VK_SWAPCHAIN_IMAGE_ACQUIRED ||
            swapchain->acquired_frame_ids[image_index] == 0u) {
            result = RIN_VK_ERROR_OUT_OF_DATE_KHR;
            device_wsi_unlock(device);
            goto present_result;
        }
        image = image_slot((RinVkDevice)device, swapchain->images[image_index]);
        if (!image || !image->memory ||
            image->memory_generation != image->memory->generation ||
            image->memory->owner != device ||
            image->memory->product_allocation == 0u ||
            image->memory_offset > image->memory->requested_size ||
            image->memory_size > image->memory->requested_size -
                                     image->memory_offset ||
            image->swapchain_owner != swapchain) {
            result = RIN_VK_ERROR_DEVICE_LOST;
            device_wsi_unlock(device);
            goto present_result;
        }
        pending = reserve_pending_present(device);
        if (!pending) {
            result = RIN_VK_ERROR_TOO_MANY_OBJECTS;
            device_wsi_unlock(device);
            goto present_result;
        }
        pending->presentation_display_id =
            swapchain->presentation_display_id;
        pending->queue_index = queue->queue_index;
        pending->image_index = image_index;
        pending->display_cookie = swapchain->surface->display->display_cookie;
        pending->image_token = image->memory->product_allocation;
        pending->output_generation = swapchain->output_generation;
        pending->device_generation = swapchain->device_generation;
        pending->frame_id = swapchain->acquired_frame_ids[image_index];

        memset(&request, 0, sizeof(request));
        request.struct_size = sizeof(request);
        request.version = RIN_VULKAN_WSI_PLATFORM_VERSION;
        request.display_cookie = pending->display_cookie;
        request.mode_cookie = swapchain->mode_cookie;
        request.allocation_handle = image->memory->product_allocation;
        request.allocation_offset = image->memory_offset;
        request.allocation_size = image->memory_size;
        request.output_generation = swapchain->output_generation;
        request.device_generation = swapchain->device_generation;
        request.frame_id = pending->frame_id;
        request.image_index = image_index;
        request.width = swapchain->image_extent.width;
        request.height = swapchain->image_extent.height;
        request.format = (uint32_t)swapchain->image_format;
        request.present_mode = swapchain->present_mode;

        platform = acquire_wsi_v4();
        if (!platform) {
            memset(pending, 0, sizeof(*pending));
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            device_wsi_unlock(device);
            goto present_result;
        }
        platform_result = platform->present(platform->context, &request,
                                            &platform_token);
        release_wsi();
        if (platform_result != RIN_VULKAN_WSI_PLATFORM_OK ||
            platform_token == 0u) {
            if (platform_token != 0u ||
                platform_result == RIN_VULKAN_WSI_PLATFORM_OK) {
                pending->platform_token = platform_token;
                pending->state = RIN_VK_PRESENT_RECORD_UNTRACKED;
                swapchain->image_states[image_index] =
                    RIN_VK_SWAPCHAIN_IMAGE_PRESENT_UNTRACKED;
                swapchain->present_tokens[image_index] = platform_token;
                result = RIN_VK_ERROR_DEVICE_LOST;
            } else {
                memset(pending, 0, sizeof(*pending));
                result = map_wsi_platform_result(platform_result);
            }
            device_wsi_unlock(device);
            goto present_result;
        }

        pending->platform_token = platform_token;
        pending->state = RIN_VK_PRESENT_RECORD_ACTIVE;
        swapchain->image_states[image_index] =
            RIN_VK_SWAPCHAIN_IMAGE_PRESENT_PENDING;
        swapchain->present_tokens[image_index] = platform_token;
        device_wsi_unlock(device);
        result = RIN_VK_SUCCESS;

present_result:
        if (present_info->pResults)
            present_info->pResults[index] = result;
        if (result != RIN_VK_SUCCESS && result != RIN_VK_SUBOPTIMAL_KHR &&
            overall == RIN_VK_SUCCESS)
            overall = result;
        else if (result == RIN_VK_SUBOPTIMAL_KHR &&
                 overall == RIN_VK_SUCCESS)
            overall = RIN_VK_SUBOPTIMAL_KHR;
    }
    return overall;

validation_failed:
    if (overall == RIN_VK_SUCCESS) overall = validation_result;
    if (present_info->pResults)
        for (index = 0u; index < present_info->swapchainCount; ++index)
            if (present_info->pResults[index] == RIN_VK_SUCCESS)
                present_info->pResults[index] = overall;
    return overall;
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
        !collect_feature_chain(features->pNext, 1, 0, &chain))
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
    if (chain.synchronization2) chain.synchronization2->synchronization2 = 0u;
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
        chain.vulkan13->synchronization2 =
            (profile.features & RIN_GPU_VK_ICD_FEATURES &
             RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2) != 0u;
        chain.vulkan13->dynamicRendering =
            (profile.features & RIN_GPU_VK_ICD_FEATURES &
             RIN_GPU_VK_FEATURE_DYNAMIC_RENDERING) != 0u;
        chain.vulkan13->maintenance4 = 0u;
    }
    if (chain.synchronization2) {
        chain.synchronization2->synchronization2 =
            (profile.features & RIN_GPU_VK_ICD_FEATURES &
             RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2) != 0u;
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
    uint32_t requested_queues[RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES] = {0u};
    uint32_t total_requested_queues = 0u;
    uint32_t primary_requested = 0u;
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
    if (!collect_feature_chain((void*)create_info->pNext, 0, 1,
                               &feature_chain) ||
        !requested_vulkan12_features(feature_chain.vulkan12,
                                     &chain_features) ||
        !requested_vulkan13_features(feature_chain.vulkan13,
                                     &chain_features) ||
        !requested_synchronization2_feature(feature_chain.synchronization2,
                                            &chain_features))
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if ((chain_features & RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE) != 0u &&
        !timeline_extension_enabled(create_info))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if ((chain_features & RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2) != 0u &&
        !synchronization2_extension_enabled(create_info))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if ((chain_features & RIN_GPU_VK_FEATURE_DYNAMIC_RENDERING) != 0u &&
        !dynamic_rendering_extension_enabled(create_info))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (create_info->flags != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    /* Device layers are deprecated and dispatch is owned by the loader. */
    if (!device_extensions_valid(create_info))
        return RIN_VK_ERROR_EXTENSION_NOT_PRESENT;
    if (create_info->queueCreateInfoCount == 0u ||
        create_info->queueCreateInfoCount >
            RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES ||
        !create_info->pQueueCreateInfos)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < create_info->queueCreateInfoCount; ++index) {
        const RinVkDeviceQueueCreateInfo* queue =
            &create_info->pQueueCreateInfos[index];
        uint32_t priority_index;
        if (queue->sType != RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO ||
            queue->pNext || queue->flags != 0u || queue->queueCount == 0u ||
            queue->queueFamilyIndex >= RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES ||
            queue->queueCount > RIN_VULKAN_PRODUCT_MAX_QUEUES ||
            total_requested_queues >
                RIN_VULKAN_PRODUCT_MAX_QUEUES - queue->queueCount ||
            !queue->pQueuePriorities ||
            requested_queues[queue->queueFamilyIndex] != 0u)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        requested_queues[queue->queueFamilyIndex] = queue->queueCount;
        total_requested_queues += queue->queueCount;
        for (priority_index = 0u; priority_index < queue->queueCount;
             ++priority_index) {
            if (queue->pQueuePriorities[priority_index] !=
                    queue->pQueuePriorities[priority_index] ||
                queue->pQueuePriorities[priority_index] < 0.0f ||
                queue->pQueuePriorities[priority_index] > 1.0f)
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
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
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES; ++index) {
        if (requested_queues[index] == 0u) continue;
        if (index >= profile.queue_family_count ||
            requested_queues[index] > profile.queue_families[index].queue_count) {
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
    slot->physical_device = physical;
    result = call_create_device(runtime, instance->runtime_handle,
                                physical->runtime_handle, &request,
                                &slot->runtime_handle, &slot->plan);
    if (result != RIN_GPU_VULKAN_OK) {
        release_runtime();
        memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
        slot->runtime_handle = 0u;
        slot->owner_instance = 0u;
        slot->physical_device = NULL;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return result == RIN_GPU_VULKAN_UNSUPPORTED
                   ? RIN_VK_ERROR_FEATURE_NOT_PRESENT
                   : map_result(result);
    }
    primary_requested =
        slot->plan.primary_queue_family < RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES &&
        requested_queues[slot->plan.primary_queue_family] != 0u;
    for (index = 0u; index < RIN_GPU_VULKAN_MAX_QUEUE_FAMILIES; ++index) {
        if (requested_queues[index] != 0u &&
            index != slot->plan.primary_queue_family &&
            index != slot->plan.transfer_queue_family) {
            primary_requested = 0u;
            break;
        }
    }
    if (!primary_requested) {
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
        slot->physical_device = NULL;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    release_runtime();
    slot->loader_magic = RIN_VK_ICD_LOADER_MAGIC;
    slot->reserved = 0u;
    slot->timeline_enabled =
        (chain_features & RIN_GPU_VK_FEATURE_TIMELINE_SEMAPHORE) != 0u;
    slot->synchronization2_enabled =
        (chain_features & RIN_GPU_VK_FEATURE_SYNCHRONIZATION_2) != 0u;
    slot->dynamic_rendering_enabled =
        (chain_features & RIN_GPU_VK_FEATURE_DYNAMIC_RENDERING) != 0u;
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
        slot->physical_device = NULL;
        slot->loader_magic = 0u;
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    __atomic_store_n(&slot->descriptor_validation_error, 0u,
                     __ATOMIC_RELEASE);
    slot->queue_count = total_requested_queues;
    slot->next_submission_order = 1u;
    slot->reserved_queue = 0u;
    memset(slot->queues, 0, sizeof(slot->queues));
    {
        uint32_t create_index;
        uint32_t flat_queue_index = 0u;
        for (create_index = 0u;
             create_index < create_info->queueCreateInfoCount;
             ++create_index) {
            const RinVkDeviceQueueCreateInfo* queue =
                &create_info->pQueueCreateInfos[create_index];
            uint32_t family_queue_index;
            for (family_queue_index = 0u;
                 family_queue_index < queue->queueCount;
                 ++family_queue_index, ++flat_queue_index) {
                struct RinVkQueue_T* queue_slot =
                    &slot->queues[flat_queue_index];
                queue_slot->loader_magic = RIN_VK_ICD_LOADER_MAGIC;
                queue_slot->device = slot;
                queue_slot->queue_family_index = queue->queueFamilyIndex;
                queue_slot->queue_index = flat_queue_index;
                queue_slot->family_queue_index = family_queue_index;
            }
        }
    }
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *device_out = slot;
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyDevice(RinVkDevice device,
                                    const void* allocator) {
    struct RinVkDevice_T* slot = device_slot(device);
    struct RinVkInstance_T* debug_owner = debug_instance_for_device(slot);
    RinGpuVulkanRuntimeV1* runtime;
    uint32_t expected = 1u;
    int result;
    (void)allocator;
    if (!slot || device_has_swapchains(slot) ||
        maintain_device_submissions(slot) != RIN_VK_SUCCESS ||
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
    {
        uint32_t command_index;
        for (command_index = 0u;
             command_index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS;
             ++command_index) {
            RinGpuVulkanCommandBufferV1* command_buffer =
                &g_command_runtime.buffers[command_index];
            if (__atomic_load_n(&command_buffer->state, __ATOMIC_ACQUIRE) ==
                    1u &&
                command_buffer->owner == (uintptr_t)slot)
                debug_object_clear(
                    debug_owner, RIN_VK_OBJECT_TYPE_COMMAND_BUFFER,
                    (uint64_t)(uintptr_t)command_buffer);
        }
    }
    (void)rin_gpu_vulkan_command_owner_cleanup(
        &g_command_runtime, (uintptr_t)slot);
    cleanup_device_sync_objects(slot);
    cleanup_device_pipelines(slot);
    cleanup_device_shader_modules(slot);
    (void)rin_gpu_vulkan_descriptor_runtime_shutdown(
        &slot->descriptor_runtime);
    debug_object_clear(debug_owner, RIN_VK_OBJECT_TYPE_DEVICE,
                       (uint64_t)(uintptr_t)slot);
    {
        uint32_t queue_index;
        for (queue_index = 0u; queue_index < slot->queue_count;
             ++queue_index)
            debug_object_clear(
                debug_owner, RIN_VK_OBJECT_TYPE_QUEUE,
                (uint64_t)(uintptr_t)&slot->queues[queue_index]);
    }
    __atomic_store_n(&slot->descriptor_validation_error, 0u,
                     __ATOMIC_RELEASE);
    slot->timeline_enabled = 0u;
    slot->synchronization2_enabled = 0u;
    slot->dynamic_rendering_enabled = 0u;
    memset(&slot->plan, 0, sizeof(slot->plan));
    memset(&slot->physical_profile, 0, sizeof(slot->physical_profile));
    memset(slot->queues, 0, sizeof(slot->queues));
    slot->queue_count = 0u;
    slot->next_submission_order = 0u;
    slot->reserved_queue = 0u;
    slot->runtime_handle = 0u;
    slot->owner_instance = 0u;
    slot->physical_device = NULL;
    slot->loader_magic = 0u;
    slot->reserved = 0u;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
}

void RIN_VKAPI_CALL vkGetDeviceQueue(RinVkDevice device,
                                     uint32_t queue_family_index,
                                     uint32_t queue_index,
                                     RinVkQueue* queue_out) {
    struct RinVkDevice_T* slot;
    uint32_t index;
    if (!queue_out) return;
    *queue_out = NULL;
    slot = device_slot(device);
    if (!slot) return;
    for (index = 0u; index < slot->queue_count; ++index) {
        struct RinVkQueue_T* candidate = &slot->queues[index];
        if (candidate->queue_family_index == queue_family_index &&
            candidate->family_queue_index == queue_index) {
            *queue_out = candidate;
            return;
        }
    }
}

static int device_has_queue_family(const struct RinVkDevice_T* device,
                                   uint32_t queue_family_index) {
    uint32_t index;
    if (!device) return 0;
    for (index = 0u; index < device->queue_count; ++index)
        if (device->queues[index].queue_family_index == queue_family_index)
            return 1;
    return 0;
}

RinVkResult RIN_VKAPI_CALL vkDeviceWaitIdle(RinVkDevice device) {
    struct RinVkDevice_T* slot = device_slot(device);

    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (;;) {
        RinVkResult result = maintain_device_submissions(slot);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
            return result;
        if (result == RIN_VK_SUCCESS &&
            !device_submission_slots_active(slot)) {
            result = poll_device_presentations(slot, UINT32_MAX);
            if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
                return result;
            if (result == RIN_VK_SUCCESS) return RIN_VK_SUCCESS;
        }
        yield_thread();
    }
}

RinVkResult RIN_VKAPI_CALL vkQueueWaitIdle(RinVkQueue queue) {
    struct RinVkQueue_T* queue_value = queue_slot(queue);

    if (!queue_value) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (;;) {
        RinVkResult result = maintain_device_submissions(queue_value->device);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
            return result;
        if (result == RIN_VK_SUCCESS &&
            !queue_submission_slots_active(queue_value->device,
                                          queue_value->queue_index)) {
            result = poll_device_presentations(queue_value->device,
                                               queue_value->queue_index);
            if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
                return result;
            if (result == RIN_VK_SUCCESS) return RIN_VK_SUCCESS;
        }
        yield_thread();
    }
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
    sync_lock();
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
    if (fence == 0u) return;
    sync_lock();
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
    sync_lock();
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
    uint64_t start_ns = 0u;
    uint32_t index;
    RinVkResult result;

    if (!owner || fence_count == 0u || !fences || wait_all > 1u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (timeout != 0u && timeout != UINT64_MAX &&
        !rinvulkan_platform_monotonic_time_ns(&start_ns))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (;;) {
        uint32_t signaled_count = 0u;
        int timed_out;

        result = maintain_device_submissions(owner);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
            return result;
        if (result == RIN_VK_SUCCESS && sync_try_lock()) {
            for (index = 0u; index < fence_count; ++index) {
                RinVkFenceSlot* slot = fence_slot(owner, fences[index]);
                if (!slot || fence_list_contains(fences, index,
                                                  fences[index])) {
                    sync_unlock();
                    return RIN_VK_ERROR_INITIALIZATION_FAILED;
                }
                if (__atomic_load_n(&slot->signaled, __ATOMIC_ACQUIRE) != 0u)
                    ++signaled_count;
            }
            sync_unlock();
        }
        if ((wait_all != 0u && signaled_count == fence_count) ||
            (wait_all == 0u && signaled_count != 0u))
            return RIN_VK_SUCCESS;
        if (timeout == 0u) return RIN_VK_NOT_READY;
        timed_out = wait_timeout_elapsed(start_ns, timeout);
        if (timed_out < 0) return RIN_VK_ERROR_INITIALIZATION_FAILED;
        if (timed_out != 0) return RIN_VK_TIMEOUT;
        yield_thread();
    }
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
    sync_lock();
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
    uint32_t submission_index;
    int referenced = 0;
    (void)allocator;
    if (semaphore == 0u) return;
    sync_lock();
    slot = semaphore_slot(device, semaphore);
    if (slot) {
        for (submission_index = 0u;
             submission_index < RIN_VK_MAX_SUBMISSIONS && !referenced;
             ++submission_index) {
            const RinVkSubmissionSlot* submission =
                &g_submissions[submission_index];
            uint32_t semaphore_index;
            const uint32_t state = __atomic_load_n(
                &submission->state, __ATOMIC_ACQUIRE);
            if ((state != RIN_VK_SUBMISSION_ACTIVE &&
                 state != RIN_VK_SUBMISSION_RESERVED &&
                 state != RIN_VK_SUBMISSION_WAITING) ||
                submission->owner != device)
                continue;
            for (semaphore_index = 0u;
                 semaphore_index < submission->wait_semaphore_count;
                 ++semaphore_index)
                if (submission->wait_semaphores[semaphore_index] == semaphore)
                    referenced = 1;
            for (semaphore_index = 0u;
                 semaphore_index < submission->signal_semaphore_count;
                 ++semaphore_index)
                if (submission->signal_semaphores[semaphore_index] == semaphore)
                    referenced = 1;
        }
        if (!referenced) clear_semaphore_slot(slot);
    }
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkGetSemaphoreCounterValue(
        RinVkDevice device, RinVkSemaphore semaphore, uint64_t* value_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkSemaphoreSlot* slot;
    RinVkResult result;
    if (!owner || !value_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions_until_available(owner);
    if (result != RIN_VK_SUCCESS) return result;
    sync_lock();
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
    RinVkResult result;
    uint64_t pending_minimum = 0u;
    uint64_t pending_maximum = 0u;
    uint64_t current;
    if (!owner) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = maintain_device_submissions_until_available(owner);
    if (result != RIN_VK_SUCCESS) return result;
    sync_lock();
    slot = semaphore_slot(owner, semaphore);
    if (!slot || slot->type != RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    current = __atomic_load_n(&slot->value, __ATOMIC_ACQUIRE);
    if (value <= current ||
        (timeline_pending_signal_range(owner, semaphore, NULL,
                                       &pending_minimum,
                                       &pending_maximum) &&
         value >= pending_minimum)) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    __atomic_store_n(&slot->value, value, __ATOMIC_RELEASE);
    {
        RinVkResult dispatch_result =
            dispatch_ready_waiting_submissions(owner);
        if (dispatch_result != RIN_VK_SUCCESS) {
            sync_unlock();
            return dispatch_result;
        }
    }
    sync_unlock();
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkWaitSemaphores(
        RinVkDevice device, const RinVkSemaphoreWaitInfo* wait_info,
        uint64_t timeout) {
    struct RinVkDevice_T* owner = device_slot(device);
    uint64_t start_ns = 0u;
    uint32_t index;
    RinVkResult result;
    if (!owner || !wait_info ||
        wait_info->sType != RIN_VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO ||
        wait_info->pNext ||
        (wait_info->flags & ~RIN_VK_SEMAPHORE_WAIT_ANY_BIT) != 0u ||
        wait_info->semaphoreCount == 0u ||
        wait_info->semaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        !wait_info->pSemaphores || !wait_info->pValues)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (timeout != 0u && timeout != UINT64_MAX &&
        !rinvulkan_platform_monotonic_time_ns(&start_ns))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (;;) {
        uint32_t satisfied = 0u;
        int timed_out;

        result = maintain_device_submissions(owner);
        if (result != RIN_VK_SUCCESS && result != RIN_VK_NOT_READY)
            return result;
        if (result == RIN_VK_SUCCESS && sync_try_lock()) {
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
            sync_unlock();
        }
        if ((wait_info->flags == 0u &&
             satisfied == wait_info->semaphoreCount) ||
            (wait_info->flags != 0u && satisfied != 0u))
            return RIN_VK_SUCCESS;
        if (timeout == 0u) return RIN_VK_NOT_READY;
        timed_out = wait_timeout_elapsed(start_ns, timeout);
        if (timed_out < 0) return RIN_VK_ERROR_INITIALIZATION_FAILED;
        if (timed_out != 0) return RIN_VK_TIMEOUT;
        yield_thread();
    }
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
        !device_has_queue_family(slot, snapshot.queueFamilyIndex))
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
    struct RinVkInstance_T* debug_owner = debug_instance_for_device(slot);
    RinGpuVulkanCommandPoolSlotV1* core_pool =
        (RinGpuVulkanCommandPoolSlotV1*)(uintptr_t)
            command_pool_to_core(command_pool);
    RinGpuVulkanCommandBufferV1*
        tracked_buffers[RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS];
    uint32_t tracked_count = 0u;
    uint32_t index;
    int result;
    (void)allocator;
    if (!slot || command_pool == (RinVkCommandPool)0) return;
    for (index = 0u; index < RIN_GPU_VULKAN_COMMAND_MAX_BUFFERS; ++index) {
        RinGpuVulkanCommandBufferV1* command_buffer =
            &g_command_runtime.buffers[index];
        if (__atomic_load_n(&command_buffer->state, __ATOMIC_ACQUIRE) == 1u &&
            command_buffer->pool == core_pool)
            tracked_buffers[tracked_count++] = command_buffer;
    }
    result = rin_gpu_vulkan_command_pool_destroy(
        &g_command_runtime, (uintptr_t)slot,
        command_pool_to_core(command_pool));
    if (result == RIN_GPU_VULKAN_COMMAND_OK)
        for (index = 0u; index < tracked_count; ++index) {
            clear_command_resource_uses(tracked_buffers[index]);
            debug_object_clear(
                debug_owner, RIN_VK_OBJECT_TYPE_COMMAND_BUFFER,
                (uint64_t)(uintptr_t)tracked_buffers[index]);
        }
}

RinVkResult RIN_VKAPI_CALL vkResetCommandPool(
        RinVkDevice device, RinVkCommandPool command_pool,
        uint32_t flags) {
    struct RinVkDevice_T* slot = device_slot(device);
    RinGpuVulkanCommandPoolSlotV1* core_pool =
        (RinGpuVulkanCommandPoolSlotV1*)(uintptr_t)
            command_pool_to_core(command_pool);
    int result;
    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = rin_gpu_vulkan_command_pool_reset(
        &g_command_runtime, (uintptr_t)slot,
        command_pool_to_core(command_pool), flags);
    if (result == RIN_GPU_VULKAN_COMMAND_OK)
        clear_command_pool_resource_uses(core_pool);
    return map_command_result(result);
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
    for (index = 0u; index < snapshot.commandBufferCount; ++index) {
        command_buffers[index] =
            (RinVkCommandBuffer)(void*)allocated[index];
        clear_command_resource_uses(allocated[index]);
    }
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkFreeCommandBuffers(
        RinVkDevice device, RinVkCommandPool command_pool,
        uint32_t command_buffer_count,
        const RinVkCommandBuffer* command_buffers) {
    struct RinVkDevice_T* slot = device_slot(device);
    struct RinVkInstance_T* debug_owner = debug_instance_for_device(slot);
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
    for (index = 0u; index < command_buffer_count; ++index)
        if (__atomic_load_n(&snapshot[index]->state, __ATOMIC_ACQUIRE) == 0u) {
            clear_command_resource_uses(snapshot[index]);
            debug_object_clear(
                debug_owner, RIN_VK_OBJECT_TYPE_COMMAND_BUFFER,
                (uint64_t)(uintptr_t)command_buffers[index]);
        }
}

RinVkResult RIN_VKAPI_CALL vkBeginCommandBuffer(
        RinVkCommandBuffer command_buffer,
        const RinVkCommandBufferBeginInfo* begin_info) {
    RinVkCommandBufferBeginInfo snapshot;
    int result;
    if (!begin_info) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    snapshot = *begin_info;
    if (snapshot.sType != RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO ||
        snapshot.pNext)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    result = rin_gpu_vulkan_command_buffer_begin(
        &g_command_runtime,
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer,
        snapshot.flags);
    if (result == RIN_GPU_VULKAN_COMMAND_OK)
        clear_command_resource_uses(
            (RinGpuVulkanCommandBufferV1*)(void*)command_buffer);
    return map_command_result(result);
}

RinVkResult RIN_VKAPI_CALL vkEndCommandBuffer(
        RinVkCommandBuffer command_buffer) {
    return map_command_result(rin_gpu_vulkan_command_buffer_end(
        &g_command_runtime,
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer));
}

RinVkResult RIN_VKAPI_CALL vkResetCommandBuffer(
        RinVkCommandBuffer command_buffer, uint32_t flags) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    int result = rin_gpu_vulkan_command_buffer_reset(
        &g_command_runtime,
        core, flags);
    if (result == RIN_GPU_VULKAN_COMMAND_OK)
        clear_command_resource_uses(core);
    return map_command_result(result);
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

static void record_transfer_ops(RinGpuVulkanCommandBufferV1* core,
                               const RinGpuVulkanTransferOpV2* operations,
                               uint32_t operation_count,
                               const RinVkCommandResourceUse uses[],
                               uint32_t use_count);

void RIN_VKAPI_CALL vkCmdCopyBuffer(
    RinVkCommandBuffer command_buffer, RinVkBuffer src_buffer,
    RinVkBuffer dst_buffer, uint32_t region_count,
    const RinVkBufferCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[
        RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkCommandResourceUse resource_uses[
        RIN_VK_MAX_COMMAND_RESOURCE_USES];
    RinVkBufferSlot* source;
    RinVkBufferSlot* destination;
    struct RinVkDevice_T* owner;
    uintptr_t owner_address = 0u;
    uint32_t operation_count = 0u;
    uint32_t resource_use_count = 0u;
    uint32_t index;
    int valid = 1;

    if (rin_gpu_vulkan_command_buffer_owner(
            &g_command_runtime, core, &owner_address) !=
            RIN_GPU_VULKAN_COMMAND_OK ||
        !(owner = device_slot((RinVkDevice)(void*)owner_address)) ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS ||
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
    memset(operations, 0, sizeof(operations));
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
        operations[operation_count].type =
            RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_COPY;
        operations[operation_count].source_allocation =
            source->memory->product_allocation;
        operations[operation_count].destination_allocation =
            destination->memory->product_allocation;
        operations[operation_count].source_gpu_address = source_address;
        operations[operation_count].destination_gpu_address =
            destination_address;
        operations[operation_count].size_bytes = regions[index].size;
        resource_uses[resource_use_count].operation_index = operation_count;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_BUFFER;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_READ;
        resource_uses[resource_use_count].resource_handle = src_buffer;
        resource_uses[resource_use_count].offset = regions[index].srcOffset;
        resource_uses[resource_use_count].size = regions[index].size;
        ++resource_use_count;
        resource_uses[resource_use_count].operation_index = operation_count;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_BUFFER;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
        resource_uses[resource_use_count].resource_handle = dst_buffer;
        resource_uses[resource_use_count].offset = regions[index].dstOffset;
        resource_uses[resource_use_count].size = regions[index].size;
        ++resource_use_count;
        ++operation_count;
    }
    if (operation_count != 0u)
        record_transfer_ops(core, operations, operation_count, resource_uses,
                            resource_use_count);

done:
    if (!valid) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
    }
}

static int image_layout_transfer_valid(uint32_t layout) {
    return layout == RIN_VK_IMAGE_LAYOUT_GENERAL ||
           layout == RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
           layout == RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
           layout == RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
}

static int image_layout_transfer_source_valid(uint32_t layout) {
    return layout == RIN_VK_IMAGE_LAYOUT_GENERAL ||
           layout == RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

static int image_layout_transfer_destination_valid(uint32_t layout) {
    return layout == RIN_VK_IMAGE_LAYOUT_GENERAL ||
           layout == RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
}

static int command_image_layout_matches(
        const RinGpuVulkanCommandBufferV1* command_buffer,
        RinVkImage image_handle, const RinVkImageSlot* image,
        uint32_t requested_layout, int source_access) {
    uint32_t current_layout;
    uint32_t operation_index;
    if (!command_buffer || !image || image_handle == 0u ||
        (source_access
             ? !image_layout_transfer_source_valid(requested_layout)
             : !image_layout_transfer_destination_valid(requested_layout)) ||
        command_buffer->transfer_op_count >
            RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS)
        return 0;
    current_layout = __atomic_load_n(&image->current_layout, __ATOMIC_ACQUIRE);
    for (operation_index = 0u;
         operation_index < command_buffer->transfer_op_count;
         ++operation_index) {
        const RinGpuVulkanTransferOpV2* operation =
            &command_buffer->transfer_ops[operation_index];
        if (operation->type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER &&
            operation->source_allocation == image_handle)
            current_layout = operation->source_height;
    }
    return current_layout == requested_layout;
}

static int image_subresource_valid(const RinVkImageSlot* image,
                                   const RinVkImageSubresourceLayers* subresource,
                                   uint32_t width, uint32_t height) {
    return image && subresource &&
           image_format_aspects(image->format) != 0u &&
           subresource->aspectMask == image_format_aspects(image->format) &&
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
           image_format_aspects(image->format) != 0u &&
           range->aspectMask == image_format_aspects(image->format) &&
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
                                uint32_t operation_count,
                                const RinVkCommandResourceUse uses[],
                                uint32_t use_count) {
    RinVkCommandResourceUse adjusted_uses[RIN_VK_MAX_COMMAND_RESOURCE_USES];
    uint32_t operation_base;
    uint32_t index;
    if (!core || !operations || operation_count == 0u ||
        use_count > RIN_VK_MAX_COMMAND_RESOURCE_USES ||
        (use_count != 0u && !uses)) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
        return;
    }
    operation_base = core->transfer_op_count;
    if (rin_gpu_vulkan_command_buffer_record_transfer_ops(
            &g_command_runtime, core, operations, operation_count) !=
        RIN_GPU_VULKAN_COMMAND_OK) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
        return;
    }
    for (index = 0u; index < use_count; ++index) {
        if (uses[index].operation_index >= operation_count ||
            operation_base > UINT32_MAX - uses[index].operation_index) {
            rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                        core);
            return;
        }
        adjusted_uses[index] = uses[index];
        adjusted_uses[index].operation_index += operation_base;
    }
    if (!append_command_resource_uses(core, adjusted_uses, use_count))
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

static int synchronization2_queue_families_valid(
        const RinGpuVulkanCommandBufferV1* core, uint32_t src_queue_family,
        uint32_t dst_queue_family) {
    uint32_t queue_family;
    struct RinVkDevice_T* owner;
    if (!core || !core->pool) return 0;
    queue_family = core->pool->queue_family_index;
    if (src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED ||
        dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED)
        return src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED &&
               dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED;
    owner = (struct RinVkDevice_T*)(uintptr_t)core->pool->owner;
    return owner &&
           src_queue_family < owner->physical_profile.queue_family_count &&
           dst_queue_family < owner->physical_profile.queue_family_count &&
           (src_queue_family == queue_family ||
            dst_queue_family == queue_family);
}

static int image_barrier_range_valid(
        const RinVkImageSlot* image,
        const RinVkImageSubresourceRange* range) {
    return image && range && image_format_aspects(image->format) != 0u &&
           range->aspectMask == image_format_aspects(image->format) &&
           range->baseMipLevel == 0u &&
           (range->levelCount == 1u ||
            range->levelCount == RIN_VK_REMAINING_MIP_LEVELS) &&
           range->baseArrayLayer == 0u &&
           (range->layerCount == 1u ||
            range->layerCount == RIN_VK_REMAINING_ARRAY_LAYERS);
}

static int image_barrier_old_layout_valid(uint32_t layout) {
    return layout == RIN_VK_IMAGE_LAYOUT_UNDEFINED ||
           image_layout_transfer_valid(layout);
}

void RIN_VKAPI_CALL vkCmdPipelineBarrier2(
        RinVkCommandBuffer command_buffer,
        const RinVkDependencyInfo* dependency_info) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    RinGpuVulkanTransferOpV2 operations[
        RIN_GPU_VULKAN_COMMAND_MAX_BARRIERS];
    uint64_t barrier_count;
    uint32_t index;
    uint32_t operation_count = 0u;
    int valid = 1;

    memset(operations, 0, sizeof(operations));
    if (!dependency_info ||
        !command_owner_device(core, &owner) ||
        !owner->synchronization2_enabled ||
        dependency_info->sType != RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO ||
        dependency_info->pNext || dependency_info->dependencyFlags != 0u ||
        (dependency_info->memoryBarrierCount != 0u &&
         !dependency_info->pMemoryBarriers) ||
        (dependency_info->bufferMemoryBarrierCount != 0u &&
         !dependency_info->pBufferMemoryBarriers) ||
        (dependency_info->imageMemoryBarrierCount != 0u &&
         !dependency_info->pImageMemoryBarriers)) {
        valid = 0;
        goto done;
    }
    barrier_count = (uint64_t)dependency_info->memoryBarrierCount +
                    dependency_info->bufferMemoryBarrierCount +
                    dependency_info->imageMemoryBarrierCount;
    if (barrier_count == 0u ||
        barrier_count > RIN_GPU_VULKAN_COMMAND_MAX_BARRIERS) {
        valid = 0;
        goto done;
    }
    for (index = 0u; index < dependency_info->memoryBarrierCount; ++index) {
        const RinVkMemoryBarrier2* barrier =
            &dependency_info->pMemoryBarriers[index];
        RinGpuVulkanTransferOpV2* operation = &operations[operation_count];
        if (barrier->sType != RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 ||
            barrier->pNext) {
            valid = 0;
            goto done;
        }
        operation->type =
            RIN_GPU_VULKAN_TRANSFER_OP_MEMORY_BARRIER;
        if (!rin_vk_sync2_barrier_scopes(
                barrier->srcStageMask, barrier->srcAccessMask,
                barrier->dstStageMask, barrier->dstAccessMask, operation)) {
            valid = 0;
            goto done;
        }
        ++operation_count;
    }
    for (index = 0u; index < dependency_info->bufferMemoryBarrierCount;
         ++index) {
        const RinVkBufferMemoryBarrier2* barrier =
            &dependency_info->pBufferMemoryBarriers[index];
        RinGpuVulkanTransferOpV2* operation = &operations[operation_count];
        RinVkBufferSlot* buffer;
        uint64_t barrier_size;
        uint64_t address;
        if (barrier->sType !=
                RIN_VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 ||
            barrier->pNext ||
            !synchronization2_queue_families_valid(
                core, barrier->srcQueueFamilyIndex,
                barrier->dstQueueFamilyIndex) ||
            !(buffer = buffer_slot((RinVkDevice)owner, barrier->buffer)) ||
            !buffer->memory || barrier->offset >= buffer->size) {
            valid = 0;
            goto done;
        }
        barrier_size = barrier->size == RIN_VK_WHOLE_SIZE
                           ? buffer->size - barrier->offset
                           : barrier->size;
        if (barrier_size == 0u ||
            barrier_size > buffer->size - barrier->offset ||
            !checked_buffer_address(buffer, barrier->offset, barrier_size,
                                    &address) ||
            !rin_vk_sync2_barrier_scopes(
                barrier->srcStageMask, barrier->srcAccessMask,
                barrier->dstStageMask, barrier->dstAccessMask, operation)) {
            valid = 0;
            goto done;
        }
        operation->type = RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_BARRIER;
        operation->source_allocation = barrier->buffer;
        operation->destination_allocation = buffer->memory->product_allocation;
        operation->destination_gpu_address = address;
        operation->size_bytes = barrier_size;
        operation->source_width = barrier->srcQueueFamilyIndex;
        operation->source_height = barrier->dstQueueFamilyIndex;
        ++operation_count;
    }
    for (index = 0u; index < dependency_info->imageMemoryBarrierCount;
         ++index) {
        const RinVkImageMemoryBarrier2* barrier =
            &dependency_info->pImageMemoryBarriers[index];
        RinGpuVulkanTransferOpV2* operation = &operations[operation_count];
        RinVkImageSlot* image = image_slot((RinVkDevice)owner, barrier->image);
        uint64_t address;
        if (barrier->sType !=
                RIN_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 ||
            barrier->pNext ||
            !synchronization2_queue_families_valid(
                core, barrier->srcQueueFamilyIndex,
                barrier->dstQueueFamilyIndex) ||
            !image || !image->memory ||
            !image_barrier_range_valid(image, &barrier->subresourceRange) ||
            !image_barrier_old_layout_valid(barrier->oldLayout) ||
            !image_layout_transfer_valid(barrier->newLayout) ||
            !checked_image_address(image, 0u, image->memory_size, &address) ||
            !rin_vk_sync2_barrier_scopes(
                barrier->srcStageMask, barrier->srcAccessMask,
                barrier->dstStageMask, barrier->dstAccessMask, operation)) {
            valid = 0;
            goto done;
        }
        operation->type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER;
        operation->source_allocation = barrier->image;
        operation->destination_allocation = image->memory->product_allocation;
        operation->destination_gpu_address = address;
        operation->size_bytes = image->memory_size;
        operation->source_width = barrier->oldLayout;
        operation->source_height = barrier->newLayout;
        operation->destination_width = barrier->subresourceRange.aspectMask;
        operation->destination_height = barrier->srcQueueFamilyIndex;
        operation->filter = barrier->dstQueueFamilyIndex;
        ++operation_count;
    }
    if (rin_gpu_vulkan_command_buffer_record_transfer_ops(
            &g_command_runtime, core, operations, operation_count) !=
        RIN_GPU_VULKAN_COMMAND_OK)
        valid = 0;

done:
    if (!valid)
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

static uint32_t query_value_count(const RinVkQueryPoolSlot* pool) {
    return pool && pool->query_type == RIN_VK_QUERY_TYPE_TIMESTAMP ? 1u : 0u;
}

RinVkResult RIN_VKAPI_CALL vkCreateQueryPool(
        RinVkDevice device, const RinVkQueryPoolCreateInfo* create_info,
        const void* allocator, RinVkQueryPool* query_pool_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkQueryPoolSlot* slot;
    uint32_t index;
    (void)allocator;
    if (!query_pool_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *query_pool_out = 0u;
    if (!owner || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO ||
        create_info->pNext || create_info->flags != 0u ||
        create_info->queryCount == 0u ||
        create_info->queryCount > RIN_VK_QUERY_POOL_MAX_QUERIES ||
        create_info->queryType > RIN_VK_QUERY_TYPE_TIMESTAMP)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (create_info->queryType != RIN_VK_QUERY_TYPE_TIMESTAMP ||
        create_info->pipelineStatistics != 0u)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    slot = reserve_query_pool_slot(owner, &index);
    if (!slot) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    slot->query_type = create_info->queryType;
    slot->query_count = create_info->queryCount;
    slot->pipeline_statistics = create_info->pipelineStatistics;
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *query_pool_out = resource_handle(RIN_VK_QUERY_POOL_TAG, index,
                                       slot->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyQueryPool(RinVkDevice device,
                                       RinVkQueryPool query_pool,
                                       const void* allocator) {
    RinVkQueryPoolSlot* slot = query_pool_slot(device, query_pool);
    uint32_t index;
    (void)allocator;
    if (!slot) return;
    for (index = 0u; index < slot->query_count; ++index)
        if (slot->queries[index].pending != 0u ||
            slot->queries[index].active != 0u)
            return;
    clear_query_pool_slot(slot);
}

RinVkResult RIN_VKAPI_CALL vkGetQueryPoolResults(
        RinVkDevice device, RinVkQueryPool query_pool, uint32_t first_query,
        uint32_t query_count, size_t data_size, void* data, uint64_t stride,
        uint32_t flags) {
    RinVkQueryPoolSlot* pool = query_pool_slot(device, query_pool);
    uint32_t value_count;
    uint64_t per_query;
    uint32_t index;
    uint32_t unavailable = 0u;
    if (!pool || (flags & ~RIN_VK_QUERY_RESULT_FLAGS_KNOWN) != 0u ||
        (flags & RIN_VK_QUERY_RESULT_64_BIT) == 0u ||
        (query_count != 0u && !data) ||
        first_query > pool->query_count ||
        query_count > pool->query_count - first_query)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (query_count == 0u) return RIN_VK_SUCCESS;
    value_count = query_value_count(pool);
    per_query = (uint64_t)value_count * sizeof(uint64_t);
    if ((flags & RIN_VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0u)
        per_query += sizeof(uint64_t);
    if (stride < per_query || stride > SIZE_MAX ||
        (uint64_t)(query_count - 1u) >
            (SIZE_MAX - per_query) / stride ||
        per_query + (uint64_t)(query_count - 1u) * stride > data_size)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < query_count; ++index) {
        if (pool->queries[first_query + index].availability == 0u)
            unavailable = 1u;
    }
    if (unavailable != 0u &&
        (flags & RIN_VK_QUERY_RESULT_WAIT_BIT) != 0u) {
        RinVkResult maintained = rin_gpu_vulkan_icd_maintain(device);
        if (maintained != RIN_VK_SUCCESS && maintained != RIN_VK_NOT_READY)
            return maintained;
        unavailable = 0u;
        for (index = 0u; index < query_count; ++index)
            if (pool->queries[first_query + index].availability == 0u)
                unavailable = 1u;
    }
    if (unavailable != 0u &&
        (flags & RIN_VK_QUERY_RESULT_PARTIAL_BIT) == 0u)
        return RIN_VK_NOT_READY;
    for (index = 0u; index < query_count; ++index) {
        RinVkQueryValue* query = &pool->queries[first_query + index];
        uint8_t* destination = (uint8_t*)data + (size_t)index * (size_t)stride;
        uint32_t value_index;
        for (value_index = 0u; value_index < value_count; ++value_index) {
            uint64_t value = query->availability != 0u
                                 ? query->values[value_index]
                                 : 0u;
            memcpy(destination + (size_t)value_index * sizeof(uint64_t),
                   &value, sizeof(value));
        }
        if ((flags & RIN_VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) != 0u) {
            uint64_t available = query->availability != 0u ? 1u : 0u;
            memcpy(destination + (size_t)value_count * sizeof(uint64_t),
                   &available, sizeof(available));
        }
    }
    return unavailable != 0u ? RIN_VK_NOT_READY : RIN_VK_SUCCESS;
}

static void record_query_failure(RinGpuVulkanCommandBufferV1* core) {
    rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

static int event_stage_mask_valid(uint64_t stage) {
    return rin_vk_sync2_recorded_stage_mask_valid(stage);
}

void RIN_VKAPI_CALL vkCmdResetQueryPool(
        RinVkCommandBuffer command_buffer, RinVkQueryPool query_pool,
        uint32_t first_query, uint32_t query_count) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    RinVkQueryPoolSlot* pool;
    uint32_t index;
    if (!command_owner_device(core, &owner) ||
        !(pool = query_pool_slot((RinVkDevice)owner, query_pool)) ||
        query_count == 0u || first_query > pool->query_count ||
        query_count > pool->query_count - first_query ||
        query_count > RIN_GPU_VULKAN_COMMAND_MAX_QUERY_COMMANDS) {
        record_query_failure(core);
        return;
    }
    for (index = 0u; index < query_count; ++index)
        if (rin_gpu_vulkan_command_buffer_record_query(
                &g_command_runtime, core, query_pool, first_query + index,
                0u, RIN_GPU_VULKAN_QUERY_COMMAND_RESET) !=
            RIN_GPU_VULKAN_COMMAND_OK) {
            record_query_failure(core);
            return;
        }
}

void RIN_VKAPI_CALL vkCmdBeginQuery(
        RinVkCommandBuffer command_buffer, RinVkQueryPool query_pool,
        uint32_t query, uint32_t flags) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    RinVkQueryPoolSlot* pool;
    if (!command_owner_device(core, &owner) ||
        !(pool = query_pool_slot((RinVkDevice)owner, query_pool)) ||
        query >= pool->query_count ||
        (flags & ~RIN_VK_QUERY_CONTROL_PRECISE_BIT) != 0u ||
        pool->query_type != RIN_VK_QUERY_TYPE_OCCLUSION ||
        rin_gpu_vulkan_command_buffer_record_query(
            &g_command_runtime, core, query_pool, query, flags,
            RIN_GPU_VULKAN_QUERY_COMMAND_BEGIN) != RIN_GPU_VULKAN_COMMAND_OK)
        record_query_failure(core);
}

void RIN_VKAPI_CALL vkCmdEndQuery(
        RinVkCommandBuffer command_buffer, RinVkQueryPool query_pool,
        uint32_t query) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    RinVkQueryPoolSlot* pool;
    if (!command_owner_device(core, &owner) ||
        !(pool = query_pool_slot((RinVkDevice)owner, query_pool)) ||
        query >= pool->query_count ||
        pool->query_type != RIN_VK_QUERY_TYPE_OCCLUSION ||
        rin_gpu_vulkan_command_buffer_record_query(
            &g_command_runtime, core, query_pool, query, 0u,
            RIN_GPU_VULKAN_QUERY_COMMAND_END) != RIN_GPU_VULKAN_COMMAND_OK)
        record_query_failure(core);
}

void RIN_VKAPI_CALL vkCmdWriteTimestamp(
        RinVkCommandBuffer command_buffer, uint64_t stage,
        RinVkQueryPool query_pool, uint32_t query) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    RinVkQueryPoolSlot* pool;
    if (!command_owner_device(core, &owner) || !event_stage_mask_valid(stage) ||
        !(pool = query_pool_slot((RinVkDevice)owner, query_pool)) ||
        query >= pool->query_count ||
        pool->query_type != RIN_VK_QUERY_TYPE_TIMESTAMP ||
        rin_gpu_vulkan_command_buffer_record_query(
            &g_command_runtime, core, query_pool, query, 0u,
            RIN_GPU_VULKAN_QUERY_COMMAND_TIMESTAMP) != RIN_GPU_VULKAN_COMMAND_OK)
        record_query_failure(core);
}

RinVkResult RIN_VKAPI_CALL vkCreateEvent(
        RinVkDevice device, const RinVkEventCreateInfo* create_info,
        const void* allocator, RinVkEvent* event_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkEventSlot* slot;
    uint32_t index;
    (void)allocator;
    if (!event_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *event_out = 0u;
    if (!owner || !create_info ||
        create_info->sType != RIN_VK_STRUCTURE_TYPE_EVENT_CREATE_INFO ||
        create_info->pNext || create_info->flags != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    slot = reserve_event_slot(owner, &index);
    if (!slot) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    __atomic_store_n(&slot->state, 1u, __ATOMIC_RELEASE);
    *event_out = resource_handle(RIN_VK_EVENT_TAG, index, slot->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyEvent(RinVkDevice device, RinVkEvent event,
                                   const void* allocator) {
    RinVkEventSlot* slot = event_slot(device, event);
    (void)allocator;
    if (slot && __atomic_load_n(&slot->pending, __ATOMIC_ACQUIRE) == 0u)
        clear_event_slot(slot);
}

RinVkResult RIN_VKAPI_CALL vkGetEventStatus(RinVkDevice device,
                                            RinVkEvent event) {
    RinVkEventSlot* slot = event_slot(device, event);
    if (!slot) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    return __atomic_load_n(&slot->signaled, __ATOMIC_ACQUIRE) != 0u
               ? RIN_VK_EVENT_SET
               : RIN_VK_EVENT_RESET;
}

RinVkResult RIN_VKAPI_CALL vkSetEvent(RinVkDevice device, RinVkEvent event) {
    RinVkEventSlot* slot = event_slot(device, event);
    if (!slot || __atomic_load_n(&slot->pending, __ATOMIC_ACQUIRE) != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    __atomic_store_n(&slot->signaled, 1u, __ATOMIC_RELEASE);
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkResetEvent(RinVkDevice device, RinVkEvent event) {
    RinVkEventSlot* slot = event_slot(device, event);
    if (!slot || __atomic_load_n(&slot->pending, __ATOMIC_ACQUIRE) != 0u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    __atomic_store_n(&slot->signaled, 0u, __ATOMIC_RELEASE);
    return RIN_VK_SUCCESS;
}

static void record_event_operation(RinVkCommandBuffer command_buffer,
                                   RinVkEvent event, uint32_t operation) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    if (!command_owner_device(core, &owner) || !event_slot((RinVkDevice)owner,
                                                            event) ||
        rin_gpu_vulkan_command_buffer_record_event(
            &g_command_runtime, core, event, operation) !=
            RIN_GPU_VULKAN_COMMAND_OK)
        record_query_failure(core);
}

void RIN_VKAPI_CALL vkCmdSetEvent(RinVkCommandBuffer command_buffer,
                                  RinVkEvent event, uint64_t stage) {
    if (!event_stage_mask_valid(stage))
        record_query_failure((RinGpuVulkanCommandBufferV1*)(void*)command_buffer);
    else
        record_event_operation(command_buffer, event,
                               RIN_GPU_VULKAN_EVENT_COMMAND_SET);
}

void RIN_VKAPI_CALL vkCmdResetEvent(RinVkCommandBuffer command_buffer,
                                    RinVkEvent event, uint64_t stage) {
    if (!event_stage_mask_valid(stage))
        record_query_failure((RinGpuVulkanCommandBufferV1*)(void*)command_buffer);
    else
        record_event_operation(command_buffer, event,
                               RIN_GPU_VULKAN_EVENT_COMMAND_RESET);
}

void RIN_VKAPI_CALL vkCmdWaitEvents(
        RinVkCommandBuffer command_buffer, uint32_t event_count,
        const RinVkEvent* events, uint64_t src_stage_mask,
        uint64_t dst_stage_mask, uint32_t memory_barrier_count,
        const void* memory_barriers, uint32_t buffer_barrier_count,
        const void* buffer_barriers, uint32_t image_barrier_count,
        const void* image_barriers) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    uint32_t index;
    if (!command_owner_device(core, &owner) || event_count == 0u ||
        event_count > RIN_GPU_VULKAN_COMMAND_MAX_EVENT_COMMANDS || !events ||
        !event_stage_mask_valid(src_stage_mask) ||
        !event_stage_mask_valid(dst_stage_mask) ||
        memory_barrier_count != 0u || memory_barriers ||
        buffer_barrier_count != 0u || buffer_barriers ||
        image_barrier_count != 0u || image_barriers) {
        record_query_failure(core);
        return;
    }
    for (index = 0u; index < event_count; ++index) {
        if (!event_slot((RinVkDevice)owner, events[index]) ||
            rin_gpu_vulkan_command_buffer_record_event(
                &g_command_runtime, core, events[index],
                RIN_GPU_VULKAN_EVENT_COMMAND_WAIT) !=
                RIN_GPU_VULKAN_COMMAND_OK) {
            record_query_failure(core);
            return;
        }
    }
}

void RIN_VKAPI_CALL vkCmdCopyImage(
        RinVkCommandBuffer command_buffer, RinVkImage src_image,
        uint32_t src_image_layout, RinVkImage dst_image,
        uint32_t dst_image_layout, uint32_t region_count,
        const RinVkImageCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkCommandResourceUse resource_uses[
        RIN_VK_MAX_COMMAND_RESOURCE_USES];
    RinVkImageSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint32_t index;
    uint32_t resource_use_count = 0u;
    int valid = 1;

    memset(operations, 0, sizeof(operations));
    if (region_count == 0u ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS || !regions ||
        !command_owner_device(core, &owner) ||
        !image_layout_transfer_source_valid(src_image_layout) ||
        !image_layout_transfer_destination_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = image_slot(owner, dst_image);
    if (!source || !destination ||
        !command_image_layout_matches(core, src_image, source,
                                      src_image_layout, 1) ||
        !command_image_layout_matches(core, dst_image, destination,
                                      dst_image_layout, 0) ||
        source->format != destination->format ||
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
        resource_uses[resource_use_count].operation_index = index;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_READ;
        resource_uses[resource_use_count].image_layout = src_image_layout;
        resource_uses[resource_use_count].resource_handle = src_image;
        resource_uses[resource_use_count].offset = 0u;
        resource_uses[resource_use_count].size = source->memory_size;
        ++resource_use_count;
        resource_uses[resource_use_count].operation_index = index;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
        resource_uses[resource_use_count].image_layout = dst_image_layout;
        resource_uses[resource_use_count].resource_handle = dst_image;
        resource_uses[resource_use_count].offset = 0u;
        resource_uses[resource_use_count].size = destination->memory_size;
        ++resource_use_count;
    }
done:
    if (valid)
        record_transfer_ops(core, operations, region_count, resource_uses,
                            resource_use_count);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdCopyBufferToImage(
        RinVkCommandBuffer command_buffer, RinVkBuffer src_buffer,
        RinVkImage dst_image, uint32_t dst_image_layout, uint32_t region_count,
        const RinVkBufferImageCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkCommandResourceUse resource_uses[
        RIN_VK_MAX_COMMAND_RESOURCE_USES];
    RinVkBufferSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint32_t index;
    uint32_t resource_use_count = 0u;
    int valid = 1;
    memset(operations, 0, sizeof(operations));
    if (region_count == 0u ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS || !regions ||
        !command_owner_device(core, &owner) ||
        !image_layout_transfer_destination_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = buffer_slot(owner, src_buffer);
    destination = image_slot(owner, dst_image);
    if (!source || !destination ||
        !command_image_layout_matches(core, dst_image, destination,
                                      dst_image_layout, 0) ||
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
        resource_uses[resource_use_count].operation_index = index;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_BUFFER;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_READ;
        resource_uses[resource_use_count].resource_handle = src_buffer;
        resource_uses[resource_use_count].offset = regions[index].bufferOffset;
        resource_uses[resource_use_count].size = size;
        ++resource_use_count;
        resource_uses[resource_use_count].operation_index = index;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
        resource_uses[resource_use_count].image_layout = dst_image_layout;
        resource_uses[resource_use_count].resource_handle = dst_image;
        resource_uses[resource_use_count].offset = 0u;
        resource_uses[resource_use_count].size = destination->memory_size;
        ++resource_use_count;
    }
done:
    if (valid)
        record_transfer_ops(core, operations, region_count, resource_uses,
                            resource_use_count);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdCopyImageToBuffer(
        RinVkCommandBuffer command_buffer, RinVkImage src_image,
        uint32_t src_image_layout, RinVkBuffer dst_buffer,
        uint32_t region_count, const RinVkBufferImageCopy* regions) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operations[RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS];
    RinVkCommandResourceUse resource_uses[
        RIN_VK_MAX_COMMAND_RESOURCE_USES];
    RinVkImageSlot* source;
    RinVkBufferSlot* destination;
    struct RinVkDevice_T* owner;
    uint32_t index;
    uint32_t resource_use_count = 0u;
    int valid = 1;
    memset(operations, 0, sizeof(operations));
    if (region_count == 0u ||
        region_count > RIN_GPU_VULKAN_COMMAND_MAX_TRANSFER_OPS || !regions ||
        !command_owner_device(core, &owner) ||
        !image_layout_transfer_source_valid(src_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = buffer_slot(owner, dst_buffer);
    if (!source || !destination ||
        !command_image_layout_matches(core, src_image, source,
                                      src_image_layout, 1) ||
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
        resource_uses[resource_use_count].operation_index = index;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_READ;
        resource_uses[resource_use_count].image_layout = src_image_layout;
        resource_uses[resource_use_count].resource_handle = src_image;
        resource_uses[resource_use_count].offset = 0u;
        resource_uses[resource_use_count].size = source->memory_size;
        ++resource_use_count;
        resource_uses[resource_use_count].operation_index = index;
        resource_uses[resource_use_count].resource_kind =
            RIN_VK_COMMAND_RESOURCE_USE_BUFFER;
        resource_uses[resource_use_count].access =
            RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
        resource_uses[resource_use_count].resource_handle = dst_buffer;
        resource_uses[resource_use_count].offset =
            regions[index].bufferOffset;
        resource_uses[resource_use_count].size = size;
        ++resource_use_count;
    }
done:
    if (valid)
        record_transfer_ops(core, operations, region_count, resource_uses,
                            resource_use_count);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdClearColorImage(
        RinVkCommandBuffer command_buffer, RinVkImage image_handle,
        uint32_t image_layout, const RinVkClearColorValue* color,
        uint32_t range_count, const RinVkImageSubresourceRange* ranges) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operation;
    RinVkCommandResourceUse resource_use;
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
    if (!image || image->format != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
        !command_image_layout_matches(core, image_handle, image, image_layout,
                                      0) ||
        (image->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u ||
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
    resource_use.operation_index = 0u;
    resource_use.resource_kind = RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
    resource_use.access = RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
    resource_use.image_layout = image_layout;
    resource_use.resource_handle = image_handle;
    resource_use.offset = 0u;
    resource_use.size = image->memory_size;
done:
    if (valid) record_transfer_ops(core, &operation, 1u, &resource_use, 1u);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
}

void RIN_VKAPI_CALL vkCmdClearDepthStencilImage(
        RinVkCommandBuffer command_buffer, RinVkImage image_handle,
        uint32_t image_layout, const RinVkClearDepthStencilValue* value,
        uint32_t range_count, const RinVkImageSubresourceRange* ranges) {
    RinGpuVulkanCommandBufferV1* core =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    RinGpuVulkanTransferOpV2 operation;
    RinVkCommandResourceUse resource_use;
    RinVkImageSlot* image;
    struct RinVkDevice_T* owner;
    uint64_t destination_address;
    int valid = 1;

    memset(&operation, 0, sizeof(operation));
    if (!command_owner_device(core, &owner) || !value ||
        value->depth != value->depth || value->depth < 0.0f ||
        value->depth > 1.0f || range_count != 1u || !ranges ||
        (image_layout != RIN_VK_IMAGE_LAYOUT_GENERAL &&
         image_layout != RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
        valid = 0;
        goto done;
    }
    image = image_slot(owner, image_handle);
    if (!image || image->format != RIN_VK_FORMAT_D32_SFLOAT ||
        !command_image_layout_matches(core, image_handle, image, image_layout,
                                      0) ||
        (image->usage & RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u ||
        !image_subresource_range_valid(image, &ranges[0]) ||
        !checked_image_address(image, 0u, image->memory_size,
                               &destination_address)) {
        valid = 0;
        goto done;
    }
    operation.type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR;
    operation.destination_allocation = image->memory->product_allocation;
    operation.destination_gpu_address = destination_address;
    operation.size_bytes = image->memory_size;
    memcpy(&operation.clear_value[0], &value->depth, sizeof(value->depth));
    resource_use.operation_index = 0u;
    resource_use.resource_kind = RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
    resource_use.access = RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
    resource_use.image_layout = image_layout;
    resource_use.resource_handle = image_handle;
    resource_use.offset = 0u;
    resource_use.size = image->memory_size;
done:
    if (valid) record_transfer_ops(core, &operation, 1u, &resource_use, 1u);
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
    RinVkCommandResourceUse resource_uses[2];
    RinVkImageSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint64_t source_address;
    uint64_t destination_address;
    int valid = 1;

    memset(&operation, 0, sizeof(operation));
    if (!command_owner_device(core, &owner) || region_count != 1u || !regions ||
        filter > RIN_VK_FILTER_LINEAR ||
        !image_layout_transfer_source_valid(src_image_layout) ||
        !image_layout_transfer_destination_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = image_slot(owner, dst_image);
    if (!source || !destination ||
        !command_image_layout_matches(core, src_image, source,
                                      src_image_layout, 1) ||
        !command_image_layout_matches(core, dst_image, destination,
                                      dst_image_layout, 0) ||
        source == destination ||
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
    resource_uses[0].operation_index = 0u;
    resource_uses[0].resource_kind = RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
    resource_uses[0].access = RIN_VK_COMMAND_RESOURCE_ACCESS_READ;
    resource_uses[0].image_layout = src_image_layout;
    resource_uses[0].resource_handle = src_image;
    resource_uses[0].offset = 0u;
    resource_uses[0].size = source->memory_size;
    resource_uses[1].operation_index = 0u;
    resource_uses[1].resource_kind = RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
    resource_uses[1].access = RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
    resource_uses[1].image_layout = dst_image_layout;
    resource_uses[1].resource_handle = dst_image;
    resource_uses[1].offset = 0u;
    resource_uses[1].size = destination->memory_size;
done:
    if (valid) record_transfer_ops(core, &operation, 1u, resource_uses, 2u);
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
    RinVkCommandResourceUse resource_uses[2];
    RinVkImageSlot* source;
    RinVkImageSlot* destination;
    struct RinVkDevice_T* owner;
    uint64_t source_address;
    uint64_t destination_address;
    int valid = 1;

    memset(&operation, 0, sizeof(operation));
    if (!command_owner_device(core, &owner) || region_count != 1u || !regions ||
        !image_layout_transfer_source_valid(src_image_layout) ||
        !image_layout_transfer_destination_valid(dst_image_layout)) {
        valid = 0;
        goto done;
    }
    source = image_slot(owner, src_image);
    destination = image_slot(owner, dst_image);
    if (!source || !destination ||
        !command_image_layout_matches(core, src_image, source,
                                      src_image_layout, 1) ||
        !command_image_layout_matches(core, dst_image, destination,
                                      dst_image_layout, 0) ||
        source == destination ||
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
    resource_uses[0].operation_index = 0u;
    resource_uses[0].resource_kind = RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
    resource_uses[0].access = RIN_VK_COMMAND_RESOURCE_ACCESS_READ;
    resource_uses[0].image_layout = src_image_layout;
    resource_uses[0].resource_handle = src_image;
    resource_uses[0].offset = 0u;
    resource_uses[0].size = source->memory_size;
    resource_uses[1].operation_index = 0u;
    resource_uses[1].resource_kind = RIN_VK_COMMAND_RESOURCE_USE_IMAGE;
    resource_uses[1].access = RIN_VK_COMMAND_RESOURCE_ACCESS_WRITE;
    resource_uses[1].image_layout = dst_image_layout;
    resource_uses[1].resource_handle = dst_image;
    resource_uses[1].offset = 0u;
    resource_uses[1].size = destination->memory_size;
done:
    if (valid) record_transfer_ops(core, &operation, 1u, resource_uses, 2u);
    else rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime, core);
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
            buffer->descriptor_bind_recorded != 0u ||
            buffer->compute_pipeline_bound != 0u ||
            buffer->compute_dispatch_count != 0u ||
            buffer->dispatch_compute_pipeline != 0u ||
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
            if (source->type == RIN_GPU_VULKAN_TRANSFER_OP_MEMORY_BARRIER) {
                *operation = *source;
                continue;
            }
            if (source->type ==
                    RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_BARRIER ||
                source->type ==
                    RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER) {
                if (!append_submission_resource(
                        resources, &resource_count,
                        source->destination_allocation,
                        RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
                            RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE))
                    return 0;
                *operation = *source;
                continue;
            }
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
    *resource_count_out = resource_count;
    return 1;
}

static RinVkResult snapshot_graphics_submission(
        struct RinVkDevice_T* device, uint32_t queue_family_index,
        uint32_t queue_index, uint32_t product_queue_id,
        RinGpuVulkanCommandBufferV1* const command_buffers[],
        uint32_t command_buffer_count,
        RinGpuVulkanGraphicsPacketV1** packet_out, size_t* packet_size_out,
        RinVulkanProductResourceV1 resources[], uint32_t* resource_count_out,
        RinVkBufferOwnershipUpdate** ownership_updates_out,
        uint32_t* ownership_update_count_out) {
    RinGpuVulkanCommandBufferV1* command;
    RinVkPipelineSlot* pipeline;
    RinVkImageViewSlot* view;
    RinVkImageSlot* image;
    RinVkBufferSlot* vertex_buffer = NULL;
    RinVkBufferOwnershipUpdate* ownership_updates = NULL;
    RinGpuVulkanGraphicsPacketV1* packet = NULL;
    uint64_t pixel_bytes, color_offset, vertex_offset = 0u;
    uint64_t vertex_size = 0u, required_vertex_bytes = 0u;
    uint32_t resource_count = 0u;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;
    size_t prefix = offsetof(RinGpuVulkanGraphicsPacketV1, shader_ir);
    size_t packet_size;
    if (packet_out) *packet_out = NULL;
    if (packet_size_out) *packet_size_out = 0u;
    if (resource_count_out) *resource_count_out = 0u;
    if (ownership_updates_out) *ownership_updates_out = NULL;
    if (ownership_update_count_out) *ownership_update_count_out = 0u;
    if (!device || !command_buffers || command_buffer_count != 1u ||
        !command_buffers[0] || !packet_out || !packet_size_out || !resources ||
        !resource_count_out || !ownership_updates_out ||
        !ownership_update_count_out)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    command = command_buffers[0];
    if (command->owner != (uintptr_t)device || !command->pool ||
        command->pool->queue_family_index != queue_family_index ||
        command->copy_count != 0u || command->transfer_op_count != 0u ||
        command->barrier_count != 0u || command->query_command_count != 0u ||
        command->event_command_count != 0u ||
        command->compute_dispatch_count != 0u ||
        command->compute_pipeline_bound != 0u ||
        command->bound_compute_pipeline != 0u ||
        command->descriptor_bind_recorded != 0u ||
        command->graphics_rendering_active != 0u ||
        command->graphics_rendering_begin_count != 1u ||
        command->graphics_rendering_end_count != 1u ||
        command->graphics_draw_count != 1u ||
        command->graphics_pipeline_bound != 1u ||
        command->bound_graphics_pipeline == 0u ||
        command->graphics_color_layout != RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
        command->graphics_width == 0u || command->graphics_width > 4096u ||
        command->graphics_height == 0u || command->graphics_height > 4096u ||
        command->graphics_vertex_count == 0u || command->graphics_vertex_count > 65535u ||
        command->graphics_instance_count != 1u || command->graphics_first_instance != 0u)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    pipeline = pipeline_slot((RinVkDevice)(void*)device,
                             command->bound_graphics_pipeline);
    if (!pipeline || pipeline->kind != RIN_VK_PIPELINE_KIND_GRAPHICS ||
        pipeline->shader_size == 0u || pipeline->fragment_shader_size == 0u ||
        !pipeline->shader_ir || !pipeline->fragment_shader_ir ||
        pipeline->graphics_backend.resource_count != 0u ||
        pipeline->graphics_backend.color_format != RIN_GPU_FORMAT_RGBA8_UNORM ||
        pipeline->graphics_backend.vertex_binding_count > 1u ||
        pipeline->graphics_backend.vertex_input_count > RIN_GPU_MAX_VERTEX_ATTRIBUTES)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    view = image_view_slot((RinVkDevice)(void*)device,
                           (RinVkImageView)command->graphics_color_view);
    image = view ? view->image : NULL;
    if (!image || image->owner != device || !image->memory ||
        image->memory_generation != image->memory->generation ||
        image->memory->owner != device || image->memory->product_allocation == 0u ||
        image->format != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
        view->format != (uint32_t)image->format ||
        view->aspect_mask != RIN_VK_IMAGE_ASPECT_COLOR_BIT ||
        (image->usage & RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0u ||
        image->samples != RIN_VK_SAMPLE_COUNT_1_BIT ||
        image->width != command->graphics_width || image->height != command->graphics_height ||
        __atomic_load_n(&image->current_layout, __ATOMIC_ACQUIRE) != command->graphics_color_layout ||
        image->ownership.transfer_pending != 0u ||
        (image->ownership.owner_queue_family != RIN_VK_QUEUE_FAMILY_IGNORED &&
         image->ownership.owner_queue_family != queue_family_index) ||
        !finite_graphics_float(pipeline->viewport.x) ||
        !finite_graphics_float(pipeline->viewport.y) ||
        !finite_graphics_float(pipeline->viewport.width) ||
        !finite_graphics_float(pipeline->viewport.height) ||
        !finite_graphics_float(pipeline->viewport.minDepth) ||
        !finite_graphics_float(pipeline->viewport.maxDepth) ||
        pipeline->viewport.x + pipeline->viewport.width > image->width ||
        pipeline->viewport.y + pipeline->viewport.height > image->height ||
        pipeline->scissor.offset.x < 0 || pipeline->scissor.offset.y < 0 ||
        (uint32_t)pipeline->scissor.offset.x > image->width ||
        (uint32_t)pipeline->scissor.offset.y > image->height ||
        pipeline->scissor.extent.width == 0u || pipeline->scissor.extent.height == 0u ||
        pipeline->scissor.extent.width > image->width - (uint32_t)pipeline->scissor.offset.x ||
        pipeline->scissor.extent.height > image->height - (uint32_t)pipeline->scissor.offset.y ||
        (uint64_t)image->width * image->height > UINT64_MAX / 4u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    pixel_bytes = (uint64_t)image->width * image->height * 4u;
    color_offset = image->memory_offset;
    if (color_offset > image->memory->requested_size ||
        pixel_bytes > image->memory->requested_size - color_offset ||
        image->memory_size < pixel_bytes)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(resources, 0, sizeof(*resources) * RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION);
    if (!append_submission_resource(resources, &resource_count,
            image->memory->product_allocation, RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE))
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;

    if (pipeline->graphics_backend.vertex_binding_count == 1u) {
        const RinGpuVertexBufferLayoutV1* layout = &pipeline->graphics_backend.vertex_bindings[0];
        RinVkBufferOwnershipRange* range;
        uint32_t range_index;
        uint64_t end_vertex;
        uint64_t vertex_span;
        if (command->graphics_vertex_buffer_bound != 1u ||
            command->graphics_vertex_buffer == 0u || layout->binding != 0u || layout->stride == 0u ||
            (uint64_t)command->graphics_first_vertex + command->graphics_vertex_count >
                UINT64_MAX / layout->stride)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        end_vertex = (uint64_t)command->graphics_first_vertex + command->graphics_vertex_count;
        vertex_span = end_vertex * layout->stride;
        vertex_buffer = buffer_slot((RinVkDevice)(void*)device,
                                    (RinVkBuffer)command->graphics_vertex_buffer);
        if (!vertex_buffer || vertex_buffer->owner != device || !vertex_buffer->memory ||
            vertex_buffer->memory_generation != vertex_buffer->memory->generation ||
            vertex_buffer->memory->owner != device ||
            vertex_buffer->memory->product_allocation == 0u ||
            (vertex_buffer->usage & RIN_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0u ||
            command->graphics_vertex_offset >= vertex_buffer->size ||
            vertex_span > vertex_buffer->size - command->graphics_vertex_offset ||
            vertex_buffer->memory_offset > UINT64_MAX - command->graphics_vertex_offset)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        vertex_offset = vertex_buffer->memory_offset + command->graphics_vertex_offset;
        required_vertex_bytes = vertex_span;
        vertex_size = vertex_buffer->size - command->graphics_vertex_offset;
        if (vertex_size < required_vertex_bytes || vertex_offset > vertex_buffer->memory->requested_size ||
            vertex_size > vertex_buffer->memory->requested_size - vertex_offset)
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        ownership_updates = (RinVkBufferOwnershipUpdate*)calloc(1u, sizeof(*ownership_updates));
        if (!ownership_updates) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        ownership_updates[0].buffer = vertex_buffer;
        ownership_updates[0].state = vertex_buffer->ownership;
        if (!rin_vk_buffer_ownership_ensure_coverage(&ownership_updates[0].state,
                command->graphics_vertex_offset,
                command->graphics_vertex_offset + required_vertex_bytes,
                RIN_VK_QUEUE_FAMILY_IGNORED, 1)) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED; goto fail;
        }
        for (range_index = 0u; range_index < ownership_updates[0].state.range_count; ++range_index) {
            range = &ownership_updates[0].state.ranges[range_index];
            if (range->offset >= command->graphics_vertex_offset + required_vertex_bytes ||
                range->offset + range->size <= command->graphics_vertex_offset) continue;
            if (range->transfer_pending ||
                (range->owner_queue_family != RIN_VK_QUEUE_FAMILY_IGNORED &&
                 range->owner_queue_family != queue_family_index)) {
                result = RIN_VK_ERROR_FEATURE_NOT_PRESENT; goto fail;
            }
            range->owner_queue_family = queue_family_index;
        }
        if (!append_submission_resource(resources, &resource_count,
                vertex_buffer->memory->product_allocation,
                RIN_VULKAN_PRODUCT_MEMORY_GPU_READ)) {
            result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY; goto fail;
        }
    } else if (command->graphics_vertex_buffer_bound != 0u ||
               command->graphics_vertex_buffer != 0u ||
               command->graphics_vertex_offset != 0u) {
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    if (pipeline->shader_size > UINT32_MAX - prefix ||
        pipeline->fragment_shader_size > UINT32_MAX - prefix - pipeline->shader_size) {
        result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY; goto fail;
    }
    packet_size = prefix + pipeline->shader_size + pipeline->fragment_shader_size;
    packet = (RinGpuVulkanGraphicsPacketV1*)calloc(1u, packet_size);
    if (!packet) { result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY; goto fail; }
    packet->struct_size = (uint32_t)packet_size;
    packet->version = RIN_GPU_VULKAN_GRAPHICS_PACKET_VERSION;
    packet->queue_family_index = queue_family_index;
    packet->queue_index = queue_index;
    packet->product_queue_id = product_queue_id;
    packet->vertex_shader_size_bytes = pipeline->shader_size;
    packet->fragment_shader_size_bytes = pipeline->fragment_shader_size;
    packet->vertex_binding = pipeline->graphics_backend.vertex_binding_count
                                 ? pipeline->graphics_backend.vertex_bindings[0].binding : 0u;
    if (vertex_buffer) {
        packet->vertex_allocation_handle = vertex_buffer->memory->product_allocation;
        packet->vertex_offset = vertex_offset;
        packet->vertex_size_bytes = vertex_size;
    }
    packet->color_allocation_handle = image->memory->product_allocation;
    packet->color_offset = color_offset;
    packet->color_size_bytes = pixel_bytes;
    packet->width = image->width; packet->height = image->height;
    packet->viewport_x = pipeline->viewport.x; packet->viewport_y = pipeline->viewport.y;
    packet->viewport_width = pipeline->viewport.width; packet->viewport_height = pipeline->viewport.height;
    packet->viewport_min_depth = pipeline->viewport.minDepth;
    packet->viewport_max_depth = pipeline->viewport.maxDepth;
    packet->scissor_x = pipeline->scissor.offset.x; packet->scissor_y = pipeline->scissor.offset.y;
    packet->scissor_width = pipeline->scissor.extent.width;
    packet->scissor_height = pipeline->scissor.extent.height;
    packet->vertex_count = command->graphics_vertex_count;
    packet->instance_count = command->graphics_instance_count;
    packet->first_vertex = command->graphics_first_vertex;
    packet->first_instance = command->graphics_first_instance;
    packet->clear_red = command->graphics_clear_red;
    packet->clear_green = command->graphics_clear_green;
    packet->clear_blue = command->graphics_clear_blue;
    packet->clear_alpha = command->graphics_clear_alpha;
    packet->pipeline = pipeline->graphics_backend;
    memcpy(packet->shader_ir, pipeline->shader_ir, pipeline->shader_size);
    memcpy(packet->shader_ir + pipeline->shader_size, pipeline->fragment_shader_ir,
           pipeline->fragment_shader_size);
    *packet_out = packet; *packet_size_out = packet_size;
    *resource_count_out = resource_count;
    *ownership_updates_out = ownership_updates;
    *ownership_update_count_out = vertex_buffer ? 1u : 0u;
    return RIN_VK_SUCCESS;
fail:
    free(packet); free(ownership_updates);
    return result;
}

static RinVkResult snapshot_compute_submission(
        struct RinVkDevice_T* device, uint32_t queue_family_index,
        uint32_t queue_index, uint32_t product_queue_id,
        RinGpuVulkanCommandBufferV1* const command_buffers[],
        uint32_t command_buffer_count,
        RinGpuVulkanComputePacketV1** packet_out, size_t* packet_size_out,
        RinVulkanProductResourceV1 resources[], uint32_t* resource_count_out,
        RinVkBufferOwnershipUpdate** ownership_updates_out,
        uint32_t* ownership_update_count_out) {
    RinGpuVulkanCommandBufferV1* command_buffer;
    RinVkPipelineSlot* pipeline;
    RinGpuVulkanComputePacketV1* packet = NULL;
    RinVkBufferOwnershipUpdate* ownership_updates = NULL;
    uint8_t resource_indices[RIN_SHADER_MAX_RESOURCES] = {0};
    uint32_t resource_count = 0u;
    uint32_t index;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;
    size_t packet_prefix = offsetof(RinGpuVulkanComputePacketV1, shader_ir);
    size_t packet_size;
    if (packet_out) *packet_out = NULL;
    if (packet_size_out) *packet_size_out = 0u;
    if (resource_count_out) *resource_count_out = 0u;
    if (ownership_updates_out) *ownership_updates_out = NULL;
    if (ownership_update_count_out) *ownership_update_count_out = 0u;
    if (!device || !command_buffers || command_buffer_count != 1u ||
        !packet_out || !packet_size_out || !resources ||
        !resource_count_out || !ownership_updates_out ||
        !ownership_update_count_out || !command_buffers[0])
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    command_buffer = command_buffers[0];
    if (command_buffer->owner != (uintptr_t)device || !command_buffer->pool ||
        command_buffer->pool->queue_family_index != queue_family_index ||
        command_buffer->copy_count != 0u ||
        command_buffer->transfer_op_count != 0u ||
        command_buffer->barrier_count != 0u ||
        command_buffer->query_command_count != 0u ||
        command_buffer->event_command_count != 0u ||
        command_buffer->compute_dispatch_count != 1u ||
        command_buffer->bound_compute_pipeline == 0u ||
        command_buffer->compute_pipeline_bound == 0u ||
        command_buffer->dispatch_compute_pipeline !=
            command_buffer->bound_compute_pipeline ||
        command_buffer->descriptor_bind_recorded == 0u ||
        command_buffer->descriptor_bind_point !=
            RIN_VK_PIPELINE_BIND_POINT_COMPUTE ||
        command_buffer->descriptor_bind_first_set != 0u ||
        command_buffer->descriptor_bind_set_count != 1u ||
        command_buffer->descriptor_dynamic_offset_count != 0u ||
        command_buffer->compute_group_count_x > 65535u ||
        command_buffer->compute_group_count_y > 65535u ||
        command_buffer->compute_group_count_z > 65535u)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    pipeline = pipeline_slot((RinVkDevice)(void*)device,
                             command_buffer->dispatch_compute_pipeline);
    if (!pipeline || pipeline->kind != RIN_VK_PIPELINE_KIND_COMPUTE ||
        pipeline->descriptor_count == 0u ||
        pipeline->descriptor_count > RIN_GPU_VULKAN_COMPUTE_MAX_BINDINGS ||
        pipeline->set_layout_count == 0u ||
        pipeline->set_layout_count >
            RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS ||
        command_buffer->descriptor_set_layouts[0] !=
            pipeline->set_layouts[0])
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (rin_gpu_vulkan_descriptor_set_matches_layout(
            &device->descriptor_runtime,
            (RinGpuVulkanDescriptorHandleV1)
                command_buffer->descriptor_sets[0],
            (RinGpuVulkanDescriptorHandleV1)pipeline->set_layouts[0]) != 1)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (pipeline->shader_size == 0u ||
        pipeline->shader_size > UINT32_MAX - packet_prefix)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    packet_size = packet_prefix + pipeline->shader_size;
    packet = (RinGpuVulkanComputePacketV1*)calloc(1u, packet_size);
    ownership_updates = (RinVkBufferOwnershipUpdate*)calloc(
        pipeline->descriptor_count, sizeof(*ownership_updates));
    if (!packet || !ownership_updates) {
        free(packet);
        free(ownership_updates);
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    packet->struct_size = (uint32_t)packet_size;
    packet->version = RIN_GPU_VULKAN_COMPUTE_PACKET_VERSION;
    packet->queue_family_index = queue_family_index;
    packet->queue_index = queue_index;
    packet->product_queue_id = product_queue_id;
    packet->group_count_x = command_buffer->compute_group_count_x;
    packet->group_count_y = command_buffer->compute_group_count_y;
    packet->group_count_z = command_buffer->compute_group_count_z;
    packet->binding_count = pipeline->descriptor_count;
    packet->shader_size_bytes = pipeline->shader_size;
    memcpy(packet->shader_ir, pipeline->shader_ir, pipeline->shader_size);
    memset(resources, 0, sizeof(*resources) *
                           RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION);

    for (index = 0u; index < pipeline->descriptor_count; ++index) {
        const RinSpirvDescriptorV1* descriptor = &pipeline->descriptors[index];
        RinGpuVulkanDescriptorWriteV1 write;
        RinVkBufferSlot* buffer;
        RinVkBufferOwnershipUpdate* ownership_update =
            &ownership_updates[index];
        RinGpuVulkanComputeBindingV1* binding = &packet->bindings[index];
        uint64_t resource_offset;
        uint64_t resource_end;
        uint64_t address;
        uint32_t required_access = 0u;
        uint32_t owner_index;
        int graphics_result;
        if (descriptor->set != 0u ||
            descriptor->resource_kind != RIN_SHADER_RESOURCE_STORAGE_BUFFER ||
            descriptor->resource_index >= pipeline->descriptor_count ||
            resource_indices[descriptor->resource_index] != 0u) {
            result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
            goto fail;
        }
        graphics_result = rin_gpu_vulkan_descriptor_set_get_write(
            &device->descriptor_runtime,
            (RinGpuVulkanDescriptorHandleV1)
                command_buffer->descriptor_sets[0],
            descriptor->set, descriptor->binding, 0u, &write);
        if (graphics_result != RIN_GPU_VULKAN_GRAPHICS_OK ||
            write.set != descriptor->set ||
            write.binding != descriptor->binding || write.array_element != 0u ||
            write.resource_index != descriptor->resource_index ||
            write.descriptor_type != RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_BUFFER ||
            write.resource == 0u || write.size_bytes == 0u ||
            write.flags != 0u || write.reserved != 0u ||
            write.mip_level != 0u || write.array_layer != 0u ||
            write.access == 0u ||
            (write.access & ~(RIN_GPU_RESOURCE_READ |
                              RIN_GPU_RESOURCE_WRITE)) != 0u) {
            result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
            goto fail;
        }
        buffer = buffer_slot((RinVkDevice)(void*)device,
                             (RinVkBuffer)write.resource);
        if (!buffer || buffer->owner != device || !buffer->memory ||
            buffer->memory_generation != buffer->memory->generation ||
            buffer->memory->owner != device ||
            buffer->memory->product_allocation == 0u ||
            (write.descriptor_type == RIN_GPU_VULKAN_DESCRIPTOR_STORAGE_BUFFER
                 ? (buffer->usage & RIN_VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == 0u
                 : (buffer->usage & RIN_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0u) ||
            !checked_buffer_address(buffer, write.offset, write.size_bytes,
                                    &address) ||
            buffer->memory_offset > UINT64_MAX - write.offset) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto fail;
        }
        resource_offset = buffer->memory_offset + write.offset;
        if (resource_offset > buffer->memory->requested_size ||
            write.size_bytes > buffer->memory->requested_size - resource_offset ||
            resource_offset > UINT64_MAX - write.size_bytes) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto fail;
        }
        resource_end = write.offset + write.size_bytes;
        ownership_update->buffer = buffer;
        ownership_update->state = buffer->ownership;
        if (!rin_vk_buffer_ownership_ensure_coverage(
                &ownership_update->state, write.offset, resource_end,
                RIN_VK_QUEUE_FAMILY_IGNORED, 1)) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto fail;
        }
        for (owner_index = 0u;
             owner_index < ownership_update->state.range_count; ++owner_index) {
            RinVkBufferOwnershipRange* range =
                &ownership_update->state.ranges[owner_index];
            if (range->offset >= resource_end ||
                range->offset + range->size <= write.offset)
                continue;
            if (range->transfer_pending != 0u ||
                (range->owner_queue_family != RIN_VK_QUEUE_FAMILY_IGNORED &&
                 range->owner_queue_family != queue_family_index)) {
                result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
                goto fail;
            }
            range->owner_queue_family = queue_family_index;
        }
        for (owner_index = 0u; owner_index < index; ++owner_index)
            if (ownership_updates[owner_index].buffer == buffer ||
                packet->bindings[owner_index].allocation_handle ==
                    buffer->memory->product_allocation) {
                result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
                goto fail;
            }
        binding->allocation_handle = buffer->memory->product_allocation;
        binding->offset = resource_offset;
        binding->size_bytes = write.size_bytes;
        binding->resource_index = descriptor->resource_index;
        binding->access = write.access;
        binding->reserved[0] = 0u;
        binding->reserved[1] = 0u;
        resource_indices[descriptor->resource_index] = 1u;
        if ((write.access & RIN_GPU_RESOURCE_READ) != 0u)
            required_access |= RIN_VULKAN_PRODUCT_MEMORY_GPU_READ;
        if ((write.access & RIN_GPU_RESOURCE_WRITE) != 0u)
            required_access |= RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE;
        if (!append_submission_resource(resources, &resource_count,
                                        binding->allocation_handle,
                                        required_access)) {
            result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
            goto fail;
        }
    }
    for (index = 0u; index < pipeline->descriptor_count; ++index)
        if (resource_indices[index] == 0u) {
            result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
            goto fail;
        }
    *packet_out = packet;
    *packet_size_out = packet_size;
    *resource_count_out = resource_count;
    *ownership_updates_out = ownership_updates;
    *ownership_update_count_out = pipeline->descriptor_count;
    return RIN_VK_SUCCESS;

fail:
    free(packet);
    free(ownership_updates);
    return (RinVkResult)result;
}

static int ownership_semaphore_wait_matches(
        struct RinVkDevice_T* device, uint32_t signal_count,
        const RinVkSemaphore signal_semaphores[],
        const uint64_t signal_values[], uint32_t wait_count,
        const RinVkSemaphore wait_semaphores[],
        const uint64_t wait_values[]) {
    uint32_t wait_index;
    uint32_t signal_index;
    if (!device || signal_count == 0u || wait_count == 0u ||
        !signal_semaphores || !signal_values || !wait_semaphores ||
        !wait_values)
        return 0;
    for (wait_index = 0u; wait_index < wait_count; ++wait_index) {
        RinVkSemaphoreSlot* wait_slot =
            semaphore_slot((RinVkDevice)device, wait_semaphores[wait_index]);
        if (!wait_slot) continue;
        for (signal_index = 0u; signal_index < signal_count; ++signal_index) {
            RinVkSemaphoreSlot* signal_slot = semaphore_slot(
                (RinVkDevice)device, signal_semaphores[signal_index]);
            if (wait_semaphores[wait_index] != signal_semaphores[signal_index] ||
                !signal_slot || signal_slot->type != wait_slot->type)
                continue;
            if (wait_slot->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
                if (wait_values[wait_index] >= signal_values[signal_index])
                    return 1;
            } else if (signal_values[signal_index] == 0u) {
                return 1;
            }
        }
    }
    return 0;
}

static int stage_image_state_update(
        RinVkImageSlot* image,
        RinVkImageSlot* images[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS],
        uint32_t layouts[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS],
        RinVkImageOwnershipState
            ownerships[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS],
        uint32_t* update_count, uint32_t* update_index_out) {
    uint32_t update_index;
    if (!image || !images || !layouts || !ownerships || !update_count ||
        !update_index_out)
        return 0;
    for (update_index = 0u; update_index < *update_count; ++update_index)
        if (images[update_index] == image) break;
    if (update_index == *update_count) {
        if (*update_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
            return 0;
        images[update_index] = image;
        layouts[update_index] = __atomic_load_n(&image->current_layout,
                                                __ATOMIC_ACQUIRE);
        ownerships[update_index] = image->ownership;
        ++*update_count;
    }
    *update_index_out = update_index;
    return 1;
}

static int prepare_image_layout_updates(
        struct RinVkDevice_T* device,
        const RinGpuVulkanTransferPacketV2* packet,
        uint32_t queue_family_index, uint32_t wait_count,
        const RinVkSemaphore wait_semaphores[], const uint64_t wait_values[],
        uint32_t signal_count, const RinVkSemaphore signal_semaphores[],
        const uint64_t signal_values[],
        const RinVkCommandResourceUse resource_uses[], uint32_t use_count,
        RinVkImageSlot* images[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS],
        uint32_t layouts[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS],
        RinVkImageOwnershipState
            ownerships[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS],
        uint32_t* count_out) {
    uint32_t update_count = 0u;
    uint32_t operation_index;
    if (!device || !packet || !images || !layouts || !ownerships || !count_out ||
        (use_count != 0u && !resource_uses) ||
        use_count > RIN_VK_MAX_SUBMISSION_RESOURCE_USES ||
        packet->op_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
        return 0;
    for (operation_index = 0u; operation_index < packet->op_count;
         ++operation_index) {
        const RinGpuVulkanTransferOpV2* operation =
            &packet->operations[operation_index];
        RinVkImageSlot* image;
        uint32_t update_index;
        uint32_t current_layout;
        uint32_t src_queue_family;
        uint32_t dst_queue_family;
        RinVkImageOwnershipState* ownership;
        uint32_t use_index;
        if (operation->type == RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER) {
            image = image_slot((RinVkDevice)device,
                               (RinVkImage)operation->source_allocation);
            if (!image || !image->memory ||
                !stage_image_state_update(image, images, layouts, ownerships,
                                          &update_count, &update_index))
                return 0;
            current_layout = layouts[update_index];
            ownership = &ownerships[update_index];
            src_queue_family = operation->destination_height;
            dst_queue_family = operation->filter;
        if (src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED &&
            dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED) {
            if (current_layout != operation->source_width ||
                ownership->transfer_pending != 0u ||
                (ownership->owner_queue_family !=
                     RIN_VK_QUEUE_FAMILY_IGNORED &&
                 ownership->owner_queue_family != queue_family_index))
                return 0;
            if (ownership->owner_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED)
                ownership->owner_queue_family = queue_family_index;
            layouts[update_index] = operation->source_height;
            continue;
        }
        if (src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED ||
            dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED ||
            src_queue_family >= device->physical_profile.queue_family_count ||
            dst_queue_family >= device->physical_profile.queue_family_count)
            return 0;
        if (src_queue_family == dst_queue_family) {
            if (queue_family_index != src_queue_family ||
                current_layout != operation->source_width ||
                ownership->transfer_pending != 0u ||
                (ownership->owner_queue_family !=
                     RIN_VK_QUEUE_FAMILY_IGNORED &&
                 ownership->owner_queue_family != queue_family_index))
                return 0;
            if (ownership->owner_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED)
                ownership->owner_queue_family = queue_family_index;
            layouts[update_index] = operation->source_height;
        } else if (queue_family_index == src_queue_family) {
            if (current_layout != operation->source_width ||
                ownership->transfer_pending != 0u || signal_count == 0u ||
                (ownership->owner_queue_family !=
                     RIN_VK_QUEUE_FAMILY_IGNORED &&
                 ownership->owner_queue_family != src_queue_family))
                return 0;
            ownership->owner_queue_family = src_queue_family;
            ownership->transfer_pending = 1u;
            ownership->transfer_source_family = src_queue_family;
            ownership->transfer_destination_family = dst_queue_family;
            ownership->transfer_old_layout = operation->source_width;
            ownership->transfer_new_layout = operation->source_height;
            ownership->transfer_semaphore_count = signal_count;
            memset(ownership->transfer_semaphores, 0,
                   sizeof(ownership->transfer_semaphores));
            memset(ownership->transfer_semaphore_values, 0,
                   sizeof(ownership->transfer_semaphore_values));
            memcpy(ownership->transfer_semaphores, signal_semaphores,
                   sizeof(*signal_semaphores) * signal_count);
            memcpy(ownership->transfer_semaphore_values, signal_values,
                   sizeof(*signal_values) * signal_count);
            layouts[update_index] = operation->source_height;
        } else if (queue_family_index == dst_queue_family) {
            if (ownership->transfer_pending == 0u ||
                ownership->transfer_source_family != src_queue_family ||
                ownership->transfer_destination_family != dst_queue_family ||
                ownership->transfer_old_layout != operation->source_width ||
                ownership->transfer_new_layout != operation->source_height ||
                current_layout != operation->source_height ||
                !ownership_semaphore_wait_matches(
                    device, ownership->transfer_semaphore_count,
                    ownership->transfer_semaphores,
                    ownership->transfer_semaphore_values, wait_count,
                    wait_semaphores, wait_values))
                return 0;
            ownership->owner_queue_family = dst_queue_family;
            ownership->transfer_pending = 0u;
            ownership->transfer_source_family = 0u;
            ownership->transfer_destination_family = 0u;
            ownership->transfer_old_layout = 0u;
            ownership->transfer_new_layout = 0u;
            ownership->transfer_semaphore_count = 0u;
            memset(ownership->transfer_semaphores, 0,
                   sizeof(ownership->transfer_semaphores));
            memset(ownership->transfer_semaphore_values, 0,
                   sizeof(ownership->transfer_semaphore_values));
            layouts[update_index] = current_layout;
        } else {
            return 0;
        }
        }
        for (use_index = 0u; use_index < use_count; ++use_index) {
            const RinVkCommandResourceUse* use = &resource_uses[use_index];
            RinVkImageOwnershipState* use_ownership;
            if (use->operation_index != operation_index ||
                use->resource_kind != RIN_VK_COMMAND_RESOURCE_USE_IMAGE)
                continue;
            image = image_slot((RinVkDevice)device,
                               (RinVkImage)use->resource_handle);
            if (!image || !image->memory || use->offset != 0u ||
                use->size != image->memory_size ||
                !stage_image_state_update(image, images, layouts, ownerships,
                                          &update_count, &update_index))
                return 0;
            use_ownership = &ownerships[update_index];
            if (layouts[update_index] != use->image_layout ||
                use_ownership->transfer_pending != 0u ||
                (use_ownership->owner_queue_family !=
                     RIN_VK_QUEUE_FAMILY_IGNORED &&
                 use_ownership->owner_queue_family != queue_family_index))
                return 0;
            if (use_ownership->owner_queue_family ==
                RIN_VK_QUEUE_FAMILY_IGNORED)
                use_ownership->owner_queue_family = queue_family_index;
        }
    }
    *count_out = update_count;
    return 1;
}

static void commit_image_layout_updates(
        RinVkImageSlot* const images[], const uint32_t layouts[],
        const RinVkImageOwnershipState ownerships[],
        uint32_t update_count) {
    uint32_t index;
    for (index = 0u; index < update_count; ++index) {
        images[index]->ownership = ownerships[index];
        __atomic_store_n(&images[index]->current_layout, layouts[index],
                         __ATOMIC_RELEASE);
    }
}

static int buffer_ownership_wait_matches(
        struct RinVkDevice_T* device,
        const RinVkBufferOwnershipRange* ownership, uint32_t wait_count,
        const RinVkSemaphore wait_semaphores[],
        const uint64_t wait_values[]) {
    if (!ownership) return 0;
    return ownership_semaphore_wait_matches(
        device, ownership->transfer_semaphore_count,
        ownership->transfer_semaphores,
        ownership->transfer_semaphore_values, wait_count, wait_semaphores,
        wait_values);
}

static int prepare_buffer_ownership_updates(
        struct RinVkDevice_T* device,
        const RinGpuVulkanTransferPacketV2* packet,
        uint32_t queue_family_index, uint32_t wait_count,
        const RinVkSemaphore wait_semaphores[], const uint64_t wait_values[],
        uint32_t signal_count, const RinVkSemaphore signal_semaphores[],
        const uint64_t signal_values[],
        const RinVkCommandResourceUse resource_uses[], uint32_t use_count,
        RinVkBufferOwnershipUpdate updates[],
        uint32_t* update_count_out) {
    uint32_t update_count = 0u;
    uint32_t operation_index;
    uint32_t merge_index;
    if (!device || !packet || !updates || !update_count_out ||
        (use_count != 0u && !resource_uses) ||
        use_count > RIN_VK_MAX_SUBMISSION_RESOURCE_USES ||
        packet->op_count > RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
        return 0;
    for (operation_index = 0u; operation_index < packet->op_count;
         ++operation_index) {
        const RinGpuVulkanTransferOpV2* operation =
            &packet->operations[operation_index];
        RinVkBufferSlot* buffer;
        RinVkBufferOwnershipUpdate* update = NULL;
        uint64_t range_start;
        uint64_t range_end;
        uint64_t buffer_start;
        uint32_t update_index;
        uint32_t src_queue_family;
        uint32_t dst_queue_family;
        uint32_t range_index;
        uint32_t use_index;
        if (operation->type == RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_BARRIER) {
        buffer = buffer_slot((RinVkDevice)device,
                             (RinVkBuffer)operation->source_allocation);
        if (!buffer || !buffer->memory ||
            buffer->memory->gpu_virtual_address >
                UINT64_MAX - buffer->memory_offset)
            return 0;
        for (update_index = 0u; update_index < update_count; ++update_index)
            if (updates[update_index].buffer == buffer) {
                update = &updates[update_index];
                break;
            }
        if (!update) {
            if (update_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return 0;
            update = &updates[update_count++];
            update->buffer = buffer;
            update->state = buffer->ownership;
        }
        src_queue_family = operation->source_width;
        dst_queue_family = operation->source_height;
        buffer_start = buffer->memory->gpu_virtual_address +
                       buffer->memory_offset;
        if (operation->destination_gpu_address < buffer_start ||
            operation->size_bytes == 0u ||
            operation->destination_gpu_address - buffer_start > buffer->size ||
            operation->size_bytes >
                buffer->size -
                    (operation->destination_gpu_address - buffer_start))
            return 0;
        range_start = operation->destination_gpu_address - buffer_start;
        range_end = range_start + operation->size_bytes;
        if (src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED &&
            dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED) {
            if (!rin_vk_buffer_ownership_ensure_coverage(
                    &update->state, range_start, range_end,
                    queue_family_index, 1))
                return 0;
            for (range_index = 0u;
                 range_index < update->state.range_count; ++range_index) {
                RinVkBufferOwnershipRange* range =
                    &update->state.ranges[range_index];
                if (range->offset < range_start ||
                    range->offset >= range_end)
                    continue;
                if (range->transfer_pending != 0u ||
                    range->owner_queue_family != queue_family_index)
                    return 0;
            }
            continue;
        }
        if (src_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED ||
            dst_queue_family == RIN_VK_QUEUE_FAMILY_IGNORED ||
            src_queue_family >= device->physical_profile.queue_family_count ||
            dst_queue_family >= device->physical_profile.queue_family_count)
            return 0;
        if (src_queue_family == dst_queue_family) {
            if (queue_family_index != src_queue_family ||
                !rin_vk_buffer_ownership_ensure_coverage(
                    &update->state, range_start, range_end,
                    src_queue_family, 1))
                return 0;
            for (range_index = 0u;
                 range_index < update->state.range_count; ++range_index) {
                const RinVkBufferOwnershipRange* range =
                    &update->state.ranges[range_index];
                if (range->offset < range_start ||
                    range->offset >= range_end)
                    continue;
                if (range->transfer_pending != 0u ||
                    range->owner_queue_family != src_queue_family)
                    return 0;
            }
            continue;
        }
        if (queue_family_index == src_queue_family) {
            if (signal_count == 0u ||
                !rin_vk_buffer_ownership_ensure_coverage(
                    &update->state, range_start, range_end,
                    src_queue_family, 1))
                return 0;
            for (range_index = 0u;
                 range_index < update->state.range_count; ++range_index) {
                RinVkBufferOwnershipRange* range =
                    &update->state.ranges[range_index];
                if (range->offset < range_start ||
                    range->offset >= range_end)
                    continue;
                if (range->transfer_pending != 0u ||
                    range->owner_queue_family != src_queue_family)
                    return 0;
                range->transfer_pending = 1u;
                range->transfer_source_family = src_queue_family;
                range->transfer_destination_family = dst_queue_family;
                range->transfer_offset = range_start;
                range->transfer_size = operation->size_bytes;
                range->transfer_semaphore_count = signal_count;
                memset(range->transfer_semaphores, 0,
                       sizeof(range->transfer_semaphores));
                memset(range->transfer_semaphore_values, 0,
                       sizeof(range->transfer_semaphore_values));
                memcpy(range->transfer_semaphores, signal_semaphores,
                       sizeof(*signal_semaphores) * signal_count);
                memcpy(range->transfer_semaphore_values, signal_values,
                       sizeof(*signal_values) * signal_count);
            }
        } else if (queue_family_index == dst_queue_family) {
            if (!rin_vk_buffer_ownership_ensure_coverage(
                    &update->state, range_start, range_end,
                    dst_queue_family, 0))
                return 0;
            for (range_index = 0u;
                 range_index < update->state.range_count; ++range_index) {
                RinVkBufferOwnershipRange* range =
                    &update->state.ranges[range_index];
                if (range->offset < range_start ||
                    range->offset >= range_end)
                    continue;
                if (range->transfer_pending == 0u ||
                    range->transfer_source_family != src_queue_family ||
                    range->transfer_destination_family != dst_queue_family ||
                    range->transfer_offset != range_start ||
                    range->transfer_size != operation->size_bytes ||
                    !buffer_ownership_wait_matches(
                        device, range, wait_count, wait_semaphores,
                        wait_values))
                    return 0;
                range->owner_queue_family = dst_queue_family;
                range->transfer_pending = 0u;
                range->transfer_source_family = 0u;
                range->transfer_destination_family = 0u;
                range->transfer_semaphore_count = 0u;
                range->transfer_offset = 0u;
                range->transfer_size = 0u;
                memset(range->transfer_semaphores, 0,
                       sizeof(range->transfer_semaphores));
                memset(range->transfer_semaphore_values, 0,
                       sizeof(range->transfer_semaphore_values));
            }
        } else {
            return 0;
        }
        }
        for (use_index = 0u; use_index < use_count; ++use_index) {
            const RinVkCommandResourceUse* use = &resource_uses[use_index];
            uint64_t use_end;
            if (use->operation_index != operation_index ||
                use->resource_kind != RIN_VK_COMMAND_RESOURCE_USE_BUFFER)
                continue;
            buffer = buffer_slot((RinVkDevice)device,
                                 (RinVkBuffer)use->resource_handle);
            if (!buffer || !buffer->memory || use->size == 0u ||
                use->offset > buffer->size ||
                use->size > buffer->size - use->offset)
                return 0;
            for (update_index = 0u; update_index < update_count; ++update_index)
                if (updates[update_index].buffer == buffer) break;
            if (update_index == update_count) {
                if (update_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                    return 0;
                updates[update_count].buffer = buffer;
                updates[update_count].state = buffer->ownership;
                ++update_count;
            }
            use_end = use->offset + use->size;
            if (!rin_vk_buffer_ownership_ensure_coverage(
                    &updates[update_index].state, use->offset, use_end,
                    queue_family_index, 1))
                return 0;
            for (range_index = 0u;
                 range_index < updates[update_index].state.range_count;
                 ++range_index) {
                const RinVkBufferOwnershipRange* range =
                    &updates[update_index].state.ranges[range_index];
                if (range->offset < use->offset ||
                    range->offset >= use_end)
                    continue;
                if (range->transfer_pending != 0u ||
                    range->owner_queue_family != queue_family_index)
                    return 0;
            }
        }
    }
    for (merge_index = 0u; merge_index < update_count; ++merge_index)
        if (!rin_vk_buffer_ownership_merge_adjacent(&updates[merge_index].state))
            return 0;
    *update_count_out = update_count;
    return 1;
}

static void commit_buffer_ownership_updates(
        const RinVkBufferOwnershipUpdate updates[], uint32_t update_count) {
    uint32_t index;
    for (index = 0u; index < update_count; ++index)
        updates[index].buffer->ownership = updates[index].state;
}

RinVkResult RIN_VKAPI_CALL vkQueueSubmit(
    RinVkQueue queue, uint32_t submit_count, const RinVkSubmitInfo* submits,
    uint64_t fence) {
    struct RinVkQueue_T* queue_slot_value = queue_slot(queue);
    struct RinVkDevice_T* device;
    RinVkSubmitInfo request;
    RinGpuVulkanCommandBufferV1*
        command_buffers[RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS];
    RinGpuVulkanComputePacketV1* validation_compute_packet = NULL;
    size_t validation_compute_packet_size = 0u;
    RinVkBufferOwnershipUpdate* validation_compute_ownership_updates = NULL;
    uint32_t validation_compute_ownership_update_count = 0u;
    RinGpuVulkanGraphicsPacketV1* validation_graphics_packet = NULL;
    size_t validation_graphics_packet_size = 0u;
    RinVkBufferOwnershipUpdate* validation_graphics_ownership_updates = NULL;
    uint32_t validation_graphics_ownership_update_count = 0u;
    RinVulkanProductResourceV1
        resources[RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION];
    RinGpuVulkanTransferPacketV2 validation_packet_v2;
    RinVkCommandResourceUse
        pending_resource_uses[RIN_VK_MAX_SUBMISSION_RESOURCE_USES];
    RinVkSubmissionSlot* slot = NULL;
    RinVkSemaphore wait_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinVkSemaphore signal_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t wait_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t signal_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    const RinVkTimelineSemaphoreSubmitInfo* timeline_submit = NULL;
    RinVkResult result;
    uint32_t resource_count = 0u;
    uint32_t pending_resource_use_count = 0u;
    uint32_t index;
    int compute_submission = 0;
    int graphics_submission = 0;
    int sync_locked = 0;
    int waits_ready = 1;
    RinVkImageSlot*
        pending_layout_images[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS];
    uint32_t pending_layouts[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS];
    RinVkImageOwnershipState
        pending_image_ownership[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS];
    uint32_t pending_layout_count = 0u;
    RinVkBufferOwnershipUpdate
        pending_buffer_ownership[RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS];
    uint32_t pending_buffer_ownership_count = 0u;

    if (!queue_slot_value) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (submit_count > 1u) {
        if (!submits) return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        for (index = 0u; index < submit_count; ++index) {
            result = vkQueueSubmit(
                queue, 1u, &submits[index],
                index + 1u == submit_count ? fence : 0u);
            if (result != RIN_VK_SUCCESS) return result;
        }
        return RIN_VK_SUCCESS;
    }
    if (__atomic_exchange_n(&queue_slot_value->submit_lock, 1u,
                            __ATOMIC_ACQUIRE) != 0u) {
        return RIN_VK_NOT_READY;
    }
    device = queue_slot_value->device;
    result = maintain_device_submissions_until_available(device);
    if (result != RIN_VK_SUCCESS) goto done;
    if (__atomic_load_n(&device->descriptor_validation_error,
                        __ATOMIC_ACQUIRE) != 0u) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (submit_count != 0u && !submits) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    if (submit_count == 0u) {
        memset(&request, 0, sizeof(request));
        request.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    } else {
        request = submits[0];
    }
    if (request.sType != RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO ||
        request.waitSemaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        request.signalSemaphoreCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        (request.waitSemaphoreCount != 0u &&
         (!request.pWaitSemaphores || !request.pWaitDstStageMask)) ||
        (request.signalSemaphoreCount != 0u && !request.pSignalSemaphores) ||
        request.commandBufferCount > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS ||
        (request.commandBufferCount != 0u && !request.pCommandBuffers)) {
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
    sync_lock();
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
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE) {
            if (!timeline_submit) {
                result = RIN_VK_ERROR_INITIALIZATION_FAILED;
                goto done;
            }
            if (__atomic_load_n(&semaphore->value, __ATOMIC_ACQUIRE) <
                wait_values[index])
                waits_ready = 0;
        } else {
            if (timeline_submit && wait_values[index] != 0u) {
                result = RIN_VK_ERROR_INITIALIZATION_FAILED;
                goto done;
            }
            if (__atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u ||
                __atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) == 0u)
                waits_ready = 0;
        }
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY &&
            __atomic_load_n(&semaphore->waiter_count, __ATOMIC_ACQUIRE) ==
                UINT32_MAX) {
            result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
    }
    for (index = 0u; index < request.signalSemaphoreCount; ++index) {
        RinVkSemaphoreSlot* semaphore =
            semaphore_slot(device, signal_semaphores[index]);
        if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY) {
            if (__atomic_load_n(&semaphore->pending, __ATOMIC_ACQUIRE) != 0u) {
                result = RIN_VK_NOT_READY;
                goto done;
            }
            if ((timeline_submit && signal_values[index] != 0u) ||
                __atomic_load_n(&semaphore->signaled, __ATOMIC_ACQUIRE) != 0u) {
                result = RIN_VK_ERROR_INITIALIZATION_FAILED;
                goto done;
            }
        } else {
            uint64_t pending_minimum = 0u;
            uint64_t pending_maximum = 0u;
            const uint64_t current = __atomic_load_n(
                &semaphore->value, __ATOMIC_ACQUIRE);
            const int has_pending = timeline_pending_signal_range(
                device, signal_semaphores[index], NULL, &pending_minimum,
                &pending_maximum);
            if (!timeline_submit || signal_values[index] <= current ||
                (has_pending && signal_values[index] <= pending_maximum)) {
                result = RIN_VK_ERROR_INITIALIZATION_FAILED;
                goto done;
            }
        }
    }
    if (request.commandBufferCount == 0u) {
        if (request.waitSemaphoreCount == 0u &&
            request.signalSemaphoreCount == 0u && fence == 0u) {
            result = RIN_VK_SUCCESS;
            goto done;
        }
        slot = reserve_submission_slot();
        if (!slot) {
            result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
        slot->owner = device;
        slot->queue_id = queue_slot_value->queue_index;
        if (device->next_submission_order == 0u ||
            device->next_submission_order == UINT64_MAX) {
            clear_submission_slot(slot);
            result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
        slot->order = device->next_submission_order++;
        slot->fence = (RinVkFence)fence;
        slot->wait_semaphore_count = request.waitSemaphoreCount;
        slot->signal_semaphore_count = request.signalSemaphoreCount;
        memcpy(slot->wait_semaphores, wait_semaphores,
               sizeof(RinVkSemaphore) * request.waitSemaphoreCount);
        memcpy(slot->wait_semaphore_values, wait_values,
               sizeof(uint64_t) * request.waitSemaphoreCount);
        memcpy(slot->signal_semaphores, signal_semaphores,
               sizeof(RinVkSemaphore) * request.signalSemaphoreCount);
        memcpy(slot->signal_semaphore_values, signal_values,
               sizeof(uint64_t) * request.signalSemaphoreCount);
        for (index = 0u; index < request.waitSemaphoreCount; ++index) {
            RinVkSemaphoreSlot* semaphore =
                semaphore_slot(device, wait_semaphores[index]);
            if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY) {
                const uint32_t waiter_count = __atomic_load_n(
                    &semaphore->waiter_count, __ATOMIC_ACQUIRE);
                __atomic_store_n(&semaphore->waiter_count, waiter_count + 1u,
                                 __ATOMIC_RELEASE);
                slot->waits_reserved = 1u;
            }
        }
        if (fence != 0u) {
            RinVkFenceSlot* fence_value =
                fence_slot(device, (RinVkFence)fence);
            __atomic_store_n(&fence_value->pending, 1u, __ATOMIC_RELEASE);
        }
        for (index = 0u; index < request.signalSemaphoreCount; ++index) {
            RinVkSemaphoreSlot* semaphore =
                semaphore_slot(device, signal_semaphores[index]);
            __atomic_store_n(&semaphore->pending, 1u, __ATOMIC_RELEASE);
            if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE)
                semaphore->pending_value = signal_values[index];
        }
        __atomic_store_n(&slot->state, RIN_VK_SUBMISSION_WAITING,
                         __ATOMIC_RELEASE);
        result = dispatch_ready_waiting_submissions(device);
        goto done;
    }
    for (index = 0u; index < request.commandBufferCount; ++index)
        command_buffers[index] =
            (RinGpuVulkanCommandBufferV1*)(void*)request.pCommandBuffers[index];
    if (rin_gpu_vulkan_command_buffers_validate_submit(
            &g_command_runtime, request.commandBufferCount,
            command_buffers) != RIN_GPU_VULKAN_COMMAND_OK ||
        !validate_submission_query_events(device, request.commandBufferCount,
                                           command_buffers)) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    for (index = 0u; index < request.commandBufferCount; ++index)
        if (command_buffers[index]->compute_dispatch_count != 0u ||
            command_buffers[index]->compute_pipeline_bound != 0u ||
            command_buffers[index]->dispatch_compute_pipeline != 0u)
            compute_submission = 1;
    for (index = 0u; index < request.commandBufferCount; ++index)
        if (command_buffers[index]->graphics_rendering_active != 0u ||
            command_buffers[index]->graphics_rendering_begin_count != 0u ||
            command_buffers[index]->graphics_rendering_end_count != 0u ||
            command_buffers[index]->graphics_draw_count != 0u ||
            command_buffers[index]->graphics_pipeline_bound != 0u ||
            command_buffers[index]->bound_graphics_pipeline != 0u ||
            command_buffers[index]->graphics_vertex_buffer_bound != 0u)
            graphics_submission = 1;
    if (compute_submission && graphics_submission) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    if (compute_submission) {
        result = snapshot_compute_submission(
            device, queue_slot_value->queue_family_index,
            queue_slot_value->family_queue_index, queue_slot_value->queue_index,
            command_buffers, request.commandBufferCount,
            &validation_compute_packet, &validation_compute_packet_size,
            resources, &resource_count, &validation_compute_ownership_updates,
            &validation_compute_ownership_update_count);
        if (result != RIN_VK_SUCCESS) goto done;
        free(validation_compute_packet);
        validation_compute_packet = NULL;
        free(validation_compute_ownership_updates);
        validation_compute_ownership_updates = NULL;
        memset(&validation_packet_v2, 0, sizeof(validation_packet_v2));
        validation_packet_v2.struct_size = sizeof(validation_packet_v2);
        validation_packet_v2.version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2;
        resource_count = 0u;
    } else if (graphics_submission) {
        result = snapshot_graphics_submission(
            device, queue_slot_value->queue_family_index,
            queue_slot_value->family_queue_index, queue_slot_value->queue_index,
            command_buffers, request.commandBufferCount,
            &validation_graphics_packet, &validation_graphics_packet_size,
            resources, &resource_count, &validation_graphics_ownership_updates,
            &validation_graphics_ownership_update_count);
        if (result != RIN_VK_SUCCESS) goto done;
        free(validation_graphics_packet);
        validation_graphics_packet = NULL;
        free(validation_graphics_ownership_updates);
        validation_graphics_ownership_updates = NULL;
        memset(&validation_packet_v2, 0, sizeof(validation_packet_v2));
        validation_packet_v2.struct_size = sizeof(validation_packet_v2);
        validation_packet_v2.version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_2;
        resource_count = 0u;
    } else if (!snapshot_submission_packet_v2(
                   device, queue_slot_value->queue_family_index,
                   command_buffers, request.commandBufferCount,
                   &validation_packet_v2, resources, &resource_count)) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (!snapshot_command_resource_uses(
            command_buffers, request.commandBufferCount,
            validation_packet_v2.op_count, pending_resource_uses,
            &pending_resource_use_count)) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (!prepare_image_layout_updates(
            device, &validation_packet_v2,
            queue_slot_value->queue_family_index,
            request.waitSemaphoreCount, wait_semaphores, wait_values,
            request.signalSemaphoreCount, signal_semaphores, signal_values,
            pending_resource_uses, pending_resource_use_count,
            pending_layout_images, pending_layouts, pending_image_ownership,
            &pending_layout_count)) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (!prepare_buffer_ownership_updates(
            device, &validation_packet_v2,
            queue_slot_value->queue_family_index,
            request.waitSemaphoreCount, wait_semaphores, wait_values,
            request.signalSemaphoreCount, signal_semaphores, signal_values,
            pending_resource_uses, pending_resource_use_count,
            pending_buffer_ownership,
            &pending_buffer_ownership_count)) {
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
    if (device->next_submission_order == 0u ||
        device->next_submission_order == UINT64_MAX) {
        clear_submission_slot(slot);
        result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        goto done;
    }
    slot->order = device->next_submission_order++;
    slot->command_buffer_count = request.commandBufferCount;
    memcpy(slot->command_buffers, command_buffers,
           sizeof(*command_buffers) * request.commandBufferCount);
    if (compute_submission) {
        result = snapshot_compute_submission(
            device, queue_slot_value->queue_family_index,
            queue_slot_value->family_queue_index, queue_slot_value->queue_index,
            command_buffers, request.commandBufferCount, &slot->compute_packet,
            &slot->compute_packet_size, slot->resources,
            &slot->resource_count, &slot->compute_buffer_ownership_updates,
            &slot->compute_buffer_ownership_update_count);
        if (result != RIN_VK_SUCCESS) {
            clear_submission_slot(slot);
            goto done;
        }
    } else if (graphics_submission) {
        result = snapshot_graphics_submission(
            device, queue_slot_value->queue_family_index,
            queue_slot_value->family_queue_index, queue_slot_value->queue_index,
            command_buffers, request.commandBufferCount, &slot->graphics_packet,
            &slot->graphics_packet_size, slot->resources, &slot->resource_count,
            &slot->compute_buffer_ownership_updates,
            &slot->compute_buffer_ownership_update_count);
        if (result != RIN_VK_SUCCESS) {
            clear_submission_slot(slot);
            goto done;
        }
    } else if (!snapshot_submission_packet_v2(
                   device, queue_slot_value->queue_family_index,
                   command_buffers, request.commandBufferCount,
                   &slot->extended_packet, slot->resources,
                   &slot->resource_count)) {
        clear_submission_slot(slot);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (rin_gpu_vulkan_command_buffers_mark_submitted(
            &g_command_runtime, request.commandBufferCount,
            command_buffers) != RIN_GPU_VULKAN_COMMAND_OK) {
        clear_submission_slot(slot);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (!compute_submission && !graphics_submission) {
        memset(&slot->routed_packet, 0, sizeof(slot->routed_packet));
        slot->routed_packet.struct_size = sizeof(slot->routed_packet);
        slot->routed_packet.version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_3;
        slot->routed_packet.op_count = slot->extended_packet.op_count;
        slot->routed_packet.queue_family_index =
            queue_slot_value->queue_family_index;
        slot->routed_packet.queue_index = queue_slot_value->family_queue_index;
        slot->routed_packet.product_queue_id = queue_slot_value->queue_index;
        memcpy(slot->routed_packet.operations,
               slot->extended_packet.operations,
               sizeof(slot->routed_packet.operations[0]) *
                   slot->extended_packet.op_count);
    }
    slot->wait_semaphore_count = request.waitSemaphoreCount;
    slot->signal_semaphore_count = request.signalSemaphoreCount;
    slot->fence = (RinVkFence)fence;
    memcpy(slot->wait_semaphores, wait_semaphores,
           sizeof(RinVkSemaphore) * request.waitSemaphoreCount);
    memcpy(slot->wait_semaphore_values, wait_values,
           sizeof(uint64_t) * request.waitSemaphoreCount);
    memcpy(slot->signal_semaphores, signal_semaphores,
           sizeof(RinVkSemaphore) * request.signalSemaphoreCount);
    memcpy(slot->signal_semaphore_values, signal_values,
           sizeof(uint64_t) * request.signalSemaphoreCount);
    if (queue_has_earlier_waiting_submission(slot)) waits_ready = 0;
    if (waits_ready) {
        result = submit_slot_to_product(slot);
        if (result == RIN_VK_SUCCESS) {
            commit_image_layout_updates(pending_layout_images, pending_layouts,
                                        pending_image_ownership,
                                        pending_layout_count);
            commit_buffer_ownership_updates(pending_buffer_ownership,
                                            pending_buffer_ownership_count);
            if (fence != 0u) {
                RinVkFenceSlot* fence_value =
                    fence_slot(device, (RinVkFence)fence);
                __atomic_store_n(&fence_value->pending, 1u, __ATOMIC_RELEASE);
            }
            for (index = 0u; index < request.signalSemaphoreCount; ++index) {
                RinVkSemaphoreSlot* semaphore =
                    semaphore_slot(device, signal_semaphores[index]);
                __atomic_store_n(&semaphore->pending, 1u, __ATOMIC_RELEASE);
                if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE)
                    semaphore->pending_value = signal_values[index];
            }
            mark_submission_query_events(slot, 1u);
            result = RIN_VK_SUCCESS;
            goto done;
        }
        if (result != RIN_VK_NOT_READY) {
            rin_gpu_vulkan_command_buffers_abort(
                &g_command_runtime, request.commandBufferCount,
                command_buffers);
            mark_submission_query_events(slot, 0u);
            clear_submission_slot(slot);
            goto done;
        }
    }
    {
        slot->waits_reserved = 1u;
        for (index = 0u; index < request.waitSemaphoreCount; ++index) {
            RinVkSemaphoreSlot* semaphore =
                semaphore_slot(device, wait_semaphores[index]);
            if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_BINARY) {
                const uint32_t waiter_count = __atomic_load_n(
                    &semaphore->waiter_count, __ATOMIC_ACQUIRE);
                __atomic_store_n(&semaphore->waiter_count, waiter_count + 1u,
                                 __ATOMIC_RELEASE);
            }
        }
        if (fence != 0u) {
            RinVkFenceSlot* fence_value =
                fence_slot(device, (RinVkFence)fence);
            __atomic_store_n(&fence_value->pending, 1u, __ATOMIC_RELEASE);
        }
        for (index = 0u; index < request.signalSemaphoreCount; ++index) {
            RinVkSemaphoreSlot* semaphore =
                semaphore_slot(device, signal_semaphores[index]);
            __atomic_store_n(&semaphore->pending, 1u, __ATOMIC_RELEASE);
            if (semaphore->type == RIN_VK_SEMAPHORE_TYPE_TIMELINE)
                semaphore->pending_value = signal_values[index];
        }
        commit_image_layout_updates(pending_layout_images, pending_layouts,
                                    pending_image_ownership,
                                    pending_layout_count);
        commit_buffer_ownership_updates(pending_buffer_ownership,
                                        pending_buffer_ownership_count);
        mark_submission_query_events(slot, 1u);
        __atomic_store_n(&slot->state, RIN_VK_SUBMISSION_WAITING,
                         __ATOMIC_RELEASE);
        result = RIN_VK_SUCCESS;
        goto done;
    }

done:
    free(validation_compute_packet);
    free(validation_compute_ownership_updates);
    free(validation_graphics_packet);
    free(validation_graphics_ownership_updates);
    if (sync_locked) sync_unlock();
    __atomic_store_n(&queue_slot_value->submit_lock, 0u, __ATOMIC_RELEASE);
    return result;
}

RinVkResult RIN_VKAPI_CALL vkQueueSubmit2(
        RinVkQueue queue, uint32_t submit_count,
        const RinVkSubmitInfo2* submits, uint64_t fence) {
    struct RinVkQueue_T* queue_value = queue_slot(queue);
    struct RinVkDevice_T* device;
    const RinVkSubmitInfo2* request;
    RinVkSemaphoreSubmitInfo wait_infos[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinVkSemaphoreSubmitInfo signal_infos[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinVkCommandBufferSubmitInfo command_infos[RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS];
    RinVkCommandBuffer command_buffers[RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS];
    uint64_t wait_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t signal_semaphores[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint32_t wait_stage_masks[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t wait_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    uint64_t signal_values[RIN_VK_MAX_SUBMIT_SEMAPHORES];
    RinVkTimelineSemaphoreSubmitInfo timeline;
    RinVkSubmitInfo legacy;
    uint64_t runtime_stage_mask;
    uint32_t index;

    if (!queue_value) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    device = queue_value->device;
    if (!device || !device->synchronization2_enabled)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if (submit_count > 1u) {
        if (!submits) return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        for (index = 0u; index < submit_count; ++index) {
            const RinVkResult batch_result = vkQueueSubmit2(
                queue, 1u, &submits[index],
                index + 1u == submit_count ? fence : 0u);
            if (batch_result != RIN_VK_SUCCESS) return batch_result;
        }
        return RIN_VK_SUCCESS;
    }
    if (submit_count != 0u && !submits)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    if (submit_count == 0u)
        return vkQueueSubmit(queue, 0u, NULL, fence);
    request = &submits[0];
    if (request->sType != RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2 ||
        request->pNext || request->flags != 0u ||
        request->waitSemaphoreInfoCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        request->signalSemaphoreInfoCount > RIN_VK_MAX_SUBMIT_SEMAPHORES ||
        request->commandBufferInfoCount > RIN_VK_MAX_SUBMIT_COMMAND_BUFFERS ||
        (request->waitSemaphoreInfoCount != 0u &&
         !request->pWaitSemaphoreInfos) ||
        (request->signalSemaphoreInfoCount != 0u &&
         !request->pSignalSemaphoreInfos) ||
        (request->commandBufferInfoCount != 0u &&
         !request->pCommandBufferInfos))
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;

    memset(wait_infos, 0, sizeof(wait_infos));
    memset(signal_infos, 0, sizeof(signal_infos));
    memset(command_infos, 0, sizeof(command_infos));
    memset(wait_semaphores, 0, sizeof(wait_semaphores));
    memset(signal_semaphores, 0, sizeof(signal_semaphores));
    memset(wait_stage_masks, 0, sizeof(wait_stage_masks));
    memset(wait_values, 0, sizeof(wait_values));
    memset(signal_values, 0, sizeof(signal_values));
    for (index = 0u; index < request->waitSemaphoreInfoCount; ++index) {
        wait_infos[index] = request->pWaitSemaphoreInfos[index];
        if (wait_infos[index].sType != RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO ||
            wait_infos[index].pNext || wait_infos[index].semaphore == 0u ||
            wait_infos[index].deviceIndex != 0u || wait_infos[index].reserved != 0u ||
            !rin_vk_sync2_legacy_wait_stage_mask(
                wait_infos[index].stageMask, &wait_stage_masks[index]))
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        wait_semaphores[index] = wait_infos[index].semaphore;
        wait_values[index] = wait_infos[index].value;
    }
    for (index = 0u; index < request->signalSemaphoreInfoCount; ++index) {
        signal_infos[index] = request->pSignalSemaphoreInfos[index];
        if (signal_infos[index].sType != RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO ||
            signal_infos[index].pNext || signal_infos[index].semaphore == 0u ||
            signal_infos[index].deviceIndex != 0u || signal_infos[index].reserved != 0u ||
            !rin_vk_sync2_stage_mask(signal_infos[index].stageMask,
                                     &runtime_stage_mask))
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        signal_semaphores[index] = signal_infos[index].semaphore;
        signal_values[index] = signal_infos[index].value;
    }
    for (index = 0u; index < request->commandBufferInfoCount; ++index) {
        command_infos[index] = request->pCommandBufferInfos[index];
        if (command_infos[index].sType !=
                RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO ||
            command_infos[index].pNext || command_infos[index].commandBuffer == NULL ||
            command_infos[index].deviceMask != 1u ||
            command_infos[index].reserved != 0u)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        command_buffers[index] = command_infos[index].commandBuffer;
    }

    memset(&timeline, 0, sizeof(timeline));
    timeline.sType = RIN_VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline.waitSemaphoreValueCount = request->waitSemaphoreInfoCount;
    timeline.pWaitSemaphoreValues = request->waitSemaphoreInfoCount != 0u
                                         ? wait_values
                                         : NULL;
    timeline.signalSemaphoreValueCount = request->signalSemaphoreInfoCount;
    timeline.pSignalSemaphoreValues = request->signalSemaphoreInfoCount != 0u
                                          ? signal_values
                                          : NULL;
    memset(&legacy, 0, sizeof(legacy));
    legacy.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    legacy.pNext = (request->waitSemaphoreInfoCount != 0u ||
                    request->signalSemaphoreInfoCount != 0u)
                       ? &timeline
                       : NULL;
    legacy.waitSemaphoreCount = request->waitSemaphoreInfoCount;
    legacy.pWaitSemaphores = request->waitSemaphoreInfoCount != 0u
                                 ? wait_semaphores
                                 : NULL;
    legacy.pWaitDstStageMask = request->waitSemaphoreInfoCount != 0u
                                   ? wait_stage_masks
                                   : NULL;
    legacy.commandBufferCount = request->commandBufferInfoCount;
    legacy.pCommandBuffers = command_buffers;
    legacy.signalSemaphoreCount = request->signalSemaphoreInfoCount;
    legacy.pSignalSemaphores = request->signalSemaphoreInfoCount != 0u
                                   ? signal_semaphores
                                   : NULL;
    return vkQueueSubmit(queue, 1u, &legacy, fence);
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
    memset(&buffer->ownership, 0, sizeof(buffer->ownership));
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

static int buffer_memory_requirement_size(const RinVkBufferSlot* buffer,
                                          uint64_t* size_out) {
    if (!buffer || !size_out ||
        buffer->size > UINT64_MAX - (RIN_VK_RESOURCE_ALIGNMENT - 1u))
        return 0;
    *size_out = (buffer->size + RIN_VK_RESOURCE_ALIGNMENT - 1u) &
                ~(RIN_VK_RESOURCE_ALIGNMENT - 1u);
    return *size_out != 0u;
}

void RIN_VKAPI_CALL vkGetBufferMemoryRequirements(
        RinVkDevice device, RinVkBuffer handle,
        RinVkMemoryRequirements* requirements) {
    RinVkBufferSlot* buffer;
    uint64_t size;
    if (!requirements) return;
    memset(requirements, 0, sizeof(*requirements));
    buffer = buffer_slot(device, handle);
    if (!buffer_memory_requirement_size(buffer, &size)) return;
    requirements->size = size;
    requirements->alignment = RIN_VK_RESOURCE_ALIGNMENT;
    requirements->memoryTypeBits = memory_type_bits(buffer->owner);
}

RinVkResult RIN_VKAPI_CALL vkBindBufferMemory(
        RinVkDevice device, RinVkBuffer buffer_handle,
        RinVkDeviceMemory memory_handle, uint64_t memory_offset) {
    RinVkBufferSlot* buffer = buffer_slot(device, buffer_handle);
    RinVkMemorySlot* memory = memory_slot(device, memory_handle);
    uint64_t requirements_size;
    if (!buffer || !memory || buffer->memory ||
        !buffer_memory_requirement_size(buffer, &requirements_size) ||
        (memory_offset & (RIN_VK_RESOURCE_ALIGNMENT - 1u)) != 0u ||
        memory_offset > memory->requested_size ||
        requirements_size > memory->requested_size - memory_offset ||
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
        image_format_aspects(request.format) == 0u ||
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
    image->current_layout = RIN_VK_IMAGE_LAYOUT_UNDEFINED;
    memset(&image->ownership, 0, sizeof(image->ownership));
    image->ownership.owner_queue_family = RIN_VK_QUEUE_FAMILY_IGNORED;
    __atomic_store_n(&image->state, 1u, __ATOMIC_RELEASE);
    *image_out = resource_handle(RIN_VK_IMAGE_TAG, index,
                                  image->generation);
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyImage(RinVkDevice device, RinVkImage handle,
                                   const void* allocator) {
    RinVkImageSlot* image = image_slot(device, handle);
    (void)allocator;
    if (!image || image->swapchain_owner || image_has_views(image)) return;
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
    if (!image || image->swapchain_owner || !memory || image->memory ||
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
        image_format_aspects(create_info->format) == 0u ||
        create_info->subresourceRange.aspectMask !=
            image_format_aspects(create_info->format) ||
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
    uint32_t limits[RIN_GPU_VULKAN_DESCRIPTOR_TYPE_COUNT];
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
        if (type == 0u || size->descriptorCount == 0u ||
            type >= RIN_GPU_VULKAN_DESCRIPTOR_TYPE_COUNT ||
            limits[type] != 0u ||
            UINT32_MAX - limits[type] < size->descriptorCount)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        limits[type] = size->descriptorCount;
    }
    (void)pool_index;
    result = rin_gpu_vulkan_descriptor_pool_create_v2(
        &owner->descriptor_runtime, info->maxSets, limits,
        RIN_GPU_VULKAN_DESCRIPTOR_TYPE_COUNT, &pool);
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
            source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
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
        if (source->descriptorType ==
            RIN_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            RinGpuVulkanCombinedImageSamplerWriteV1 combined[
                RIN_SHADER_MAX_RESOURCES];
            memset(combined, 0, sizeof(combined));
            if (source->dstBinding >= RIN_SHADER_MAX_RESOURCES / 2u) {
                __atomic_store_n(&owner->descriptor_validation_error, 1u,
                                 __ATOMIC_RELEASE);
                continue;
            }
            for (item = 0u; item < source->descriptorCount; ++item) {
                const RinVkDescriptorImageInfo* image_info =
                    &source->pImageInfo[item];
                RinVkImageViewSlot* view =
                    image_view_slot(owner, image_info->imageView);
                if (!view || !sampler_slot(owner, image_info->sampler) ||
                    (image_info->imageLayout != RIN_VK_IMAGE_LAYOUT_GENERAL &&
                     image_info->imageLayout != 5u)) {
                    invalid = 1;
                    break;
                }
                combined[item].struct_size = sizeof(combined[item]);
                combined[item].version = 1u;
                combined[item].set = 0u;
                combined[item].binding = source->dstBinding;
                combined[item].array_element = source->dstArrayElement + item;
                /* The public ICD's bounded profile assigns two stable RSH1
                 * slots to each logical binding. */
                combined[item].image_resource_index = source->dstBinding * 2u;
                combined[item].sampler_resource_index = source->dstBinding * 2u + 1u;
                combined[item].image_resource = (RinGpuHandle)resource_handle(
                    RIN_VK_IMAGE_TAG,
                    (uint32_t)(view->image - &g_images[0]),
                    view->image->generation);
                combined[item].sampler_resource =
                    (RinGpuHandle)image_info->sampler;
            }
            if (invalid || rin_gpu_vulkan_descriptor_set_update_combined(
                                &owner->descriptor_runtime,
                                (RinGpuVulkanDescriptorHandleV1)source->dstSet,
                                combined, source->descriptorCount) !=
                            RIN_GPU_VULKAN_GRAPHICS_OK)
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
                    destination->access =
                        source->descriptorType == RIN_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                            ? RIN_GPU_RESOURCE_READ | RIN_GPU_RESOURCE_WRITE
                            : RIN_GPU_RESOURCE_READ;
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
    if (!owner || !layout || command->compute_dispatch_count != 0u ||
        first_set > layout->set_layout_count ||
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
            RIN_GPU_VULKAN_COMMAND_OK) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    command->descriptor_bind_point = pipeline_bind_point;
    for (index = 0u; index < descriptor_set_count; ++index)
        command->descriptor_set_layouts[index] =
            layout->set_layouts[first_set + index];
}

void RIN_VKAPI_CALL vkCmdBindPipeline(
        RinVkCommandBuffer command_buffer, uint32_t pipeline_bind_point,
        RinVkPipeline pipeline_handle) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    uintptr_t owner_address = 0u;
    RinVkDevice owner_device;
    if (rin_gpu_vulkan_command_buffer_owner(
            &g_command_runtime, command, &owner_address) !=
        RIN_GPU_VULKAN_COMMAND_OK) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    owner_device = (RinVkDevice)(void*)owner_address;
    RinVkPipelineSlot* pipeline = pipeline_slot(owner_device, pipeline_handle);
    if (!device_slot(owner_device) || !pipeline ||
        command->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    if (pipeline_bind_point == RIN_VK_PIPELINE_BIND_POINT_COMPUTE &&
        pipeline->kind == RIN_VK_PIPELINE_KIND_COMPUTE &&
        command->compute_dispatch_count == 0u &&
        command->graphics_draw_count == 0u) {
        command->bound_compute_pipeline = pipeline_handle;
        command->compute_pipeline_bound = 1u;
    } else if (pipeline_bind_point == RIN_VK_PIPELINE_BIND_POINT_GRAPHICS &&
               pipeline->kind == RIN_VK_PIPELINE_KIND_GRAPHICS &&
               command->graphics_draw_count == 0u &&
               command->compute_dispatch_count == 0u) {
        command->bound_graphics_pipeline = pipeline_handle;
        command->graphics_pipeline_bound = 1u;
    } else {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
    }
}

void RIN_VKAPI_CALL vkCmdBeginRendering(
        RinVkCommandBuffer command_buffer,
        const RinVkRenderingInfo* rendering_info) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner = NULL;
    const RinVkRenderingAttachmentInfo* attachment;
    RinVkImageViewSlot* view;
    RinVkImageSlot* image;
    if (!rendering_info || !command_owner_device(command, &owner) ||
        !owner->dynamic_rendering_enabled ||
        command->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        command->graphics_rendering_active != 0u ||
        command->graphics_rendering_begin_count != 0u ||
        command->compute_dispatch_count != 0u ||
        rendering_info->sType != RIN_VK_STRUCTURE_TYPE_RENDERING_INFO ||
        rendering_info->pNext || rendering_info->flags != 0u ||
        rendering_info->renderArea.offset.x != 0 ||
        rendering_info->renderArea.offset.y != 0 ||
        rendering_info->layerCount != 1u || rendering_info->viewMask != 0u ||
        rendering_info->colorAttachmentCount != 1u ||
        !rendering_info->pColorAttachments || rendering_info->pDepthAttachment ||
        rendering_info->pStencilAttachment) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    attachment = &rendering_info->pColorAttachments[0];
    view = image_view_slot((RinVkDevice)(void*)owner, attachment->imageView);
    image = view ? view->image : NULL;
    if (attachment->sType != RIN_VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO ||
        attachment->pNext ||
        attachment->imageLayout != RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
        attachment->resolveMode != 0u || attachment->resolveImageView != 0u ||
        attachment->resolveImageLayout != RIN_VK_IMAGE_LAYOUT_UNDEFINED ||
        attachment->loadOp != RIN_VK_ATTACHMENT_LOAD_OP_CLEAR ||
        attachment->storeOp != RIN_VK_ATTACHMENT_STORE_OP_STORE ||
        !view || !image || image->owner != owner || !image->memory ||
        image->memory_generation != image->memory->generation ||
        image->memory->owner != owner ||
        (int32_t)view->format != image->format ||
        view->aspect_mask != RIN_VK_IMAGE_ASPECT_COLOR_BIT ||
        image->format != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
        (image->usage & RIN_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0u ||
        image->samples != RIN_VK_SAMPLE_COUNT_1_BIT || image->width == 0u ||
        image->height == 0u ||
        rendering_info->renderArea.extent.width != image->width ||
        rendering_info->renderArea.extent.height != image->height ||
        __atomic_load_n(&image->current_layout, __ATOMIC_ACQUIRE) !=
            RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
        image->ownership.transfer_pending != 0u ||
        (image->ownership.owner_queue_family != RIN_VK_QUEUE_FAMILY_IGNORED &&
         image->ownership.owner_queue_family != command->pool->queue_family_index)) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    command->graphics_rendering_active = 1u;
    command->graphics_rendering_begin_count = 1u;
    command->graphics_color_layout = attachment->imageLayout;
    command->graphics_color_view = attachment->imageView;
    command->graphics_width = image->width;
    command->graphics_height = image->height;
    command->graphics_clear_red = attachment->clearValue.color.float32[0];
    command->graphics_clear_green = attachment->clearValue.color.float32[1];
    command->graphics_clear_blue = attachment->clearValue.color.float32[2];
    command->graphics_clear_alpha = attachment->clearValue.color.float32[3];
}

void RIN_VKAPI_CALL vkCmdBeginRenderingKHR(
        RinVkCommandBuffer command_buffer,
        const RinVkRenderingInfo* rendering_info) {
    vkCmdBeginRendering(command_buffer, rendering_info);
}

void RIN_VKAPI_CALL vkCmdEndRendering(
        RinVkCommandBuffer command_buffer) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    if (!command || command->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        command->graphics_rendering_active == 0u ||
        command->graphics_rendering_end_count != 0u ||
        command->graphics_draw_count != 1u ||
        command->graphics_pipeline_bound == 0u ||
        command->bound_graphics_pipeline == 0u) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    command->graphics_rendering_active = 0u;
    command->graphics_rendering_end_count = 1u;
}

void RIN_VKAPI_CALL vkCmdEndRenderingKHR(
        RinVkCommandBuffer command_buffer) {
    vkCmdEndRendering(command_buffer);
}

void RIN_VKAPI_CALL vkCmdBindVertexBuffers(
        RinVkCommandBuffer command_buffer, uint32_t first_binding,
        uint32_t binding_count, const RinVkBuffer* buffers,
        const uint64_t* offsets) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    struct RinVkDevice_T* owner;
    RinVkBufferSlot* buffer;
    if (!command_owner_device(command, &owner) ||
        command->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        first_binding != 0u || binding_count != 1u || !buffers || !offsets ||
        command->graphics_vertex_buffer_bound != 0u || buffers[0] == 0u ||
        !(buffer = buffer_slot((RinVkDevice)(void*)owner, buffers[0])) ||
        !buffer->memory || buffer->memory_generation != buffer->memory->generation ||
        (buffer->usage & RIN_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0u ||
        offsets[0] >= buffer->size) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    command->graphics_vertex_buffer_bound = 1u;
    command->graphics_vertex_buffer = buffers[0];
    command->graphics_vertex_offset = offsets[0];
}

void RIN_VKAPI_CALL vkCmdDraw(
        RinVkCommandBuffer command_buffer, uint32_t vertex_count,
        uint32_t instance_count, uint32_t first_vertex,
        uint32_t first_instance) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    if (!command || command->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        command->graphics_rendering_active == 0u ||
        command->graphics_draw_count != 0u ||
        command->graphics_pipeline_bound == 0u ||
        command->bound_graphics_pipeline == 0u || vertex_count == 0u ||
        vertex_count > 65535u || instance_count != 1u || first_instance != 0u ||
        command->compute_dispatch_count != 0u) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    command->graphics_draw_count = 1u;
    command->graphics_vertex_count = vertex_count;
    command->graphics_instance_count = instance_count;
    command->graphics_first_vertex = first_vertex;
    command->graphics_first_instance = first_instance;
}

void RIN_VKAPI_CALL vkCmdDispatch(
        RinVkCommandBuffer command_buffer, uint32_t group_count_x,
        uint32_t group_count_y, uint32_t group_count_z) {
    RinGpuVulkanCommandBufferV1* command =
        (RinGpuVulkanCommandBufferV1*)(void*)command_buffer;
    uintptr_t owner_address = 0u;
    RinVkDevice owner_device;
    if (rin_gpu_vulkan_command_buffer_owner(
            &g_command_runtime, command, &owner_address) !=
        RIN_GPU_VULKAN_COMMAND_OK) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    owner_device = (RinVkDevice)(void*)owner_address;
    if (!device_slot(owner_device) ||
        command->lifecycle != RIN_GPU_VULKAN_COMMAND_BUFFER_RECORDING ||
        command->compute_pipeline_bound == 0u ||
        command->bound_compute_pipeline == 0u ||
        command->compute_dispatch_count != 0u || group_count_x == 0u ||
        group_count_y == 0u || group_count_z == 0u ||
        group_count_x > 65535u || group_count_y > 65535u ||
        group_count_z > 65535u ||
        !pipeline_slot(owner_device, command->bound_compute_pipeline)) {
        rin_gpu_vulkan_command_buffer_record_failure(&g_command_runtime,
                                                     command);
        return;
    }
    command->dispatch_compute_pipeline = command->bound_compute_pipeline;
    command->compute_group_count_x = group_count_x;
    command->compute_group_count_y = group_count_y;
    command->compute_group_count_z = group_count_z;
    command->compute_dispatch_count = 1u;
}

typedef struct RinVkPipelineCacheBlobHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    uint32_t reserved;
    uint8_t device_uuid[16];
    uint8_t driver_digest[32];
} RinVkPipelineCacheBlobHeader;

static void pipeline_cache_header(const struct RinVkDevice_T* device,
                                  uint32_t payload_size,
                                  RinVkPipelineCacheBlobHeader* header) {
    memset(header, 0, sizeof(*header));
    header->magic = RIN_VK_PIPELINE_CACHE_MAGIC;
    header->version = RIN_VK_PIPELINE_CACHE_VERSION;
    header->payload_size = payload_size;
    memcpy(header->device_uuid, device->physical_profile.device_uuid,
           sizeof(header->device_uuid));
    memcpy(header->driver_digest, device->physical_profile.driver_digest,
           sizeof(header->driver_digest));
}

static int map_graphics_topology(uint32_t value, uint32_t* out) {
    if (!out) return 0;
    switch (value) {
        case RIN_VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
            *out = RIN_GPU_PRIMITIVE_POINT_LIST; return 1;
        case RIN_VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
            *out = RIN_GPU_PRIMITIVE_LINE_LIST; return 1;
        case RIN_VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
            *out = RIN_GPU_PRIMITIVE_LINE_STRIP; return 1;
        case RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
            *out = RIN_GPU_PRIMITIVE_TRIANGLE_LIST; return 1;
        case RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
            *out = RIN_GPU_PRIMITIVE_TRIANGLE_STRIP; return 1;
        case RIN_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
            *out = RIN_GPU_PRIMITIVE_TRIANGLE_FAN; return 1;
        default: return 0;
    }
}

static int map_graphics_blend_factor(uint32_t value, uint32_t* out) {
    if (!out) return 0;
    switch (value) {
        case RIN_VK_BLEND_FACTOR_ZERO: *out = RIN_GPU_BLEND_ZERO; return 1;
        case RIN_VK_BLEND_FACTOR_ONE: *out = RIN_GPU_BLEND_ONE; return 1;
        case RIN_VK_BLEND_FACTOR_SRC_COLOR:
            *out = RIN_GPU_BLEND_SOURCE_COLOR; return 1;
        case RIN_VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
            *out = RIN_GPU_BLEND_ONE_MINUS_SOURCE_COLOR; return 1;
        case RIN_VK_BLEND_FACTOR_DST_COLOR:
            *out = RIN_GPU_BLEND_DESTINATION_COLOR; return 1;
        case RIN_VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
            *out = RIN_GPU_BLEND_ONE_MINUS_DESTINATION_COLOR; return 1;
        case RIN_VK_BLEND_FACTOR_SRC_ALPHA:
            *out = RIN_GPU_BLEND_SOURCE_ALPHA; return 1;
        case RIN_VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
            *out = RIN_GPU_BLEND_ONE_MINUS_SOURCE_ALPHA; return 1;
        case RIN_VK_BLEND_FACTOR_DST_ALPHA:
            *out = RIN_GPU_BLEND_DESTINATION_ALPHA; return 1;
        case RIN_VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
            *out = RIN_GPU_BLEND_ONE_MINUS_DESTINATION_ALPHA; return 1;
        case RIN_VK_BLEND_FACTOR_CONSTANT_COLOR:
            *out = RIN_GPU_BLEND_CONSTANT_COLOR; return 1;
        case RIN_VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
            *out = RIN_GPU_BLEND_ONE_MINUS_CONSTANT_COLOR; return 1;
        case RIN_VK_BLEND_FACTOR_CONSTANT_ALPHA:
            *out = RIN_GPU_BLEND_CONSTANT_ALPHA; return 1;
        case RIN_VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
            *out = RIN_GPU_BLEND_ONE_MINUS_CONSTANT_ALPHA; return 1;
        case RIN_VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
            *out = RIN_GPU_BLEND_SOURCE_ALPHA_SATURATE; return 1;
        default: return 0;
    }
}

static int map_graphics_blend_operation(uint32_t value, uint32_t* out) {
    if (!out) return 0;
    switch (value) {
        case RIN_VK_BLEND_OP_ADD: *out = RIN_GPU_BLEND_ADD; return 1;
        case RIN_VK_BLEND_OP_SUBTRACT:
            *out = RIN_GPU_BLEND_SUBTRACT; return 1;
        case RIN_VK_BLEND_OP_REVERSE_SUBTRACT:
            *out = RIN_GPU_BLEND_REVERSE_SUBTRACT; return 1;
        case RIN_VK_BLEND_OP_MIN: *out = RIN_GPU_BLEND_MINIMUM; return 1;
        case RIN_VK_BLEND_OP_MAX: *out = RIN_GPU_BLEND_MAXIMUM; return 1;
        default: return 0;
    }
}

static int map_graphics_vertex_format(uint32_t value, uint32_t* out) {
    if (!out) return 0;
    switch (value) {
        case RIN_VK_FORMAT_R32_SFLOAT:
        case RIN_VK_FORMAT_R32G32_SFLOAT:
        case RIN_VK_FORMAT_R32G32B32_SFLOAT:
        case RIN_VK_FORMAT_R32G32B32A32_SFLOAT:
            *out = RIN_GPU_VERTEX_FLOAT32; return 1;
        default: return 0;
    }
}

static int finite_graphics_float(float value) {
    return value == value && value >= -FLT_MAX && value <= FLT_MAX;
}

static int pipeline_cache_blob_valid(const struct RinVkDevice_T* device,
                                     const void* data, size_t data_size,
                                     const RinVkPipelineCacheBlobHeader**
                                         header_out) {
    const RinVkPipelineCacheBlobHeader* header;
    if (!device || !data || data_size < sizeof(*header)) return 0;
    header = (const RinVkPipelineCacheBlobHeader*)data;
    if (header->magic != RIN_VK_PIPELINE_CACHE_MAGIC ||
        header->version != RIN_VK_PIPELINE_CACHE_VERSION ||
        header->reserved != 0u ||
        header->payload_size > RIN_VK_PIPELINE_CACHE_MAX_PAYLOAD ||
        sizeof(*header) + (size_t)header->payload_size != data_size ||
        memcmp(header->device_uuid, device->physical_profile.device_uuid,
               sizeof(header->device_uuid)) != 0 ||
        memcmp(header->driver_digest, device->physical_profile.driver_digest,
               sizeof(header->driver_digest)) != 0)
        return 0;
    if (header_out) *header_out = header;
    return 1;
}

static int shader_module_code_valid(
        const RinVkShaderModuleCreateInfo* info) {
    const uint32_t* words;
    size_t word_count;
    size_t cursor;
    if (!info || !info->pCode || info->codeSize < 6u * sizeof(uint32_t) ||
        info->codeSize % sizeof(uint32_t) != 0u ||
        (uintptr_t)info->pCode % sizeof(uint32_t) != 0u)
        return 0;
    words = info->pCode;
    word_count = info->codeSize / sizeof(uint32_t);
    if (words[0] != UINT32_C(0x07230203) ||
        (words[1] & UINT32_C(0xff0000ff)) != 0u ||
        (words[1] >> 16u) != 1u || ((words[1] >> 8u) & 0xffu) > 6u ||
        words[3] == 0u || words[4] != 0u)
        return 0;
    cursor = 5u;
    while (cursor < word_count) {
        size_t instruction_word_count = (size_t)(words[cursor] >> 16u);
        if (instruction_word_count == 0u ||
            instruction_word_count > word_count - cursor)
            return 0;
        cursor += instruction_word_count;
    }
    return cursor == word_count;
}

RinVkResult RIN_VKAPI_CALL vkCreateShaderModule(
        RinVkDevice device, const RinVkShaderModuleCreateInfo* info,
        const void* allocator, RinVkShaderModule* shader_module_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkShaderModuleSlot* module;
    uint32_t* code_copy;
    uint32_t index;
    (void)allocator;
    if (!shader_module_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *shader_module_out = 0u;
    if (!owner || !info ||
        info->sType != RIN_VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO ||
        info->pNext || info->flags != 0u || !shader_module_code_valid(info))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    code_copy = (uint32_t*)malloc(info->codeSize);
    if (!code_copy) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    memcpy(code_copy, info->pCode, info->codeSize);
    sync_lock();
    owner = device_slot(device);
    module = owner ? reserve_shader_module_slot(owner, &index) : NULL;
    if (!module) {
        sync_unlock();
        free(code_copy);
        return owner ? RIN_VK_ERROR_OUT_OF_HOST_MEMORY
                     : RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    module->code_size = info->codeSize;
    module->code = code_copy;
    __atomic_store_n(&module->state, 1u, __ATOMIC_RELEASE);
    *shader_module_out = resource_handle(RIN_VK_SHADER_MODULE_TAG, index,
                                         module->generation);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyShaderModule(
        RinVkDevice device, RinVkShaderModule shader_module,
        const void* allocator) {
    RinVkShaderModuleSlot* module;
    (void)allocator;
    if (shader_module == 0u) return;
    sync_lock();
    module = shader_module_slot(device, shader_module);
    if (module) clear_shader_module_slot(module);
    sync_unlock();
}

static RinVkResult create_graphics_pipeline_one(
        RinVkDevice device, const RinVkGraphicsPipelineCreateInfo* info,
        RinVkPipeline* pipeline_out) {
    struct RinVkDevice_T* owner;
    RinVkPipelineLayoutSlot* layout;
    RinVkShaderModuleSlot* vertex_module;
    RinVkShaderModuleSlot* fragment_module;
    RinVkPipelineSlot* pipeline;
    const RinVkPipelineRenderingCreateInfo* rendering;
    const RinVkPipelineShaderStageCreateInfo* vertex_stage = NULL;
    const RinVkPipelineShaderStageCreateInfo* fragment_stage = NULL;
    RinGpuVulkanVertexInputBindingV1 binding;
    RinGpuVulkanVertexInputAttributeV1 attributes[RIN_GPU_MAX_VERTEX_ATTRIBUTES];
    RinGpuVulkanGraphicsStateV1 state;
    RinGpuVulkanGraphicsPipelinePlanV1 plan;
    RinSpirvTranslationInfoV1 vertex_translation;
    RinSpirvTranslationInfoV1 fragment_translation;
    uint32_t* vertex_spirv = NULL;
    uint32_t* fragment_spirv = NULL;
    uint8_t* vertex_ir = NULL;
    uint8_t* fragment_ir = NULL;
    size_t vertex_spirv_size = 0u, fragment_spirv_size = 0u;
    size_t vertex_ir_size, fragment_ir_size;
    uint32_t binding_count = 0u, attribute_count, index, slot_index = 0u;
    int translated;
    RinVkResult result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;

    if (!info || !pipeline_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *pipeline_out = 0u;
    if (info->sType != RIN_VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO ||
        info->flags != 0u || info->stageCount != 2u || !info->pStages ||
        !info->pVertexInputState || !info->pInputAssemblyState ||
        info->pTessellationState || !info->pViewportState ||
        !info->pRasterizationState || !info->pMultisampleState ||
        info->pDepthStencilState || !info->pColorBlendState ||
        info->pDynamicState || info->renderPass != 0u || info->subpass != 0u ||
        info->basePipelineHandle != 0u || info->basePipelineIndex != -1 ||
        !info->pNext)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    rendering = (const RinVkPipelineRenderingCreateInfo*)info->pNext;
    if (rendering->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO ||
        rendering->pNext || rendering->viewMask != 0u ||
        rendering->colorAttachmentCount != 1u ||
        !rendering->pColorAttachmentFormats ||
        rendering->pColorAttachmentFormats[0] != RIN_VK_FORMAT_R8G8B8A8_UNORM ||
        rendering->depthAttachmentFormat != 0 || rendering->stencilAttachmentFormat != 0)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    for (index = 0u; index < info->stageCount; ++index) {
        const RinVkPipelineShaderStageCreateInfo* stage = &info->pStages[index];
        if (stage->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO ||
            stage->pNext || stage->flags != 0u || !stage->pName ||
            stage->pSpecializationInfo)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        if (stage->stage == RIN_VK_SHADER_STAGE_VERTEX_BIT && !vertex_stage)
            vertex_stage = stage;
        else if (stage->stage == RIN_VK_SHADER_STAGE_FRAGMENT_BIT && !fragment_stage)
            fragment_stage = stage;
        else
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }
    if (!vertex_stage || !fragment_stage ||
        info->pVertexInputState->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO ||
        info->pVertexInputState->pNext || info->pVertexInputState->flags != 0u ||
        info->pVertexInputState->vertexBindingDescriptionCount > 1u ||
        (info->pVertexInputState->vertexBindingDescriptionCount &&
         !info->pVertexInputState->pVertexBindingDescriptions) ||
        info->pVertexInputState->vertexAttributeDescriptionCount > RIN_GPU_MAX_VERTEX_ATTRIBUTES ||
        (info->pVertexInputState->vertexAttributeDescriptionCount &&
         !info->pVertexInputState->pVertexAttributeDescriptions) ||
        info->pInputAssemblyState->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO ||
        info->pInputAssemblyState->pNext || info->pInputAssemblyState->flags != 0u ||
        info->pInputAssemblyState->primitiveRestartEnable != 0u ||
        info->pViewportState->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO ||
        info->pViewportState->pNext || info->pViewportState->flags != 0u ||
        info->pViewportState->viewportCount != 1u || !info->pViewportState->pViewports ||
        info->pViewportState->scissorCount != 1u || !info->pViewportState->pScissors ||
        info->pRasterizationState->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO ||
        info->pRasterizationState->pNext || info->pRasterizationState->flags != 0u ||
        info->pMultisampleState->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO ||
        info->pMultisampleState->pNext || info->pMultisampleState->flags != 0u ||
        info->pColorBlendState->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO ||
        info->pColorBlendState->pNext || info->pColorBlendState->flags != 0u ||
        info->pColorBlendState->logicOpEnable != 0u || info->pColorBlendState->logicOp != 0u ||
        info->pColorBlendState->attachmentCount != 1u || !info->pColorBlendState->pAttachments)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    {
        const RinVkViewport* vp = info->pViewportState->pViewports;
        const RinVkRect2D* sc = info->pViewportState->pScissors;
        const RinVkPipelineRasterizationStateCreateInfo* rs = info->pRasterizationState;
        const RinVkPipelineMultisampleStateCreateInfo* ms = info->pMultisampleState;
        const RinVkPipelineColorBlendAttachmentState* bl = info->pColorBlendState->pAttachments;
        uint32_t mapped_topology;
        if (!finite_graphics_float(vp->x) || !finite_graphics_float(vp->y) ||
            !finite_graphics_float(vp->width) || !finite_graphics_float(vp->height) ||
            !finite_graphics_float(vp->minDepth) || !finite_graphics_float(vp->maxDepth) ||
            vp->x < 0.0f || vp->y < 0.0f || vp->width <= 0.0f || vp->height <= 0.0f ||
            vp->minDepth < 0.0f || vp->maxDepth < vp->minDepth || vp->maxDepth > 1.0f ||
            sc->offset.x < 0 || sc->offset.y < 0 || !sc->extent.width || !sc->extent.height ||
            rs->depthClampEnable || rs->rasterizerDiscardEnable ||
            rs->polygonMode != RIN_VK_POLYGON_MODE_FILL ||
            (rs->cullMode != RIN_VK_CULL_MODE_NONE && rs->cullMode != RIN_VK_CULL_MODE_FRONT_BIT &&
             rs->cullMode != RIN_VK_CULL_MODE_BACK_BIT) || rs->frontFace > RIN_VK_FRONT_FACE_CLOCKWISE ||
            rs->depthBiasEnable || rs->depthBiasConstantFactor != 0.0f || rs->depthBiasClamp != 0.0f ||
            rs->depthBiasSlopeFactor != 0.0f || rs->lineWidth != 1.0f ||
            ms->rasterizationSamples != RIN_VK_SAMPLE_COUNT_1_BIT || ms->sampleShadingEnable ||
            ms->minSampleShading != 0.0f || ms->pSampleMask || ms->alphaToCoverageEnable ||
            ms->alphaToOneEnable || bl->blendEnable > 1u || (bl->colorWriteMask & ~15u) ||
            !map_graphics_topology(info->pInputAssemblyState->topology, &mapped_topology))
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }
    memset(&binding, 0, sizeof(binding));
    if (info->pVertexInputState->vertexBindingDescriptionCount == 1u) {
        const RinVkVertexInputBindingDescription* b = info->pVertexInputState->pVertexBindingDescriptions;
        if (b->binding != 0u || !b->stride || b->stride > RIN_GPU_MAX_VERTEX_STRIDE ||
            b->inputRate != RIN_VK_VERTEX_INPUT_RATE_VERTEX)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        binding.binding = b->binding; binding.stride = b->stride; binding_count = 1u;
    }
    memset(attributes, 0, sizeof(attributes));
    attribute_count = info->pVertexInputState->vertexAttributeDescriptionCount;
    for (index = 0u; index < attribute_count; ++index) {
        const RinVkVertexInputAttributeDescription* a = &info->pVertexInputState->pVertexAttributeDescriptions[index];
        uint32_t prior;
        if (a->binding != 0u || !map_graphics_vertex_format(a->format, &attributes[index].format) ||
            a->offset > UINT32_MAX - 12u)
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        for (prior = 0u; prior < index; ++prior)
            if (info->pVertexInputState->pVertexAttributeDescriptions[prior].location == a->location)
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
        attributes[index].location = a->location; attributes[index].binding = a->binding;
        attributes[index].offset = a->offset;
    }
    sync_lock();
    owner = device_slot(device);
    layout = owner ? pipeline_layout_slot(device, info->layout) : NULL;
    vertex_module = owner ? shader_module_slot(device, vertex_stage->module) : NULL;
    fragment_module = owner ? shader_module_slot(device, fragment_stage->module) : NULL;
    if (!owner || !owner->dynamic_rendering_enabled || !layout || layout->set_layout_count ||
        !vertex_module || !fragment_module) {
        sync_unlock(); return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    vertex_spirv_size = vertex_module->code_size; fragment_spirv_size = fragment_module->code_size;
    vertex_spirv = (uint32_t*)malloc(vertex_spirv_size); fragment_spirv = (uint32_t*)malloc(fragment_spirv_size);
    if (vertex_spirv) memcpy(vertex_spirv, vertex_module->code, vertex_spirv_size);
    if (fragment_spirv) memcpy(fragment_spirv, fragment_module->code, fragment_spirv_size);
    sync_unlock();
    if (!vertex_spirv || !fragment_spirv) { result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY; goto done; }
    vertex_ir = (uint8_t*)malloc(RIN_SHADER_MAX_SOURCE_BYTES);
    fragment_ir = (uint8_t*)malloc(RIN_SHADER_MAX_SOURCE_BYTES);
    if (!vertex_ir || !fragment_ir) { result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY; goto done; }
    memset(&vertex_translation, 0, sizeof(vertex_translation));
    translated = ringpu_vulkan_graphics_translate_shader(vertex_spirv, vertex_spirv_size / 4u,
        RIN_SHADER_STAGE_VERTEX, NULL, 0u, vertex_ir, RIN_SHADER_MAX_SOURCE_BYTES, &vertex_translation);
    if (translated != RIN_GPU_VULKAN_GRAPHICS_OK) {
        result = translated == RIN_GPU_VULKAN_GRAPHICS_LIMIT ? RIN_VK_ERROR_OUT_OF_HOST_MEMORY : RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    memset(&fragment_translation, 0, sizeof(fragment_translation));
    translated = ringpu_vulkan_graphics_translate_shader(fragment_spirv, fragment_spirv_size / 4u,
        RIN_SHADER_STAGE_FRAGMENT, NULL, 0u, fragment_ir, RIN_SHADER_MAX_SOURCE_BYTES, &fragment_translation);
    if (translated != RIN_GPU_VULKAN_GRAPHICS_OK) {
        result = translated == RIN_GPU_VULKAN_GRAPHICS_LIMIT ? RIN_VK_ERROR_OUT_OF_HOST_MEMORY : RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    if (strcmp(vertex_stage->pName, vertex_translation.entry_name) ||
        strcmp(fragment_stage->pName, fragment_translation.entry_name) ||
        vertex_translation.descriptor_count || fragment_translation.descriptor_count ||
        (!binding_count && (vertex_translation.input_count || attribute_count)))
        goto done;
    if (binding_count) for (index = 0u; index < vertex_translation.input_count; ++index) {
        const RinSpirvIoV1* input = &vertex_translation.inputs[index];
        const RinVkVertexInputAttributeDescription* a = NULL;
        uint32_t ai;
        for (ai = 0u; ai < attribute_count; ++ai)
            if (info->pVertexInputState->pVertexAttributeDescriptions[ai].location == input->location) {
                a = &info->pVertexInputState->pVertexAttributeDescriptions[ai]; break;
            }
        if (!a || !input->width || input->width > 4u || a->offset > binding.stride ||
            input->width * 4u > binding.stride - a->offset) goto done;
    }
    memset(&state, 0, sizeof(state));
    state.struct_size = sizeof(state); state.version = RIN_GPU_VULKAN_GRAPHICS_PROFILE_VERSION;
    state.color_format = RIN_GPU_FORMAT_RGBA8_UNORM;
    if (!map_graphics_topology(info->pInputAssemblyState->topology, &state.primitive_topology)) goto done;
    state.blend_enabled = info->pColorBlendState->pAttachments[0].blendEnable;
    state.color_write_mask = info->pColorBlendState->pAttachments[0].colorWriteMask;
    state.position_output_location = 0u; state.render_pass_model = RIN_GPU_VULKAN_GRAPHICS_RENDER_PASS;
    state.cull_mode = info->pRasterizationState->cullMode == RIN_VK_CULL_MODE_NONE ? RIN_GPU_CULL_NONE :
        (info->pRasterizationState->cullMode == RIN_VK_CULL_MODE_FRONT_BIT ? RIN_GPU_CULL_FRONT : RIN_GPU_CULL_BACK);
    state.front_face = info->pRasterizationState->frontFace == RIN_VK_FRONT_FACE_COUNTER_CLOCKWISE ?
        RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE : RIN_GPU_FRONT_FACE_CLOCKWISE;
    if (state.blend_enabled) {
        const RinVkPipelineColorBlendAttachmentState* b = info->pColorBlendState->pAttachments;
        if (!map_graphics_blend_factor(b->srcColorBlendFactor, &state.source_color_factor) ||
            !map_graphics_blend_factor(b->dstColorBlendFactor, &state.destination_color_factor) ||
            !map_graphics_blend_operation(b->colorBlendOp, &state.color_operation) ||
            !map_graphics_blend_factor(b->srcAlphaBlendFactor, &state.source_alpha_factor) ||
            !map_graphics_blend_factor(b->dstAlphaBlendFactor, &state.destination_alpha_factor) ||
            !map_graphics_blend_operation(b->alphaBlendOp, &state.alpha_operation)) goto done;
        state.blend_constant_red = info->pColorBlendState->blendConstants[0];
        state.blend_constant_green = info->pColorBlendState->blendConstants[1];
        state.blend_constant_blue = info->pColorBlendState->blendConstants[2];
        state.blend_constant_alpha = info->pColorBlendState->blendConstants[3];
        if (!finite_graphics_float(state.blend_constant_red) || !finite_graphics_float(state.blend_constant_green) ||
            !finite_graphics_float(state.blend_constant_blue) || !finite_graphics_float(state.blend_constant_alpha)) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED; goto done;
        }
    }
    memset(&plan, 0, sizeof(plan));
    if (ringpu_vulkan_graphics_build_pipeline(&vertex_translation, &fragment_translation, &state,
            binding_count ? &binding : NULL, binding_count, attributes, attribute_count, &plan) !=
            RIN_GPU_VULKAN_GRAPHICS_OK || plan.backend.resource_count || plan.backend.vertex_binding_count > 1u)
        goto done;
    vertex_ir_size = ((const RinShaderHeaderV1*)(const void*)vertex_ir)->total_size;
    fragment_ir_size = ((const RinShaderHeaderV1*)(const void*)fragment_ir)->total_size;
    if (vertex_ir_size < sizeof(RinShaderHeaderV1) || vertex_ir_size > RIN_SHADER_MAX_SOURCE_BYTES ||
        fragment_ir_size < sizeof(RinShaderHeaderV1) || fragment_ir_size > RIN_SHADER_MAX_SOURCE_BYTES) goto done;
    sync_lock();
    owner = device_slot(device); layout = owner ? pipeline_layout_slot(device, info->layout) : NULL;
    if (!owner || !owner->dynamic_rendering_enabled || !layout || layout->set_layout_count) {
        sync_unlock(); result = RIN_VK_ERROR_INITIALIZATION_FAILED; goto done;
    }
    pipeline = reserve_pipeline_slot(owner, &slot_index);
    if (!pipeline) { sync_unlock(); result = RIN_VK_ERROR_OUT_OF_HOST_MEMORY; goto done; }
    pipeline->kind = RIN_VK_PIPELINE_KIND_GRAPHICS;
    pipeline->shader_size = (uint32_t)vertex_ir_size;
    pipeline->fragment_shader_size = (uint32_t)fragment_ir_size;
    pipeline->graphics_backend = plan.backend;
    pipeline->viewport = *info->pViewportState->pViewports;
    pipeline->scissor = *info->pViewportState->pScissors;
    pipeline->shader_ir = vertex_ir; pipeline->fragment_shader_ir = fragment_ir;
    vertex_ir = NULL; fragment_ir = NULL;
    __atomic_store_n(&pipeline->state, 1u, __ATOMIC_RELEASE);
    *pipeline_out = resource_handle(RIN_VK_PIPELINE_TAG, slot_index, pipeline->generation);
    sync_unlock(); result = RIN_VK_SUCCESS;
done:
    free(vertex_spirv); free(fragment_spirv); free(vertex_ir); free(fragment_ir);
    return result;
}

RinVkResult RIN_VKAPI_CALL vkCreateGraphicsPipelines(
        RinVkDevice device, RinVkPipelineCache pipeline_cache,
        uint32_t create_info_count, const RinVkGraphicsPipelineCreateInfo* create_infos,
        const void* allocator, RinVkPipeline* pipelines) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkResult result = RIN_VK_SUCCESS;
    uint32_t index;
    (void)allocator;
    if (!create_info_count || create_info_count > RIN_VK_MAX_PIPELINES || !create_infos || !pipelines)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < create_info_count; ++index) pipelines[index] = 0u;
    if (!owner || (pipeline_cache && !pipeline_cache_slot(device, pipeline_cache)))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < create_info_count; ++index) {
        result = create_graphics_pipeline_one(device, &create_infos[index], &pipelines[index]);
        if (result != RIN_VK_SUCCESS) break;
    }
    if (result != RIN_VK_SUCCESS) for (index = 0u; index < create_info_count; ++index) {
        RinVkPipelineSlot* pipeline;
        if (!pipelines[index]) continue;
        sync_lock(); pipeline = pipeline_slot(device, pipelines[index]);
        if (pipeline) clear_pipeline_slot(pipeline);
        sync_unlock(); pipelines[index] = 0u;
    }
    return result;
}

static RinVkResult create_compute_pipeline_one(
        RinVkDevice device, const RinVkComputePipelineCreateInfo* info,
        RinVkPipeline* pipeline_out) {
    struct RinVkDevice_T* owner;
    RinVkShaderModuleSlot* module;
    RinVkPipelineLayoutSlot* layout;
    RinVkPipelineSlot* pipeline;
    RinSpirvTranslationInfoV1 translation;
    RinVkDescriptorSetLayout set_layouts[
        RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS];
    uint32_t* spirv_copy = NULL;
    uint8_t* shader_ir = NULL;
    size_t spirv_size = 0u;
    size_t shader_size = 0u;
    uint32_t set_layout_count = 0u;
    uint32_t index;
    uint32_t slot_index = 0u;
    const RinShaderHeaderV1* shader_header;
    int translate_result;

    if (!info || !pipeline_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *pipeline_out = 0u;
    if (info->sType != RIN_VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO ||
        info->pNext || info->flags != 0u ||
        info->stage.sType !=
            RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO ||
        info->stage.pNext || info->stage.flags != 0u ||
        info->stage.stage != RIN_VK_SHADER_STAGE_COMPUTE_BIT ||
        !info->stage.pName || info->stage.pSpecializationInfo ||
        info->basePipelineHandle != 0u || info->basePipelineIndex != -1)
        return RIN_VK_ERROR_FEATURE_NOT_PRESENT;

    sync_lock();
    owner = device_slot(device);
    module = owner ? shader_module_slot(device, info->stage.module) : NULL;
    layout = owner ? pipeline_layout_slot(device, info->layout) : NULL;
    if (!owner || !module || !layout ||
        layout->set_layout_count == 0u ||
        layout->set_layout_count >
            RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    spirv_copy = (uint32_t*)malloc(module->code_size);
    spirv_size = module->code_size;
    set_layout_count = layout->set_layout_count;
    if (spirv_copy)
        memcpy(spirv_copy, module->code, module->code_size);
    memcpy(set_layouts, layout->set_layouts,
           sizeof(set_layouts[0]) * set_layout_count);
    sync_unlock();
    if (!spirv_copy) return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;

    shader_ir = (uint8_t*)malloc(RIN_SHADER_MAX_SOURCE_BYTES);
    if (!shader_ir) {
        free(spirv_copy);
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    memset(&translation, 0, sizeof(translation));
    translate_result = ringpu_vulkan_graphics_translate_shader(
        spirv_copy, spirv_size / sizeof(uint32_t),
        RIN_SHADER_STAGE_COMPUTE, NULL, 0u, shader_ir,
        RIN_SHADER_MAX_SOURCE_BYTES, &translation);
    free(spirv_copy);
    shader_header = (const RinShaderHeaderV1*)(const void*)shader_ir;
    if (translate_result != RIN_GPU_VULKAN_GRAPHICS_OK ||
        translation.descriptor_count == 0u ||
        translation.descriptor_count > RIN_SPIRV_MAX_RESOURCES ||
        shader_header->total_size < sizeof(RinShaderHeaderV1) ||
        shader_header->total_size > RIN_SHADER_MAX_SOURCE_BYTES ||
        shader_header->stage != RIN_SHADER_STAGE_COMPUTE ||
        strcmp(info->stage.pName, translation.entry_name) != 0) {
        free(shader_ir);
        return translate_result == RIN_GPU_VULKAN_GRAPHICS_LIMIT
                   ? RIN_VK_ERROR_OUT_OF_HOST_MEMORY
                   : RIN_VK_ERROR_FEATURE_NOT_PRESENT;
    }
    shader_size = shader_header->total_size;
    for (index = 0u; index < translation.descriptor_count; ++index) {
        const RinSpirvDescriptorV1* descriptor =
            &translation.descriptors[index];
        uint32_t prior;
        if (descriptor->set != 0u ||
            descriptor->resource_kind != RIN_SHADER_RESOURCE_STORAGE_BUFFER ||
            descriptor->resource_index >= RIN_SHADER_MAX_RESOURCES ||
            descriptor->binding != descriptor->resource_index) {
            free(shader_ir);
            return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        }
        for (prior = 0u; prior < index; ++prior)
            if (translation.descriptors[prior].resource_index ==
                    descriptor->resource_index ||
                (translation.descriptors[prior].set == descriptor->set &&
                 translation.descriptors[prior].binding ==
                     descriptor->binding)) {
                free(shader_ir);
                return RIN_VK_ERROR_FEATURE_NOT_PRESENT;
            }
    }

    sync_lock();
    owner = device_slot(device);
    layout = owner ? pipeline_layout_slot(device, info->layout) : NULL;
    if (!owner || !layout ||
        layout->set_layout_count != set_layout_count ||
        layout->set_layout_count == 0u ||
        layout->set_layout_count > RIN_GPU_VULKAN_COMMAND_MAX_DESCRIPTOR_SETS ||
        memcmp(set_layouts, layout->set_layouts,
               sizeof(set_layouts[0]) * layout->set_layout_count) != 0) {
        sync_unlock();
        free(shader_ir);
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    pipeline = reserve_pipeline_slot(owner, &slot_index);
    if (!pipeline) {
        sync_unlock();
        free(shader_ir);
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    pipeline->set_layout_count = layout->set_layout_count;
    pipeline->descriptor_count = translation.descriptor_count;
    pipeline->shader_size = (uint32_t)shader_size;
    pipeline->kind = RIN_VK_PIPELINE_KIND_COMPUTE;
    memcpy(pipeline->set_layouts, set_layouts,
           sizeof(set_layouts[0]) * layout->set_layout_count);
    memcpy(pipeline->descriptors, translation.descriptors,
           sizeof(pipeline->descriptors[0]) * translation.descriptor_count);
    pipeline->shader_ir = shader_ir;
    __atomic_store_n(&pipeline->state, 1u, __ATOMIC_RELEASE);
    *pipeline_out = resource_handle(RIN_VK_PIPELINE_TAG, slot_index,
                                    pipeline->generation);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkCreateComputePipelines(
        RinVkDevice device, RinVkPipelineCache pipeline_cache,
        uint32_t create_info_count,
        const RinVkComputePipelineCreateInfo* create_infos,
        const void* allocator, RinVkPipeline* pipelines) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkResult result = RIN_VK_SUCCESS;
    uint32_t index;
    (void)allocator;
    if (create_info_count == 0u ||
        create_info_count > RIN_VK_MAX_PIPELINES || !create_infos ||
        !pipelines)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < create_info_count; ++index) pipelines[index] = 0u;
    if (!owner ||
        (pipeline_cache != 0u && !pipeline_cache_slot(device, pipeline_cache)))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    for (index = 0u; index < create_info_count; ++index) {
        result = create_compute_pipeline_one(device, &create_infos[index],
                                             &pipelines[index]);
        if (result != RIN_VK_SUCCESS) break;
    }
    if (result != RIN_VK_SUCCESS) {
        while (index != 0u) {
            --index;
            vkDestroyPipeline(device, pipelines[index], NULL);
            pipelines[index] = 0u;
        }
    }
    return result;
}

void RIN_VKAPI_CALL vkDestroyPipeline(RinVkDevice device,
                                      RinVkPipeline handle,
                                      const void* allocator) {
    RinVkPipelineSlot* pipeline;
    (void)allocator;
    if (handle == 0u) return;
    sync_lock();
    pipeline = pipeline_slot(device, handle);
    if (pipeline) clear_pipeline_slot(pipeline);
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkCreatePipelineCache(
        RinVkDevice device, const RinVkPipelineCacheCreateInfo* info,
        const void* allocator, RinVkPipelineCache* cache_out) {
    struct RinVkDevice_T* owner = device_slot(device);
    RinVkPipelineCacheSlot* cache;
    const RinVkPipelineCacheBlobHeader* header = NULL;
    uint32_t index;
    (void)allocator;
    if (!cache_out) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    *cache_out = 0u;
    if (!owner || !info ||
        info->sType != RIN_VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO ||
        info->pNext || info->flags != 0u ||
        (info->initialDataSize != 0u && !info->pInitialData) ||
        info->initialDataSize > sizeof(RinVkPipelineCacheBlobHeader) +
                                    RIN_VK_PIPELINE_CACHE_MAX_PAYLOAD ||
        (info->initialDataSize != 0u &&
         !pipeline_cache_blob_valid(owner, info->pInitialData,
                                    info->initialDataSize, &header)))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    sync_lock();
    cache = reserve_pipeline_cache_slot(owner, &index);
    if (!cache) {
        sync_unlock();
        return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    if (header) {
        cache->payload_size = header->payload_size;
        memcpy(cache->payload,
               (const uint8_t*)info->pInitialData + sizeof(*header),
               cache->payload_size);
    }
    __atomic_store_n(&cache->state, 1u, __ATOMIC_RELEASE);
    *cache_out = resource_handle(RIN_VK_PIPELINE_CACHE_TAG, index,
                                 cache->generation);
    sync_unlock();
    return RIN_VK_SUCCESS;
}

void RIN_VKAPI_CALL vkDestroyPipelineCache(
        RinVkDevice device, RinVkPipelineCache cache, const void* allocator) {
    RinVkPipelineCacheSlot* slot;
    (void)allocator;
    if (cache == 0u) return;
    sync_lock();
    slot = pipeline_cache_slot(device, cache);
    if (slot) clear_pipeline_cache_slot(slot);
    sync_unlock();
}

RinVkResult RIN_VKAPI_CALL vkGetPipelineCacheData(
        RinVkDevice device, RinVkPipelineCache cache, size_t* data_size,
        void* data) {
    RinVkPipelineCacheSlot* slot;
    RinVkPipelineCacheBlobHeader header;
    size_t required;
    size_t capacity;
    if (!data_size) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    sync_lock();
    slot = pipeline_cache_slot(device, cache);
    if (!slot) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    pipeline_cache_header(slot->owner, slot->payload_size, &header);
    required = sizeof(header) + slot->payload_size;
    capacity = *data_size;
    *data_size = required;
    if (data) {
        if (capacity != 0u)
            memcpy(data, &header, capacity < sizeof(header) ? capacity
                                                               : sizeof(header));
        if (capacity > sizeof(header))
            memcpy((uint8_t*)data + sizeof(header), slot->payload,
                   (capacity - sizeof(header)) < slot->payload_size
                       ? capacity - sizeof(header)
                       : slot->payload_size);
    }
    sync_unlock();
    return data && capacity < required ? RIN_VK_INCOMPLETE : RIN_VK_SUCCESS;
}

RinVkResult RIN_VKAPI_CALL vkMergePipelineCaches(
        RinVkDevice device, RinVkPipelineCache dst_cache,
        uint32_t src_cache_count, const RinVkPipelineCache* src_caches) {
    RinVkPipelineCacheSlot* destination;
    RinVkPipelineCacheSlot* sources[RIN_VK_MAX_PIPELINE_CACHES];
    uint32_t index;
    uint32_t total;
    if (src_cache_count > RIN_VK_MAX_PIPELINE_CACHES ||
        (src_cache_count != 0u && !src_caches))
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    sync_lock();
    destination = pipeline_cache_slot(device, dst_cache);
    if (!destination) {
        sync_unlock();
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    }
    total = destination->payload_size;
    for (index = 0u; index < src_cache_count; ++index) {
        uint32_t prior;
        sources[index] = pipeline_cache_slot(device, src_caches[index]);
        if (!sources[index] || src_caches[index] == dst_cache) {
            sync_unlock();
            return RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
        for (prior = 0u; prior < index; ++prior)
            if (src_caches[prior] == src_caches[index]) {
                sync_unlock();
                return RIN_VK_ERROR_INITIALIZATION_FAILED;
            }
        if (UINT32_MAX - total < sources[index]->payload_size ||
            total + sources[index]->payload_size >
                RIN_VK_PIPELINE_CACHE_MAX_PAYLOAD) {
            sync_unlock();
            return RIN_VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        total += sources[index]->payload_size;
    }
    total = destination->payload_size;
    for (index = 0u; index < src_cache_count; ++index) {
        memcpy(destination->payload + total, sources[index]->payload,
               sources[index]->payload_size);
        total += sources[index]->payload_size;
    }
    destination->payload_size = total;
    sync_unlock();
    return RIN_VK_SUCCESS;
}

RinVkVoidFunction RIN_VKAPI_CALL vkGetDeviceProcAddr(
        RinVkDevice device, const char* name) {
    struct RinVkDevice_T* device_value = device_slot(device);
    struct RinVkInstance_T* instance =
        debug_instance_for_device(device_value);
    if (!device_value || !name) return NULL;
    if (instance && instance->debug_utils_enabled) {
        if (name_equal(name, "vkSetDebugUtilsObjectNameEXT"))
            return (RinVkVoidFunction)vkSetDebugUtilsObjectNameEXT;
        if (name_equal(name, "vkSetDebugUtilsObjectTagEXT"))
            return (RinVkVoidFunction)vkSetDebugUtilsObjectTagEXT;
        if (name_equal(name, "vkCmdBeginDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkCmdBeginDebugUtilsLabelEXT;
        if (name_equal(name, "vkCmdEndDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkCmdEndDebugUtilsLabelEXT;
        if (name_equal(name, "vkCmdInsertDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkCmdInsertDebugUtilsLabelEXT;
        if (name_equal(name, "vkQueueBeginDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkQueueBeginDebugUtilsLabelEXT;
        if (name_equal(name, "vkQueueEndDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkQueueEndDebugUtilsLabelEXT;
        if (name_equal(name, "vkQueueInsertDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkQueueInsertDebugUtilsLabelEXT;
    }
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
    if (name_equal(name, "vkQueueSubmit2"))
        return (RinVkVoidFunction)vkQueueSubmit2;
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
    if (name_equal(name, "vkCmdPipelineBarrier2"))
        return (RinVkVoidFunction)vkCmdPipelineBarrier2;
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
    if (name_equal(name, "vkCmdClearDepthStencilImage"))
        return (RinVkVoidFunction)vkCmdClearDepthStencilImage;
    if (name_equal(name, "vkCreateQueryPool"))
        return (RinVkVoidFunction)vkCreateQueryPool;
    if (name_equal(name, "vkDestroyQueryPool"))
        return (RinVkVoidFunction)vkDestroyQueryPool;
    if (name_equal(name, "vkGetQueryPoolResults"))
        return (RinVkVoidFunction)vkGetQueryPoolResults;
    if (name_equal(name, "vkCmdResetQueryPool"))
        return (RinVkVoidFunction)vkCmdResetQueryPool;
    if (name_equal(name, "vkCmdBeginQuery"))
        return (RinVkVoidFunction)vkCmdBeginQuery;
    if (name_equal(name, "vkCmdEndQuery"))
        return (RinVkVoidFunction)vkCmdEndQuery;
    if (name_equal(name, "vkCmdWriteTimestamp"))
        return (RinVkVoidFunction)vkCmdWriteTimestamp;
    if (name_equal(name, "vkCreateEvent"))
        return (RinVkVoidFunction)vkCreateEvent;
    if (name_equal(name, "vkDestroyEvent"))
        return (RinVkVoidFunction)vkDestroyEvent;
    if (name_equal(name, "vkGetEventStatus"))
        return (RinVkVoidFunction)vkGetEventStatus;
    if (name_equal(name, "vkSetEvent"))
        return (RinVkVoidFunction)vkSetEvent;
    if (name_equal(name, "vkResetEvent"))
        return (RinVkVoidFunction)vkResetEvent;
    if (name_equal(name, "vkCmdSetEvent"))
        return (RinVkVoidFunction)vkCmdSetEvent;
    if (name_equal(name, "vkCmdResetEvent"))
        return (RinVkVoidFunction)vkCmdResetEvent;
    if (name_equal(name, "vkCmdWaitEvents"))
        return (RinVkVoidFunction)vkCmdWaitEvents;
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
    if (name_equal(name, "vkCreateComputePipelines"))
        return (RinVkVoidFunction)vkCreateComputePipelines;
    if (name_equal(name, "vkCreateGraphicsPipelines"))
        return (RinVkVoidFunction)vkCreateGraphicsPipelines;
    if (name_equal(name, "vkDestroyPipeline"))
        return (RinVkVoidFunction)vkDestroyPipeline;
    if (name_equal(name, "vkCmdBindDescriptorSets"))
        return (RinVkVoidFunction)vkCmdBindDescriptorSets;
    if (name_equal(name, "vkCmdBindPipeline"))
        return (RinVkVoidFunction)vkCmdBindPipeline;
    if (name_equal(name, "vkCmdBindVertexBuffers"))
        return (RinVkVoidFunction)vkCmdBindVertexBuffers;
    if (name_equal(name, "vkCmdDraw"))
        return (RinVkVoidFunction)vkCmdDraw;
    if (device_value->dynamic_rendering_enabled &&
        name_equal(name, "vkCmdBeginRenderingKHR"))
        return (RinVkVoidFunction)vkCmdBeginRenderingKHR;
    if (device_value->dynamic_rendering_enabled &&
        name_equal(name, "vkCmdEndRenderingKHR"))
        return (RinVkVoidFunction)vkCmdEndRenderingKHR;
    if (name_equal(name, "vkCmdDispatch"))
        return (RinVkVoidFunction)vkCmdDispatch;
    if (name_equal(name, "vkCreatePipelineCache"))
        return (RinVkVoidFunction)vkCreatePipelineCache;
    if (name_equal(name, "vkDestroyPipelineCache"))
        return (RinVkVoidFunction)vkDestroyPipelineCache;
    if (name_equal(name, "vkGetPipelineCacheData"))
        return (RinVkVoidFunction)vkGetPipelineCacheData;
    if (name_equal(name, "vkMergePipelineCaches"))
        return (RinVkVoidFunction)vkMergePipelineCaches;
    if (name_equal(name, "vkCreateShaderModule"))
        return (RinVkVoidFunction)vkCreateShaderModule;
    if (name_equal(name, "vkDestroyShaderModule"))
        return (RinVkVoidFunction)vkDestroyShaderModule;
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
    if (instance_slot(instance)->debug_utils_enabled) {
        if (name_equal(name, "vkCreateDebugUtilsMessengerEXT"))
            return (RinVkVoidFunction)vkCreateDebugUtilsMessengerEXT;
        if (name_equal(name, "vkDestroyDebugUtilsMessengerEXT"))
            return (RinVkVoidFunction)vkDestroyDebugUtilsMessengerEXT;
        if (name_equal(name, "vkSetDebugUtilsObjectNameEXT"))
            return (RinVkVoidFunction)vkSetDebugUtilsObjectNameEXT;
        if (name_equal(name, "vkSetDebugUtilsObjectTagEXT"))
            return (RinVkVoidFunction)vkSetDebugUtilsObjectTagEXT;
        if (name_equal(name, "vkSubmitDebugUtilsMessageEXT"))
            return (RinVkVoidFunction)vkSubmitDebugUtilsMessageEXT;
        if (name_equal(name, "vkCmdBeginDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkCmdBeginDebugUtilsLabelEXT;
        if (name_equal(name, "vkCmdEndDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkCmdEndDebugUtilsLabelEXT;
        if (name_equal(name, "vkCmdInsertDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkCmdInsertDebugUtilsLabelEXT;
        if (name_equal(name, "vkQueueBeginDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkQueueBeginDebugUtilsLabelEXT;
        if (name_equal(name, "vkQueueEndDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkQueueEndDebugUtilsLabelEXT;
        if (name_equal(name, "vkQueueInsertDebugUtilsLabelEXT"))
            return (RinVkVoidFunction)vkQueueInsertDebugUtilsLabelEXT;
    }
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
