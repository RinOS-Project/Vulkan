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
process-local RinVk runtime before `vkCreateInstance`. Before device creation,
it composes the SDK-backed V1–V5 memory/resource adapter with the V4
logical-to-physical queue mapping and V6–V8 canonical process-submit adapter.
The one-frame example uses the transfer/clear/readback operations supported by
that canonical path; operations without a real provider remain unsupported.
It completes queue work before handing pixels to the ordinary Compositor
surface rather than pretending process-submit supports native wait
semaphores. This is not a `VK_KHR_display` direct-scanout path. The example
does not initialize the host software platform or silently fall back when the
physical runtime is unavailable.

For the RinOS x86_64 debug user-app artifact, build the focused Meson target
from the repository root:
`python scripts/build.py --profile x86_64-debug --target app:rinvk_native_window_surface64`.
This creates a normal `.RIN` executable; it does not add a system-service
catalog entry or run at boot. The optional POSIX CMake target remains useful
for host-side compilation and links RinRuntime's Compositor transport.

`rinvulkan_rinos_process_submit_backend_v1.h` adds an opt-in RinOS SDK adapter
for V6 canonical process-submit and V7/V8 process-compute object lifecycle.
It binds authenticated process queue tokens, translates resource records,
verifies generation-bound receipts, and retains queue leases when release must
be retried. `rinos_process_product_platform_v1.h` supplies the SDK-backed
status, allocation, mapping, synchronization, and resource-binding parent used
by the app bootstrap. The sample unbinds V8→V6, detaches actual queue tokens,
then releases the V5→V1 parent before the runtime. Native-window surface stays
on RinRuntime's Compositor import path; physical-display WSI/provider
registration and final scanout remain separate and are not claimed complete.

The user-app target now has a process-local catalog/runtime and V1–V8
SDK-backed memory/submit bootstrap for this supported transfer example. It
reports real SDK/Vulkan errors; it does not manufacture a device, install a
compatibility renderer, or fall back to the software backend. Building the
artifact verifies the application source and app-link graph, not that the
`.RIN` artifact builds or a hardware-backed frame is displayed. Host fixture
success and provenance validity are separate evidence, and real-device
validation remains open until the exact platform/GPU routing is tested.
