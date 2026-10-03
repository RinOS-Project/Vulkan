/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_ATOMIC_COMPAT_H
#define RINVULKAN_ATOMIC_COMPAT_H

#if defined(_MSC_VER)
#include <intrin.h>
#include <stdint.h>
#include <string.h>

#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange64)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedCompareExchangePointer)
#pragma intrinsic(_InterlockedExchangePointer)

static __inline uint32_t rin_vk_atomic_load_u32(void* address) {
    return (uint32_t)_InterlockedCompareExchange(
        (volatile long*)address, 0, 0);
}

static __inline uint64_t rin_vk_atomic_load_u64(void* address) {
    return (uint64_t)_InterlockedCompareExchange64(
        (volatile __int64*)address, 0, 0);
}

static __inline void* rin_vk_atomic_load_pointer(void* address) {
    return _InterlockedCompareExchangePointer(
        (void* volatile*)address, NULL, NULL);
}

static __inline void rin_vk_atomic_store_u32(void* address,
                                               uint32_t value) {
    (void)_InterlockedExchange((volatile long*)address, (long)value);
}

static __inline void rin_vk_atomic_store_u64(void* address,
                                               uint64_t value) {
    (void)_InterlockedExchange64((volatile __int64*)address,
                                 (__int64)value);
}

static __inline void rin_vk_atomic_store_pointer(void* address,
                                                  void* value) {
    (void)_InterlockedExchangePointer((void* volatile*)address, value);
}

static __inline int rin_vk_atomic_compare_exchange_u32(
        void* address, void* expected_address, uint32_t desired) {
    uint32_t expected;
    uint32_t observed;
    memcpy(&expected, expected_address, sizeof(expected));
    observed = (uint32_t)_InterlockedCompareExchange(
        (volatile long*)address, (long)desired, (long)expected);
    if (observed == expected) return 1;
    memcpy(expected_address, &observed, sizeof(observed));
    return 0;
}

static __inline int rin_vk_atomic_compare_exchange_u64(
        void* address, void* expected_address, uint64_t desired) {
    uint64_t expected;
    uint64_t observed;
    memcpy(&expected, expected_address, sizeof(expected));
    observed = (uint64_t)_InterlockedCompareExchange64(
        (volatile __int64*)address, (__int64)desired, (__int64)expected);
    if (observed == expected) return 1;
    memcpy(expected_address, &observed, sizeof(observed));
    return 0;
}

static __inline int rin_vk_atomic_compare_exchange_pointer(
        void* address, void* expected_address, void* desired) {
    void* expected;
    void* observed;
    memcpy(&expected, expected_address, sizeof(expected));
    observed = _InterlockedCompareExchangePointer(
        (void* volatile*)address, desired, expected);
    if (observed == expected) return 1;
    memcpy(expected_address, &observed, sizeof(observed));
    return 0;
}

static __inline uint32_t rin_vk_atomic_exchange_u32(void* address,
                                                    uint32_t value) {
    return (uint32_t)_InterlockedExchange((volatile long*)address,
                                           (long)value);
}

static __inline uint32_t rin_vk_atomic_add_fetch_u32(void* address,
                                                      uint32_t value) {
    uint32_t previous = (uint32_t)_InterlockedExchangeAdd(
        (volatile long*)address, (long)value);
    return previous + value;
}

static __inline uint32_t rin_vk_atomic_sub_fetch_u32(void* address,
                                                      uint32_t value) {
    uint32_t delta_bits = 0u - value;
    long delta;
    uint32_t previous;
    memcpy(&delta, &delta_bits, sizeof(delta));
    previous = (uint32_t)_InterlockedExchangeAdd((volatile long*)address,
                                                  delta);
    return previous - value;
}

static __inline uint64_t rin_vk_atomic_add_fetch_u64(void* address,
                                                      uint64_t value) {
    uint64_t previous = (uint64_t)_InterlockedExchangeAdd64(
        (volatile __int64*)address, (__int64)value);
    return previous + value;
}

static __inline uint64_t rin_vk_atomic_sub_fetch_u64(void* address,
                                                      uint64_t value) {
    uint64_t delta_bits = UINT64_C(0) - value;
    __int64 delta;
    uint64_t previous;
    memcpy(&delta, &delta_bits, sizeof(delta));
    previous = (uint64_t)_InterlockedExchangeAdd64(
        (volatile __int64*)address, delta);
    return previous - value;
}

#ifndef __ATOMIC_RELAXED
#define __ATOMIC_RELAXED 0
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#endif

#define __atomic_load_n(object, order) \
    (_Generic(*(object), \
        uint32_t: rin_vk_atomic_load_u32, \
        uint64_t: rin_vk_atomic_load_u64, \
        default: rin_vk_atomic_load_pointer)((void*)(object)))
#define __atomic_store_n(object, value, order) \
    (_Generic(*(object), \
        uint32_t: rin_vk_atomic_store_u32, \
        uint64_t: rin_vk_atomic_store_u64, \
        default: rin_vk_atomic_store_pointer)((void*)(object), (value)))
#define __atomic_compare_exchange_n(object, expected, desired, weak, success, \
                                    failure) \
    (_Generic(*(object), \
        uint32_t: rin_vk_atomic_compare_exchange_u32, \
        uint64_t: rin_vk_atomic_compare_exchange_u64, \
        default: rin_vk_atomic_compare_exchange_pointer)( \
            (void*)(object), (void*)(expected), (desired)))
#define __atomic_exchange_n(object, value, order) \
    (_Generic(*(object), uint32_t: rin_vk_atomic_exchange_u32)( \
        (void*)(object), (value)))
#define __atomic_add_fetch(object, value, order) \
    (_Generic(*(object), \
        uint32_t: rin_vk_atomic_add_fetch_u32, \
        uint64_t: rin_vk_atomic_add_fetch_u64)((void*)(object), (value)))
#define __atomic_sub_fetch(object, value, order) \
    (_Generic(*(object), \
        uint32_t: rin_vk_atomic_sub_fetch_u32, \
        uint64_t: rin_vk_atomic_sub_fetch_u64)((void*)(object), (value)))
#endif

#endif
