# RinOS Vulkan examples

`rinos_native_window_surface.c` demonstrates application-side setup for the
ordinary RinRuntime window → Compositor surface path. It obtains the current
versioned callback table from `wnd_get_gpu_surface_ops_v1`, passes it to
`vkCreateRinOSNativeWindowSurfaceV1`, and asks the ICD which queue families
support that surface. The table and window remain alive until after surface
destruction.

The example enables `VK_KHR_surface` and
`VK_RINOS_native_window_surface`; it does not enable or call `VK_KHR_display`.
It stops after checking surface/queue compatibility. Applications create and
present their own swapchain images through the ordinary Compositor route; those
images are not required to be direct-scanout allocations. The system must
already have an admitted, bound Vulkan runtime. The example does not initialize
the host software platform or silently fall back when the physical runtime is
unavailable.

On a POSIX RinOS/host development environment, enable the optional CMake target
with `-DRINVULKAN_BUILD_RINOS_NATIVE_WINDOW_EXAMPLE=ON`, then build
`rinvulkan-native-window-surface-example`. The target includes the real
RinRuntime Compositor transport dependency.
