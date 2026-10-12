/* SPDX-License-Identifier: MIT */
#ifndef RINOS_NATIVE_WINDOW_RUNTIME_LIFECYCLE_V1_H
#define RINOS_NATIVE_WINDOW_RUNTIME_LIFECYCLE_V1_H

#include <stdint.h>

#define RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_V1_VERSION UINT32_C(1)

#define RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_CATALOG_CONNECTED UINT32_C(1)
#define RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_INITIALIZED UINT32_C(2)
#define RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_BOUND UINT32_C(4)
#define RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_KNOWN_FLAGS UINT32_C(7)

typedef struct RinVulkanExampleRuntimeLifecycleV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved;
} RinVulkanExampleRuntimeLifecycleV1;

typedef int (*RinVulkanExampleRuntimeLifecycleActionV1)(void* context);

typedef struct RinVulkanExampleRuntimeLifecycleActionsV1 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinVulkanExampleRuntimeLifecycleActionV1 unbind_runtime;
    RinVulkanExampleRuntimeLifecycleActionV1 shutdown_runtime;
    RinVulkanExampleRuntimeLifecycleActionV1 disconnect_catalog;
} RinVulkanExampleRuntimeLifecycleActionsV1;

typedef enum RinVulkanExampleRuntimeLifecycleStageV1 {
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_NONE = 0,
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_UNBIND = 1,
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_SHUTDOWN = 2,
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_DISCONNECT = 3
} RinVulkanExampleRuntimeLifecycleStageV1;

typedef enum RinVulkanExampleRuntimeLifecycleResultV1 {
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_OK = 0,
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_INVALID = -1,
    RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_ACTION_FAILED = -2
} RinVulkanExampleRuntimeLifecycleResultV1;

static inline int rin_vulkan_example_runtime_lifecycle_cleanup_v1(
    RinVulkanExampleRuntimeLifecycleV1* lifecycle,
    const RinVulkanExampleRuntimeLifecycleActionsV1* actions,
    uint32_t* failed_stage_out, int* action_result_out) {
    int action_result;

    if (!lifecycle || !actions || !failed_stage_out || !action_result_out ||
        lifecycle->struct_size != sizeof(*lifecycle) ||
        lifecycle->version !=
            RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_V1_VERSION ||
        (lifecycle->flags & ~RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_KNOWN_FLAGS) !=
            0u || lifecycle->reserved != 0u ||
        actions->struct_size != sizeof(*actions) || actions->version !=
            RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_V1_VERSION ||
        !actions->context || !actions->unbind_runtime ||
        !actions->shutdown_runtime || !actions->disconnect_catalog)
        return RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_INVALID;

    *failed_stage_out = RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_NONE;
    *action_result_out = 0;

    if ((lifecycle->flags &
         RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_BOUND) != 0u) {
        action_result = actions->unbind_runtime(actions->context);
        if (action_result != 0) {
            *failed_stage_out = RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_UNBIND;
            *action_result_out = action_result;
            return RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_ACTION_FAILED;
        }
        lifecycle->flags &=
            ~RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_BOUND;
    }

    if ((lifecycle->flags &
         RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_INITIALIZED) != 0u) {
        action_result = actions->shutdown_runtime(actions->context);
        if (action_result != 0) {
            *failed_stage_out =
                RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_SHUTDOWN;
            *action_result_out = action_result;
            return RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_ACTION_FAILED;
        }
        lifecycle->flags &=
            ~RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_RUNTIME_INITIALIZED;
    }

    if ((lifecycle->flags &
         RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_CATALOG_CONNECTED) != 0u) {
        action_result = actions->disconnect_catalog(actions->context);
        if (action_result != 0) {
            *failed_stage_out =
                RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_STAGE_DISCONNECT;
            *action_result_out = action_result;
            return RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_ACTION_FAILED;
        }
        lifecycle->flags &=
            ~RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_CATALOG_CONNECTED;
    }

    return RIN_VULKAN_EXAMPLE_RUNTIME_LIFECYCLE_OK;
}

#endif
