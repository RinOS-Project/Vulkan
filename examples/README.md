# RinOS Vulkan examples

`rinos_native_window_surface.c` is a small RinVk application example for the
ordinary RinRuntime window → Compositor surface path. It obtains the current
versioned callback table from `wnd_get_gpu_surface_ops_v1`, passes it to
`vkCreateRinOSNativeWindowSurfaceV1`, selects a surface-compatible queue,
checks the reported capabilities/format/FIFO mode, and submits one cleared
swapchain image through `vkQueuePresentKHR`. The image layout transitions use
the reported synchronization2 extension/feature; the example has no legacy or
software fallback. The table and window remain alive until after surface
destruction.

The example enables `VK_KHR_surface` and
`VK_RINOS_native_window_surface`; it does not enable or call `VK_KHR_display`.
This is a reference consumer, not a production app/bootstrap registration or
proof that the Compositor renderer accepted/displayed the frame. The ICD's
ordinary path copies application swapchain images into the Compositor callback;
those images are not required to be direct-scanout allocations. Final-output
scanout remains separate. The system must already have an admitted, bound
Vulkan runtime. The example does not initialize the host software platform or
silently fall back when the physical runtime is unavailable.

On a POSIX RinOS/host development environment, enable the optional CMake target
with `-DRINVULKAN_BUILD_RINOS_NATIVE_WINDOW_EXAMPLE=ON`, then build
`rinvulkan-native-window-surface-example`. The target includes the real
RinRuntime Compositor transport dependency.
