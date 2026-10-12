# RinOS Vulkan examples

`rinos_native_window_surface.c` is a small RinVk application example for the
ordinary RinRuntime window → Compositor surface path. It obtains the current
versioned callback table from `wnd_get_gpu_surface_ops_v1`, passes it to
`vkCreateRinOSNativeWindowSurfaceV1`, selects a surface-compatible queue,
checks the reported capabilities/format/FIFO mode, and submits one cleared
swapchain image through `vkQueuePresentKHR`. It then dispatches Runtime
Compositor completions and waits up to ten seconds for the damage/commit
response before teardown. The image layout transitions use the reported
synchronization2 extension/feature; the example has no legacy or software
fallback. The table and window remain alive until after surface destruction.

The example enables `VK_KHR_surface` and
`VK_RINOS_native_window_surface`; it does not enable or call `VK_KHR_display`.
This is a reference consumer, not a production app/bootstrap registration or
proof that the Compositor renderer displayed the frame. A successful
damage/commit response confirms service acceptance of that transaction only.
The ICD's ordinary path copies application swapchain images into the Compositor
callback; those images are not required to be direct-scanout allocations.
Final-output scanout remains separate. On RinOS, the example binds the SDK
target backend to the real `RIN_SYS_SDK_CALL` syscall, queries the authenticated
`ringpu-capability` catalog, and binds admitted physical profiles into the
process-local RinVk runtime before `vkCreateInstance`. The queue-route adapter
can translate the actual device queue-create order into V4
logical-to-physical mappings. This is catalog/bootstrap integration only: it
does not yet compose the V1–V8 product memory, submission, and WSI platform
owners needed to accept a production device and frame. It does not initialize
the host software platform or silently fall back when the physical runtime is
unavailable.

For the RinOS x86_64 debug user-app artifact, build the focused Meson target
from the repository root:
`python scripts/build.py --profile x86_64-debug --target app:rinvk_native_window_surface64`.
This creates a normal `.RIN` executable; it does not add a system-service
catalog entry or run at boot. The optional POSIX CMake target remains useful
for host-side compilation and links RinRuntime's Compositor transport.

`rinvulkan_rinos_process_submit_backend_v1.h` adds an opt-in RinOS SDK adapter
for the V6 canonical process-submit callback. It binds authenticated process
queue tokens, translates resource records, verifies the returned generation-
bound receipt, and retains queue leases when release must be retried. It is a
submit/lifetime slice only: it does not create the per-process Vulkan runtime,
provide the V5 memory/resource parent, establish a trusted device profile, or
register native-window WSI. Applications must quiesce and unbind V6 before
detaching this adapter.

The user-app target therefore still has no process-local production Vulkan
runtime bootstrap. RinOS-SDK provides process-owned GPU allocation/queue calls,
but no process API that creates or attaches the RinVulkan runtime and its
device/WSI callbacks; the remaining platform objects are not yet composed by
the application path. The sample reports real `vkCreateInstance`/device errors
and exits; it does not manufacture a device, install a compatibility renderer,
or fall back to the software backend. Building this artifact verifies the
application source and app-link graph, not that the `.RIN` artifact builds or a
hardware-backed frame is displayed. Keep app runtime/bootstrap and
Compositor-present validation unchecked until the process binding and
production WSI path are connected.
