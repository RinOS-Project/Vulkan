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
Final-output scanout remains separate. The system must already have an
admitted, bound Vulkan runtime. The example does not initialize the host
software platform or silently fall back when the physical runtime is
unavailable.

For the RinOS x86_64 debug user-app artifact, build the focused Meson target
from the repository root:
`python scripts/build.py --profile x86_64-debug --target app:rinvk_native_window_surface64`.
This creates a normal `.RIN` executable; it does not add a system-service
catalog entry or run at boot. The optional POSIX CMake target remains useful
for host-side compilation and links RinRuntime's Compositor transport.

The user-app target currently has no process-local production Vulkan runtime
bootstrap: the process SDK does not yet expose a device/runtime binding, while
the existing ICD bind entry points consume OS-Core-owned pointers. Therefore
the sample reports the real `vkCreateInstance`/device errors and exits; it does
not manufacture a device, install a compatibility renderer, or fall back to
the software backend. Building this artifact verifies the application and
surface API are packaged through the normal user-app linker, not that a
hardware-backed frame is displayed. Keep app/bootstrap registration and
runtime validation unchecked until that binding and production WSI provider
are connected.
