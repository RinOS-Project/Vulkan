/* SPDX-License-Identifier: MIT */
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#include <rinvulkan/icd.h>
#include <rinvulkan/software_platform.h>
#include <rin/contract_abi.h>
#include <rinruntime/window.h>

#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST)
#include "compositor_native_window_vulkan_bridge.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <rin/ipc/shm_abi.h>

/* Compile the actual RinRuntime callback provider into this POSIX-only
 * integration variant. The ordinary Windows test below keeps its small
 * callback fixture. */
#include "../../rinruntime/src/compositor_gui.c"
#endif

typedef struct TestCompositorSurface {
    uint32_t width;
    uint32_t height;
    uint64_t generation;
    uint32_t busy_imports;
    uint32_t import_attempts;
    uint32_t accepted_imports;
    uint32_t expected_word;
} TestCompositorSurface;

typedef struct TestMemoryMapping {
    uint64_t allocation;
    uint64_t lease;
    uint32_t active;
} TestMemoryMapping;

typedef struct TestResourceBinding {
    uint64_t resource_cookie;
    uint64_t allocation_handle;
    uint64_t allocation_offset;
    uint64_t size_bytes;
    uint32_t required_gpu_access;
    uint32_t active;
} TestResourceBinding;

typedef struct TestProductAdapters {
    RinGpuVulkanSoftwarePlatformV1* software;
    RinVulkanProductPlatformV2 v2;
    RinVulkanProductPlatformV3 v3;
    RinVulkanProductPlatformV4 v4;
    RinVulkanProductPlatformV5 v5;
    uint64_t next_mapping_lease;
    TestMemoryMapping mappings[RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS];
    TestResourceBinding resources[RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS];
    uint64_t fail_unbind_cookie;
    uint32_t fail_unbind_attempts;
    uint32_t resource_bind_calls;
    uint32_t resource_unbind_calls;
    uint32_t active_resource_count;
} TestProductAdapters;

#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
typedef struct RuntimeExportPeer {
    int fd;
    uint64_t acquire_fences[2];
    int result;
} RuntimeExportPeer;

typedef struct RuntimeTestMapping {
    int fd;
    void* address;
    size_t bytes;
} RuntimeTestMapping;

static RuntimeTestMapping runtime_test_mappings[8];
static char runtime_test_unlink_names[8][RIN_SHM_NAME_MAX + 2u];
static size_t runtime_test_unlink_count;
static int runtime_test_unlink_cleanup_registered;

static void runtime_test_unlink_cleanup(void) {
    size_t index;
    for (index = 0u; index < runtime_test_unlink_count; ++index)
        (void)shm_unlink(runtime_test_unlink_names[index]);
}

/* POSIX host implementations for the actual Runtime platform boundary. */
uint64_t rin_monotonic_ms(void) {
    struct timespec now;
    uint64_t seconds;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
        return UINT64_MAX;
    seconds = (uint64_t)now.tv_sec;
    if (seconds > (UINT64_MAX - (uint64_t)now.tv_nsec / UINT64_C(1000000)) /
                      UINT64_C(1000))
        return UINT64_MAX;
    return seconds * UINT64_C(1000) +
           (uint64_t)now.tv_nsec / UINT64_C(1000000);
}

void rin_sleep(unsigned int milliseconds) {
    struct timespec remaining;
    remaining.tv_sec = (time_t)(milliseconds / 1000u);
    remaining.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {}
}

int rin_shm_get(const char* name, uint32_t size, uint32_t flags) {
    char posix_name[RIN_SHM_NAME_MAX + 2u];
    size_t length;
    int descriptor;
    int created = 0;
    if (!name || name[0] == '\0' || size == 0u ||
        (flags & ~(RIN_SHM_FLAG_CREAT | RIN_SHM_FLAG_EXCL |
                   RIN_SHM_FLAG_UNLINK_ON_CLOSE)) != 0u) {
        errno = EINVAL;
        return -1;
    }
    length = strnlen(name, RIN_SHM_NAME_MAX);
    if (length == 0u || length >= RIN_SHM_NAME_MAX ||
        memchr(name, '/', length) != NULL ||
        ((flags & RIN_SHM_FLAG_EXCL) != 0u &&
         (flags & RIN_SHM_FLAG_CREAT) == 0u)) {
        errno = EINVAL;
        return -1;
    }
    posix_name[0] = '/';
    memcpy(posix_name + 1u, name, length + 1u);
    if ((flags & RIN_SHM_FLAG_CREAT) == 0u) {
        descriptor = shm_open(posix_name, O_RDWR, 0u);
    } else if ((flags & RIN_SHM_FLAG_EXCL) != 0u) {
        descriptor = shm_open(posix_name, O_RDWR | O_CREAT | O_EXCL, 0600u);
        created = descriptor >= 0;
    } else {
        descriptor = shm_open(posix_name, O_RDWR | O_CREAT | O_EXCL, 0600u);
        if (descriptor >= 0) {
            created = 1;
        } else if (errno == EEXIST) {
            descriptor = shm_open(posix_name, O_RDWR, 0u);
        }
    }
    if (descriptor < 0) return -1;
    if (created && ftruncate(descriptor, (off_t)size) != 0) {
        int saved_errno = errno;
        (void)close(descriptor);
        (void)shm_unlink(posix_name);
        errno = saved_errno;
        return -1;
    }
    if ((flags & RIN_SHM_FLAG_UNLINK_ON_CLOSE) != 0u) {
        if (runtime_test_unlink_count >=
                sizeof(runtime_test_unlink_names) /
                    sizeof(runtime_test_unlink_names[0]) ||
            (!runtime_test_unlink_cleanup_registered &&
             atexit(runtime_test_unlink_cleanup) != 0)) {
            (void)close(descriptor);
            if (created) (void)shm_unlink(posix_name);
            errno = ENOSPC;
            return -1;
        }
        runtime_test_unlink_cleanup_registered = 1;
        memcpy(runtime_test_unlink_names[runtime_test_unlink_count],
               posix_name, length + 2u);
        ++runtime_test_unlink_count;
    }
    return descriptor;
}

void* rin_shm_at(int fd, void* address_hint, uint32_t prot) {
    struct stat status;
    int native_prot = 0;
    size_t index;
    void* address;
    if (fd < 0 || (prot & ~(RIN_SHM_PROT_READ | RIN_SHM_PROT_WRITE)) != 0u ||
        prot == 0u) {
        errno = EINVAL;
        return NULL;
    }
    if (fstat(fd, &status) != 0) return NULL;
    if (status.st_size <= 0 || (uintmax_t)status.st_size > SIZE_MAX) {
        errno = EINVAL;
        return NULL;
    }
    if ((prot & RIN_SHM_PROT_READ) != 0u) native_prot |= PROT_READ;
    if ((prot & RIN_SHM_PROT_WRITE) != 0u) native_prot |= PROT_WRITE;
    address = mmap(address_hint, (size_t)status.st_size, native_prot,
                   MAP_SHARED, fd, 0);
    if (address == MAP_FAILED) return NULL;
    if (address == NULL) {
        (void)munmap(address, (size_t)status.st_size);
        errno = ENOMEM;
        return NULL;
    }
    for (index = 0u; index < sizeof(runtime_test_mappings) /
                                sizeof(runtime_test_mappings[0]); ++index) {
        RuntimeTestMapping* mapping = &runtime_test_mappings[index];
        if (mapping->address != NULL) continue;
        mapping->fd = fd;
        mapping->address = address;
        mapping->bytes = (size_t)status.st_size;
        return address;
    }
    (void)munmap(address, (size_t)status.st_size);
    errno = ENOSPC;
    return NULL;
}

int rin_shm_dt(int fd, void* address) {
    size_t index;
    if (fd < 0 || !address) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0u; index < sizeof(runtime_test_mappings) /
                                sizeof(runtime_test_mappings[0]); ++index) {
        RuntimeTestMapping* mapping = &runtime_test_mappings[index];
        if (mapping->fd != fd || mapping->address != address) continue;
        if (munmap(mapping->address, mapping->bytes) != 0) return -1;
        memset(mapping, 0, sizeof(*mapping));
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static int runtime_test_read_exact(int fd, void* destination, size_t size) {
    uint8_t* bytes = (uint8_t*)destination;
    size_t offset = 0u;
    while (offset < size) {
        ssize_t count = recv(fd, bytes + offset, size - offset, 0);
        if (count > 0) {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static int runtime_test_write_exact(int fd, const void* source, size_t size) {
    const uint8_t* bytes = (const uint8_t*)source;
    size_t offset = 0u;
    while (offset < size) {
        ssize_t count = send(fd, bytes + offset, size - offset, MSG_NOSIGNAL);
        if (count > 0) {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static void* runtime_test_export_peer(void* opaque) {
    RuntimeExportPeer* peer = (RuntimeExportPeer*)opaque;
    uint32_t reply_index;
    if (!peer || peer->fd < 0) return NULL;
    for (reply_index = 0u; reply_index < 2u; ++reply_index) {
        RinCompositorHeader request_header;
        RinCompositorGpuExportImageV1 request;
        RinCompositorGpuImageV1 image;
        RinCompositorHeader reply_header;
        memset(&request_header, 0, sizeof(request_header));
        memset(&request, 0, sizeof(request));
        memset(&image, 0, sizeof(image));
        memset(&reply_header, 0, sizeof(reply_header));
        if (runtime_test_read_exact(peer->fd, &request_header,
                                    sizeof(request_header)) != 0 ||
            request_header.magic != RIN_COMPOSITOR_MAGIC ||
            request_header.version != RIN_COMPOSITOR_PROTOCOL_VERSION ||
            request_header.type != RIN_COMPOSITOR_EXPORT_GPU_IMAGE ||
            request_header.payload_size != sizeof(request) ||
            request_header.request_id == 0u ||
            request_header.reserved != 0u ||
            runtime_test_read_exact(peer->fd, &request, sizeof(request)) != 0 ||
            request.struct_size != sizeof(request) ||
            request.version != RIN_COMPOSITOR_GPU_SURFACE_ABI_VERSION ||
            request.flags != 0u || request.reserved != 0u ||
            request.surface_id != 7u || request.buffer_slot != 0u ||
            request.expected_surface_generation != 9u) {
            peer->result = -1;
            return NULL;
        }

        image.struct_size = sizeof(image);
        image.version = RIN_COMPOSITOR_GPU_SURFACE_ABI_VERSION;
        image.surface_id = request.surface_id;
        image.buffer_slot = request.buffer_slot;
        image.surface_generation = request.expected_surface_generation;
        image.image_handle = UINT64_C(0x700010001);
        image.acquire_fence = peer->acquire_fences[reply_index];
        image.width = 4u;
        image.height = 4u;
        image.pitch = 16u;
        image.format = RIN_RUNTIME_COMPOSITOR_GPU_PIXEL_BGRA8;
        image.bytes = 256u;

        reply_header.magic = RIN_COMPOSITOR_MAGIC;
        reply_header.version = request_header.version;
        reply_header.type = request_header.type;
        reply_header.payload_size = sizeof(image);
        reply_header.request_id = request_header.request_id;
        if (runtime_test_write_exact(peer->fd, &reply_header,
                                     sizeof(reply_header)) != 0 ||
            runtime_test_write_exact(peer->fd, &image, sizeof(image)) != 0) {
            peer->result = -1;
            return NULL;
        }
    }
    peer->result = 0;
    return NULL;
}
#endif

#if !defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
static int test_surface_query(
        void* context, RinRuntimeGuiHandle handle,
        RinRuntimeCompositorGpuSurfaceV1* surface_out) {
    TestCompositorSurface* surface = (TestCompositorSurface*)context;
    if (!surface || handle != UINT64_C(0x7711) || !surface_out)
        return RIN_ERROR_INVALID_ARGUMENT;
    memset(surface_out, 0, sizeof(*surface_out));
    surface_out->struct_size = sizeof(*surface_out);
    surface_out->version = RIN_RUNTIME_COMPOSITOR_GPU_SURFACE_V1_VERSION;
    surface_out->width = surface->width;
    surface_out->height = surface->height;
    surface_out->supported_frame_formats =
        RIN_RUNTIME_COMPOSITOR_GPU_SURFACE_FORMAT_RGBA8_BIT;
    surface_out->surface_generation = surface->generation;
    return RIN_RESULT_OK;
}

static int test_surface_import(
        void* context, RinRuntimeGuiHandle handle,
        uint64_t surface_generation,
        const RinRuntimeCompositorGpuFrameV1* frame) {
    TestCompositorSurface* surface = (TestCompositorSurface*)context;
    uint8_t expected_pixel[sizeof(surface->expected_word)];
    const uint8_t* pixels;
    uint64_t pixel_count;
    uint64_t pixel_index;
    if (!surface || handle != UINT64_C(0x7711) || !frame ||
        surface_generation != surface->generation ||
        frame->struct_size != sizeof(*frame) ||
        frame->version != RIN_RUNTIME_COMPOSITOR_GPU_FRAME_V1_VERSION ||
        frame->width != surface->width || frame->height != surface->height ||
        frame->row_pitch != frame->width * 4u ||
        frame->bytes != (uint64_t)frame->row_pitch * frame->height ||
        frame->format != RIN_RUNTIME_COMPOSITOR_GPU_PIXEL_RGBA8 ||
        !frame->pixels)
        return RIN_ERROR_INVALID_ARGUMENT;
    ++surface->import_attempts;
    if (surface->busy_imports != 0u) {
        --surface->busy_imports;
        return RIN_ERROR_BUSY;
    }
    memcpy(expected_pixel, &surface->expected_word, sizeof(expected_pixel));
    pixels = (const uint8_t*)frame->pixels;
    pixel_count = (uint64_t)frame->width * frame->height;
    for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
        if (memcmp(pixels + pixel_index * sizeof(expected_pixel),
                   expected_pixel, sizeof(expected_pixel)) != 0)
            return RIN_ERROR_INVALID_ARGUMENT;
    }
    ++surface->accepted_imports;
    return RIN_RESULT_OK;
}
#endif

static int test_prepare_submission_v2(
        void* context, uint32_t queue_id, uint64_t command_cookie,
        uint32_t wait_count, const RinVulkanProductSubmissionWaitV1* waits,
        RinVulkanProductSubmissionV2* submission_out) {
    RinGpuVulkanSoftwarePlatformV1* software =
        (RinGpuVulkanSoftwarePlatformV1*)context;
    int result;
    if (!software || !submission_out)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (wait_count != 0u) return RIN_VULKAN_PRODUCT_UNSUPPORTED;
    if (waits && memcmp(waits,
                        (RinVulkanProductSubmissionWaitV1[1]){{0}},
                        sizeof(*waits)) != 0)
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    memset(submission_out, 0, sizeof(*submission_out));
    submission_out->struct_size = sizeof(*submission_out);
    submission_out->version = RIN_VULKAN_PRODUCT_SUBMISSION_V2_VERSION;
    result = software->platform.prepare_submission(
        software->platform.context, queue_id, command_cookie,
        &submission_out->base);
    return result;
}

static int test_submit_v2(
        void* context, const RinVulkanProductSubmissionV2* submission,
        const RinVulkanProductResourceV1* resources,
        uint32_t resource_count) {
    RinGpuVulkanSoftwarePlatformV1* software =
        (RinGpuVulkanSoftwarePlatformV1*)context;
    if (!software || !submission ||
        submission->struct_size != sizeof(*submission) ||
        submission->version != RIN_VULKAN_PRODUCT_SUBMISSION_V2_VERSION ||
        submission->wait_count != 0u || submission->reserved0 != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    return software->platform.submit(software->platform.context,
                                     &submission->base, resources,
                                     resource_count);
}

static int test_prepare_submission_v3(
        void* context, uint32_t queue_id, uint64_t command_cookie,
        uint32_t wait_count, const RinVulkanProductSubmissionWaitV2* waits,
        RinVulkanProductSubmissionV3* submission_out) {
    RinGpuVulkanSoftwarePlatformV1* software =
        (RinGpuVulkanSoftwarePlatformV1*)context;
    int result;
    if (!software || !submission_out)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (wait_count != 0u) return RIN_VULKAN_PRODUCT_UNSUPPORTED;
    if (waits && memcmp(waits,
                        (RinVulkanProductSubmissionWaitV2[1]){{0}},
                        sizeof(*waits)) != 0)
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    memset(submission_out, 0, sizeof(*submission_out));
    submission_out->struct_size = sizeof(*submission_out);
    submission_out->version = RIN_VULKAN_PRODUCT_SUBMISSION_V3_VERSION;
    result = software->platform.prepare_submission(
        software->platform.context, queue_id, command_cookie,
        &submission_out->base);
    return result;
}

static int test_submit_v3(
        void* context, const RinVulkanProductSubmissionV3* submission,
        const RinVulkanProductResourceV1* resources,
        uint32_t resource_count) {
    RinGpuVulkanSoftwarePlatformV1* software =
        (RinGpuVulkanSoftwarePlatformV1*)context;
    if (!software || !submission ||
        submission->struct_size != sizeof(*submission) ||
        submission->version != RIN_VULKAN_PRODUCT_SUBMISSION_V3_VERSION ||
        submission->wait_count != 0u || submission->reserved0 != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    return software->platform.submit(software->platform.context,
                                     &submission->base, resources,
                                     resource_count);
}

static RinGpuVulkanSoftwareAllocationV1* test_find_allocation(
        RinGpuVulkanSoftwarePlatformV1* software, uint64_t handle) {
    uint32_t index;
    if (!software || handle == 0u) return NULL;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index)
        if (software->allocations[index].handle == handle)
            return &software->allocations[index];
    return NULL;
}

static int test_map_memory_v4(
        void* context, uint64_t allocation_handle,
        uint32_t required_cpu_access, uint64_t* process_address_out,
        uint64_t* allocation_size_out, uint64_t* mapping_lease_out) {
    TestProductAdapters* adapters = (TestProductAdapters*)context;
    RinGpuVulkanSoftwareAllocationV1* allocation;
    uint32_t index;
    if (process_address_out) *process_address_out = 0u;
    if (allocation_size_out) *allocation_size_out = 0u;
    if (mapping_lease_out) *mapping_lease_out = 0u;
    if (!adapters || !process_address_out || !allocation_size_out ||
        !mapping_lease_out || required_cpu_access == 0u ||
        (required_cpu_access & ~RIN_VULKAN_PRODUCT_CPU_ACCESS_KNOWN) != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    allocation = test_find_allocation(adapters->software, allocation_handle);
    if (!allocation || !allocation->bytes || allocation->size_bytes == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index)
        if (adapters->mappings[index].active != 0u &&
            adapters->mappings[index].allocation == allocation_handle)
            return RIN_VULKAN_PRODUCT_BUSY;
    if (adapters->next_mapping_lease == 0u ||
        adapters->next_mapping_lease == UINT64_MAX)
        return RIN_VULKAN_PRODUCT_LIMIT;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        TestMemoryMapping* mapping = &adapters->mappings[index];
        if (mapping->active != 0u) continue;
        mapping->allocation = allocation_handle;
        mapping->lease = adapters->next_mapping_lease++;
        mapping->active = 1u;
        *process_address_out = (uint64_t)(uintptr_t)allocation->bytes;
        *allocation_size_out = allocation->size_bytes;
        *mapping_lease_out = mapping->lease;
        return RIN_VULKAN_PRODUCT_OK;
    }
    return RIN_VULKAN_PRODUCT_LIMIT;
}

static int test_unmap_memory_v4(
        void* context, uint64_t allocation_handle, uint64_t mapping_lease) {
    TestProductAdapters* adapters = (TestProductAdapters*)context;
    uint32_t index;
    if (!adapters || allocation_handle == 0u || mapping_lease == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (!test_find_allocation(adapters->software, allocation_handle))
        return RIN_VULKAN_PRODUCT_STATE;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        TestMemoryMapping* mapping = &adapters->mappings[index];
        if (mapping->active != 0u &&
            mapping->allocation == allocation_handle &&
            mapping->lease == mapping_lease) {
            memset(mapping, 0, sizeof(*mapping));
            return RIN_VULKAN_PRODUCT_OK;
        }
    }
    return RIN_VULKAN_PRODUCT_STATE;
}

static int test_sync_memory_v4(
        void* context, uint64_t allocation_handle, uint32_t action,
        uint64_t offset, uint64_t length) {
    TestProductAdapters* adapters = (TestProductAdapters*)context;
    RinGpuVulkanSoftwareAllocationV1* allocation;
    if (!adapters || (action != RIN_VULKAN_PRODUCT_SYNC_CPU_TO_DEVICE &&
                      action != RIN_VULKAN_PRODUCT_SYNC_DEVICE_TO_CPU) ||
        length == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    allocation = test_find_allocation(adapters->software, allocation_handle);
    if (!allocation || offset > allocation->size_bytes ||
        length > allocation->size_bytes - offset)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    atomic_thread_fence(action == RIN_VULKAN_PRODUCT_SYNC_CPU_TO_DEVICE
                            ? memory_order_release
                            : memory_order_acquire);
    return RIN_VULKAN_PRODUCT_OK;
}

static int test_bind_resource_v5(
        void* context, uint64_t resource_cookie, uint64_t allocation_handle,
        uint64_t allocation_offset, uint64_t size_bytes,
        uint32_t required_gpu_access) {
    TestProductAdapters* adapters = (TestProductAdapters*)context;
    RinGpuVulkanSoftwareAllocationV1* allocation;
    uint32_t index;
    if (!adapters || resource_cookie == 0u || allocation_handle == 0u ||
        size_bytes == 0u || allocation_offset > UINT64_MAX - size_bytes ||
        required_gpu_access == 0u ||
        (required_gpu_access &
         ~(RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
           RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE)) != 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    allocation = test_find_allocation(adapters->software, allocation_handle);
    if (!allocation || allocation_offset > allocation->size_bytes ||
        size_bytes > allocation->size_bytes - allocation_offset)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        TestResourceBinding* binding = &adapters->resources[index];
        if (binding->active != 0u &&
            binding->resource_cookie == resource_cookie)
            return RIN_VULKAN_PRODUCT_BUSY;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        TestResourceBinding* binding = &adapters->resources[index];
        if (binding->active != 0u) continue;
        binding->resource_cookie = resource_cookie;
        binding->allocation_handle = allocation_handle;
        binding->allocation_offset = allocation_offset;
        binding->size_bytes = size_bytes;
        binding->required_gpu_access = required_gpu_access;
        binding->active = 1u;
        ++adapters->active_resource_count;
        ++adapters->resource_bind_calls;
        return RIN_VULKAN_PRODUCT_OK;
    }
    return RIN_VULKAN_PRODUCT_LIMIT;
}

static int test_unbind_resource_v5(void* context, uint64_t resource_cookie) {
    TestProductAdapters* adapters = (TestProductAdapters*)context;
    uint32_t index;
    if (!adapters || resource_cookie == 0u)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (adapters->fail_unbind_cookie == resource_cookie &&
        adapters->fail_unbind_attempts != 0u) {
        --adapters->fail_unbind_attempts;
        return RIN_VULKAN_PRODUCT_BUSY;
    }
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        TestResourceBinding* binding = &adapters->resources[index];
        if (binding->active == 0u ||
            binding->resource_cookie != resource_cookie)
            continue;
        if (adapters->active_resource_count == 0u)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        memset(binding, 0, sizeof(*binding));
        --adapters->active_resource_count;
        ++adapters->resource_unbind_calls;
        return RIN_VULKAN_PRODUCT_OK;
    }
    return RIN_VULKAN_PRODUCT_STATE;
}

static const TestResourceBinding* test_find_resource_binding(
        const TestProductAdapters* adapters, uint64_t resource_cookie) {
    uint32_t index;
    if (!adapters || resource_cookie == 0u) return NULL;
    for (index = 0u; index < RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS;
         ++index) {
        const TestResourceBinding* binding = &adapters->resources[index];
        if (binding->active != 0u &&
            binding->resource_cookie == resource_cookie)
            return binding;
    }
    return NULL;
}

static void initialize_product_adapters(
        TestProductAdapters* adapters,
        RinGpuVulkanSoftwarePlatformV1* software) {
    memset(adapters, 0, sizeof(*adapters));
    adapters->software = software;
    adapters->next_mapping_lease = 1u;
    adapters->v2.struct_size = sizeof(adapters->v2);
    adapters->v2.version = RIN_VULKAN_PRODUCT_PLATFORM_V2_VERSION;
    adapters->v2.base = &software->platform;
    adapters->v2.prepare_submission_v2 = test_prepare_submission_v2;
    adapters->v2.submit_v2 = test_submit_v2;
    adapters->v3.struct_size = sizeof(adapters->v3);
    adapters->v3.version = RIN_VULKAN_PRODUCT_PLATFORM_V3_VERSION;
    adapters->v3.base = &adapters->v2;
    adapters->v3.prepare_submission_v3 = test_prepare_submission_v3;
    adapters->v3.submit_v3 = test_submit_v3;
    adapters->v4.struct_size = sizeof(adapters->v4);
    adapters->v4.version = RIN_VULKAN_PRODUCT_PLATFORM_V4_VERSION;
    adapters->v4.base = &adapters->v3;
    adapters->v4.context = adapters;
    adapters->v4.map_memory = test_map_memory_v4;
    adapters->v4.unmap_memory = test_unmap_memory_v4;
    adapters->v4.sync_memory = test_sync_memory_v4;
    adapters->v5.struct_size = sizeof(adapters->v5);
    adapters->v5.version = RIN_VULKAN_PRODUCT_PLATFORM_V5_VERSION;
    adapters->v5.base = &adapters->v4;
    adapters->v5.context = adapters;
    adapters->v5.bind_resource = test_bind_resource_v5;
    adapters->v5.unbind_resource = test_unbind_resource_v5;
}

static void make_profile(RinGpuVulkanPhysicalDeviceV2* profile) {
    memset(profile, 0, sizeof(*profile));
    profile->struct_size = sizeof(*profile);
    profile->version = RIN_GPU_VULKAN_PHYSICAL_VERSION;
    profile->api_version = RIN_GPU_VK_MAKE_VERSION(1u, 3u, 0u);
    profile->flags = RIN_GPU_VK_PHYSICAL_DMA_ISOLATED |
                     RIN_GPU_VK_PHYSICAL_RESET_CAPABLE;
    profile->features = RIN_GPU_VK_FEATURE_KNOWN;
    profile->device_uuid[0] = 0x31u;
    profile->driver_digest[0] = 0x42u;
    profile->iommu_domain_cookie = UINT64_C(0x3001);
    profile->device_epoch = 7u;
    profile->queue_family_count = 1u;
    profile->queue_families[0].flags = RIN_GPU_VK_QUEUE_GRAPHICS |
                                       RIN_GPU_VK_QUEUE_COMPUTE |
                                       RIN_GPU_VK_QUEUE_TRANSFER;
    profile->queue_families[0].queue_count = 1u;
    profile->queue_families[0].timestamp_valid_bits = 64u;
    profile->memory_heap_count = 2u;
    profile->memory_heaps[0].size_bytes = UINT64_C(512) * 1024u * 1024u;
    profile->memory_heaps[0].flags = RIN_GPU_VK_HEAP_DEVICE_LOCAL;
    profile->memory_heaps[1].size_bytes = UINT64_C(128) * 1024u * 1024u;
    profile->memory_type_count = 2u;
    profile->memory_types[0].heap_index = 0u;
    profile->memory_types[0].property_flags =
        RIN_GPU_VK_MEMORY_DEVICE_LOCAL;
    profile->memory_types[1].heap_index = 1u;
    profile->memory_types[1].property_flags =
        RIN_GPU_VK_MEMORY_HOST_VISIBLE | RIN_GPU_VK_MEMORY_HOST_COHERENT;
    profile->max_sampler_anisotropy = 1.0f;
    profile->max_image_dimension_2d = 4096u;
    profile->max_bound_descriptor_sets = 4u;
    profile->max_per_stage_resources = 128u;
    profile->max_push_constants_size = 128u;
    profile->max_memory_allocation_count = 4096u;
    profile->timestamp_period_ns_x1000 = 1000u;
    profile->max_buffer_size = UINT64_C(1) << 30u;
    profile->driver_version = RIN_GPU_VK_MAKE_VERSION(1u, 0u, 0u);
    profile->vendor_id = 0x1af4u;
    profile->device_id = 0x1050u;
    profile->device_type = RIN_GPU_VK_PHYSICAL_TYPE_VIRTUAL_GPU;
    memcpy(profile->device_name, "RinGPU native test",
           sizeof("RinGPU native test"));
    profile->pipeline_cache_uuid[0] = 0x53u;
}

static RinVkResult record_present_layout_and_clear(
        RinVkDevice device, RinVkQueue queue, RinVkImage image,
        RinVkSemaphore acquire_semaphore,
        RinVkSemaphore render_complete_semaphore,
        uint32_t clear_word) {
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = 0u;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = 0u;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkImageSubresourceRange range;
    RinVkImageMemoryBarrier2 barrier;
    RinVkDependencyInfo dependency;
    RinVkClearColorValue clear;
    RinVkSubmitInfo submit;
    RinVkFenceCreateInfo fence_info;
    RinVkFence fence = 0u;
    const RinVkCommandBuffer* command_buffers = &command_buffer;
    const uint64_t* wait_semaphores = &acquire_semaphore;
    const uint64_t* signal_semaphores = &render_complete_semaphore;
    uint32_t wait_stage = RIN_VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    RinVkResult result;

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = 0u;
    result = vkCreateCommandPool(device, &pool_info, NULL, &pool);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    result = vkAllocateCommandBuffers(device, &allocate_info, &command_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != RIN_VK_SUCCESS) goto done;

    memset(&range, 0, sizeof(range));
    range.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1u;
    range.layerCount = 1u;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = image;
    barrier.subresourceRange = range;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1u;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    memset(&clear, 0, sizeof(clear));
    clear.uint32[0] = clear_word;
    vkCmdClearColorImage(command_buffer, image,
                         RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear, 1u, &range);

    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier2(command_buffer, &dependency);

    /* UNDEFINED is a valid discard source even when the tracked current
     * layout is PRESENT. Reinitialize the image before importing its pixels. */
    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = 0u;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    memset(&clear, 0, sizeof(clear));
    clear.uint32[0] = clear_word;
    vkCmdClearColorImage(command_buffer, image,
                         RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear, 1u, &range);
    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    result = vkEndCommandBuffer(command_buffer);
    if (result != RIN_VK_SUCCESS) goto done;

    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = vkCreateFence(device, &fence_info, NULL, &fence);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1u;
    submit.pWaitSemaphores = wait_semaphores;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = command_buffers;
    submit.signalSemaphoreCount = 1u;
    submit.pSignalSemaphores = signal_semaphores;
    result = vkQueueSubmit(queue, 1u, &submit, fence);
    if (result == RIN_VK_SUCCESS)
        result = vkWaitForFences(device, 1u, &fence, 1u, UINT64_MAX);

done:
    if (fence != 0u) vkDestroyFence(device, fence, NULL);
    if (pool != (RinVkCommandPool)0)
        vkDestroyCommandPool(device, pool, NULL);
    return result;
}

int main(void) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanRuntimeV1 runtime;
    RinGpuVulkanSoftwarePlatformV1 software;
    TestProductAdapters product_adapters;
    RinRuntimeCompositorGpuSurfaceOpsV1 surface_ops;
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    RinRuntimeGuiSurface* runtime_surface = NULL;
    RuntimeExportPeer runtime_peer;
    RinRuntimeGuiAsyncJob* runtime_job = NULL;
    RinCompositorDamage runtime_damage;
    RinCompositorCommitV2 runtime_commit;
    pthread_t runtime_peer_thread;
    uint8_t runtime_pixels[512];
    uint8_t runtime_expected_rgba[4];
    uint32_t runtime_pixel_index;
    int runtime_sockets[2] = {-1, -1};
    int runtime_peer_started = 0;
    int runtime_peer_joined = 0;
#endif
    RinVkApplicationInfo application;
    RinVkInstanceCreateInfo instance_info;
    const char* instance_extensions[] = {
        RIN_VK_KHR_SURFACE_EXTENSION,
        RIN_VK_RINOS_NATIVE_WINDOW_SURFACE_EXTENSION
    };
    const char* device_extensions[] = {
        RIN_VK_KHR_SYNCHRONIZATION_2_EXTENSION,
        RIN_VK_KHR_SWAPCHAIN_EXTENSION
    };
    RinVkInstance instance = NULL;
    RinVkPhysicalDevice physical_devices[1];
    RinVkPhysicalDevice physical = NULL;
    RinVkExtensionProperties extension_properties[4];
    uint32_t extension_count = 4u;
    uint32_t device_extension_count = 0u;
    RinVkRinOSNativeWindowSurfaceCreateInfoV1 surface_info;
    RinVkSurfaceKHR surface = 0u;
    TestCompositorSurface compositor = {4u, 4u, 1u, 1u, 0u, 0u,
                                         UINT32_C(0x6a3c17e5)};
    RinVkSurfaceCapabilitiesKHR capabilities;
    RinVkSurfaceFormatKHR surface_format;
    RinVkDeviceQueueCreateInfo queue_info;
    RinVkPhysicalDeviceSynchronization2Features synchronization2;
    RinVkDeviceCreateInfo device_info;
    RinVkDevice device = NULL;
    RinVkQueue queue = NULL;
    const float queue_priority = 1.0f;
    RinVkSwapchainCreateInfoKHR swapchain_info;
    RinVkSwapchainKHR swapchain = 0u;
    RinVkImage images[2];
    uint32_t image_count = 2u;
    RinVkSemaphoreCreateInfo semaphore_info;
    RinVkSemaphore acquire_semaphore = 0u;
    RinVkSemaphore render_complete_semaphore = 0u;
    uint32_t image_index = UINT32_MAX;
    RinVkPresentInfoKHR present;
    RinVkResult present_result = RIN_VK_ERROR_INITIALIZATION_FAILED;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;
    uint32_t instance_initialized = 0u;
    uint32_t runtime_bound = 0u;
    uint32_t software_initialized = 0u;
    uint32_t product_bound = 0u;
    uint32_t product_v2_bound = 0u;
    uint32_t product_v3_bound = 0u;
    uint32_t product_v4_bound = 0u;
    uint32_t product_v5_bound = 0u;
    uint32_t surface_created = 0u;
    uint32_t device_created = 0u;
    uint32_t swapchain_created = 0u;
    int runtime_init_result;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

    make_profile(&profile);
    runtime_init_result = rin_gpu_vulkan_runtime_init(
        &runtime, &profile, 1u, UINT64_C(0x123456789abcdef0));
    if (runtime_init_result != RIN_GPU_VULKAN_OK)
        fprintf(stderr, "Vulkan runtime init returned %d\n",
                runtime_init_result);
    CHECK(runtime_init_result == RIN_GPU_VULKAN_OK);
    instance_initialized = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_runtime(&runtime) == RIN_GPU_VULKAN_OK);
    runtime_bound = 1u;
    CHECK(rin_gpu_vulkan_software_platform_init(
              &software, profile.iommu_domain_cookie, profile.device_epoch,
              profile.queue_families[0].queue_count,
              UINT64_C(16) * 1024u * 1024u) == RIN_VULKAN_PRODUCT_OK);
    software_initialized = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform(&software.platform) ==
          RIN_GPU_VULKAN_OK);
    product_bound = 1u;
    initialize_product_adapters(&product_adapters, &software);
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v2(
              &product_adapters.v2) == RIN_GPU_VULKAN_OK);
    product_v2_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v3(
              &product_adapters.v3) == RIN_GPU_VULKAN_OK);
    product_v3_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v4(
              &product_adapters.v4) == RIN_GPU_VULKAN_OK);
    product_v4_bound = 1u;

    extension_count = 4u;
    CHECK(vkEnumerateInstanceExtensionProperties(
              NULL, &extension_count, extension_properties) ==
          RIN_VK_SUCCESS);
    CHECK(extension_count == 3u &&
          strcmp(extension_properties[2].extensionName,
                 RIN_VK_RINOS_NATIVE_WINDOW_SURFACE_EXTENSION) == 0);
    memset(&application, 0, sizeof(application));
    application.sType = RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "RinVulkan native-window test";
    application.apiVersion = RIN_GPU_VK_ICD_API_VERSION;
    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application;
    instance_info.enabledExtensionCount = 2u;
    instance_info.ppEnabledExtensionNames = instance_extensions;
    result = vkCreateInstance(&instance_info, NULL, &instance);
    if (result != RIN_VK_SUCCESS)
        fprintf(stderr, "native-window instance creation returned %d\n",
                result);
    CHECK(result == RIN_VK_SUCCESS);
    CHECK(vkGetInstanceProcAddr(instance,
                               "vkGetPhysicalDeviceDisplayPropertiesKHR") ==
          NULL);
    CHECK(vkGetInstanceProcAddr(instance,
                               "vkCreateRinOSNativeWindowSurfaceV1") != NULL);
    extension_count = 1u;
    CHECK(vkEnumeratePhysicalDevices(instance, &extension_count,
                                     physical_devices) == RIN_VK_SUCCESS);
    CHECK(extension_count == 1u);
    physical = physical_devices[0];

    CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v4(
              &product_adapters.v4) == RIN_GPU_VULKAN_OK);
    product_v4_bound = 0u;
    CHECK(vkEnumerateDeviceExtensionProperties(
              physical, NULL, &device_extension_count, NULL) ==
          RIN_VK_SUCCESS);
    CHECK(device_extension_count == 3u);
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v4(
              &product_adapters.v4) == RIN_GPU_VULKAN_OK);
    product_v4_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v5(
              &product_adapters.v5) == RIN_GPU_VULKAN_OK);
    product_v5_bound = 1u;

#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    memset(g_surfaces, 0, sizeof(g_surfaces));
    memset(&g_async_requests, 0, sizeof(g_async_requests));
    memset(runtime_pixels, 0, sizeof(runtime_pixels));
    runtime_surface = &g_surfaces[0];
    runtime_surface->active = 1u;
    runtime_surface->handle_token = 33u;
    runtime_surface->handle_generation = 1u;
    runtime_surface->id = 7u;
    runtime_surface->width = 4u;
    runtime_surface->height = 4u;
    runtime_surface->pitch = 16u;
    runtime_surface->bytes = 256u;
    runtime_surface->draw_slot = 0u;
    runtime_surface->frame_sequence = 12u;
    runtime_surface->slot_submitted_sequence[0] = 5u;
    runtime_surface->render_target_generation = 1u;
    runtime_surface->compositor_surface_generation = 9u;
    runtime_surface->shm_handles[0] = -1;
    runtime_surface->shm_handles[1] = -1;
    runtime_surface->pixels[0] = runtime_pixels;
    runtime_surface->pixels[1] = runtime_pixels + 256u;
    g_compositor_features = RIN_COMPOSITOR_FEATURE_GPU_SURFACE_ABI;
    CHECK(wnd_get_gpu_surface_ops_v1(&surface_ops) == RIN_RESULT_OK);
#else
    memset(&surface_ops, 0, sizeof(surface_ops));
    surface_ops.struct_size = sizeof(surface_ops);
    surface_ops.version = RIN_RUNTIME_COMPOSITOR_GPU_SURFACE_OPS_V1_VERSION;
    surface_ops.context = &compositor;
    surface_ops.query = test_surface_query;
    surface_ops.import_frame = test_surface_import;
#endif
    memset(&surface_info, 0, sizeof(surface_info));
    surface_info.struct_size = sizeof(surface_info);
    surface_info.version = RIN_VK_RINOS_NATIVE_WINDOW_SURFACE_SPEC_VERSION;
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    surface_info.window_handle = 33u;
#else
    surface_info.window_handle = UINT64_C(0x7711);
#endif
    surface_info.surface_ops = &surface_ops;
    CHECK(vkCreateRinOSNativeWindowSurfaceV1(
              instance, &surface_info, NULL, &surface) == RIN_VK_SUCCESS);
    surface_created = 1u;
    CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
              physical, 0u, surface, &extension_count) == RIN_VK_SUCCESS);
    CHECK(extension_count == 1u);
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
              physical, surface, &capabilities) == RIN_VK_SUCCESS);
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    CHECK(capabilities.currentExtent.width == 4u &&
          capabilities.currentExtent.height == 4u &&
#else
    CHECK(capabilities.currentExtent.width == compositor.width &&
          capabilities.currentExtent.height == compositor.height &&
#endif
          (capabilities.supportedUsageFlags &
           RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u &&
          (capabilities.supportedUsageFlags &
           RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u);
    extension_count = 1u;
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
              physical, surface, &extension_count, &surface_format) ==
          RIN_VK_SUCCESS);
    CHECK(extension_count == 1u &&
          surface_format.format == RIN_VK_FORMAT_R8G8B8A8_UNORM);

    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = 0u;
    queue_info.queueCount = 1u;
    queue_info.pQueuePriorities = &queue_priority;
    memset(&synchronization2, 0, sizeof(synchronization2));
    synchronization2.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2.synchronization2 = 1u;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &synchronization2;
    device_info.queueCreateInfoCount = 1u;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 2u;
    device_info.ppEnabledExtensionNames = device_extensions;
    CHECK(vkCreateDevice(physical, &device_info, NULL, &device) ==
          RIN_VK_SUCCESS);
    device_created = 1u;
    vkGetDeviceQueue(device, 0u, 0u, &queue);
    CHECK(queue != NULL);

    {
        RinVkSemaphore empty_scope_semaphore = 0u;
        RinVkSemaphoreSubmitInfo semaphore_submit_info;
        RinVkSubmitInfo2 empty_submit;

        memset(&semaphore_info, 0, sizeof(semaphore_info));
        semaphore_info.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        CHECK(vkCreateSemaphore(device, &semaphore_info, NULL,
                                &empty_scope_semaphore) == RIN_VK_SUCCESS);
        memset(&semaphore_submit_info, 0, sizeof(semaphore_submit_info));
        semaphore_submit_info.sType =
            RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphore_submit_info.semaphore = empty_scope_semaphore;
        semaphore_submit_info.stageMask =
            RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        memset(&empty_submit, 0, sizeof(empty_submit));
        empty_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        empty_submit.signalSemaphoreInfoCount = 1u;
        empty_submit.pSignalSemaphoreInfos = &semaphore_submit_info;
        CHECK(vkQueueSubmit2(queue, 1u, &empty_submit, 0u) ==
              RIN_VK_SUCCESS);

        semaphore_submit_info.stageMask = 0u;
        empty_submit.signalSemaphoreInfoCount = 0u;
        empty_submit.pSignalSemaphoreInfos = NULL;
        empty_submit.waitSemaphoreInfoCount = 1u;
        empty_submit.pWaitSemaphoreInfos = &semaphore_submit_info;
        CHECK(vkQueueSubmit2(queue, 1u, &empty_submit, 0u) ==
              RIN_VK_SUCCESS);
        CHECK(vkQueueWaitIdle(queue) == RIN_VK_SUCCESS);
        vkDestroySemaphore(device, empty_scope_semaphore, NULL);
    }

    memset(&swapchain_info, 0, sizeof(swapchain_info));
    swapchain_info.sType = RIN_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = 2u;
    swapchain_info.imageFormat = RIN_VK_FORMAT_R8G8B8A8_UNORM;
    swapchain_info.imageColorSpace = RIN_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    swapchain_info.imageExtent.width = 4u;
    swapchain_info.imageExtent.height = 4u;
#else
    swapchain_info.imageExtent.width = compositor.width;
    swapchain_info.imageExtent.height = compositor.height;
#endif
    swapchain_info.imageArrayLayers = 1u;
    swapchain_info.imageUsage = RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_info.imageSharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = RIN_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_info.compositeAlpha = RIN_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = RIN_VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = 1u;
    CHECK(vkCreateSwapchainKHR(device, &swapchain_info, NULL, &swapchain) ==
          RIN_VK_SUCCESS);
    swapchain_created = 1u;
    CHECK(product_adapters.resource_bind_calls == 2u &&
          product_adapters.active_resource_count == 2u &&
          rin_gpu_vulkan_icd_unbind_product_platform_v5(
              &product_adapters.v5) == RIN_GPU_VULKAN_BUSY);
    CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v4(
              &product_adapters.v4) == RIN_GPU_VULKAN_BUSY);
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images) ==
          RIN_VK_SUCCESS);
    CHECK(image_count == 2u);

    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CHECK(vkCreateSemaphore(device, &semaphore_info, NULL,
                            &acquire_semaphore) == RIN_VK_SUCCESS);
    CHECK(vkCreateSemaphore(device, &semaphore_info, NULL,
                            &render_complete_semaphore) == RIN_VK_SUCCESS);
    CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                acquire_semaphore, 0u, &image_index) ==
          RIN_VK_SUCCESS);
    CHECK(image_index < image_count);
    CHECK(record_present_layout_and_clear(
              device, queue, images[image_index], acquire_semaphore,
              render_complete_semaphore,
              compositor.expected_word) == RIN_VK_SUCCESS);
    memset(&present, 0, sizeof(present));
    present.sType = RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1u;
    present.pWaitSemaphores = &render_complete_semaphore;
    present.swapchainCount = 1u;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &image_index;
    present.pResults = &present_result;
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, runtime_sockets) == 0);
    g_compositor_fd = runtime_sockets[0];
    memset(&runtime_peer, 0, sizeof(runtime_peer));
    runtime_peer.fd = runtime_sockets[1];
    runtime_peer.acquire_fences[0] = 4u;
    runtime_peer.acquire_fences[1] = 5u;
    CHECK(pthread_create(&runtime_peer_thread, NULL,
                         runtime_test_export_peer, &runtime_peer) == 0);
    runtime_peer_started = 1;
#else
    compositor.busy_imports = 1u;
#endif
    CHECK(vkQueuePresentKHR(queue, &present) == RIN_VK_NOT_READY);
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    CHECK(present_result == RIN_VK_NOT_READY);
#else
    CHECK(present_result == RIN_VK_NOT_READY &&
          compositor.import_attempts == 1u &&
          compositor.accepted_imports == 0u);
#endif
    {
        RinVkSubmitInfo retry_signal;
        memset(&retry_signal, 0, sizeof(retry_signal));
        retry_signal.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
        retry_signal.signalSemaphoreCount = 1u;
        retry_signal.pSignalSemaphores = &render_complete_semaphore;
        CHECK(vkQueueSubmit(queue, 1u, &retry_signal, 0u) == RIN_VK_SUCCESS);
    }
    CHECK(vkQueuePresentKHR(queue, &present) == RIN_VK_SUCCESS);
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    CHECK(present_result == RIN_VK_SUCCESS);
    CHECK(pthread_join(runtime_peer_thread, NULL) == 0);
    runtime_peer_joined = 1;
    CHECK(runtime_peer.result == 0);
    CHECK(g_async_requests.count == 1u);
    runtime_job = &g_async_requests.jobs[g_async_requests.head];
    CHECK(runtime_job->type == RIN_COMPOSITOR_DAMAGE &&
          runtime_job->payload_size == sizeof(runtime_damage));
    CHECK(runtime_job->second_type == RIN_COMPOSITOR_COMMIT_V2 &&
          runtime_job->second_payload_size == sizeof(runtime_commit));
    memcpy(&runtime_damage, runtime_job->payload, sizeof(runtime_damage));
    memcpy(&runtime_commit, runtime_job->second_payload,
           sizeof(runtime_commit));
    CHECK(runtime_damage.surface_id == 7u && runtime_damage.x == 0 &&
          runtime_damage.y == 0 && runtime_damage.w == 4u &&
          runtime_damage.h == 4u);
    CHECK(runtime_commit.surface_id == 7u &&
          runtime_commit.buffer_slot == 0u &&
          runtime_commit.frame_sequence == 13u &&
          runtime_commit.reserved == 0u);
    CHECK(runtime_surface->frame_present_pending == 1u &&
          runtime_surface->frame_sequence == 13u);
    memcpy(runtime_expected_rgba, &compositor.expected_word,
           sizeof(runtime_expected_rgba));
    for (runtime_pixel_index = 0u; runtime_pixel_index < 16u;
         ++runtime_pixel_index) {
        const uint8_t* pixel = runtime_pixels + runtime_pixel_index * 4u;
        CHECK(pixel[0] == runtime_expected_rgba[2] &&
              pixel[1] == runtime_expected_rgba[1] &&
              pixel[2] == runtime_expected_rgba[0] &&
              pixel[3] == runtime_expected_rgba[3]);
    }
#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST)
    CHECK(compositor_native_window_vulkan_reset(
              runtime_surface->id, 77u, runtime_surface->width,
              runtime_surface->height, runtime_surface->pixels[0],
              runtime_surface->bytes, runtime_surface->pixels[1],
              runtime_surface->bytes, runtime_surface->pitch) == 0);
    CHECK(compositor_native_window_vulkan_dispatch_frame(
              &runtime_damage, sizeof(runtime_damage), &runtime_commit,
              sizeof(runtime_commit), 77u, runtime_pixels,
              runtime_surface->bytes) == 0);
#endif
#else
    CHECK(present_result == RIN_VK_SUCCESS &&
          compositor.import_attempts == 2u &&
          compositor.accepted_imports == 1u);
#endif

#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    runtime_surface->width = 8u;
    runtime_surface->height = 8u;
    runtime_surface->pitch = 32u;
    runtime_surface->render_target_generation = 2u;
    runtime_surface->compositor_surface_generation = 10u;
#else
    ++compositor.generation;
    compositor.width = 8u;
    compositor.height = 8u;
#endif
    image_index = UINT32_MAX;
    CHECK(vkAcquireNextImageKHR(device, swapchain, 0u,
                                acquire_semaphore, 0u, &image_index) ==
          RIN_VK_ERROR_OUT_OF_DATE_KHR);
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    CHECK(image_index == UINT32_MAX);
#else
    CHECK(image_index == UINT32_MAX && compositor.accepted_imports == 1u);
#endif

    {
        RinVkImage bound_images[2];
        uint32_t bound_image_count = 2u;
        const TestResourceBinding* first;
        const TestResourceBinding* second;
        CHECK(vkGetSwapchainImagesKHR(device, swapchain, &bound_image_count,
                                      bound_images) == RIN_VK_SUCCESS &&
              bound_image_count == 2u);
        first = test_find_resource_binding(&product_adapters,
                                           bound_images[0]);
        second = test_find_resource_binding(&product_adapters,
                                            bound_images[1]);
        CHECK(first != NULL && second != NULL &&
              first->allocation_handle != second->allocation_handle &&
              first->allocation_offset == 0u &&
              second->allocation_offset == 0u &&
              first->size_bytes != 0u && second->size_bytes != 0u &&
              first->required_gpu_access ==
                  (RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
                   RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) &&
              second->required_gpu_access ==
                  (RIN_VULKAN_PRODUCT_MEMORY_GPU_READ |
                   RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE));
        product_adapters.fail_unbind_cookie = bound_images[0];
        product_adapters.fail_unbind_attempts = 1u;
        vkDestroySwapchainKHR(device, swapchain, NULL);
        CHECK(product_adapters.active_resource_count == 2u &&
              product_adapters.resource_unbind_calls == 0u &&
              vkGetSwapchainImagesKHR(device, swapchain, &bound_image_count,
                                      bound_images) == RIN_VK_SUCCESS &&
              bound_image_count == 2u);
        vkDestroySwapchainKHR(device, swapchain, NULL);
        CHECK(product_adapters.active_resource_count == 0u &&
              product_adapters.resource_unbind_calls == 2u &&
              product_adapters.fail_unbind_attempts == 0u);
        swapchain_created = 0u;
        CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v5(
                  &product_adapters.v5) == RIN_GPU_VULKAN_OK);
        product_v5_bound = 0u;
    }

    result = RIN_VK_SUCCESS;
cleanup:
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    if (runtime_peer_started && !runtime_peer_joined) {
        if (runtime_sockets[0] >= 0)
            (void)shutdown(runtime_sockets[0], SHUT_RDWR);
        (void)pthread_join(runtime_peer_thread, NULL);
        runtime_peer_joined = 1;
    }
    g_compositor_fd = -1;
    if (runtime_sockets[0] >= 0) (void)close(runtime_sockets[0]);
    if (runtime_sockets[1] >= 0) (void)close(runtime_sockets[1]);
#endif
    if (device_created && queue) (void)vkQueueWaitIdle(queue);
    if (swapchain_created) vkDestroySwapchainKHR(device, swapchain, NULL);
    if (render_complete_semaphore != 0u)
        vkDestroySemaphore(device, render_complete_semaphore, NULL);
    if (acquire_semaphore != 0u)
        vkDestroySemaphore(device, acquire_semaphore, NULL);
    if (device_created) vkDestroyDevice(device, NULL);
    if (surface_created) vkDestroySurfaceKHR(instance, surface, NULL);
    if (instance) vkDestroyInstance(instance, NULL);
    if (product_v5_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform_v5(
            &product_adapters.v5);
    if (product_v4_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform_v4(
            &product_adapters.v4);
    if (product_v3_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform_v3(
            &product_adapters.v3);
    if (product_v2_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform_v2(
            &product_adapters.v2);
    if (product_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform(&software.platform);
    if (software_initialized)
        (void)rin_gpu_vulkan_software_platform_shutdown(&software);
    if (runtime_bound) (void)rin_gpu_vulkan_icd_unbind_runtime(&runtime);
    if (instance_initialized) (void)rin_gpu_vulkan_runtime_shutdown(&runtime);
#undef CHECK
    if (result != RIN_VK_SUCCESS) return 1;
    puts("RinVulkan native-window Compositor integration: PASS");
    return 0;
}
