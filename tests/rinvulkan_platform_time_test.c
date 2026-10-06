/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "../src/platform/time.h"

int main(void) {
    uint64_t first = 0u;
    uint64_t second = 0u;

    assert(!rinvulkan_platform_monotonic_time_ns(NULL));
    assert(rinvulkan_platform_monotonic_time_ns(&first));
    rinvulkan_platform_yield_thread();
    assert(rinvulkan_platform_monotonic_time_ns(&second));
    assert(first <= second);
    return 0;
}
