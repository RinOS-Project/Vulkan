# RinVulkan ownership

RinVulkan owns the Vulkan-facing ABI, loader entry points, command/runtime
translation, and WSI translation.  It depends on the graphics-neutral RinGPU
public headers and library only.

OS-Core owns physical driver execution and supplies the
`RinVulkanProductPlatformV1` callback table.  The callback table is validated
at bind time; missing callbacks, stale epochs, and callback failures close the
operation with an error.  RinVulkan never includes OS-Core private headers and
never fabricates a software-success result for an absent physical owner.

The ICD manifest is installed by the OS image integration, while the template
and ABI definition remain in this repository.
