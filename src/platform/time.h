/* SPDX-License-Identifier: MIT */

#ifndef RIN_VULKAN_PLATFORM_TIME_H
#define RIN_VULKAN_PLATFORM_TIME_H

#include <stdint.h>

int rinvulkan_platform_monotonic_time_ns(uint64_t* value_out);
void rinvulkan_platform_yield_thread(void);

#endif /* RIN_VULKAN_PLATFORM_TIME_H */
