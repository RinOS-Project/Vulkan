/* SPDX-License-Identifier: MIT */

#include "../src/atomic_compat.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void) {
    uint32_t value32 = 7u;
    uint32_t expected32 = 11u;
    uint64_t value64 = UINT64_C(0x100000002);
    uint64_t expected64 = UINT64_C(0x200000004);
    uintptr_t owner = 9u;
    uintptr_t expected_owner = 11u;
    void* pointer = &value32;
    void* expected_pointer = &value32;

    CHECK(__atomic_load_n(&value32, __ATOMIC_RELAXED) == 7u);
    __atomic_store_n(&value32, 11u, __ATOMIC_RELEASE);
    CHECK(__atomic_load_n(&value32, __ATOMIC_ACQUIRE) == 11u);
    CHECK(__atomic_compare_exchange_n(&value32, &expected32, 13u, 0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED));
    CHECK(value32 == 13u && expected32 == 11u);
    expected32 = 5u;
    CHECK(!__atomic_compare_exchange_n(&value32, &expected32, 15u, 0,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_RELAXED));
    CHECK(value32 == 13u && expected32 == 13u);
    CHECK(__atomic_exchange_n(&value32, 17u, __ATOMIC_ACQUIRE) == 13u);
    CHECK(__atomic_add_fetch(&value32, 3u, __ATOMIC_ACQ_REL) == 20u);
    CHECK(__atomic_sub_fetch(&value32, 4u, __ATOMIC_RELEASE) == 16u);

    CHECK(__atomic_load_n(&value64, __ATOMIC_ACQUIRE) ==
          UINT64_C(0x100000002));
    __atomic_store_n(&value64, UINT64_C(0x200000004), __ATOMIC_RELEASE);
    CHECK(__atomic_compare_exchange_n(&value64, &expected64,
                                      UINT64_C(0x300000006), 0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED));
    CHECK(value64 == UINT64_C(0x300000006));
    CHECK(expected64 == UINT64_C(0x200000004));
    CHECK(__atomic_add_fetch(&value64, UINT64_C(5), __ATOMIC_ACQUIRE) ==
          UINT64_C(0x30000000b));
    CHECK(__atomic_sub_fetch(&value64, UINT64_C(3), __ATOMIC_RELEASE) ==
          UINT64_C(0x300000008));

    __atomic_store_n(&owner, (uintptr_t)11u, __ATOMIC_RELEASE);
    CHECK(__atomic_load_n(&owner, __ATOMIC_ACQUIRE) == (uintptr_t)11u);
    CHECK(__atomic_compare_exchange_n(&owner, &expected_owner,
                                      (uintptr_t)13u, 0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED));
    CHECK(owner == (uintptr_t)13u);

    CHECK(__atomic_load_n(&pointer, __ATOMIC_ACQUIRE) == &value32);
    __atomic_store_n(&pointer, &value64, __ATOMIC_RELEASE);
    CHECK(pointer == &value64);
    CHECK(!__atomic_compare_exchange_n(&pointer, &expected_pointer, &value32,
                                       0, __ATOMIC_ACQ_REL,
                                       __ATOMIC_RELAXED));
    CHECK(expected_pointer == &value64);
    expected_pointer = &value64;
    CHECK(__atomic_compare_exchange_n(&pointer, &expected_pointer, &value32,
                                      0, __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED));
    CHECK(pointer == &value32);
    return 0;
}
