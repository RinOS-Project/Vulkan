/* SPDX-License-Identifier: MIT */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "time.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

int rinvulkan_platform_monotonic_time_ns(uint64_t* value_out) {
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    uint64_t ticks;
    uint64_t frequency_hz;
    uint64_t seconds;
    uint64_t remainder;
    uint64_t fraction_ns;

    if (!value_out || !QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter) || frequency.QuadPart <= 0 ||
        counter.QuadPart < 0)
        return 0;
    ticks = (uint64_t)counter.QuadPart;
    frequency_hz = (uint64_t)frequency.QuadPart;
    seconds = ticks / frequency_hz;
    remainder = ticks % frequency_hz;
    if (seconds > UINT64_MAX / UINT64_C(1000000000)) return 0;
    if (remainder > UINT64_MAX / UINT64_C(1000000000))
        fraction_ns = (uint64_t)(((long double)remainder * 1000000000.0L) /
                                 (long double)frequency_hz);
    else
        fraction_ns = remainder * UINT64_C(1000000000) / frequency_hz;
    if (fraction_ns > UINT64_MAX - seconds * UINT64_C(1000000000))
        return 0;
    *value_out = seconds * UINT64_C(1000000000) + fraction_ns;
    return 1;
#else
    struct timespec now;
    uint64_t seconds;

    if (!value_out || clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
        now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
        return 0;
    seconds = (uint64_t)now.tv_sec;
    if (seconds > (UINT64_MAX - (uint64_t)now.tv_nsec) /
                      UINT64_C(1000000000))
        return 0;
    *value_out = seconds * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
    return 1;
#endif
}

void rinvulkan_platform_yield_thread(void) {
#if defined(_WIN32)
    (void)SwitchToThread();
#else
    (void)sched_yield();
#endif
}
