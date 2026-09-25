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

Build and test:

```sh
cmake -S . -B build -DRINGPU_DIR=../RinGPU -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
