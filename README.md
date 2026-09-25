# RinVulkan

RinVulkan owns the Vulkan-facing ABI, loader entry points, runtime/profile,
command translation, graphics translation, and WSI. It depends on the public
graphics-neutral RinGPU API and is built as `RinVulkan::RinVulkan` by the
standalone CMake project.

The ICD manifest template is in `icd.d/`. OS-Core controls installation and
supplies physical execution through `RinVulkanProductPlatformV1`; the binding
rejects absent callbacks and stale product state. This repository does not
claim physical GPU driver, IRQ/DMA, external backend, or hardware evidence.

The public ICD currently advertises only the host-validated Vulkan 1.0
bounded profile. Vulkan 1.1-1.3 feature chains and all extensions are
fail-closed until their command, lifetime, synchronization, and error
semantics are wired and tested; the internal product profile version is not a
public capability claim.

`RinGpuVulkanSoftwarePlatformV1` is the host-only product owner used by the
software-path regression. It owns zeroed allocation bytes, validates GPU
address ranges and resource leases, executes the versioned transfer packet,
and publishes completion through `poll`. The host profile includes bounded
2D RGBA8 single-mip/layer `vkCmdCopyImage`, buffer-to-image, and
image-to-buffer operations with actual byte movement and readback. The same
host profile also executes full-image packed-RGBA8 `vkCmdClearColorImage`
operations and bounded full-image RGBA8 `vkCmdBlitImage` with nearest/linear
sampling. It also resolves full-image RGBA8 sample-major 2x/4x images into a
single-sample image using integer averaging. It does not advertise or emulate
a physical GPU, and malformed or unsupported packets fail closed.

Build and test:

```sh
cmake -S . -B build -DRINGPU_DIR=../RinGPU -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
