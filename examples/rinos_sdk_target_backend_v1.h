/* SPDX-License-Identifier: MIT */
#ifndef RIN_VULKAN_EXAMPLE_SDK_TARGET_BACKEND_V1_H
#define RIN_VULKAN_EXAMPLE_SDK_TARGET_BACKEND_V1_H

#include <rin/abi.h>
#include <rin/syscall_abi.h>

#include <stdint.h>
#include <string.h>

#ifndef RIN_VULKAN_EXAMPLE_SDK_SYSCALL6
#include <sys/syscall.h>
#define RIN_VULKAN_EXAMPLE_SDK_SYSCALL6 _syscall6
#endif

static inline RinResult rin_vulkan_example_sdk_syscall_invoke(
        uint64_t context, uint64_t syscall_number,
        uint64_t arguments_address, uint64_t arguments_size,
        uint64_t library_id, uint64_t operation,
        uint64_t response_address, uint64_t response_size) {
    intptr_t raw_result;

    if (context != 0u || syscall_number != RIN_SYS_SDK_CALL ||
        arguments_address > (uint64_t)UINTPTR_MAX ||
        arguments_size > (uint64_t)UINTPTR_MAX ||
        library_id > (uint64_t)UINTPTR_MAX ||
        operation > (uint64_t)UINTPTR_MAX ||
        response_address > (uint64_t)UINTPTR_MAX ||
        response_size > (uint64_t)UINTPTR_MAX)
        return RIN_ERROR_INVALID_ARGUMENT;

    /* Preserve the kernel's signed 32-bit RinResult. libc syscall() would
     * translate errors to -1/errno and lose the SDK status. */
    raw_result = RIN_VULKAN_EXAMPLE_SDK_SYSCALL6(
        (uintptr_t)syscall_number, (uintptr_t)arguments_address,
        (uintptr_t)arguments_size, (uintptr_t)library_id,
        (uintptr_t)operation, (uintptr_t)response_address,
        (uintptr_t)response_size);
#if UINTPTR_MAX > UINT32_MAX
    {
        const uint32_t status_bits = (uint32_t)(uintptr_t)raw_result;
        const uint64_t raw_bits = (uint64_t)(uintptr_t)raw_result;
        const uint64_t sign_extended =
            (uint64_t)(int64_t)(int32_t)status_bits;
        /* The syscall entry paths expose RinResult either sign-extended or
         * as a zero-extended 32-bit register result. Reject other shapes. */
        if (raw_bits != (uint64_t)status_bits &&
            raw_bits != sign_extended)
            return RIN_ERROR_ABI_MISMATCH;
    }
#endif
    return (RinResult)(int32_t)(uint32_t)(uintptr_t)raw_result;
}

static inline RinResult rin_vulkan_example_bind_sdk_target_backend_v1(void) {
    RinSdkTargetBackendV1 backend;
    memset(&backend, 0, sizeof(backend));
    backend.struct_size = sizeof(backend);
    backend.version = RIN_SDK_STRUCT_VERSION_1;
    backend.syscall_number = RIN_SYS_SDK_CALL;
    backend.invoke = (uint64_t)(uintptr_t)
        &rin_vulkan_example_sdk_syscall_invoke;
    return rin_sdk_bind_target_backend_v1(&backend);
}

#endif
