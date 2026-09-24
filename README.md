# RinVulkan

RinVulkan owns the Vulkan-facing ABI, loader entry points, runtime/profile,
command translation, graphics translation, and WSI. It depends on the public
graphics-neutral RinGPU API and is built as `RinVulkan::RinVulkan` by the
standalone CMake project.

The ICD manifest template is in `icd.d/`. OS-Core controls installation and
supplies physical execution through `RinVulkanProductPlatformV1`; the binding
rejects absent callbacks and stale product state. This repository does not
claim physical GPU driver, IRQ/DMA, external backend, or hardware evidence.

Build and test:

```sh
cmake -S . -B build -DRINGPU_DIR=../RinGPU -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
