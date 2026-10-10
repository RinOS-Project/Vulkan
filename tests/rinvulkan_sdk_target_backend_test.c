/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>

static intptr_t test_syscall6(uintptr_t number, uintptr_t argument1,
                              uintptr_t argument2, uintptr_t argument3,
                              uintptr_t argument4, uintptr_t argument5,
                              uintptr_t argument6);
#define RIN_VULKAN_EXAMPLE_SDK_SYSCALL6 test_syscall6
#include "../examples/rinos_sdk_target_backend_v1.h"

static RinSdkTargetBackendV1 captured_backend;
static RinResult bind_result = RIN_SUCCESS;
static intptr_t raw_result;
static uintptr_t raw_arguments[7];
static uint32_t bind_count;
static uint32_t raw_call_count;

RinResult rin_sdk_bind_target_backend_v1(
        const RinSdkTargetBackendV1* backend) {
    assert(backend != NULL);
    captured_backend = *backend;
    ++bind_count;
    return bind_result;
}

static intptr_t test_syscall6(uintptr_t number, uintptr_t argument1,
                              uintptr_t argument2, uintptr_t argument3,
                              uintptr_t argument4, uintptr_t argument5,
                              uintptr_t argument6) {
    raw_arguments[0] = number;
    raw_arguments[1] = argument1;
    raw_arguments[2] = argument2;
    raw_arguments[3] = argument3;
    raw_arguments[4] = argument4;
    raw_arguments[5] = argument5;
    raw_arguments[6] = argument6;
    ++raw_call_count;
    return raw_result;
}

int main(void) {
    RinSdkTargetInvokeV1 invoke;

    assert(rin_vulkan_example_bind_sdk_target_backend_v1() == RIN_SUCCESS);
    assert(bind_count == 1u);
    assert(captured_backend.struct_size == sizeof(captured_backend));
    assert(captured_backend.version == RIN_SDK_STRUCT_VERSION_1);
    assert(captured_backend.context == 0u);
    assert(captured_backend.syscall_number == RIN_SYS_SDK_CALL);
    assert(captured_backend.invoke != 0u);
    invoke = (RinSdkTargetInvokeV1)(uintptr_t)captured_backend.invoke;

    raw_result = (intptr_t)(int64_t)RIN_ERROR_NOT_SUPPORTED;
    assert(invoke(captured_backend.context, captured_backend.syscall_number,
                  UINT64_C(0x1000), 56u, RIN_SDK_LIBRARY_GPU, 7u,
                  UINT64_C(0x2000), 24u) == RIN_ERROR_NOT_SUPPORTED);
    assert(raw_call_count == 1u);
    assert(raw_arguments[0] == RIN_SYS_SDK_CALL);
    assert(raw_arguments[1] == (uintptr_t)UINT64_C(0x1000));
    assert(raw_arguments[2] == 56u);
    assert(raw_arguments[3] == RIN_SDK_LIBRARY_GPU);
    assert(raw_arguments[4] == 7u);
    assert(raw_arguments[5] == (uintptr_t)UINT64_C(0x2000));
    assert(raw_arguments[6] == 24u);

    raw_result = (intptr_t)(uint32_t)RIN_ERROR_IO;
    assert(invoke(captured_backend.context, captured_backend.syscall_number,
                  UINT64_C(0x1000), 56u, RIN_SDK_LIBRARY_GPU, 7u,
                  UINT64_C(0x2000), 24u) == RIN_ERROR_IO);
    assert(raw_call_count == 2u);

    assert(invoke(1u, captured_backend.syscall_number, 0u, 0u, 0u, 1u,
                  0u, 0u) == RIN_ERROR_INVALID_ARGUMENT);
    assert(invoke(captured_backend.context,
                  captured_backend.syscall_number + 1u,
                  0u, 0u, 0u, 1u, 0u, 0u) == RIN_ERROR_INVALID_ARGUMENT);
    assert(raw_call_count == 2u);

    bind_result = RIN_ERROR_BUSY;
    assert(rin_vulkan_example_bind_sdk_target_backend_v1() ==
           RIN_ERROR_BUSY);
    assert(bind_count == 2u);
    return 0;
}
