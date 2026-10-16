/* SPDX-License-Identifier: MIT */
#if defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#include <rinvulkan/icd.h>
#include <rinvulkan/rinos_process_submit_backend_v1.h>
#include <rinvulkan/software_platform.h>
#include <ringpu/rin_shader.h>
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
#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST) && \
    !defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    uint8_t compositor_pixels[2][256];
    RinCompositorDamage compositor_damage;
    RinCompositorCommitV2 compositor_commit;
#endif
} TestCompositorSurface;

#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST)
typedef struct TestNativeWindowAppSurface {
    RinRuntimeGuiCompletionCallback completion_callback;
    void* completion_context;
    uint64_t compositor_generation;
    uint32_t compositor_surface_id;
    uint32_t compositor_buffer_slot;
    uint32_t frame_import_calls;
    uint32_t frame_import_attempts;
    int32_t last_import_result;
    uint32_t compositor_dispatch_calls;
    uint32_t surface_ops_calls;
    uint32_t window_create_calls;
    uint32_t window_close_calls;
} TestNativeWindowAppSurface;

static TestNativeWindowAppSurface g_native_window_app_surface;

static RinRuntimeGuiHandle test_native_window_app_wnd_create(
    const char*, int, int, int, int);
static void test_native_window_app_wnd_close(RinRuntimeGuiHandle);
static int test_native_window_app_wnd_dispatch_compositor(uint32_t,
                                                           uint32_t);
static int test_native_window_app_wnd_get_gpu_surface_ops_v1(
    RinRuntimeCompositorGpuSurfaceOpsV1*);
static int test_native_window_app_wnd_get_size(RinRuntimeGuiHandle,
                                               int*, int*);
static int test_native_window_app_wnd_set_completion_callback(
    RinRuntimeGuiHandle, RinRuntimeGuiCompletionCallback, void*);

/* Compile the actual sample entrypoint against the ICD under test. Only the
 * native window/Compositor boundary is fixture-backed; Vulkan calls remain
 * the production public entrypoints linked into this test. */
#define wnd_create test_native_window_app_wnd_create
#define wnd_close test_native_window_app_wnd_close
#define wnd_dispatch_compositor \
    test_native_window_app_wnd_dispatch_compositor
#define wnd_get_gpu_surface_ops_v1 \
    test_native_window_app_wnd_get_gpu_surface_ops_v1
#define wnd_get_size test_native_window_app_wnd_get_size
#define wnd_set_compositor_completion_callback \
    test_native_window_app_wnd_set_completion_callback
#define RIN_VULKAN_NATIVE_WINDOW_EXAMPLE_NO_MAIN 1
#include "../examples/rinos_native_window_surface.c"
#undef RIN_VULKAN_NATIVE_WINDOW_EXAMPLE_NO_MAIN
#undef wnd_set_compositor_completion_callback
#undef wnd_get_size
#undef wnd_get_gpu_surface_ops_v1
#undef wnd_dispatch_compositor
#undef wnd_close
#undef wnd_create
#endif

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

typedef struct TestSdkProcessQueue {
    uint32_t queue_id;
    uint32_t active;
    RinGpuProcessQueueV1 token;
} TestSdkProcessQueue;

typedef struct TestSdkProcessResource {
    uint32_t active;
    RinGpuProcessResourceBindingV1 binding;
} TestSdkProcessResource;

#define TEST_SDK_PROCESS_QUERY_CAPACITY 4u
#define TEST_SDK_PROCESS_COMMAND_CAPTURE 8u

typedef struct TestSdkProcessQuery {
    uint32_t active;
    uint32_t reserved;
    uint64_t token;
    RinGpuProcessQueryDescV1 descriptor;
    RinGpuProcessQueryResultV1 result;
} TestSdkProcessQuery;

typedef struct TestProductAdapters {
    RinGpuVulkanSoftwarePlatformV1* software;
    RinVulkanProductPlatformV2 v2;
    RinVulkanProductPlatformV3 v3;
    RinVulkanProductPlatformV4 v4;
    RinVulkanProductPlatformV5 v5;
    RinVulkanProductPlatformV6* v6;
    RinVulkanProductPlatformV7* v7;
    RinVulkanProductPlatformV8* v8;
    RinVulkanProductPlatformV9* v9;
    RinVulkanProductPlatformV6 software_v6;
    RinVulkanRinOSProcessSubmitBackendV5 process_submit_backend;
    TestSdkProcessQueue sdk_queues[RIN_GPU_PROCESS_MAX_QUEUES_V1];
    TestSdkProcessResource sdk_resources[
        RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1];
    TestSdkProcessQuery sdk_queries[TEST_SDK_PROCESS_QUERY_CAPACITY];
    uint32_t sdk_queue_bind_calls;
    uint32_t sdk_queue_release_calls;
    uint32_t sdk_process_submit_calls;
    uint32_t sdk_active_queue_count;
    uint32_t sdk_shader_module_create_calls;
    uint32_t sdk_compute_pipeline_create_calls;
    uint32_t sdk_object_destroy_calls;
    uint32_t sdk_query_create_calls;
    uint32_t sdk_query_result_calls;
    uint64_t sdk_last_query_token;
    uint32_t sdk_last_command_count;
    RinGpuBackendCommandV1 sdk_last_commands[
        TEST_SDK_PROCESS_COMMAND_CAPTURE];
    uint32_t sdk_last_submit_resource_count;
    RinVulkanProductResourceV1 sdk_last_submit_resources[
        RIN_GPU_PROCESS_MAX_RESOURCES_V1];
    uint32_t sdk_resource_bind_calls;
    uint32_t sdk_resource_unbind_calls;
    uint32_t sdk_bind_group_create_calls;
    uint32_t sdk_bind_group_destroy_busy_attempts;
    RinGpuProcessResourceBindingV1 sdk_last_resource_binding;
    uint32_t sdk_last_shader_resource_count;
    uint64_t sdk_last_shader_module_token;
    uint64_t sdk_last_pipeline_token;
    uint64_t sdk_last_pipeline_shader_token;
    uint64_t sdk_last_bind_group_token;
    uint64_t sdk_last_bind_group_pipeline_token;
    uint32_t sdk_last_bind_group_count;
    RinGpuProcessComputeBindingV1 sdk_last_bind_group_bindings[
        RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1];
    uint64_t sdk_destroy_tokens[2];
    uint32_t canonical_submit_calls;
    uint32_t canonical_command_count;
    uint32_t canonical_resource_count;
    RinVulkanProductResourceV1 canonical_last_resources[
        RIN_GPU_PROCESS_MAX_RESOURCES_V1];
    uint32_t canonical_dispatch_calls;
    RinGpuBackendCommandV1 canonical_last_command;
    uint32_t canonical_resolve_command_count;
    uint32_t canonical_graphics_barrier_count;
    RinGpuBackendGraphicsBarrierV2 canonical_last_graphics_barrier;
    uint32_t defer_next_process_completion;
    uint32_t deferred_completion_queue_id;
    uint64_t deferred_completion_value;
    uint64_t next_mapping_lease;
    TestMemoryMapping mappings[RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS];
    TestResourceBinding resources[RIN_GPU_VULKAN_SOFTWARE_MAX_ALLOCATIONS];
    uint64_t fail_unbind_cookie;
    uint32_t fail_unbind_attempts;
    uint32_t resource_bind_calls;
    uint32_t resource_unbind_calls;
    uint32_t active_resource_count;
} TestProductAdapters;

static uint32_t g_opaque_process_prepare_calls;
static uint32_t g_opaque_process_submit_calls;
static TestProductAdapters* g_process_submit_adapters;

#define TEST_PROCESS_SUBMIT_DEVICE_ID UINT64_C(0xD15A0001)
#define TEST_PROCESS_SUBMIT_DEVICE_GENERATION UINT64_C(7)

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
#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST) && \
    !defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
    if (frame->bytes > sizeof(surface->compositor_pixels[0]))
        return RIN_ERROR_INVALID_ARGUMENT;
    for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
        const uint8_t* source = pixels + pixel_index * 4u;
        uint8_t* destination = surface->compositor_pixels[0] +
                               pixel_index * 4u;
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        destination[3] = source[3];
    }
    memset(&surface->compositor_damage, 0,
           sizeof(surface->compositor_damage));
    surface->compositor_damage.surface_id = 7u;
    surface->compositor_damage.w = frame->width;
    surface->compositor_damage.h = frame->height;
    memset(&surface->compositor_commit, 0,
           sizeof(surface->compositor_commit));
    surface->compositor_commit.surface_id = 7u;
    surface->compositor_commit.buffer_slot = 0u;
    surface->compositor_commit.frame_sequence = 13u;
#endif
    ++surface->accepted_imports;
    return RIN_RESULT_OK;
}
#endif

#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST) && \
    !defined(RIN_VULKAN_NATIVE_WINDOW_RUNTIME_TEST)
static int test_dispatch_fixture_frame_to_compositor(
        TestCompositorSurface* surface) {
    uint32_t row_pitch;
    uint64_t frame_bytes;
    if (!surface || surface->width == 0u || surface->height == 0u ||
        surface->width > UINT32_MAX / 4u)
        return -1;
    row_pitch = surface->width * 4u;
    if (surface->height >
        sizeof(surface->compositor_pixels[0]) / row_pitch)
        return -1;
    frame_bytes = (uint64_t)row_pitch * surface->height;
    if (compositor_native_window_vulkan_reset(
            7u, 77u, surface->width, surface->height,
            surface->compositor_pixels[0], frame_bytes,
            surface->compositor_pixels[1], frame_bytes, row_pitch) != 0)
        return -1;
    return compositor_native_window_vulkan_dispatch_frame(
        &surface->compositor_damage, sizeof(surface->compositor_damage),
        &surface->compositor_commit, sizeof(surface->compositor_commit),
        77u, surface->compositor_pixels[0], frame_bytes);
}
#endif

#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST)
static int test_native_window_app_surface_query(
        void* context, RinRuntimeGuiHandle handle,
        RinRuntimeCompositorGpuSurfaceV1* surface_out) {
    TestNativeWindowAppSurface* surface =
        (TestNativeWindowAppSurface*)context;
    if (surface != &g_native_window_app_surface ||
        handle != UINT64_C(0x7712) || !surface_out)
        return RIN_ERROR_INVALID_ARGUMENT;
    memset(surface_out, 0, sizeof(*surface_out));
    surface_out->struct_size = sizeof(*surface_out);
    surface_out->version = RIN_RUNTIME_COMPOSITOR_GPU_SURFACE_V1_VERSION;
    surface_out->width = 2u;
    surface_out->height = 2u;
    surface_out->supported_frame_formats =
        RIN_RUNTIME_COMPOSITOR_GPU_SURFACE_FORMAT_RGBA8_BIT;
    surface_out->surface_generation = 1u;
    return RIN_RESULT_OK;
}

static int test_native_window_app_surface_import(
        void* context, RinRuntimeGuiHandle handle,
        uint64_t surface_generation,
        const RinRuntimeCompositorGpuFrameV1* frame) {
    TestNativeWindowAppSurface* surface =
        (TestNativeWindowAppSurface*)context;
    RinCompositorGpuReadbackLeasePresentV1 request;
    uint32_t buffer_slot = UINT32_MAX;
    int import_result;
    if (surface == &g_native_window_app_surface)
        ++surface->frame_import_attempts;
    if (surface != &g_native_window_app_surface ||
        handle != UINT64_C(0x7712) || surface_generation != 1u || !frame ||
        frame->struct_size != sizeof(*frame) ||
        frame->version != RIN_RUNTIME_COMPOSITOR_GPU_FRAME_V1_VERSION ||
        frame->width != 2u || frame->height != 2u ||
        frame->row_pitch != 8u || frame->bytes != 16u ||
        frame->format != RIN_RUNTIME_COMPOSITOR_GPU_PIXEL_RGBA8 ||
        !frame->pixels || frame->reserved[0] != 0u ||
        frame->reserved[1] != 0u) {
        if (surface == &g_native_window_app_surface)
            surface->last_import_result = -1001;
        return RIN_ERROR_INVALID_ARGUMENT;
    }

    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = RIN_COMPOSITOR_GPU_READBACK_LEASE_VERSION;
    request.surface_id = surface->compositor_surface_id;
    request.format = RIN_COMPOSITOR_GPU_READBACK_FORMAT_RGBA8;
    request.expected_surface_generation = surface->compositor_generation;
    request.frame_sequence = 1u;
    request.bytes = frame->bytes;
    request.capability.struct_size = sizeof(request.capability);
    request.capability.version =
        RIN_GPU_CROSS_PROCESS_CAPABILITY_V2_VERSION;
    request.capability.token = 9u;
    request.capability.device_generation = 3u;
    request.capability.rights = RIN_GPU_CROSS_PROCESS_RIGHT_READ;
    request.width = frame->width;
    request.height = frame->height;
    request.row_pitch = frame->row_pitch;
    import_result = compositor_gpu_readback_runtime_core_import(
        &request, frame->pixels, frame->bytes, 77u, &buffer_slot);
    if (import_result != RIN_RESULT_OK) {
        surface->last_import_result = import_result;
        return RIN_RESULT_IO;
    }
    surface->last_import_result = 0;
    surface->compositor_buffer_slot = buffer_slot;
    ++surface->frame_import_calls;
    return RIN_RESULT_OK;
}

static RinRuntimeGuiHandle test_native_window_app_wnd_create(
        const char* title, int x, int y, int width, int height) {
    TestNativeWindowAppSurface* surface = &g_native_window_app_surface;
    if (!title || strcmp(title, "RinVulkan native-window surface") != 0 ||
        x != 64 || y != 64 || width != 960 || height != 540)
        return RIN_WINDOW_HANDLE_INVALID;
    ++surface->window_create_calls;
    return UINT64_C(0x7712);
}

static void test_native_window_app_wnd_close(RinRuntimeGuiHandle handle) {
    TestNativeWindowAppSurface* surface = &g_native_window_app_surface;
    if (handle == UINT64_C(0x7712) &&
        surface->completion_callback == NULL)
        ++surface->window_close_calls;
}

static int test_native_window_app_wnd_dispatch_compositor(
        uint32_t timeout_ms, uint32_t max_completions) {
    TestNativeWindowAppSurface* surface = &g_native_window_app_surface;
    RinRuntimeGuiCompletionV1 completion;
    if (timeout_ms != 50u || max_completions != 8u ||
        surface->frame_import_calls != 1u ||
        !surface->completion_callback)
        return -1;
    memset(&completion, 0, sizeof(completion));
    completion.struct_size = sizeof(completion);
    completion.version = 1u;
    completion.request_type = RIN_COMPOSITOR_DAMAGE;
    completion.status = RIN_RESULT_OK;
    completion.handle = UINT64_C(0x7712);
    completion.cookie = 14u;
    surface->completion_callback(&completion, surface->completion_context);
    ++surface->compositor_dispatch_calls;
    return 1;
}

static int test_native_window_app_wnd_get_gpu_surface_ops_v1(
        RinRuntimeCompositorGpuSurfaceOpsV1* ops_out) {
    TestNativeWindowAppSurface* surface = &g_native_window_app_surface;
    if (!ops_out) return RIN_ERROR_INVALID_ARGUMENT;
    memset(ops_out, 0, sizeof(*ops_out));
    ops_out->struct_size = sizeof(*ops_out);
    ops_out->version = RIN_RUNTIME_COMPOSITOR_GPU_SURFACE_OPS_V1_VERSION;
    ops_out->context = surface;
    ops_out->query = test_native_window_app_surface_query;
    ops_out->import_frame = test_native_window_app_surface_import;
    ++surface->surface_ops_calls;
    return RIN_RESULT_OK;
}

static int test_native_window_app_wnd_get_size(
        RinRuntimeGuiHandle handle, int* width, int* height) {
    if (handle != UINT64_C(0x7712) || !width || !height)
        return RIN_ERROR_INVALID_ARGUMENT;
    *width = 2;
    *height = 2;
    return RIN_RESULT_OK;
}

static int test_native_window_app_wnd_set_completion_callback(
        RinRuntimeGuiHandle handle, RinRuntimeGuiCompletionCallback callback,
        void* context) {
    TestNativeWindowAppSurface* surface = &g_native_window_app_surface;
    if (handle != UINT64_C(0x7712)) return RIN_ERROR_INVALID_ARGUMENT;
    if (callback) {
        if (surface->completion_callback) return RIN_ERROR_BUSY;
        surface->completion_callback = callback;
        surface->completion_context = context;
        return RIN_RESULT_OK;
    }
    if (!surface->completion_callback || context != NULL)
        return RIN_ERROR_INVALID_ARGUMENT;
    surface->completion_callback = NULL;
    surface->completion_context = NULL;
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
    ++g_opaque_process_prepare_calls;
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
    ++g_opaque_process_submit_calls;
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
    ++g_opaque_process_prepare_calls;
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
    ++g_opaque_process_submit_calls;
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

static int test_backend_state_to_vk_layout(uint32_t state,
                                           uint32_t* layout_out) {
    if (!layout_out) return 0;
    switch (state) {
    case RIN_GPU_IMAGE_STATE_UNDEFINED:
        *layout_out = RIN_VK_IMAGE_LAYOUT_UNDEFINED;
        return 1;
    case RIN_GPU_IMAGE_STATE_COPY_SOURCE:
        *layout_out = RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        return 1;
    case RIN_GPU_IMAGE_STATE_COPY_DESTINATION:
        *layout_out = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        return 1;
    case RIN_GPU_IMAGE_STATE_COLOR_TARGET:
        *layout_out = RIN_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        return 1;
    case RIN_GPU_IMAGE_STATE_PRESENT:
        *layout_out = RIN_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        return 1;
    default:
        return 0;
    }
}

/* The host fake consumes Vulkan operation packets, while V6 now carries the
 * API-independent RinGPU barrier schema. Reconstruct a legal representative
 * Vulkan scope for this test backend instead of mixing mask namespaces. */
static int test_backend_barrier_scope_to_vk(
        uint32_t backend_stage, uint32_t backend_access,
        uint32_t* stage_out, uint32_t* access_out) {
    const uint32_t known_stages = RIN_GPU_PIPELINE_STAGE_COPY |
        RIN_GPU_PIPELINE_STAGE_HOST | RIN_GPU_PIPELINE_STAGE_VERTEX_INPUT |
        RIN_GPU_PIPELINE_STAGE_VERTEX_SHADER |
        RIN_GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS |
        RIN_GPU_PIPELINE_STAGE_FRAGMENT_SHADER |
        RIN_GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS |
        RIN_GPU_PIPELINE_STAGE_COLOR_OUTPUT |
        RIN_GPU_PIPELINE_STAGE_COMPUTE_SHADER;
    const uint32_t known_access = RIN_GPU_RESOURCE_READ |
                                  RIN_GPU_RESOURCE_WRITE;
    uint32_t stage = 0u;
    uint32_t access = 0u;
    if (!stage_out || !access_out || (backend_stage & ~known_stages) != 0u ||
        (backend_access & ~known_access) != 0u ||
        (backend_stage == 0u && backend_access != 0u))
        return 0;
#define TEST_MAP_BACKEND_SCOPE(stage_bit, vk_stage, read_access, write_access) \
    do { \
        if ((backend_stage & (stage_bit)) != 0u) { \
            stage |= (vk_stage); \
            if ((backend_access & RIN_GPU_RESOURCE_READ) != 0u) \
                access |= (read_access); \
            if ((backend_access & RIN_GPU_RESOURCE_WRITE) != 0u) \
                access |= (write_access); \
        } \
    } while (0)
    TEST_MAP_BACKEND_SCOPE(RIN_GPU_PIPELINE_STAGE_COPY,
        RIN_GPU_VULKAN_BARRIER_STAGE_TRANSFER,
        RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_READ,
        RIN_GPU_VULKAN_BARRIER_ACCESS_TRANSFER_WRITE);
    TEST_MAP_BACKEND_SCOPE(RIN_GPU_PIPELINE_STAGE_HOST,
        RIN_GPU_VULKAN_BARRIER_STAGE_HOST,
        RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_READ,
        RIN_GPU_VULKAN_BARRIER_ACCESS_HOST_WRITE);
    if ((backend_stage & (RIN_GPU_PIPELINE_STAGE_VERTEX_INPUT |
                          RIN_GPU_PIPELINE_STAGE_VERTEX_SHADER |
                          RIN_GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS |
                          RIN_GPU_PIPELINE_STAGE_FRAGMENT_SHADER |
                          RIN_GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS |
                          RIN_GPU_PIPELINE_STAGE_COLOR_OUTPUT)) != 0u) {
        stage |= RIN_GPU_VULKAN_BARRIER_STAGE_GRAPHICS;
        if ((backend_access & RIN_GPU_RESOURCE_READ) != 0u)
            access |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_READ;
        if ((backend_access & RIN_GPU_RESOURCE_WRITE) != 0u)
            access |= RIN_GPU_VULKAN_BARRIER_ACCESS_GRAPHICS_WRITE;
    }
    TEST_MAP_BACKEND_SCOPE(RIN_GPU_PIPELINE_STAGE_COMPUTE_SHADER,
        RIN_GPU_VULKAN_BARRIER_STAGE_COMPUTE,
        RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_READ,
        RIN_GPU_VULKAN_BARRIER_ACCESS_COMPUTE_WRITE);
#undef TEST_MAP_BACKEND_SCOPE
    *stage_out = stage;
    *access_out = access;
    return 1;
}

static int test_submit_process_commands_v6(
        void* context, uint32_t queue_id,
        const RinGpuBackendCommandV1* commands, uint32_t command_count,
        const RinVulkanProductResourceV1* resources, uint32_t resource_count,
        RinVulkanProductProcessSubmitReceiptV1* receipt_out) {
    TestProductAdapters* adapters = (TestProductAdapters*)context;
    RinGpuVulkanTransferPacketV3 packet;
    RinVulkanProductSubmissionV1 submission;
    uint64_t pending_image = 0u;
    uint32_t pending_before = 0u;
    uint32_t pending_after = 0u;
    uint32_t operation_count = 0u;
    uint32_t index;
    uint64_t previous_completed_value;
    uint32_t defer_completion;
    int result;
    if (!adapters || !adapters->software || !commands || !receipt_out ||
        command_count == 0u ||
        resource_count > RIN_VULKAN_PRODUCT_MAX_RESOURCES_PER_SUBMISSION ||
        (resource_count != 0u && !resources) ||
        queue_id >= adapters->software->queue_count)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    if (command_count == 1u &&
        commands[0].type == RIN_GPU_BACKEND_COMMAND_DISPATCH) {
        const RinGpuBackendDispatchV1* dispatch =
            &commands[0].value.dispatch;
        if (commands[0].reserved != 0u ||
            dispatch->pipeline_cookie == 0u ||
            dispatch->pipeline_cookie != adapters->sdk_last_pipeline_token ||
            dispatch->group_count_x != 2u ||
            dispatch->group_count_y != 3u ||
            dispatch->group_count_z != 4u)
            return RIN_VULKAN_PRODUCT_PROTOCOL;
        if (dispatch->bind_group_cookie == 0u) {
            if (resource_count != 0u) return RIN_VULKAN_PRODUCT_PROTOCOL;
        } else {
            if (dispatch->bind_group_cookie !=
                    adapters->sdk_last_bind_group_token ||
                adapters->sdk_last_bind_group_count != 1u ||
                resource_count != 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
        }
        adapters->canonical_last_command = commands[0];
        adapters->canonical_resource_count = resource_count;
        memset(adapters->canonical_last_resources, 0,
               sizeof(adapters->canonical_last_resources));
        if (resource_count != 0u)
            memcpy(adapters->canonical_last_resources, resources,
                   resource_count * sizeof(*resources));
        ++adapters->canonical_dispatch_calls;
        /* This host provider validates the process-submit handoff and gives
         * the ICD a real software-queue completion token. It does not claim
         * to execute a GPU shader. */
        memset(&packet, 0, sizeof(packet));
        packet.struct_size = sizeof(packet);
        packet.version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_3;
        packet.product_queue_id = queue_id;
        packet.op_count = 1u;
        packet.operations[0].type =
            RIN_GPU_VULKAN_TRANSFER_OP_MEMORY_BARRIER;
        packet.operations[0].barrier.src_stage_mask =
            RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS;
        packet.operations[0].barrier.dst_stage_mask =
            RIN_GPU_VULKAN_BARRIER_STAGE_ALL_COMMANDS;
        memset(&submission, 0, sizeof(submission));
        result = adapters->software->platform.prepare_submission(
            adapters->software->platform.context, queue_id,
            (uint64_t)(uintptr_t)&packet, &submission);
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
        result = adapters->software->platform.submit(
            adapters->software->platform.context, &submission, NULL, 0u);
        if (result != RIN_VULKAN_PRODUCT_OK) return result;
        memset(receipt_out, 0, sizeof(*receipt_out));
        receipt_out->struct_size = sizeof(*receipt_out);
        receipt_out->version =
            RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION;
        receipt_out->queue_id = submission.queue_id;
        receipt_out->sequence = submission.sequence;
        receipt_out->completion_value = submission.completion_value;
        receipt_out->device_epoch = submission.device_epoch;
        receipt_out->iommu_map_generation =
            submission.iommu_map_generation;
        ++adapters->canonical_submit_calls;
        adapters->canonical_command_count = command_count;
        return RIN_VULKAN_PRODUCT_OK;
    }
    memset(&packet, 0, sizeof(packet));
    packet.struct_size = sizeof(packet);
    packet.version = RIN_GPU_VULKAN_TRANSFER_BATCH_VERSION_3;
    packet.product_queue_id = queue_id;
    previous_completed_value =
        adapters->software->completed_values[queue_id];
    defer_completion = adapters->defer_next_process_completion;
    for (index = 0u; index < command_count; ++index) {
        const RinGpuBackendCommandV1* command = &commands[index];
        RinGpuVulkanTransferOpV2* operation;
        if (command->reserved != 0u) return RIN_VULKAN_PRODUCT_PROTOCOL;
        if (command->type == RIN_GPU_BACKEND_COMMAND_COPY_BUFFER) {
            const RinGpuBackendBufferCopyV1* copy =
                &command->value.buffer_copy;
            const TestResourceBinding* source_binding =
                test_find_resource_binding(adapters, copy->source_cookie);
            const TestResourceBinding* destination_binding =
                test_find_resource_binding(adapters,
                                           copy->destination_cookie);
            RinGpuVulkanSoftwareAllocationV1* source_allocation =
                source_binding
                    ? test_find_allocation(adapters->software,
                                           source_binding->allocation_handle)
                    : NULL;
            RinGpuVulkanSoftwareAllocationV1* destination_allocation =
                destination_binding
                    ? test_find_allocation(adapters->software,
                                           destination_binding->allocation_handle)
                    : NULL;
            if (pending_image != 0u || !source_binding ||
                !destination_binding || !source_allocation ||
                !destination_allocation || copy->size_bytes == 0u ||
                copy->source_offset > source_binding->size_bytes ||
                copy->size_bytes >
                    source_binding->size_bytes - copy->source_offset ||
                copy->destination_offset > destination_binding->size_bytes ||
                copy->size_bytes > destination_binding->size_bytes -
                                       copy->destination_offset ||
                (source_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) == 0u ||
                (destination_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) == 0u ||
                source_binding->allocation_offset > UINT64_MAX -
                    source_allocation->gpu_virtual_address ||
                copy->source_offset > UINT64_MAX -
                    (source_allocation->gpu_virtual_address +
                     source_binding->allocation_offset) ||
                destination_binding->allocation_offset > UINT64_MAX -
                    destination_allocation->gpu_virtual_address ||
                copy->destination_offset > UINT64_MAX -
                    (destination_allocation->gpu_virtual_address +
                     destination_binding->allocation_offset))
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            if (operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return RIN_VULKAN_PRODUCT_LIMIT;
            operation = &packet.operations[operation_count++];
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_COPY;
            operation->source_allocation = source_binding->allocation_handle;
            operation->destination_allocation =
                destination_binding->allocation_handle;
            operation->source_gpu_address =
                source_allocation->gpu_virtual_address +
                source_binding->allocation_offset + copy->source_offset;
            operation->destination_gpu_address =
                destination_allocation->gpu_virtual_address +
                destination_binding->allocation_offset +
                copy->destination_offset;
            operation->size_bytes = copy->size_bytes;
            continue;
        }
        if (command->type == RIN_GPU_BACKEND_COMMAND_COPY_IMAGE) {
            const RinGpuBackendImageCopyV1* copy =
                &command->value.image_copy;
            const RinGpuImageCopyRegionV1* region = &copy->region;
            const TestResourceBinding* source_binding =
                test_find_resource_binding(adapters, copy->source_cookie);
            const TestResourceBinding* destination_binding =
                test_find_resource_binding(adapters,
                                           copy->destination_cookie);
            RinGpuVulkanSoftwareAllocationV1* source_allocation =
                source_binding
                    ? test_find_allocation(adapters->software,
                                           source_binding->allocation_handle)
                    : NULL;
            RinGpuVulkanSoftwareAllocationV1* destination_allocation =
                destination_binding
                    ? test_find_allocation(adapters->software,
                                           destination_binding->allocation_handle)
                    : NULL;
            uint64_t copy_bytes;
            if (pending_image != 0u || !source_binding ||
                !destination_binding || !source_allocation ||
                !destination_allocation ||
                source_binding->allocation_handle ==
                    destination_binding->allocation_handle ||
                region->abi_version != RIN_GPU_ABI_VERSION ||
                region->struct_size != sizeof(*region) ||
                region->source_mip_level != 0u ||
                region->source_array_layer != 0u ||
                region->source_x != 0u || region->source_y != 0u ||
                region->source_z != 0u ||
                region->destination_mip_level != 0u ||
                region->destination_array_layer != 0u ||
                region->destination_x != 0u || region->destination_y != 0u ||
                region->destination_z != 0u || region->width == 0u ||
                region->height == 0u || region->depth != 1u ||
                region->flags != 0u ||
                (uint64_t)region->width > UINT64_MAX / region->height / 4u ||
                (source_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) == 0u ||
                (destination_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) == 0u ||
                source_binding->size_bytes !=
                    destination_binding->size_bytes ||
                source_binding->allocation_offset >
                    source_allocation->size_bytes ||
                source_binding->size_bytes > source_allocation->size_bytes -
                    source_binding->allocation_offset ||
                destination_binding->allocation_offset >
                    destination_allocation->size_bytes ||
                destination_binding->size_bytes >
                    destination_allocation->size_bytes -
                        destination_binding->allocation_offset ||
                source_binding->allocation_offset > UINT64_MAX -
                    source_allocation->gpu_virtual_address ||
                destination_binding->allocation_offset > UINT64_MAX -
                    destination_allocation->gpu_virtual_address ||
                operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            copy_bytes = (uint64_t)region->width * region->height * 4u;
            if (copy_bytes != source_binding->size_bytes)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation = &packet.operations[operation_count++];
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_COPY;
            operation->source_allocation = source_binding->allocation_handle;
            operation->destination_allocation =
                destination_binding->allocation_handle;
            operation->source_gpu_address =
                source_allocation->gpu_virtual_address +
                source_binding->allocation_offset;
            operation->destination_gpu_address =
                destination_allocation->gpu_virtual_address +
                destination_binding->allocation_offset;
            operation->size_bytes = copy_bytes;
            operation->source_width = region->width;
            operation->source_height = region->height;
            operation->destination_width = region->width;
            operation->destination_height = region->height;
            continue;
        }
        if (command->type == RIN_GPU_BACKEND_COMMAND_BLIT_IMAGE) {
            const RinGpuBackendImageBlitV1* blit =
                &command->value.image_blit;
            const RinGpuBackendImageBlitDescV1* region = &blit->blit;
            const TestResourceBinding* source_binding =
                test_find_resource_binding(adapters, blit->source_cookie);
            const TestResourceBinding* destination_binding =
                test_find_resource_binding(adapters,
                                           blit->destination_cookie);
            RinGpuVulkanSoftwareAllocationV1* source_allocation =
                source_binding
                    ? test_find_allocation(adapters->software,
                                           source_binding->allocation_handle)
                    : NULL;
            RinGpuVulkanSoftwareAllocationV1* destination_allocation =
                destination_binding
                    ? test_find_allocation(adapters->software,
                                           destination_binding->allocation_handle)
                    : NULL;
            uint64_t source_bytes;
            uint64_t destination_bytes;
            if (pending_image != 0u || !source_binding ||
                !destination_binding || !source_allocation ||
                !destination_allocation ||
                source_binding->allocation_handle ==
                    destination_binding->allocation_handle ||
                region->abi_version != RIN_GPU_ABI_VERSION ||
                region->struct_size != sizeof(*region) ||
                region->source_mip_level != 0u ||
                region->source_array_layer != 0u ||
                region->source_x != 0u || region->source_y != 0u ||
                region->destination_mip_level != 0u ||
                region->destination_array_layer != 0u ||
                region->destination_x != 0u || region->destination_y != 0u ||
                region->source_width == 0u || region->source_height == 0u ||
                region->destination_width == 0u ||
                region->destination_height == 0u ||
                region->filter > RIN_GPU_IMAGE_BLIT_LINEAR ||
                region->flags != 0u || region->reserved != 0u ||
                (uint64_t)region->source_width >
                    UINT64_MAX / region->source_height / 4u ||
                (uint64_t)region->destination_width >
                    UINT64_MAX / region->destination_height / 4u ||
                (source_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) == 0u ||
                (destination_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) == 0u ||
                source_binding->allocation_offset >
                    source_allocation->size_bytes ||
                source_binding->size_bytes > source_allocation->size_bytes -
                    source_binding->allocation_offset ||
                destination_binding->allocation_offset >
                    destination_allocation->size_bytes ||
                destination_binding->size_bytes >
                    destination_allocation->size_bytes -
                        destination_binding->allocation_offset ||
                source_binding->allocation_offset > UINT64_MAX -
                    source_allocation->gpu_virtual_address ||
                destination_binding->allocation_offset > UINT64_MAX -
                    destination_allocation->gpu_virtual_address ||
                operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source_bytes = (uint64_t)region->source_width *
                           region->source_height * 4u;
            destination_bytes = (uint64_t)region->destination_width *
                                region->destination_height * 4u;
            if (source_bytes != source_binding->size_bytes ||
                destination_bytes != destination_binding->size_bytes)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation = &packet.operations[operation_count++];
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BLIT;
            operation->source_allocation = source_binding->allocation_handle;
            operation->destination_allocation =
                destination_binding->allocation_handle;
            operation->source_gpu_address =
                source_allocation->gpu_virtual_address +
                source_binding->allocation_offset;
            operation->destination_gpu_address =
                destination_allocation->gpu_virtual_address +
                destination_binding->allocation_offset;
            operation->size_bytes = destination_bytes;
            operation->source_width = region->source_width;
            operation->source_height = region->source_height;
            operation->destination_width = region->destination_width;
            operation->destination_height = region->destination_height;
            operation->filter = region->filter;
            continue;
        }
        if (command->type == RIN_GPU_BACKEND_COMMAND_RESOLVE_IMAGE) {
            const RinGpuBackendImageResolveV1* resolve =
                &command->value.image_resolve;
            const RinGpuBackendImageResolveDescV1* region =
                &resolve->resolve;
            const TestResourceBinding* source_binding =
                test_find_resource_binding(adapters, resolve->source_cookie);
            const TestResourceBinding* destination_binding =
                test_find_resource_binding(adapters,
                                           resolve->destination_cookie);
            RinGpuVulkanSoftwareAllocationV1* source_allocation =
                source_binding
                    ? test_find_allocation(adapters->software,
                                           source_binding->allocation_handle)
                    : NULL;
            RinGpuVulkanSoftwareAllocationV1* destination_allocation =
                destination_binding
                    ? test_find_allocation(adapters->software,
                                           destination_binding->allocation_handle)
                    : NULL;
            uint64_t destination_bytes;
            uint32_t sample_count;
            if (pending_image != 0u || !source_binding ||
                !destination_binding || !source_allocation ||
                !destination_allocation ||
                source_binding->allocation_handle ==
                    destination_binding->allocation_handle ||
                region->abi_version != RIN_GPU_ABI_VERSION ||
                region->struct_size != sizeof(*region) ||
                region->source_mip_level != 0u ||
                region->source_array_layer != 0u || region->source_x != 0u ||
                region->source_y != 0u ||
                region->destination_mip_level != 0u ||
                region->destination_array_layer != 0u ||
                region->destination_x != 0u || region->destination_y != 0u ||
                region->width == 0u || region->height == 0u ||
                region->flags != 0u || region->reserved != 0u ||
                (uint64_t)region->width >
                    UINT64_MAX / region->height / 4u ||
                (source_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) == 0u ||
                (destination_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) == 0u ||
                source_binding->allocation_offset != 0u ||
                destination_binding->allocation_offset != 0u ||
                source_binding->size_bytes > source_allocation->size_bytes ||
                destination_binding->size_bytes >
                    destination_allocation->size_bytes ||
                operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            destination_bytes = (uint64_t)region->width * region->height * 4u;
            if (destination_bytes != destination_binding->size_bytes ||
                destination_bytes > UINT64_MAX / 4u ||
                (source_binding->size_bytes != destination_bytes * 2u &&
                 source_binding->size_bytes != destination_bytes * 4u) ||
                source_binding->size_bytes != source_allocation->size_bytes ||
                destination_binding->size_bytes !=
                    destination_allocation->size_bytes ||
                source_binding->allocation_offset > UINT64_MAX -
                    source_allocation->gpu_virtual_address ||
                destination_binding->allocation_offset > UINT64_MAX -
                    destination_allocation->gpu_virtual_address)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            sample_count =
                (uint32_t)(source_binding->size_bytes / destination_bytes);
            operation = &packet.operations[operation_count++];
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_RESOLVE;
            operation->source_allocation = source_binding->allocation_handle;
            operation->destination_allocation =
                destination_binding->allocation_handle;
            operation->source_gpu_address =
                source_allocation->gpu_virtual_address;
            operation->destination_gpu_address =
                destination_allocation->gpu_virtual_address;
            operation->size_bytes = destination_bytes;
            operation->source_width = region->width;
            operation->source_height = region->height;
            operation->destination_width = region->width;
            operation->destination_height = region->height;
            operation->sample_count = sample_count;
            ++adapters->canonical_resolve_command_count;
            continue;
        }
        if (command->type ==
            RIN_GPU_BACKEND_COMMAND_COPY_BUFFER_TO_IMAGE) {
            const RinGpuBackendBufferToImageCopyV1* copy =
                &command->value.buffer_to_image_copy;
            const TestResourceBinding* buffer_binding =
                test_find_resource_binding(adapters, copy->buffer_cookie);
            const TestResourceBinding* image_binding =
                test_find_resource_binding(adapters, copy->image_cookie);
            RinGpuVulkanSoftwareAllocationV1* buffer_allocation =
                buffer_binding
                    ? test_find_allocation(adapters->software,
                                           buffer_binding->allocation_handle)
                    : NULL;
            RinGpuVulkanSoftwareAllocationV1* image_allocation =
                image_binding
                    ? test_find_allocation(adapters->software,
                                           image_binding->allocation_handle)
                    : NULL;
            const uint64_t row_bytes =
                (uint64_t)copy->copy_width * copy->bytes_per_texel;
            uint64_t source_span;
            uint64_t image_size;
            uint64_t copied_bytes;
            uint64_t row_pitch;
            uint32_t row_length;
            if (pending_image != 0u || !buffer_binding || !image_binding ||
                !buffer_allocation || !image_allocation ||
                copy->image_mip_level != 0u ||
                copy->image_array_layer != 0u || copy->flags != 0u ||
                copy->reserved != 0u || copy->image_width == 0u ||
                copy->image_height == 0u || copy->copy_width == 0u ||
                copy->copy_height == 0u || copy->bytes_per_texel != 4u ||
                copy->image_offset_x > copy->image_width ||
                copy->copy_width > copy->image_width -
                                       copy->image_offset_x ||
                copy->image_offset_y > copy->image_height ||
                copy->copy_height > copy->image_height -
                                        copy->image_offset_y ||
                copy->buffer_row_pitch_bytes % copy->bytes_per_texel != 0u ||
                copy->buffer_row_pitch_bytes / copy->bytes_per_texel >
                    UINT32_MAX ||
                copy->buffer_row_pitch_bytes / copy->bytes_per_texel <
                    copy->copy_width ||
                copy->buffer_image_height_texels < copy->copy_height ||
                (buffer_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) == 0u ||
                (image_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) == 0u ||
                copy->copy_width > UINT64_MAX / copy->bytes_per_texel ||
                copy->copy_height > UINT64_MAX /
                    (uint64_t)copy->copy_width / copy->bytes_per_texel ||
                copy->image_width > UINT64_MAX /
                    (uint64_t)copy->image_height / copy->bytes_per_texel ||
                buffer_binding->allocation_offset > UINT64_MAX -
                    buffer_allocation->gpu_virtual_address ||
                copy->buffer_offset > UINT64_MAX -
                    (buffer_allocation->gpu_virtual_address +
                     buffer_binding->allocation_offset) ||
                image_binding->allocation_offset > UINT64_MAX -
                    image_allocation->gpu_virtual_address)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            row_pitch = copy->buffer_row_pitch_bytes;
            row_length = (uint32_t)(row_pitch / copy->bytes_per_texel);
            if ((uint64_t)(copy->copy_height - 1u) >
                (UINT64_MAX - row_bytes) / row_pitch)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            source_span = (uint64_t)(copy->copy_height - 1u) * row_pitch +
                          row_bytes;
            image_size = (uint64_t)copy->image_width * copy->image_height *
                         copy->bytes_per_texel;
            copied_bytes = row_bytes * copy->copy_height;
            if (copy->buffer_size_bytes != source_span ||
                copy->buffer_offset > buffer_binding->size_bytes ||
                source_span > buffer_binding->size_bytes - copy->buffer_offset ||
                image_size > image_binding->size_bytes ||
                buffer_binding->allocation_offset >
                    buffer_allocation->size_bytes ||
                buffer_binding->size_bytes > buffer_allocation->size_bytes -
                    buffer_binding->allocation_offset ||
                image_binding->allocation_offset >
                    image_allocation->size_bytes ||
                image_binding->size_bytes > image_allocation->size_bytes -
                    image_binding->allocation_offset ||
                operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation = &packet.operations[operation_count++];
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_BUFFER_TO_IMAGE;
            operation->source_allocation = buffer_binding->allocation_handle;
            operation->destination_allocation = image_binding->allocation_handle;
            operation->source_gpu_address =
                buffer_allocation->gpu_virtual_address +
                buffer_binding->allocation_offset + copy->buffer_offset;
            operation->destination_gpu_address =
                image_allocation->gpu_virtual_address +
                image_binding->allocation_offset;
            operation->size_bytes = copied_bytes;
            operation->source_width = copy->image_width;
            operation->source_height = copy->image_height;
            operation->destination_width = row_length;
            operation->destination_height =
                copy->buffer_image_height_texels;
            operation->buffer_to_image_region.image_offset_x =
                copy->image_offset_x;
            operation->buffer_to_image_region.image_offset_y =
                copy->image_offset_y;
            operation->buffer_to_image_region.copy_width = copy->copy_width;
            operation->buffer_to_image_region.copy_height = copy->copy_height;
            continue;
        }
        if (command->type ==
            RIN_GPU_BACKEND_COMMAND_COPY_IMAGE_TO_BUFFER) {
            const RinGpuBackendImageToBufferCopyV1* copy =
                &command->value.image_to_buffer_copy;
            const TestResourceBinding* image_binding =
                test_find_resource_binding(adapters, copy->image_cookie);
            const TestResourceBinding* buffer_binding =
                test_find_resource_binding(adapters, copy->buffer_cookie);
            RinGpuVulkanSoftwareAllocationV1* image_allocation =
                image_binding
                    ? test_find_allocation(adapters->software,
                                           image_binding->allocation_handle)
                    : NULL;
            RinGpuVulkanSoftwareAllocationV1* buffer_allocation =
                buffer_binding
                    ? test_find_allocation(adapters->software,
                                           buffer_binding->allocation_handle)
                    : NULL;
            uint64_t row_bytes;
            uint64_t destination_span;
            uint64_t source_size;
            uint64_t copied_bytes;
            if (pending_image != 0u || !image_binding || !buffer_binding ||
                !image_allocation || !buffer_allocation ||
                copy->image_mip_level != 0u ||
                copy->image_array_layer != 0u || copy->flags != 0u ||
                copy->reserved != 0u || copy->image_width == 0u ||
                copy->image_height == 0u || copy->copy_width == 0u ||
                copy->copy_height == 0u || copy->bytes_per_texel != 4u ||
                copy->image_offset_x > copy->image_width ||
                copy->copy_width > copy->image_width -
                                       copy->image_offset_x ||
                copy->image_offset_y > copy->image_height ||
                copy->copy_height > copy->image_height -
                                        copy->image_offset_y ||
                copy->buffer_row_pitch_bytes % copy->bytes_per_texel != 0u ||
                copy->buffer_row_pitch_bytes / copy->bytes_per_texel >
                    UINT32_MAX ||
                copy->buffer_row_pitch_bytes / copy->bytes_per_texel <
                    copy->copy_width ||
                copy->buffer_image_height_texels < copy->copy_height ||
                (image_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_READ) == 0u ||
                (buffer_binding->required_gpu_access &
                 RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE) == 0u ||
                copy->copy_width > UINT64_MAX / copy->bytes_per_texel ||
                copy->copy_height > UINT64_MAX /
                    (uint64_t)copy->copy_width / copy->bytes_per_texel ||
                copy->image_width > UINT64_MAX /
                    (uint64_t)copy->image_height / copy->bytes_per_texel ||
                image_binding->allocation_offset > UINT64_MAX -
                    image_allocation->gpu_virtual_address ||
                buffer_binding->allocation_offset > UINT64_MAX -
                    buffer_allocation->gpu_virtual_address ||
                copy->buffer_offset > UINT64_MAX -
                    (buffer_allocation->gpu_virtual_address +
                     buffer_binding->allocation_offset))
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            row_bytes = (uint64_t)copy->copy_width * copy->bytes_per_texel;
            if ((uint64_t)(copy->copy_height - 1u) >
                (UINT64_MAX - row_bytes) / copy->buffer_row_pitch_bytes)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            destination_span =
                (uint64_t)(copy->copy_height - 1u) *
                    copy->buffer_row_pitch_bytes + row_bytes;
            source_size = (uint64_t)copy->image_width *
                          copy->image_height * copy->bytes_per_texel;
            copied_bytes = row_bytes * copy->copy_height;
            if (copy->buffer_size_bytes != destination_span ||
                copy->buffer_offset > buffer_binding->size_bytes ||
                destination_span >
                    buffer_binding->size_bytes - copy->buffer_offset ||
                source_size > image_binding->size_bytes ||
                image_binding->allocation_offset >
                    image_allocation->size_bytes ||
                image_binding->size_bytes >
                    image_allocation->size_bytes -
                        image_binding->allocation_offset ||
                buffer_binding->allocation_offset >
                    buffer_allocation->size_bytes ||
                buffer_binding->size_bytes >
                    buffer_allocation->size_bytes -
                        buffer_binding->allocation_offset ||
                operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation = &packet.operations[operation_count++];
            operation->type =
                RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_TO_BUFFER_REGION;
            operation->source_allocation = image_binding->allocation_handle;
            operation->destination_allocation =
                buffer_binding->allocation_handle;
            operation->source_gpu_address =
                image_allocation->gpu_virtual_address +
                image_binding->allocation_offset;
            operation->destination_gpu_address =
                buffer_allocation->gpu_virtual_address +
                buffer_binding->allocation_offset + copy->buffer_offset;
            operation->size_bytes = copied_bytes;
            operation->source_width = copy->image_width;
            operation->source_height = copy->image_height;
            operation->destination_width = (uint32_t)(
                copy->buffer_row_pitch_bytes / copy->bytes_per_texel);
            operation->destination_height =
                copy->buffer_image_height_texels;
            operation->image_to_buffer_region.image_offset_x =
                copy->image_offset_x;
            operation->image_to_buffer_region.image_offset_y =
                copy->image_offset_y;
            operation->image_to_buffer_region.copy_width = copy->copy_width;
            operation->image_to_buffer_region.copy_height = copy->copy_height;
            continue;
        }
        if (command->type == RIN_GPU_BACKEND_COMMAND_BEGIN_QUERY ||
            command->type == RIN_GPU_BACKEND_COMMAND_END_QUERY ||
            command->type == RIN_GPU_BACKEND_COMMAND_RESET_QUERY) {
            uint32_t query_index;
            int query_found = 0;
            if (pending_image != 0u ||
                command->value.query.query_cookie == 0u ||
                command->value.query.query_type !=
                    RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1 ||
                command->value.query.reserved != 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            for (query_index = 0u;
                 query_index < TEST_SDK_PROCESS_QUERY_CAPACITY;
                 ++query_index) {
                const TestSdkProcessQuery* query =
                    &adapters->sdk_queries[query_index];
                if (query->active == 1u &&
                    query->token == command->value.query.query_cookie) {
                    query_found = 1;
                    break;
                }
            }
            if (!query_found) return RIN_VULKAN_PRODUCT_PROTOCOL;
            continue;
        }
        if (command->type == RIN_GPU_BACKEND_COMMAND_TRANSITION_IMAGE) {
            if (pending_image != 0u ||
                command->value.image_transition.image_cookie == 0u ||
                command->value.image_transition.transition.abi_version !=
                    RIN_GPU_ABI_VERSION ||
                command->value.image_transition.transition.struct_size !=
                    sizeof(command->value.image_transition.transition) ||
                command->value.image_transition.transition.base_mip_level != 0u ||
                command->value.image_transition.transition.mip_level_count != 1u ||
                command->value.image_transition.transition.base_array_layer != 0u ||
                command->value.image_transition.transition.array_layer_count != 1u ||
                command->value.image_transition.transition.flags != 0u ||
                !test_backend_state_to_vk_layout(
                    command->value.image_transition.transition.before_state,
                    &pending_before) ||
                !test_backend_state_to_vk_layout(
                    command->value.image_transition.transition.after_state,
                    &pending_after))
                return RIN_VULKAN_PRODUCT_UNSUPPORTED;
            pending_image = command->value.image_transition.image_cookie;
            continue;
        }
        if (operation_count >= RIN_GPU_VULKAN_TRANSFER_BATCH_MAX_OPS)
            return RIN_VULKAN_PRODUCT_LIMIT;
        operation = &packet.operations[operation_count];
        memset(operation, 0, sizeof(*operation));
        if (command->type == RIN_GPU_BACKEND_COMMAND_GRAPHICS_BARRIER_V2) {
            const TestResourceBinding* binding;
            RinGpuVulkanSoftwareAllocationV1* allocation;
            ++adapters->canonical_graphics_barrier_count;
            adapters->canonical_last_graphics_barrier =
                command->value.graphics_barrier_v2;
            if (pending_image == 0u) {
                if (command->value.graphics_barrier_v2.flags != 0u ||
                    command->value.graphics_barrier_v2.reserved != 0u)
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                operation->type = RIN_GPU_VULKAN_TRANSFER_OP_MEMORY_BARRIER;
                if (!test_backend_barrier_scope_to_vk(
                        command->value.graphics_barrier_v2.source_stage,
                        command->value.graphics_barrier_v2.source_access,
                        &operation->barrier.src_stage_mask,
                        &operation->barrier.src_access_mask) ||
                    !test_backend_barrier_scope_to_vk(
                        command->value.graphics_barrier_v2.destination_stage,
                        command->value.graphics_barrier_v2.destination_access,
                        &operation->barrier.dst_stage_mask,
                        &operation->barrier.dst_access_mask))
                    return RIN_VULKAN_PRODUCT_PROTOCOL;
                ++operation_count;
                continue;
            }
            binding = test_find_resource_binding(adapters, pending_image);
            allocation = binding
                ? test_find_allocation(adapters->software,
                                       binding->allocation_handle)
                : NULL;
            if (!binding || !allocation ||
                command->value.graphics_barrier_v2.flags != 0u ||
                command->value.graphics_barrier_v2.reserved != 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_BARRIER;
            operation->source_allocation = pending_image;
            operation->destination_allocation = binding->allocation_handle;
            operation->destination_gpu_address = allocation->gpu_virtual_address;
            operation->size_bytes = allocation->size_bytes;
            if (!test_backend_barrier_scope_to_vk(
                    command->value.graphics_barrier_v2.source_stage,
                    command->value.graphics_barrier_v2.source_access,
                    &operation->barrier.src_stage_mask,
                    &operation->barrier.src_access_mask) ||
                !test_backend_barrier_scope_to_vk(
                    command->value.graphics_barrier_v2.destination_stage,
                    command->value.graphics_barrier_v2.destination_access,
                    &operation->barrier.dst_stage_mask,
                    &operation->barrier.dst_access_mask))
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation->source_width = pending_before;
            operation->source_height = pending_after;
            operation->destination_width = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
            operation->destination_height = RIN_VK_QUEUE_FAMILY_IGNORED;
            operation->filter = RIN_VK_QUEUE_FAMILY_IGNORED;
            pending_image = 0u;
        } else if (command->type == RIN_GPU_BACKEND_COMMAND_CLEAR_IMAGE) {
            const TestResourceBinding* binding = test_find_resource_binding(
                adapters, command->value.image_clear.destination_cookie);
            RinGpuVulkanSoftwareAllocationV1* allocation = binding
                ? test_find_allocation(adapters->software,
                                       binding->allocation_handle)
                : NULL;
            if (!binding || !allocation || pending_image != 0u ||
                command->value.image_clear.mip_level != 0u ||
                command->value.image_clear.array_layer != 0u ||
                command->value.image_clear.aspects !=
                    RIN_VK_IMAGE_ASPECT_COLOR_BIT ||
                command->value.image_clear.flags != 0u ||
                command->value.image_clear.depth != 0.0f ||
                command->value.image_clear.stencil != 0u ||
                command->value.image_clear.reserved != 0u)
                return RIN_VULKAN_PRODUCT_PROTOCOL;
            operation->type = RIN_GPU_VULKAN_TRANSFER_OP_IMAGE_CLEAR;
            operation->destination_allocation = binding->allocation_handle;
            operation->destination_gpu_address = allocation->gpu_virtual_address;
            operation->size_bytes = allocation->size_bytes;
            memcpy(&operation->clear_value[0],
                   &command->value.image_clear.color_red, sizeof(float));
            memcpy(&operation->clear_value[1],
                   &command->value.image_clear.color_green, sizeof(float));
            memcpy(&operation->clear_value[2],
                   &command->value.image_clear.color_blue, sizeof(float));
            memcpy(&operation->clear_value[3],
                   &command->value.image_clear.color_alpha, sizeof(float));
        } else {
            return RIN_VULKAN_PRODUCT_UNSUPPORTED;
        }
        ++operation_count;
    }
    if (pending_image != 0u || operation_count == 0u)
        return RIN_VULKAN_PRODUCT_PROTOCOL;
    packet.op_count = operation_count;
    memset(&submission, 0, sizeof(submission));
    result = adapters->software->platform.prepare_submission(
        adapters->software->platform.context, queue_id,
        (uint64_t)(uintptr_t)&packet, &submission);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        fprintf(stderr, "V6 canonical prepare rejected command batch: %d\n",
                result);
        return result;
    }
    result = adapters->software->platform.submit(
        adapters->software->platform.context, &submission, resources,
        resource_count);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        fprintf(stderr, "V6 canonical submit rejected command batch: %d\n",
                result);
        for (index = 0u; index < operation_count; ++index) {
            const RinGpuVulkanTransferOpV2* rejected =
                &packet.operations[index];
            fprintf(stderr,
                    "  op[%u] type=%u src=%llu dst=%llu src_va=%llu "
                    "dst_va=%llu size=%llu dims=%u,%u,%u,%u filter=%u "
                    "samples=%u\n",
                    index, rejected->type,
                    (unsigned long long)rejected->source_allocation,
                    (unsigned long long)rejected->destination_allocation,
                    (unsigned long long)rejected->source_gpu_address,
                    (unsigned long long)rejected->destination_gpu_address,
                    (unsigned long long)rejected->size_bytes,
                    rejected->source_width, rejected->source_height,
                    rejected->destination_width,
                    rejected->destination_height, rejected->filter,
                    rejected->sample_count);
        }
        for (index = 0u; index < resource_count; ++index)
            fprintf(stderr, "  resource[%u] handle=%llu access=%u\n", index,
                    (unsigned long long)resources[index].allocation_handle,
                    resources[index].required_gpu_access);
        return result;
    }
    if (defer_completion != 0u) {
        const uint32_t report_index =
            (adapters->software->report_tail +
             RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS - 1u) %
            RIN_GPU_VULKAN_SOFTWARE_MAX_REPORTS;
        adapters->software->completed_values[queue_id] =
            previous_completed_value;
        adapters->software->reports[report_index].completed_values[queue_id] =
            previous_completed_value;
        adapters->defer_next_process_completion = 0u;
        adapters->deferred_completion_queue_id = queue_id;
        adapters->deferred_completion_value = submission.completion_value;
    }
    memset(receipt_out, 0, sizeof(*receipt_out));
    receipt_out->struct_size = sizeof(*receipt_out);
    receipt_out->version =
        RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION;
    receipt_out->queue_id = submission.queue_id;
    receipt_out->sequence = submission.sequence;
    receipt_out->completion_value = submission.completion_value;
    receipt_out->device_epoch = submission.device_epoch;
    receipt_out->iommu_map_generation = submission.iommu_map_generation;
    ++adapters->canonical_submit_calls;
    adapters->canonical_command_count = command_count;
    return RIN_VULKAN_PRODUCT_OK;
}

RinResult rin_gpu_process_queue_bind_v1(
        uint64_t device_id, uint64_t device_generation, uint32_t queue_id,
        RinGpuProcessQueueV1* queue_out) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || !adapters->software || !queue_out ||
        device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        queue_id >= adapters->software->queue_count ||
        queue_id >= RIN_GPU_PROCESS_MAX_QUEUES_V1)
        return RIN_ERROR_INVALID_ARGUMENT;
    *queue_out = 0u;
    for (index = 0u; index < RIN_GPU_PROCESS_MAX_QUEUES_V1; ++index) {
        TestSdkProcessQueue* queue = &adapters->sdk_queues[index];
        if (queue->active != 0u && queue->queue_id == queue_id)
            return RIN_ERROR_BUSY;
        if (queue->active == 0u) break;
    }
    if (index == RIN_GPU_PROCESS_MAX_QUEUES_V1)
        return RIN_ERROR_TABLE_FULL;
    adapters->sdk_queues[index].queue_id = queue_id;
    adapters->sdk_queues[index].token =
        UINT64_C(0xA500000000000000) | ((uint64_t)queue_id + 1u);
    adapters->sdk_queues[index].active = 1u;
    *queue_out = adapters->sdk_queues[index].token;
    ++adapters->sdk_queue_bind_calls;
    ++adapters->sdk_active_queue_count;
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_queue_release_v1(RinGpuProcessQueueV1 token) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || token == 0u) return RIN_ERROR_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_GPU_PROCESS_MAX_QUEUES_V1; ++index) {
        TestSdkProcessQueue* queue = &adapters->sdk_queues[index];
        if (queue->active == 1u && queue->token == token) {
            queue->active = 0u;
            queue->token = 0u;
            ++adapters->sdk_queue_release_calls;
            --adapters->sdk_active_queue_count;
            return RIN_SUCCESS;
        }
    }
    return RIN_ERROR_STALE_HANDLE;
}

RinResult rin_gpu_process_shader_module_create_v1(
        uint64_t device_id, uint64_t device_generation,
        uint64_t shader_module_token, const void* rin_shader_ir,
        uint64_t shader_ir_size) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    const RinShaderHeaderV1* header =
        (const RinShaderHeaderV1*)rin_shader_ir;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        shader_module_token == 0u || !rin_shader_ir ||
        shader_ir_size < sizeof(RinShaderHeaderV1) ||
        shader_ir_size > RIN_GPU_PROCESS_SHADER_MAX_BYTES_V1 ||
        header->magic != RIN_SHADER_MAGIC ||
        header->version != RIN_SHADER_IR_VERSION ||
        header->header_size != sizeof(RinShaderHeaderV1) ||
        header->total_size != shader_ir_size ||
        header->stage != RIN_SHADER_STAGE_COMPUTE ||
        header->resource_count > RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1)
        return RIN_ERROR_INVALID_ARGUMENT;
    adapters->sdk_last_shader_module_token = shader_module_token;
    adapters->sdk_last_shader_resource_count = header->resource_count;
    ++adapters->sdk_shader_module_create_calls;
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_compute_pipeline_create_v1(
        uint64_t device_id, uint64_t device_generation,
        uint64_t pipeline_token, uint64_t shader_module_token) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        pipeline_token == 0u || shader_module_token == 0u ||
        shader_module_token != adapters->sdk_last_shader_module_token ||
        pipeline_token == shader_module_token)
        return RIN_ERROR_INVALID_ARGUMENT;
    adapters->sdk_last_pipeline_token = pipeline_token;
    adapters->sdk_last_pipeline_shader_token = shader_module_token;
    ++adapters->sdk_compute_pipeline_create_calls;
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_resource_bind_v1(
    uint64_t device_id, uint64_t device_generation,
    const RinGpuProcessResourceBindingV1* binding) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        !binding || binding->struct_size != sizeof(*binding) ||
        binding->version != RIN_GPU_PROCESS_RESOURCE_BINDING_VERSION_V1 ||
        binding->resource_cookie == 0u || binding->allocation == 0u ||
        binding->size_bytes == 0u || binding->required_gpu_access == 0u ||
        (binding->required_gpu_access &
         ~(RIN_GPU_ALLOCATION_GPU_READ | RIN_GPU_ALLOCATION_GPU_WRITE)) != 0u ||
        binding->reserved != 0u ||
        binding->allocation_offset > UINT64_MAX - binding->size_bytes)
        return RIN_ERROR_INVALID_ARGUMENT;
    ++adapters->sdk_resource_bind_calls;
    for (index = 0u; index <
         RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1; ++index) {
        TestSdkProcessResource* resource = &adapters->sdk_resources[index];
        if (resource->active != 0u &&
            resource->binding.resource_cookie == binding->resource_cookie)
            return RIN_ERROR_BUSY;
        if (resource->active == 0u) {
            resource->binding = *binding;
            resource->active = 1u;
            adapters->sdk_last_resource_binding = *binding;
            return RIN_SUCCESS;
        }
    }
    return RIN_ERROR_TABLE_FULL;
}

RinResult rin_gpu_process_resource_unbind_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t resource_cookie) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        resource_cookie == 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    ++adapters->sdk_resource_unbind_calls;
    for (index = 0u; index <
         RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1; ++index) {
        TestSdkProcessResource* resource = &adapters->sdk_resources[index];
        if (resource->active != 0u &&
            resource->binding.resource_cookie == resource_cookie) {
            memset(resource, 0, sizeof(*resource));
            return RIN_SUCCESS;
        }
    }
    return RIN_ERROR_STALE_HANDLE;
}

RinResult rin_gpu_process_compute_bind_group_create_v1(
    uint64_t device_id, uint64_t device_generation,
    uint64_t bind_group_token, uint64_t pipeline_token,
    const RinGpuProcessComputeBindingV1* bindings, uint32_t binding_count) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        bind_group_token == 0u || pipeline_token == 0u ||
        pipeline_token != adapters->sdk_last_pipeline_token || !bindings ||
        binding_count == 0u ||
        binding_count > RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1 ||
        binding_count != adapters->sdk_last_shader_resource_count)
        return RIN_ERROR_INVALID_ARGUMENT;
    for (index = 0u; index < binding_count; ++index) {
        uint32_t resource_index;
        int found = 0;
        const RinGpuProcessComputeBindingV1* binding = &bindings[index];
        if (binding->struct_size != sizeof(*binding) ||
            binding->version != RIN_GPU_PROCESS_COMPUTE_BINDING_VERSION_V1 ||
            binding->resource_cookie == 0u || binding->size_bytes == 0u ||
            binding->offset > UINT64_MAX - binding->size_bytes ||
            binding->access == 0u || binding->reserved != 0u)
            return RIN_ERROR_INVALID_ARGUMENT;
        for (resource_index = 0u; resource_index <
             RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1; ++resource_index)
            if (adapters->sdk_resources[resource_index].active != 0u &&
                adapters->sdk_resources[resource_index].binding.resource_cookie ==
                    binding->resource_cookie)
                found = 1;
        if (!found) return RIN_ERROR_STALE_HANDLE;
    }
    adapters->sdk_last_bind_group_token = bind_group_token;
    adapters->sdk_last_bind_group_pipeline_token = pipeline_token;
    adapters->sdk_last_bind_group_count = binding_count;
    memcpy(adapters->sdk_last_bind_group_bindings, bindings,
           binding_count * sizeof(*bindings));
    ++adapters->sdk_bind_group_create_calls;
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_object_destroy_v1(
        uint64_t device_id, uint64_t device_generation,
        uint64_t object_token) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        object_token == 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    adapters->sdk_destroy_tokens[0] = adapters->sdk_destroy_tokens[1];
    adapters->sdk_destroy_tokens[1] = object_token;
    ++adapters->sdk_object_destroy_calls;
    if (object_token == adapters->sdk_last_bind_group_token &&
        adapters->sdk_bind_group_destroy_busy_attempts != 0u) {
        --adapters->sdk_bind_group_destroy_busy_attempts;
        return RIN_ERROR_BUSY;
    }
    for (index = 0u; index < TEST_SDK_PROCESS_QUERY_CAPACITY; ++index) {
        if (adapters->sdk_queries[index].active == 1u &&
            adapters->sdk_queries[index].token == object_token) {
            memset(&adapters->sdk_queries[index], 0,
                   sizeof(adapters->sdk_queries[index]));
            break;
        }
    }
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_query_create_v1(
        uint64_t device_id, uint64_t device_generation, uint64_t query_token,
        const RinGpuProcessQueryDescV1* descriptor) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    TestSdkProcessQuery* free_query = NULL;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        query_token == 0u || !descriptor ||
        descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_QUERY_DESC_VERSION_V1 ||
        descriptor->query_type != RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1 ||
        descriptor->flags != 0u || descriptor->reserved0 != 0u ||
        descriptor->reserved1 != 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    for (index = 0u; index < TEST_SDK_PROCESS_QUERY_CAPACITY; ++index) {
        TestSdkProcessQuery* query = &adapters->sdk_queries[index];
        if (query->active == 1u && query->token == query_token)
            return RIN_ERROR_BUSY;
        if (!free_query && query->active == 0u) free_query = query;
    }
    if (!free_query) return RIN_ERROR_TABLE_FULL;
    memset(free_query, 0, sizeof(*free_query));
    free_query->active = 1u;
    free_query->token = query_token;
    free_query->descriptor = *descriptor;
    free_query->result.struct_size = sizeof(free_query->result);
    free_query->result.version = RIN_GPU_PROCESS_QUERY_RESULT_VERSION_V1;
    free_query->result.query_type = descriptor->query_type;
    adapters->sdk_last_query_token = query_token;
    ++adapters->sdk_query_create_calls;
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_query_result_v1(
        uint64_t device_id, uint64_t device_generation, uint64_t query_token,
        RinGpuProcessQueryResultV1* result_out) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    uint32_t index;
    if (!adapters || device_id != TEST_PROCESS_SUBMIT_DEVICE_ID ||
        device_generation != TEST_PROCESS_SUBMIT_DEVICE_GENERATION ||
        query_token == 0u || !result_out ||
        result_out->struct_size != sizeof(*result_out) ||
        result_out->version != RIN_GPU_PROCESS_QUERY_RESULT_VERSION_V1)
        return RIN_ERROR_INVALID_ARGUMENT;
    for (index = 0u; index < TEST_SDK_PROCESS_QUERY_CAPACITY; ++index) {
        const TestSdkProcessQuery* query = &adapters->sdk_queries[index];
        if (query->active == 1u && query->token == query_token) {
            *result_out = query->result;
            ++adapters->sdk_query_result_calls;
            return RIN_SUCCESS;
        }
    }
    return RIN_ERROR_STALE_HANDLE;
}

static RinResult test_product_result_to_sdk(int result) {
    switch (result) {
        case RIN_VULKAN_PRODUCT_OK:
            return RIN_SUCCESS;
        case RIN_VULKAN_PRODUCT_INVALID_ARGUMENT:
            return RIN_ERROR_INVALID_ARGUMENT;
        case RIN_VULKAN_PRODUCT_BUSY:
            return RIN_ERROR_BUSY;
        case RIN_VULKAN_PRODUCT_LIMIT:
            return RIN_ERROR_LIMIT_EXCEEDED;
        case RIN_VULKAN_PRODUCT_NO_SPACE:
            return RIN_ERROR_TABLE_FULL;
        case RIN_VULKAN_PRODUCT_UNSUPPORTED:
            return RIN_ERROR_NOT_SUPPORTED;
        case RIN_VULKAN_PRODUCT_LOST:
            return RIN_ERROR_STALE_HANDLE;
        case RIN_VULKAN_PRODUCT_PROTOCOL:
            return RIN_ERROR_ABI_MISMATCH;
        default:
            return RIN_ERROR_IO;
    }
}

RinResult rin_gpu_process_submit_v1(
        RinGpuProcessQueueV1 token,
        const RinGpuProcessSubmitDescV1* descriptor,
        RinGpuProcessSubmitReceiptV1* receipt_out) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    RinVulkanProductResourceV1 product_resources[
        RIN_GPU_PROCESS_MAX_RESOURCES_V1];
    RinVulkanProductProcessSubmitReceiptV1 product_receipt;
    const RinGpuBackendCommandV1* commands;
    const RinGpuProcessResourceV1* sdk_resources = NULL;
    uint64_t expected_command_bytes;
    uint32_t queue_id = UINT32_MAX;
    uint32_t index;
    int result;
    if (!adapters || !descriptor || !receipt_out)
        return RIN_ERROR_INVALID_ARGUMENT;
    ++adapters->sdk_process_submit_calls;
    if (descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V1 ||
        descriptor->reserved != 0u || descriptor->command_count == 0u ||
        descriptor->command_count > RIN_GPU_PROCESS_MAX_COMMANDS_V1 ||
        descriptor->command_record_size !=
            RIN_GPU_PROCESS_COMMAND_RECORD_SIZE_V1 ||
        descriptor->command_record_size != sizeof(RinGpuBackendCommandV1) ||
        descriptor->commands.address == 0u ||
        descriptor->resource_count > RIN_GPU_PROCESS_MAX_RESOURCES_V1)
        return RIN_ERROR_INVALID_ARGUMENT;
    expected_command_bytes = (uint64_t)descriptor->command_count *
                             RIN_GPU_PROCESS_COMMAND_RECORD_SIZE_V1;
    if (descriptor->commands.size != expected_command_bytes)
        return RIN_ERROR_ABI_MISMATCH;
    if (descriptor->resource_count == 0u) {
        if (descriptor->resources.address != 0u ||
            descriptor->resources.size != 0u)
            return RIN_ERROR_ABI_MISMATCH;
    } else {
        if (descriptor->resources.address == 0u ||
            descriptor->resources.size !=
                (uint64_t)descriptor->resource_count *
                    sizeof(RinGpuProcessResourceV1))
            return RIN_ERROR_ABI_MISMATCH;
        sdk_resources = (const RinGpuProcessResourceV1*)
            (uintptr_t)descriptor->resources.address;
    }
    for (index = 0u; index < RIN_GPU_PROCESS_MAX_QUEUES_V1; ++index) {
        const TestSdkProcessQueue* queue = &adapters->sdk_queues[index];
        if (queue->active == 1u && queue->token == token) {
            queue_id = queue->queue_id;
            break;
        }
    }
    if (queue_id == UINT32_MAX) return RIN_ERROR_STALE_HANDLE;
    memset(product_resources, 0, sizeof(product_resources));
    for (index = 0u; index < descriptor->resource_count; ++index) {
        const RinGpuProcessResourceV1* source = &sdk_resources[index];
        if (source->allocation == 0u || source->reserved != 0u ||
            source->required_gpu_access == 0u ||
            (source->required_gpu_access &
             ~(RIN_GPU_ALLOCATION_GPU_READ |
               RIN_GPU_ALLOCATION_GPU_WRITE)) != 0u)
            return RIN_ERROR_INVALID_ARGUMENT;
        product_resources[index].allocation_handle = source->allocation;
        product_resources[index].required_gpu_access =
            source->required_gpu_access;
    }
    commands = (const RinGpuBackendCommandV1*)
        (uintptr_t)descriptor->commands.address;
    adapters->sdk_last_command_count = 0u;
    memset(adapters->sdk_last_commands, 0,
           sizeof(adapters->sdk_last_commands));
    if (descriptor->command_count <= TEST_SDK_PROCESS_COMMAND_CAPTURE) {
        adapters->sdk_last_command_count = descriptor->command_count;
        memcpy(adapters->sdk_last_commands, commands,
               descriptor->command_count * sizeof(*commands));
    }
    memset(&product_receipt, 0, sizeof(product_receipt));
    result = adapters->software_v6.submit_process_commands(
        adapters->software_v6.context, queue_id, commands,
        descriptor->command_count, product_resources,
        descriptor->resource_count, &product_receipt);
    if (result != RIN_VULKAN_PRODUCT_OK)
        return test_product_result_to_sdk(result);
    if (product_receipt.struct_size != sizeof(product_receipt) ||
        product_receipt.version !=
            RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION ||
        product_receipt.queue_id != queue_id ||
        product_receipt.sequence == 0u ||
        product_receipt.completion_value == 0u ||
        product_receipt.device_epoch == 0u ||
        product_receipt.iommu_map_generation == 0u)
        return RIN_ERROR_ABI_MISMATCH;
    memset(receipt_out, 0, sizeof(*receipt_out));
    receipt_out->struct_size = sizeof(*receipt_out);
    receipt_out->version = RIN_GPU_PROCESS_SUBMIT_RECEIPT_VERSION_V1;
    receipt_out->queue_id = product_receipt.queue_id;
    receipt_out->sequence = product_receipt.sequence;
    receipt_out->completion_value = product_receipt.completion_value;
    receipt_out->device_epoch = product_receipt.device_epoch;
    receipt_out->iommu_map_generation =
        product_receipt.iommu_map_generation;
    return RIN_SUCCESS;
}

static RinResult test_process_submit_add_v2_resource(
        TestProductAdapters* adapters, uint64_t resource_cookie,
        uint64_t resource_offset, uint64_t resource_size,
        uint32_t required_gpu_access, int range_limited, int optional,
        RinVulkanProductResourceV1* resources, uint32_t* resource_count) {
    const uint32_t known_access = RIN_GPU_ALLOCATION_GPU_READ |
                                  RIN_GPU_ALLOCATION_GPU_WRITE;
    const RinGpuProcessResourceBindingV1* sdk_binding = NULL;
    const TestResourceBinding* product_binding;
    uint32_t lease_access;
    uint32_t product_access = 0u;
    uint32_t index;

    if (!adapters || !resources || !resource_count ||
        *resource_count > RIN_GPU_PROCESS_MAX_RESOURCES_V1 ||
        (required_gpu_access & ~known_access) != 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    if (resource_cookie == 0u)
        return optional ? RIN_SUCCESS : RIN_ERROR_INVALID_ARGUMENT;
    for (index = 0u; index < RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1;
         ++index) {
        const TestSdkProcessResource* resource =
            &adapters->sdk_resources[index];
        if (resource->active != 0u &&
            resource->binding.resource_cookie == resource_cookie) {
            sdk_binding = &resource->binding;
            break;
        }
    }
    /* Native-window resources enter this fixture through the authenticated
     * V5 product bind callback; process-SDK tests additionally populate the
     * syscall-shaped registry above. The product binding is the common
     * capability table consumed by both paths. */
    product_binding = test_find_resource_binding(adapters, resource_cookie);
    if (!product_binding) return RIN_ERROR_STALE_HANDLE;
    if (product_binding->resource_cookie != resource_cookie ||
        product_binding->allocation_handle == 0u ||
        product_binding->size_bytes == 0u ||
        product_binding->required_gpu_access == 0u ||
        (product_binding->required_gpu_access & ~known_access) != 0u ||
        product_binding->allocation_offset >
            UINT64_MAX - product_binding->size_bytes)
        return RIN_ERROR_ABI_MISMATCH;
    if ((required_gpu_access &
         ~product_binding->required_gpu_access) != 0u) {
        fprintf(stderr,
                "V2 submit resource rights mismatch: cookie=%llu "
                "requested=%u bound=%u\n",
                (unsigned long long)resource_cookie, required_gpu_access,
                product_binding->required_gpu_access);
        return RIN_ERROR_ACCESS_DENIED;
    }
    if (range_limited &&
        (resource_size == 0u ||
         resource_offset > product_binding->size_bytes ||
         resource_size > product_binding->size_bytes - resource_offset))
        return RIN_ERROR_INVALID_ARGUMENT;

    /* When the syscall-shaped SDK fixture also owns this cookie, require it
     * to match the product owner exactly. V2 never trusts an allocation handle
     * supplied by the command descriptor. */
    if (sdk_binding &&
        (sdk_binding->struct_size != sizeof(*sdk_binding) ||
         sdk_binding->version !=
             RIN_GPU_PROCESS_RESOURCE_BINDING_VERSION_V1 ||
         sdk_binding->resource_cookie != resource_cookie ||
         sdk_binding->allocation != product_binding->allocation_handle ||
         sdk_binding->allocation_offset !=
             product_binding->allocation_offset ||
         sdk_binding->size_bytes != product_binding->size_bytes ||
         sdk_binding->required_gpu_access !=
             product_binding->required_gpu_access ||
         sdk_binding->reserved != 0u))
        return RIN_ERROR_ABI_MISMATCH;

    lease_access = required_gpu_access != 0u
                       ? required_gpu_access
                       : product_binding->required_gpu_access;
    if ((lease_access & RIN_GPU_ALLOCATION_GPU_READ) != 0u)
        product_access |= RIN_VULKAN_PRODUCT_MEMORY_GPU_READ;
    if ((lease_access & RIN_GPU_ALLOCATION_GPU_WRITE) != 0u)
        product_access |= RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE;
    for (index = 0u; index < *resource_count; ++index) {
        if (resources[index].allocation_handle ==
            product_binding->allocation_handle) {
            resources[index].required_gpu_access |= product_access;
            return RIN_SUCCESS;
        }
    }
    if (*resource_count >= RIN_GPU_PROCESS_MAX_RESOURCES_V1)
        return RIN_ERROR_LIMIT_EXCEEDED;
    resources[*resource_count].allocation_handle =
        product_binding->allocation_handle;
    resources[*resource_count].required_gpu_access = product_access;
    ++*resource_count;
    return RIN_SUCCESS;
}

static RinResult test_process_submit_derive_v2_resources(
        TestProductAdapters* adapters, const RinGpuBackendCommandV1* commands,
        uint32_t command_count, RinVulkanProductResourceV1* resources,
        uint32_t* resource_count) {
    const uint32_t read = RIN_GPU_ALLOCATION_GPU_READ;
    const uint32_t write = RIN_GPU_ALLOCATION_GPU_WRITE;
    const uint32_t read_write = read | write;
    uint32_t command_index;
    RinResult result;

    if (!adapters || !commands || command_count == 0u || !resources ||
        !resource_count)
        return RIN_ERROR_INVALID_ARGUMENT;
    *resource_count = 0u;
    memset(resources, 0, sizeof(*resources) *
                              RIN_GPU_PROCESS_MAX_RESOURCES_V1);
#define TEST_V2_REQUIRE_COOKIE(cookie, access)                               \
    do {                                                                      \
        result = test_process_submit_add_v2_resource(                         \
            adapters, (cookie), 0u, 0u, (access), 0, 0, resources,            \
            resource_count);                                                  \
        if (result != RIN_SUCCESS) {                                           \
            fprintf(stderr, "V2 submit resolve failed: command=%u type=%u "  \
                    "cookie=%llu access=%u result=%d\n", command_index,    \
                    commands[command_index].type,                            \
                    (unsigned long long)(cookie), (unsigned)(access), result);\
            return result;                                                    \
        }                                                                     \
    } while (0)
#define TEST_V2_OPTIONAL_COOKIE(cookie, access)                              \
    do {                                                                      \
        result = test_process_submit_add_v2_resource(                         \
            adapters, (cookie), 0u, 0u, (access), 0, 1, resources,            \
            resource_count);                                                  \
        if (result != RIN_SUCCESS) {                                           \
            fprintf(stderr, "V2 submit resolve failed: command=%u type=%u "  \
                    "optional_cookie=%llu access=%u result=%d\n",          \
                    command_index, commands[command_index].type,             \
                    (unsigned long long)(cookie), (unsigned)(access), result);\
            return result;                                                    \
        }                                                                     \
    } while (0)
#define TEST_V2_REQUIRE_RANGE(cookie, offset, size, access)                  \
    do {                                                                      \
        result = test_process_submit_add_v2_resource(                         \
            adapters, (cookie), (offset), (size), (access), 1, 0, resources,  \
            resource_count);                                                  \
        if (result != RIN_SUCCESS) {                                           \
            fprintf(stderr, "V2 submit resolve failed: command=%u type=%u "  \
                    "range_cookie=%llu offset=%llu size=%llu access=%u "     \
                    "result=%d\n", command_index,                            \
                    commands[command_index].type,                            \
                    (unsigned long long)(cookie),                            \
                    (unsigned long long)(offset),                            \
                    (unsigned long long)(size), (unsigned)(access), result);  \
            return result;                                                    \
        }                                                                     \
    } while (0)
#define TEST_V2_VERTEX_BINDINGS(bindings, count)                             \
    do {                                                                      \
        if ((count) > RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS)                     \
            return RIN_ERROR_INVALID_ARGUMENT;                                \
        for (uint32_t binding_index = 0u; binding_index < (count);            \
             ++binding_index) {                                               \
            if ((bindings)[binding_index].reserved != 0u)                    \
                return RIN_ERROR_ABI_MISMATCH;                                \
            TEST_V2_REQUIRE_COOKIE(                                           \
                (bindings)[binding_index].buffer_cookie, read);               \
        }                                                                     \
    } while (0)

    for (command_index = 0u; command_index < command_count; ++command_index) {
        const RinGpuBackendCommandV1* command = &commands[command_index];
        if (command->reserved != 0u) return RIN_ERROR_ABI_MISMATCH;
        switch (command->type) {
        case RIN_GPU_BACKEND_COMMAND_COPY_BUFFER:
            TEST_V2_REQUIRE_RANGE(command->value.buffer_copy.source_cookie,
                                  command->value.buffer_copy.source_offset,
                                  command->value.buffer_copy.size_bytes, read);
            TEST_V2_REQUIRE_RANGE(
                command->value.buffer_copy.destination_cookie,
                command->value.buffer_copy.destination_offset,
                command->value.buffer_copy.size_bytes, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_COPY_IMAGE:
            TEST_V2_REQUIRE_COOKIE(command->value.image_copy.source_cookie,
                                   read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_copy.destination_cookie, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_COPY_IMAGE_TO_BUFFER:
            TEST_V2_REQUIRE_RANGE(
                command->value.image_to_buffer_copy.buffer_cookie,
                command->value.image_to_buffer_copy.buffer_offset,
                command->value.image_to_buffer_copy.buffer_size_bytes, write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_to_buffer_copy.image_cookie, read);
            break;
        case RIN_GPU_BACKEND_COMMAND_COPY_BUFFER_TO_IMAGE:
            TEST_V2_REQUIRE_RANGE(
                command->value.buffer_to_image_copy.buffer_cookie,
                command->value.buffer_to_image_copy.buffer_offset,
                command->value.buffer_to_image_copy.buffer_size_bytes, read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.buffer_to_image_copy.image_cookie, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_TRANSITION_IMAGE:
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_transition.image_cookie, 0u);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw.color_target_cookie, read_write);
            break;
        case RIN_GPU_BACKEND_COMMAND_DISPATCH:
            break;
        case RIN_GPU_BACKEND_COMMAND_DISPATCH_INDIRECT:
            TEST_V2_REQUIRE_COOKIE(
                command->value.dispatch_indirect.indirect_buffer_cookie, read);
            break;
        case RIN_GPU_BACKEND_COMMAND_BEGIN_RENDER_PASS:
            TEST_V2_REQUIRE_COOKIE(
                command->value.render_pass_begin.color_target_cookie,
                read_write);
            break;
        case RIN_GPU_BACKEND_COMMAND_END_RENDER_PASS:
        case RIN_GPU_BACKEND_COMMAND_COMPUTE_BARRIER:
        case RIN_GPU_BACKEND_COMMAND_GRAPHICS_BARRIER:
        case RIN_GPU_BACKEND_COMMAND_SET_RASTER_STATE:
        case RIN_GPU_BACKEND_COMMAND_COMPUTE_BARRIER_V2:
        case RIN_GPU_BACKEND_COMMAND_GRAPHICS_BARRIER_V2:
        case RIN_GPU_BACKEND_COMMAND_SET_PUSH_CONSTANTS:
        case RIN_GPU_BACKEND_COMMAND_BEGIN_QUERY:
        case RIN_GPU_BACKEND_COMMAND_END_QUERY:
        case RIN_GPU_BACKEND_COMMAND_RESET_QUERY:
            break;
        case RIN_GPU_BACKEND_COMMAND_PRESENT:
            TEST_V2_REQUIRE_COOKIE(command->value.present.image_cookie, read);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_VERTICES:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_vertices.color_target_cookie, read_write);
            TEST_V2_OPTIONAL_COOKIE(
                command->value.draw_vertices.vertex_buffer_cookie, read);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_INDEXED:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed.color_target_cookie, read_write);
            TEST_V2_OPTIONAL_COOKIE(
                command->value.draw_indexed.vertex_buffer_cookie, read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed.index_buffer_cookie, read);
            break;
        case RIN_GPU_BACKEND_COMMAND_BEGIN_RENDER_PASS_DEPTH:
            TEST_V2_REQUIRE_COOKIE(
                command->value.render_pass_depth_begin.color_target_cookie,
                read_write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.render_pass_depth_begin.depth_target_cookie,
                read_write);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_INDEXED_BASE_VERTEX:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_base_vertex.color_target_cookie,
                read_write);
            TEST_V2_OPTIONAL_COOKIE(
                command->value.draw_indexed_base_vertex.vertex_buffer_cookie,
                read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_base_vertex.index_buffer_cookie,
                read);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_VERTICES_V2:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_vertices_v2.color_target_cookie,
                read_write);
            TEST_V2_VERTEX_BINDINGS(
                command->value.draw_vertices_v2.vertex_buffers,
                command->value.draw_vertices_v2.vertex_binding_count);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_INDEXED_V2:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_v2.color_target_cookie,
                read_write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_v2.index_buffer_cookie, read);
            TEST_V2_VERTEX_BINDINGS(
                command->value.draw_indexed_v2.vertex_buffers,
                command->value.draw_indexed_v2.vertex_binding_count);
            break;
        case RIN_GPU_BACKEND_COMMAND_BEGIN_RENDER_PASS_DEPTH_STENCIL:
            TEST_V2_REQUIRE_COOKIE(
                command->value.render_pass_depth_stencil_begin
                    .color_target_cookie,
                read_write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.render_pass_depth_stencil_begin
                    .depth_target_cookie,
                read_write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.render_pass_depth_stencil_begin
                    .stencil_target_cookie,
                read_write);
            break;
        case RIN_GPU_BACKEND_COMMAND_BEGIN_RENDER_PASS_MRT: {
            const RinGpuBackendRenderPassMrtBeginV1* mrt =
                &command->value.render_pass_mrt_begin;
            if ((mrt->active_color_mask & ~UINT32_C(0x0f)) != 0u)
                return RIN_ERROR_INVALID_ARGUMENT;
            for (uint32_t target = 0u; target < RIN_GPU_MAX_COLOR_TARGETS;
                 ++target) {
                if ((mrt->active_color_mask & (UINT32_C(1) << target)) != 0u)
                    TEST_V2_REQUIRE_COOKIE(mrt->color_target_cookies[target],
                                           read_write);
            }
            TEST_V2_OPTIONAL_COOKIE(mrt->depth_target_cookie, read_write);
            TEST_V2_OPTIONAL_COOKIE(mrt->stencil_target_cookie, read_write);
            break;
        }
        case RIN_GPU_BACKEND_COMMAND_CLEAR_BUFFER:
            TEST_V2_REQUIRE_RANGE(
                command->value.buffer_clear.destination_cookie,
                command->value.buffer_clear.offset,
                command->value.buffer_clear.size_bytes, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_CLEAR_IMAGE:
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_clear.destination_cookie, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_BLIT_IMAGE:
            TEST_V2_REQUIRE_COOKIE(command->value.image_blit.source_cookie,
                                   read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_blit.destination_cookie, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_TRANSFER_IMAGE_OWNERSHIP:
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_ownership_transfer.image_cookie, 0u);
            break;
        case RIN_GPU_BACKEND_COMMAND_RESOLVE_IMAGE:
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_resolve.source_cookie, read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.image_resolve.destination_cookie, write);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_INDIRECT:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indirect.color_target_cookie, read_write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indirect.indirect_buffer_cookie, read);
            TEST_V2_VERTEX_BINDINGS(
                command->value.draw_indirect.vertex_buffers,
                command->value.draw_indirect.vertex_binding_count);
            break;
        case RIN_GPU_BACKEND_COMMAND_DRAW_INDEXED_INDIRECT:
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_indirect.color_target_cookie,
                read_write);
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_indirect.index_buffer_cookie, read);
            TEST_V2_REQUIRE_COOKIE(
                command->value.draw_indexed_indirect.indirect_buffer_cookie,
                read);
            TEST_V2_VERTEX_BINDINGS(
                command->value.draw_indexed_indirect.vertex_buffers,
                command->value.draw_indexed_indirect.vertex_binding_count);
            break;
        default:
            return RIN_ERROR_NOT_SUPPORTED;
        }
    }
#undef TEST_V2_VERTEX_BINDINGS
#undef TEST_V2_REQUIRE_RANGE
#undef TEST_V2_OPTIONAL_COOKIE
#undef TEST_V2_REQUIRE_COOKIE
    return RIN_SUCCESS;
}

RinResult rin_gpu_process_submit_v2(
        RinGpuProcessQueueV1 token,
        const RinGpuProcessSubmitDescV2* descriptor,
        RinGpuProcessSubmitReceiptV1* receipt_out) {
    TestProductAdapters* adapters = g_process_submit_adapters;
    RinVulkanProductResourceV1 product_resources[
        RIN_GPU_PROCESS_MAX_RESOURCES_V1];
    RinVulkanProductProcessSubmitReceiptV1 product_receipt;
    RinGpuBackendCommandV1* commands;
    uint64_t expected_command_bytes;
    uint32_t resource_count = 0u;
    uint32_t queue_id = UINT32_MAX;
    uint32_t index;
    int result;

    if (!adapters || !descriptor || !receipt_out)
        return RIN_ERROR_INVALID_ARGUMENT;
    ++adapters->sdk_process_submit_calls;
    if (descriptor->struct_size != sizeof(*descriptor) ||
        descriptor->version != RIN_GPU_PROCESS_SUBMIT_DESC_VERSION_V2 ||
        descriptor->reserved[0] != 0u || descriptor->reserved[1] != 0u ||
        descriptor->command_count == 0u ||
        descriptor->command_count > RIN_GPU_PROCESS_MAX_COMMANDS_V1 ||
        descriptor->command_record_size !=
            RIN_GPU_PROCESS_COMMAND_RECORD_SIZE_V1 ||
        descriptor->command_record_size != sizeof(RinGpuBackendCommandV1) ||
        descriptor->commands.address == 0u)
        return RIN_ERROR_INVALID_ARGUMENT;
    expected_command_bytes =
        (uint64_t)descriptor->command_count *
        RIN_GPU_PROCESS_COMMAND_RECORD_SIZE_V1;
    if (descriptor->commands.size != expected_command_bytes ||
        expected_command_bytes > SIZE_MAX)
        return RIN_ERROR_ABI_MISMATCH;
    commands = (RinGpuBackendCommandV1*)malloc(
        (size_t)expected_command_bytes);
    if (!commands) return RIN_ERROR_LIMIT_EXCEEDED;
    memcpy(commands, (const void*)(uintptr_t)descriptor->commands.address,
           (size_t)expected_command_bytes);

    for (index = 0u; index < RIN_GPU_PROCESS_MAX_QUEUES_V1; ++index) {
        const TestSdkProcessQueue* queue = &adapters->sdk_queues[index];
        if (queue->active == 1u && queue->token == token) {
            queue_id = queue->queue_id;
            break;
        }
    }
    if (queue_id == UINT32_MAX) {
        result = RIN_ERROR_STALE_HANDLE;
        goto done;
    }
    result = test_process_submit_derive_v2_resources(
        adapters, commands, descriptor->command_count, product_resources,
        &resource_count);
    if (result != RIN_SUCCESS) goto done;
    if (!adapters->software_v6.submit_process_commands) {
        result = RIN_ERROR_NOT_SUPPORTED;
        goto done;
    }

    adapters->sdk_last_command_count = 0u;
    memset(adapters->sdk_last_commands, 0,
           sizeof(adapters->sdk_last_commands));
    if (descriptor->command_count <= TEST_SDK_PROCESS_COMMAND_CAPTURE) {
        adapters->sdk_last_command_count = descriptor->command_count;
        memcpy(adapters->sdk_last_commands, commands,
               descriptor->command_count * sizeof(*commands));
    }
    memset(&product_receipt, 0, sizeof(product_receipt));
    result = adapters->software_v6.submit_process_commands(
        adapters->software_v6.context, queue_id, commands,
        descriptor->command_count, product_resources, resource_count,
        &product_receipt);
    if (result != RIN_VULKAN_PRODUCT_OK) {
        result = test_product_result_to_sdk(result);
        goto done;
    }
    if (product_receipt.struct_size != sizeof(product_receipt) ||
        product_receipt.version !=
            RIN_VULKAN_PRODUCT_PROCESS_SUBMIT_RECEIPT_V1_VERSION ||
        product_receipt.queue_id != queue_id ||
        product_receipt.sequence == 0u ||
        product_receipt.completion_value == 0u ||
        product_receipt.device_epoch == 0u ||
        product_receipt.iommu_map_generation == 0u) {
        result = RIN_ERROR_ABI_MISMATCH;
        goto done;
    }
    memset(receipt_out, 0, sizeof(*receipt_out));
    receipt_out->struct_size = sizeof(*receipt_out);
    receipt_out->version = RIN_GPU_PROCESS_SUBMIT_RECEIPT_VERSION_V1;
    receipt_out->queue_id = product_receipt.queue_id;
    receipt_out->sequence = product_receipt.sequence;
    receipt_out->completion_value = product_receipt.completion_value;
    receipt_out->device_epoch = product_receipt.device_epoch;
    receipt_out->iommu_map_generation = product_receipt.iommu_map_generation;
    adapters->sdk_last_submit_resource_count = resource_count;
    memset(adapters->sdk_last_submit_resources, 0,
           sizeof(adapters->sdk_last_submit_resources));
    memcpy(adapters->sdk_last_submit_resources, product_resources,
           resource_count * sizeof(product_resources[0]));
    result = RIN_SUCCESS;

done:
    free(commands);
    return (RinResult)result;
}

static int initialize_product_adapters(
        TestProductAdapters* adapters,
        RinGpuVulkanSoftwarePlatformV1* software) {
    RinVulkanRinOSProcessQueueMapV4 queue_mappings[
        RIN_GPU_PROCESS_MAX_QUEUES_V1];
    uint32_t index;
    int result;
    memset(adapters, 0, sizeof(*adapters));
    adapters->software = software;
    g_process_submit_adapters = adapters;
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
    adapters->software_v6.struct_size = sizeof(adapters->software_v6);
    adapters->software_v6.version = RIN_VULKAN_PRODUCT_PLATFORM_V6_VERSION;
    adapters->software_v6.base = &adapters->v5;
    adapters->software_v6.context = adapters;
    adapters->software_v6.submit_process_commands =
        test_submit_process_commands_v6;
    if (software->queue_count == 0u ||
        software->queue_count > RIN_GPU_PROCESS_MAX_QUEUES_V1)
        return RIN_VULKAN_PRODUCT_INVALID_ARGUMENT;
    for (index = 0u; index < software->queue_count; ++index) {
        queue_mappings[index].logical_queue_id = index;
        queue_mappings[index].physical_queue_id = index;
    }
    result = rin_vulkan_rinos_process_submit_backend_v5_init(
        &adapters->process_submit_backend, &adapters->v5,
        TEST_PROCESS_SUBMIT_DEVICE_ID,
        TEST_PROCESS_SUBMIT_DEVICE_GENERATION, queue_mappings,
        software->queue_count);
    if (result != RIN_VULKAN_PRODUCT_OK) return result;
    {
        adapters->v6 =
            &adapters->process_submit_backend.submit_backend.platform_v6;
        adapters->v7 =
            &adapters->process_submit_backend.submit_backend.platform_v7;
        adapters->v8 =
            &adapters->process_submit_backend.submit_backend.platform_v8;
        adapters->v9 =
            rin_vulkan_rinos_process_submit_backend_v5_platform_v9(
                &adapters->process_submit_backend);
        if (!adapters->v6 || !adapters->v7 || !adapters->v8 ||
            !adapters->v9 || adapters->v7->base != adapters->v6 ||
            adapters->v8->base != adapters->v7 ||
            adapters->v9->base != adapters->v8)
            return RIN_VULKAN_PRODUCT_STATE;
    }
    return RIN_VULKAN_PRODUCT_OK;
}

static void emit_test_spirv_instruction(uint32_t* words, size_t* count,
                                        uint16_t opcode,
                                        uint16_t word_count,
                                        const uint32_t* operands) {
    uint16_t index;
    words[(*count)++] = ((uint32_t)word_count << 16u) | opcode;
    for (index = 1u; index < word_count; ++index)
        words[(*count)++] = operands[index - 1u];
}

static size_t make_descriptor_free_compute_module(uint32_t* words) {
    size_t count = 5u;
    uint32_t operands[5];
    words[0] = UINT32_C(0x07230203);
    words[1] = 0x00010300u;
    words[2] = 0u;
    words[3] = 12u;
    words[4] = 0u;
    operands[0] = 1u;
    emit_test_spirv_instruction(words, &count, 17u, 2u, operands);
    operands[0] = 0u;
    operands[1] = 3u;
    emit_test_spirv_instruction(words, &count, 14u, 3u, operands);
    operands[0] = 5u;
    operands[1] = 10u;
    operands[2] = UINT32_C(0x6e69616d);
    operands[3] = 0u;
    emit_test_spirv_instruction(words, &count, 15u, 5u, operands);
    operands[0] = 10u;
    operands[1] = 17u;
    operands[2] = 1u;
    operands[3] = 1u;
    operands[4] = 1u;
    emit_test_spirv_instruction(words, &count, 16u, 6u, operands);
    operands[0] = 1u;
    emit_test_spirv_instruction(words, &count, 19u, 2u, operands);
    operands[0] = 2u;
    operands[1] = 1u;
    emit_test_spirv_instruction(words, &count, 33u, 3u, operands);
    operands[0] = 1u;
    operands[1] = 10u;
    operands[2] = 0u;
    operands[3] = 2u;
    emit_test_spirv_instruction(words, &count, 54u, 5u, operands);
    operands[0] = 11u;
    emit_test_spirv_instruction(words, &count, 248u, 2u, operands);
    emit_test_spirv_instruction(words, &count, 253u, 1u, operands);
    emit_test_spirv_instruction(words, &count, 56u, 1u, operands);
    return count;
}

static int test_process_timestamp_query(
        RinVkDevice device, RinVkQueue queue,
        TestProductAdapters* adapters) {
    RinVkQueryPoolCreateInfo query_info;
    RinVkQueryPool query_pool = 0u;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool command_pool = 0u;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = NULL;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkMemoryBarrier2 barrier;
    RinVkDependencyInfo dependency_info;
    RinVkSubmitInfo submit_info;
    uint64_t results[2] = {UINT64_MAX, UINT64_MAX};
    const uint64_t expected_timestamp = UINT64_C(0x123456789abcdef0);
    uint32_t query_create_before;
    uint32_t query_result_before;
    uint32_t submit_before;
    uint32_t canonical_submit_before;
    int ok = 0;

#define PROCESS_QUERY_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "process timestamp query check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

    if (!adapters || !adapters->v9 || !queue) return -1;
    query_create_before = adapters->sdk_query_create_calls;
    query_result_before = adapters->sdk_query_result_calls;
    submit_before = adapters->sdk_process_submit_calls;
    canonical_submit_before = adapters->canonical_submit_calls;

    memset(&query_info, 0, sizeof(query_info));
    query_info.sType = RIN_VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_info.queryType = RIN_VK_QUERY_TYPE_TIMESTAMP;
    query_info.queryCount = 1u;
    PROCESS_QUERY_CHECK(vkCreateQueryPool(
        device, &query_info, NULL, &query_pool) == RIN_VK_SUCCESS);
    PROCESS_QUERY_CHECK(query_pool != 0u &&
        adapters->sdk_query_create_calls == query_create_before + 1u &&
        adapters->sdk_last_query_token != 0u);

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = 0u;
    PROCESS_QUERY_CHECK(vkCreateCommandPool(
        device, &pool_info, NULL, &command_pool) == RIN_VK_SUCCESS);
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    PROCESS_QUERY_CHECK(vkAllocateCommandBuffers(
        device, &allocate_info, &command_buffer) == RIN_VK_SUCCESS);
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = RIN_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    PROCESS_QUERY_CHECK(vkBeginCommandBuffer(
        command_buffer, &begin_info) == RIN_VK_SUCCESS);
    vkCmdResetQueryPool(command_buffer, query_pool, 0u, 1u);
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    memset(&dependency_info, 0, sizeof(dependency_info));
    dependency_info.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.memoryBarrierCount = 1u;
    dependency_info.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    vkCmdWriteTimestamp(command_buffer,
                        RIN_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        query_pool, 0u);
    PROCESS_QUERY_CHECK(vkEndCommandBuffer(command_buffer) ==
                        RIN_VK_SUCCESS);
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer;
    PROCESS_QUERY_CHECK(vkQueueSubmit(queue, 1u, &submit_info, 0u) ==
                        RIN_VK_SUCCESS);
    PROCESS_QUERY_CHECK(vkQueueWaitIdle(queue) == RIN_VK_SUCCESS);
    PROCESS_QUERY_CHECK(adapters->sdk_process_submit_calls ==
                            submit_before + 1u &&
        adapters->canonical_submit_calls == canonical_submit_before + 1u &&
        adapters->sdk_last_command_count == 4u &&
        adapters->sdk_last_commands[0].type ==
            RIN_GPU_BACKEND_COMMAND_RESET_QUERY &&
        adapters->sdk_last_commands[1].type ==
            RIN_GPU_BACKEND_COMMAND_GRAPHICS_BARRIER_V2 &&
        adapters->sdk_last_commands[2].type ==
            RIN_GPU_BACKEND_COMMAND_BEGIN_QUERY &&
        adapters->sdk_last_commands[3].type ==
            RIN_GPU_BACKEND_COMMAND_END_QUERY);
    PROCESS_QUERY_CHECK(
        adapters->sdk_last_commands[0].value.query.query_cookie ==
            adapters->sdk_last_query_token &&
        adapters->sdk_last_commands[2].value.query.query_cookie ==
            adapters->sdk_last_query_token &&
        adapters->sdk_last_commands[3].value.query.query_cookie ==
            adapters->sdk_last_query_token &&
        adapters->sdk_last_commands[0].value.query.query_type ==
            RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1 &&
        adapters->sdk_last_commands[2].value.query.query_type ==
            RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1 &&
        adapters->sdk_last_commands[3].value.query.query_type ==
            RIN_GPU_PROCESS_QUERY_TIMESTAMP_V1);

    PROCESS_QUERY_CHECK(vkGetQueryPoolResults(
        device, query_pool, 0u, 1u, sizeof(results), results, sizeof(results),
        RIN_VK_QUERY_RESULT_64_BIT |
            RIN_VK_QUERY_RESULT_WITH_AVAILABILITY_BIT |
            RIN_VK_QUERY_RESULT_PARTIAL_BIT) == RIN_VK_NOT_READY);
    PROCESS_QUERY_CHECK(adapters->sdk_query_result_calls ==
                            query_result_before + 1u &&
                        results[0] == 0u && results[1] == 0u);
    {
        uint32_t index;
        int query_found = 0;
        for (index = 0u; index < TEST_SDK_PROCESS_QUERY_CAPACITY; ++index) {
            TestSdkProcessQuery* query = &adapters->sdk_queries[index];
            if (query->active == 1u &&
                query->token == adapters->sdk_last_query_token) {
                query->result.available = 1u;
                query->result.values[0] = expected_timestamp;
                query_found = 1;
                break;
            }
        }
        PROCESS_QUERY_CHECK(query_found);
    }
    results[0] = 0u;
    results[1] = 0u;
    PROCESS_QUERY_CHECK(vkGetQueryPoolResults(
        device, query_pool, 0u, 1u, sizeof(results), results, sizeof(results),
        RIN_VK_QUERY_RESULT_64_BIT |
            RIN_VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) == RIN_VK_SUCCESS);
    PROCESS_QUERY_CHECK(adapters->sdk_query_result_calls ==
                            query_result_before + 2u &&
                        results[0] == expected_timestamp &&
                        results[1] == 1u);

    vkDestroyQueryPool(device, query_pool, NULL);
    query_pool = 0u;
    {
        uint32_t index;
        for (index = 0u; index < TEST_SDK_PROCESS_QUERY_CAPACITY; ++index)
            PROCESS_QUERY_CHECK(adapters->sdk_queries[index].active == 0u ||
                adapters->sdk_queries[index].token !=
                    adapters->sdk_last_query_token);
    }
    ok = 1;

cleanup:
    if (command_pool != NULL)
        vkDestroyCommandPool(device, command_pool, NULL);
    if (query_pool != 0u) vkDestroyQueryPool(device, query_pool, NULL);
#undef PROCESS_QUERY_CHECK
    return ok ? 0 : -1;
}

static int test_process_compute_pipeline_dispatch(
        RinVkDevice device, RinVkQueue queue,
        TestProductAdapters* adapters) {
    uint32_t spirv[48];
    const size_t spirv_word_count =
        make_descriptor_free_compute_module(spirv);
    RinVkShaderModuleCreateInfo shader_info;
    RinVkPipelineLayoutCreateInfo layout_info;
    RinVkComputePipelineCreateInfo pipeline_info;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkSubmitInfo submit_info;
    RinVkShaderModule shader = 0u;
    RinVkPipelineLayout layout = 0u;
    RinVkPipeline pipeline = 0u;
    RinVkCommandPool pool = 0u;
    RinVkCommandBuffer command_buffer = NULL;
    uint32_t sdk_submit_before;
    uint32_t canonical_submit_before;
    uint32_t canonical_dispatch_before;
    uint32_t sdk_destroy_before;
    uint64_t expected_shader_token = 0u;
    int ok = 0;

#define PROCESS_COMPUTE_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "process compute check failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        ok = -1; \
        goto cleanup; \
    } \
} while (0)

    if (!device || !queue || !adapters || !adapters->v7) {
        fprintf(stderr,
                "process compute fixture unavailable: device=%p queue=%p adapters=%p v7=%p\n",
                (void*)device, (void*)queue, (void*)adapters,
                adapters ? (void*)adapters->v7 : NULL);
        return -1;
    }
    sdk_submit_before = adapters->sdk_process_submit_calls;
    canonical_submit_before = adapters->canonical_submit_calls;
    canonical_dispatch_before = adapters->canonical_dispatch_calls;
    sdk_destroy_before = adapters->sdk_object_destroy_calls;
    memset(&shader_info, 0, sizeof(shader_info));
    shader_info.sType = RIN_VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = spirv_word_count * sizeof(spirv[0]);
    shader_info.pCode = spirv;
    PROCESS_COMPUTE_CHECK(vkCreateShaderModule(
        device, &shader_info, NULL, &shader) == RIN_VK_SUCCESS);

    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = RIN_VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PROCESS_COMPUTE_CHECK(vkCreatePipelineLayout(
        device, &layout_info, NULL, &layout) == RIN_VK_SUCCESS);

    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = RIN_VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = RIN_VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = shader;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = layout;
    pipeline_info.basePipelineIndex = -1;
    PROCESS_COMPUTE_CHECK(vkCreateComputePipelines(
        device, 0u, 1u, &pipeline_info, NULL, &pipeline) == RIN_VK_SUCCESS);
    expected_shader_token =
        ((uint64_t)RIN_VULKAN_RINOS_PROCESS_SHADER_MODULE_TOKEN_TAG << 48u) |
        (pipeline & UINT64_C(0x0000FFFFFFFFFFFF));
    PROCESS_COMPUTE_CHECK(adapters->sdk_shader_module_create_calls == 1u &&
        adapters->sdk_compute_pipeline_create_calls == 1u &&
        adapters->sdk_last_pipeline_token == pipeline &&
        adapters->sdk_last_shader_module_token == expected_shader_token &&
        adapters->sdk_last_pipeline_shader_token == expected_shader_token);

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = 0u;
    PROCESS_COMPUTE_CHECK(vkCreateCommandPool(
        device, &pool_info, NULL, &pool) == RIN_VK_SUCCESS);
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    PROCESS_COMPUTE_CHECK(vkAllocateCommandBuffers(
        device, &allocate_info, &command_buffer) == RIN_VK_SUCCESS);
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = RIN_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    PROCESS_COMPUTE_CHECK(vkBeginCommandBuffer(
        command_buffer, &begin_info) == RIN_VK_SUCCESS);
    vkCmdBindPipeline(command_buffer, RIN_VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline);
    vkCmdDispatch(command_buffer, 2u, 3u, 4u);
    PROCESS_COMPUTE_CHECK(vkEndCommandBuffer(command_buffer) ==
                          RIN_VK_SUCCESS);
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer;
    {
        const RinVkResult submit_result =
            vkQueueSubmit(queue, 1u, &submit_info, 0u);
        PROCESS_COMPUTE_CHECK(submit_result == RIN_VK_SUCCESS);
    }
    PROCESS_COMPUTE_CHECK(vkQueueWaitIdle(queue) == RIN_VK_SUCCESS);
    PROCESS_COMPUTE_CHECK(adapters->sdk_process_submit_calls ==
                              sdk_submit_before + 1u &&
        adapters->canonical_submit_calls == canonical_submit_before + 1u &&
        adapters->canonical_dispatch_calls == canonical_dispatch_before + 1u &&
        adapters->canonical_command_count == 1u &&
        adapters->canonical_last_command.type ==
            RIN_GPU_BACKEND_COMMAND_DISPATCH &&
        adapters->canonical_last_command.reserved == 0u &&
        adapters->canonical_last_command.value.dispatch.pipeline_cookie ==
            pipeline &&
        adapters->canonical_last_command.value.dispatch.group_count_x == 2u &&
        adapters->canonical_last_command.value.dispatch.group_count_y == 3u &&
        adapters->canonical_last_command.value.dispatch.group_count_z == 4u);

cleanup:
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    if (pipeline != 0u) {
        vkDestroyPipeline(device, pipeline, NULL);
        if (adapters->sdk_object_destroy_calls != sdk_destroy_before + 2u ||
            adapters->sdk_destroy_tokens[0] != pipeline ||
            adapters->sdk_destroy_tokens[1] != expected_shader_token) {
            fprintf(stderr,
                    "process compute destroy mismatch: before=%u after=%u expected_tokens=%llu,%llu actual_tokens=%llu,%llu\n",
                    sdk_destroy_before, adapters->sdk_object_destroy_calls,
                    (unsigned long long)pipeline,
                    (unsigned long long)expected_shader_token,
                    (unsigned long long)adapters->sdk_destroy_tokens[0],
                    (unsigned long long)adapters->sdk_destroy_tokens[1]);
            ok = -1;
        }
    }
    if (layout != 0u) vkDestroyPipelineLayout(device, layout, NULL);
    if (shader != 0u) vkDestroyShaderModule(device, shader, NULL);
#undef PROCESS_COMPUTE_CHECK
    return ok;
}

static size_t make_storage_compute_module(uint32_t* words) {
    size_t count = 5u;
    uint32_t operands[6];
    words[0] = UINT32_C(0x07230203);
    words[1] = 0x00010300u;
    words[2] = 0u;
    words[3] = 17u;
    words[4] = 0u;
    operands[0] = 1u;
    emit_test_spirv_instruction(words, &count, 17u, 2u, operands);
    operands[0] = 0u; operands[1] = 3u;
    emit_test_spirv_instruction(words, &count, 14u, 3u, operands);
    operands[0] = 5u; operands[1] = 10u;
    operands[2] = UINT32_C(0x6e69616d); operands[3] = 0u;
    operands[4] = 7u;
    emit_test_spirv_instruction(words, &count, 15u, 6u, operands);
    operands[0] = 10u; operands[1] = 17u;
    operands[2] = 1u; operands[3] = 1u; operands[4] = 1u;
    emit_test_spirv_instruction(words, &count, 16u, 6u, operands);
    operands[0] = 3u; operands[1] = 2u;
    emit_test_spirv_instruction(words, &count, 71u, 3u, operands);
    operands[0] = 3u; operands[1] = 0u;
    operands[2] = 35u; operands[3] = 0u;
    emit_test_spirv_instruction(words, &count, 72u, 5u, operands);
    operands[0] = 3u; operands[1] = 1u;
    operands[2] = 35u; operands[3] = 4u;
    emit_test_spirv_instruction(words, &count, 72u, 5u, operands);
    operands[0] = 7u; operands[1] = 33u; operands[2] = 0u;
    emit_test_spirv_instruction(words, &count, 71u, 4u, operands);
    operands[0] = 7u; operands[1] = 34u; operands[2] = 0u;
    emit_test_spirv_instruction(words, &count, 71u, 4u, operands);
    operands[0] = 1u;
    emit_test_spirv_instruction(words, &count, 19u, 2u, operands);
    operands[0] = 2u; operands[1] = 32u; operands[2] = 0u;
    emit_test_spirv_instruction(words, &count, 21u, 4u, operands);
    operands[0] = 3u; operands[1] = 2u; operands[2] = 2u;
    emit_test_spirv_instruction(words, &count, 30u, 4u, operands);
    operands[0] = 4u; operands[1] = 12u; operands[2] = 3u;
    emit_test_spirv_instruction(words, &count, 32u, 4u, operands);
    operands[0] = 5u; operands[1] = 12u; operands[2] = 2u;
    emit_test_spirv_instruction(words, &count, 32u, 4u, operands);
    operands[0] = 6u; operands[1] = 1u;
    emit_test_spirv_instruction(words, &count, 33u, 3u, operands);
    operands[0] = 4u; operands[1] = 7u; operands[2] = 12u;
    emit_test_spirv_instruction(words, &count, 59u, 4u, operands);
    operands[0] = 2u; operands[1] = 8u; operands[2] = 0u;
    emit_test_spirv_instruction(words, &count, 43u, 4u, operands);
    operands[0] = 2u; operands[1] = 9u; operands[2] = 42u;
    emit_test_spirv_instruction(words, &count, 43u, 4u, operands);
    operands[0] = 2u; operands[1] = 16u; operands[2] = 1u;
    emit_test_spirv_instruction(words, &count, 43u, 4u, operands);
    operands[0] = 1u; operands[1] = 10u; operands[2] = 0u;
    operands[3] = 6u;
    emit_test_spirv_instruction(words, &count, 54u, 5u, operands);
    operands[0] = 11u;
    emit_test_spirv_instruction(words, &count, 248u, 2u, operands);
    operands[0] = 5u; operands[1] = 12u;
    operands[2] = 7u; operands[3] = 8u;
    emit_test_spirv_instruction(words, &count, 65u, 5u, operands);
    operands[0] = 2u; operands[1] = 13u; operands[2] = 12u;
    emit_test_spirv_instruction(words, &count, 61u, 4u, operands);
    operands[0] = 2u; operands[1] = 14u;
    operands[2] = 13u; operands[3] = 9u;
    emit_test_spirv_instruction(words, &count, 128u, 5u, operands);
    operands[0] = 5u; operands[1] = 15u;
    operands[2] = 7u; operands[3] = 16u;
    emit_test_spirv_instruction(words, &count, 65u, 5u, operands);
    operands[0] = 15u; operands[1] = 14u;
    emit_test_spirv_instruction(words, &count, 62u, 3u, operands);
    emit_test_spirv_instruction(words, &count, 253u, 1u, operands);
    emit_test_spirv_instruction(words, &count, 56u, 1u, operands);
    return count;
}

static int test_process_compute_descriptor_dispatch(
        RinVkDevice device, RinVkQueue queue, RinVkPhysicalDevice physical,
        TestProductAdapters* adapters) {
    const uint64_t buffer_size = 8u;
    uint32_t spirv[128];
    const size_t spirv_word_count = make_storage_compute_module(spirv);
    RinVkPhysicalDeviceMemoryProperties memory_properties;
    RinVkDescriptorSetLayoutBinding descriptor_binding;
    RinVkDescriptorSetLayoutCreateInfo descriptor_layout_info;
    RinVkDescriptorPoolSize descriptor_pool_size;
    RinVkDescriptorPoolCreateInfo descriptor_pool_info;
    RinVkDescriptorSetAllocateInfo descriptor_allocate_info;
    RinVkPipelineLayoutCreateInfo pipeline_layout_info;
    RinVkShaderModuleCreateInfo shader_info;
    RinVkComputePipelineCreateInfo pipeline_info;
    RinVkBufferCreateInfo buffer_info;
    RinVkMemoryRequirements memory_requirements;
    RinVkMemoryAllocateInfo memory_allocate_info;
    RinVkDescriptorBufferInfo descriptor_buffer_info;
    RinVkWriteDescriptorSet descriptor_write;
    RinVkCommandPoolCreateInfo command_pool_info;
    RinVkCommandBufferAllocateInfo command_allocate_info;
    RinVkCommandBufferBeginInfo command_begin_info;
    RinVkSubmitInfo submit_info;
    RinVkDescriptorSetLayout descriptor_layout = 0u;
    RinVkDescriptorPool descriptor_pool = 0u;
    RinVkDescriptorSet descriptor_set = 0u;
    RinVkPipelineLayout pipeline_layout = 0u;
    RinVkShaderModule shader = 0u;
    RinVkPipeline pipeline = 0u;
    RinVkBuffer buffer = 0u;
    RinVkDeviceMemory memory = 0u;
    RinVkCommandPool command_pool = 0u;
    RinVkCommandBuffer command_buffer = NULL;
    uint32_t memory_type_index = UINT32_MAX;
    uint32_t sdk_bind_before;
    uint32_t sdk_unbind_before;
    uint32_t group_create_before;
    uint32_t group_destroy_before;
    uint32_t canonical_submit_before;
    uint32_t canonical_dispatch_before;
    uint32_t resource_index;
    uint32_t active_sdk_resources = 0u;
    int submitted = 0;
    int ok = 0;

#define PROCESS_DESCRIPTOR_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "process compute descriptor check failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        ok = -1; \
        goto cleanup; \
    } \
} while (0)

    if (!device || !queue || !physical || !adapters || !adapters->v8) {
        fprintf(stderr,
                "process compute descriptor fixture unavailable: device=%p queue=%p physical=%p adapters=%p v8=%p\n",
                (void*)device, (void*)queue, (void*)physical,
                (void*)adapters, adapters ? (void*)adapters->v8 : NULL);
        return -1;
    }
    sdk_bind_before = adapters->sdk_resource_bind_calls;
    sdk_unbind_before = adapters->sdk_resource_unbind_calls;
    group_create_before = adapters->sdk_bind_group_create_calls;
    group_destroy_before = adapters->sdk_object_destroy_calls;
    canonical_submit_before = adapters->canonical_submit_calls;
    canonical_dispatch_before = adapters->canonical_dispatch_calls;

    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = RIN_VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    PROCESS_DESCRIPTOR_CHECK(vkCreateBuffer(
        device, &buffer_info, NULL, &buffer) == RIN_VK_SUCCESS);
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
    PROCESS_DESCRIPTOR_CHECK(memory_requirements.size >= buffer_size &&
        memory_requirements.memoryTypeBits != 0u);
    memset(&memory_properties, 0, sizeof(memory_properties));
    vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);
    for (resource_index = 0u;
         resource_index < memory_properties.memoryTypeCount &&
             resource_index < 32u;
         ++resource_index)
        if ((memory_requirements.memoryTypeBits &
             (UINT32_C(1) << resource_index)) != 0u) {
            memory_type_index = resource_index;
            break;
        }
    PROCESS_DESCRIPTOR_CHECK(memory_type_index != UINT32_MAX);
    memset(&memory_allocate_info, 0, sizeof(memory_allocate_info));
    memory_allocate_info.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_type_index;
    PROCESS_DESCRIPTOR_CHECK(vkAllocateMemory(
        device, &memory_allocate_info, NULL, &memory) == RIN_VK_SUCCESS);
    PROCESS_DESCRIPTOR_CHECK(vkBindBufferMemory(
        device, buffer, memory, 0u) == RIN_VK_SUCCESS);

    memset(&descriptor_binding, 0, sizeof(descriptor_binding));
    descriptor_binding.binding = 0u;
    descriptor_binding.descriptorType = RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_binding.descriptorCount = 1u;
    descriptor_binding.stageFlags = RIN_VK_SHADER_STAGE_COMPUTE_BIT;
    memset(&descriptor_layout_info, 0, sizeof(descriptor_layout_info));
    descriptor_layout_info.sType =
        RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout_info.bindingCount = 1u;
    descriptor_layout_info.pBindings = &descriptor_binding;
    PROCESS_DESCRIPTOR_CHECK(vkCreateDescriptorSetLayout(
        device, &descriptor_layout_info, NULL, &descriptor_layout) ==
        RIN_VK_SUCCESS);

    memset(&descriptor_pool_size, 0, sizeof(descriptor_pool_size));
    descriptor_pool_size.type = RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_pool_size.descriptorCount = 1u;
    memset(&descriptor_pool_info, 0, sizeof(descriptor_pool_info));
    descriptor_pool_info.sType = RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.maxSets = 1u;
    descriptor_pool_info.poolSizeCount = 1u;
    descriptor_pool_info.pPoolSizes = &descriptor_pool_size;
    PROCESS_DESCRIPTOR_CHECK(vkCreateDescriptorPool(
        device, &descriptor_pool_info, NULL, &descriptor_pool) ==
        RIN_VK_SUCCESS);
    memset(&descriptor_allocate_info, 0, sizeof(descriptor_allocate_info));
    descriptor_allocate_info.sType =
        RIN_VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_allocate_info.descriptorPool = descriptor_pool;
    descriptor_allocate_info.descriptorSetCount = 1u;
    descriptor_allocate_info.pSetLayouts = &descriptor_layout;
    PROCESS_DESCRIPTOR_CHECK(vkAllocateDescriptorSets(
        device, &descriptor_allocate_info, &descriptor_set) == RIN_VK_SUCCESS);
    memset(&descriptor_buffer_info, 0, sizeof(descriptor_buffer_info));
    descriptor_buffer_info.buffer = buffer;
    descriptor_buffer_info.range = buffer_size;
    memset(&descriptor_write, 0, sizeof(descriptor_write));
    descriptor_write.dstSet = descriptor_set;
    descriptor_write.dstBinding = 0u;
    descriptor_write.descriptorCount = 1u;
    descriptor_write.descriptorType = RIN_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_write.pBufferInfo = &descriptor_buffer_info;
    vkUpdateDescriptorSets(device, 1u, &descriptor_write, 0u, NULL);

    memset(&pipeline_layout_info, 0, sizeof(pipeline_layout_info));
    pipeline_layout_info.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1u;
    pipeline_layout_info.pSetLayouts = &descriptor_layout;
    PROCESS_DESCRIPTOR_CHECK(vkCreatePipelineLayout(
        device, &pipeline_layout_info, NULL, &pipeline_layout) ==
        RIN_VK_SUCCESS);
    memset(&shader_info, 0, sizeof(shader_info));
    shader_info.sType = RIN_VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = spirv_word_count * sizeof(spirv[0]);
    shader_info.pCode = spirv;
    PROCESS_DESCRIPTOR_CHECK(vkCreateShaderModule(
        device, &shader_info, NULL, &shader) == RIN_VK_SUCCESS);
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = RIN_VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType =
        RIN_VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = RIN_VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = shader;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = pipeline_layout;
    pipeline_info.basePipelineIndex = -1;
    PROCESS_DESCRIPTOR_CHECK(vkCreateComputePipelines(
        device, 0u, 1u, &pipeline_info, NULL, &pipeline) == RIN_VK_SUCCESS);
    PROCESS_DESCRIPTOR_CHECK(adapters->sdk_last_shader_resource_count == 1u &&
        adapters->sdk_last_pipeline_token == pipeline);

    memset(&command_pool_info, 0, sizeof(command_pool_info));
    command_pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = 0u;
    PROCESS_DESCRIPTOR_CHECK(vkCreateCommandPool(
        device, &command_pool_info, NULL, &command_pool) == RIN_VK_SUCCESS);
    memset(&command_allocate_info, 0, sizeof(command_allocate_info));
    command_allocate_info.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_allocate_info.commandPool = command_pool;
    command_allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_allocate_info.commandBufferCount = 1u;
    PROCESS_DESCRIPTOR_CHECK(vkAllocateCommandBuffers(
        device, &command_allocate_info, &command_buffer) == RIN_VK_SUCCESS);
    memset(&command_begin_info, 0, sizeof(command_begin_info));
    command_begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_begin_info.flags = RIN_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    PROCESS_DESCRIPTOR_CHECK(vkBeginCommandBuffer(
        command_buffer, &command_begin_info) == RIN_VK_SUCCESS);
    vkCmdBindPipeline(command_buffer, RIN_VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline);
    vkCmdBindDescriptorSets(command_buffer,
        RIN_VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0u, 1u,
        &descriptor_set, 0u, NULL);
    vkCmdDispatch(command_buffer, 2u, 3u, 4u);
    PROCESS_DESCRIPTOR_CHECK(vkEndCommandBuffer(command_buffer) ==
                             RIN_VK_SUCCESS);
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer;
    PROCESS_DESCRIPTOR_CHECK(vkQueueSubmit(queue, 1u, &submit_info, 0u) ==
                             RIN_VK_SUCCESS);
    submitted = 1;
    PROCESS_DESCRIPTOR_CHECK(adapters->sdk_resource_bind_calls ==
                                 sdk_bind_before + 1u &&
        adapters->sdk_bind_group_create_calls == group_create_before + 1u &&
        adapters->sdk_last_resource_binding.allocation != 0u &&
        adapters->sdk_last_resource_binding.allocation_offset == 0u &&
        adapters->sdk_last_resource_binding.size_bytes == buffer_size &&
        adapters->sdk_last_resource_binding.required_gpu_access ==
            (RIN_GPU_ALLOCATION_GPU_READ | RIN_GPU_ALLOCATION_GPU_WRITE) &&
        adapters->sdk_last_bind_group_count == 1u &&
        adapters->sdk_last_bind_group_pipeline_token == pipeline &&
        adapters->sdk_last_bind_group_bindings[0].resource_cookie ==
            adapters->sdk_last_resource_binding.resource_cookie &&
        adapters->sdk_last_bind_group_bindings[0].offset == 0u &&
        adapters->sdk_last_bind_group_bindings[0].size_bytes == buffer_size &&
        adapters->sdk_last_bind_group_bindings[0].access ==
            (RIN_GPU_ALLOCATION_GPU_READ | RIN_GPU_ALLOCATION_GPU_WRITE));
    PROCESS_DESCRIPTOR_CHECK(adapters->canonical_last_command.type ==
            RIN_GPU_BACKEND_COMMAND_DISPATCH &&
        adapters->canonical_last_command.value.dispatch.pipeline_cookie ==
            pipeline &&
        adapters->canonical_last_command.value.dispatch.bind_group_cookie ==
            adapters->sdk_last_bind_group_token &&
        adapters->canonical_last_command.value.dispatch.group_count_x == 2u &&
        adapters->canonical_last_command.value.dispatch.group_count_y == 3u &&
        adapters->canonical_last_command.value.dispatch.group_count_z == 4u &&
        adapters->canonical_resource_count == 0u &&
        adapters->canonical_last_resources[0].allocation_handle == 0u &&
        adapters->canonical_last_resources[0].required_gpu_access == 0u);

    adapters->sdk_bind_group_destroy_busy_attempts = 1u;
    PROCESS_DESCRIPTOR_CHECK(vkQueueWaitIdle(queue) == RIN_VK_SUCCESS);
    submitted = 0;
    PROCESS_DESCRIPTOR_CHECK(adapters->sdk_bind_group_destroy_busy_attempts == 0u &&
        adapters->sdk_resource_unbind_calls == sdk_unbind_before + 1u &&
        adapters->sdk_object_destroy_calls >= group_destroy_before + 2u);
    for (resource_index = 0u;
         resource_index < RIN_GPU_PROCESS_MAX_COMPUTE_BINDINGS_V1;
         ++resource_index)
        active_sdk_resources += adapters->sdk_resources[resource_index].active;
    PROCESS_DESCRIPTOR_CHECK(active_sdk_resources == 0u &&
        adapters->canonical_submit_calls == canonical_submit_before + 1u &&
        adapters->canonical_dispatch_calls == canonical_dispatch_before + 1u);
    ok = 0;

cleanup:
    if (submitted) {
        const RinVkResult idle_result = vkQueueWaitIdle(queue);
        if (idle_result != RIN_VK_SUCCESS) ok = -1;
    }
    if (command_pool != NULL)
        vkDestroyCommandPool(device, command_pool, NULL);
    if (pipeline != 0u) vkDestroyPipeline(device, pipeline, NULL);
    if (shader != 0u) vkDestroyShaderModule(device, shader, NULL);
    if (pipeline_layout != 0u)
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    if (descriptor_pool != 0u)
        vkDestroyDescriptorPool(device, descriptor_pool, NULL);
    if (descriptor_layout != 0u)
        vkDestroyDescriptorSetLayout(device, descriptor_layout, NULL);
    if (buffer != 0u) vkDestroyBuffer(device, buffer, NULL);
    if (memory != 0u) vkFreeMemory(device, memory, NULL);
#undef PROCESS_DESCRIPTOR_CHECK
    return ok;
}

typedef struct TestRecordedBarrier {
    RinVkCommandPool pool;
    RinVkCommandBuffer command_buffer;
} TestRecordedBarrier;

static int record_sync2_queue_family_stage_case(
        RinVkDevice device, uint32_t queue_family, uint64_t source_stage,
        uint64_t source_access, uint64_t destination_stage,
        uint64_t destination_access, int expected_success) {
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = NULL;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = NULL;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkMemoryBarrier2 barrier;
    RinVkDependencyInfo dependency;
    RinVkResult end_result;
    int result = 0;

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    if (vkCreateCommandPool(device, &pool_info, NULL, &pool) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    if (vkAllocateCommandBuffers(device, &allocate_info, &command_buffer) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != RIN_VK_SUCCESS)
        goto done;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = source_stage;
    barrier.srcAccessMask = source_access;
    barrier.dstStageMask = destination_stage;
    barrier.dstAccessMask = destination_access;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 1u;
    dependency.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    end_result = vkEndCommandBuffer(command_buffer);
    result = expected_success ? end_result == RIN_VK_SUCCESS
                              : end_result != RIN_VK_SUCCESS;

done:
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    return result;
}

static int test_sync2_queue_family_stage_validation(RinVkDevice device) {
    return record_sync2_queue_family_stage_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 1) &&
           record_sync2_queue_family_stage_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 0) &&
           record_sync2_queue_family_stage_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 0) &&
           record_sync2_queue_family_stage_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 0) &&
           record_sync2_queue_family_stage_case(
               device, 1u,
               RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 0) &&
           record_sync2_queue_family_stage_case(
               device, 0u, RIN_VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 1) &&
           record_sync2_queue_family_stage_case(
               device, 0u,
               RIN_VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
               0u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 0u, 1) &&
           record_sync2_queue_family_stage_case(
               device, 1u, 0u,
               RIN_VK_ACCESS_2_MEMORY_READ_BIT |
                   RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
               0u,
               RIN_VK_ACCESS_2_MEMORY_READ_BIT |
                   RIN_VK_ACCESS_2_MEMORY_WRITE_BIT,
               1);
}

static int record_sync2_transfer_queue_event_case(
        RinVkDevice device, uint32_t event_operation) {
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = NULL;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = NULL;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkEventCreateInfo event_info;
    RinVkEvent event = 0u;
    RinVkMemoryBarrier2 barrier;
    RinVkDependencyInfo dependency;
    RinVkResult end_result;
    int result = 0;

    memset(&event_info, 0, sizeof(event_info));
    event_info.sType = RIN_VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    if (vkCreateEvent(device, &event_info, NULL, &event) != RIN_VK_SUCCESS)
        goto done;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = 1u;
    if (vkCreateCommandPool(device, &pool_info, NULL, &pool) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    if (vkAllocateCommandBuffers(device, &allocate_info, &command_buffer) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != RIN_VK_SUCCESS)
        goto done;

    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_COPY_BIT;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 1u;
    dependency.pMemoryBarriers = &barrier;

    if (event_operation == 0u) {
        vkCmdSetEvent2(command_buffer, event, &dependency);
    } else if (event_operation == 1u) {
        RinVkEvent events[1] = {event};
        if (vkSetEvent(device, event) != RIN_VK_SUCCESS) goto done;
        barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.srcAccessMask = RIN_VK_ACCESS_2_HOST_WRITE_BIT;
        vkCmdWaitEvents2(command_buffer, 1u, events, &dependency);
    } else if (event_operation == 2u) {
        vkCmdResetEvent2(command_buffer, event,
                         RIN_VK_PIPELINE_STAGE_2_COPY_BIT);
    } else {
        goto done;
    }
    end_result = vkEndCommandBuffer(command_buffer);
    result = end_result != RIN_VK_SUCCESS;

done:
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    if (event != 0u) vkDestroyEvent(device, event, NULL);
    return result;
}

static int test_sync2_event_queue_type_validation(RinVkDevice device) {
    return record_sync2_transfer_queue_event_case(device, 0u) &&
           record_sync2_transfer_queue_event_case(device, 1u) &&
           record_sync2_transfer_queue_event_case(device, 2u);
}

static int test_sync2_queue_submit_stage_validation(
        RinVkDevice device, RinVkQueue transfer_queue) {
    RinVkSemaphoreCreateInfo semaphore_info;
    RinVkSemaphore semaphore = 0u;
    RinVkSemaphoreSubmitInfo semaphore_submit;
    RinVkSubmitInfo2 submit;
    RinVkSubmitInfo legacy_submit;
    RinVkSemaphore legacy_wait_semaphores[1];
    uint32_t legacy_wait_stage;
    int result = 0;

    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device, &semaphore_info, NULL, &semaphore) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&semaphore_submit, 0, sizeof(semaphore_submit));
    semaphore_submit.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphore_submit.semaphore = semaphore;
    semaphore_submit.stageMask = RIN_VK_PIPELINE_STAGE_2_COPY_BIT;
    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.signalSemaphoreInfoCount = 1u;
    submit.pSignalSemaphoreInfos = &semaphore_submit;
    if (vkQueueSubmit2(transfer_queue, 1u, &submit, 0u) != RIN_VK_SUCCESS)
        goto done;

    semaphore_submit.stageMask = RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (vkQueueSubmit2(transfer_queue, 1u, &submit, 0u) !=
        RIN_VK_ERROR_FEATURE_NOT_PRESENT)
        goto done;

    submit.signalSemaphoreInfoCount = 0u;
    submit.pSignalSemaphoreInfos = NULL;
    submit.waitSemaphoreInfoCount = 1u;
    submit.pWaitSemaphoreInfos = &semaphore_submit;
    if (vkQueueSubmit2(transfer_queue, 1u, &submit, 0u) !=
        RIN_VK_ERROR_FEATURE_NOT_PRESENT)
        goto done;

    memset(&legacy_submit, 0, sizeof(legacy_submit));
    legacy_submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    legacy_wait_semaphores[0] = semaphore;
    legacy_submit.waitSemaphoreCount = 1u;
    legacy_submit.pWaitSemaphores = legacy_wait_semaphores;
    legacy_submit.pWaitDstStageMask = &legacy_wait_stage;
    legacy_wait_stage = RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (vkQueueSubmit(transfer_queue, 1u, &legacy_submit, 0u) !=
        RIN_VK_ERROR_FEATURE_NOT_PRESENT)
        goto done;

    semaphore_submit.stageMask = RIN_VK_PIPELINE_STAGE_2_COPY_BIT;
    if (vkQueueSubmit2(transfer_queue, 1u, &submit, 0u) != RIN_VK_SUCCESS ||
        vkQueueWaitIdle(transfer_queue) != RIN_VK_SUCCESS)
        goto done;
    result = 1;

done:
    if (semaphore != 0u) vkDestroySemaphore(device, semaphore, NULL);
    return result;
}

static int record_timestamp_queue_family_case(
        RinVkDevice device, uint32_t queue_family, uint64_t stage,
        int synchronization2, int expected_success) {
    RinVkQueryPoolCreateInfo query_info;
    RinVkQueryPool query_pool = 0u;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = NULL;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = NULL;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkResult end_result;
    int result = 0;

    memset(&query_info, 0, sizeof(query_info));
    query_info.sType = RIN_VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_info.queryType = RIN_VK_QUERY_TYPE_TIMESTAMP;
    query_info.queryCount = 1u;
    if (vkCreateQueryPool(device, &query_info, NULL, &query_pool) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    if (vkCreateCommandPool(device, &pool_info, NULL, &pool) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    if (vkAllocateCommandBuffers(device, &allocate_info, &command_buffer) !=
        RIN_VK_SUCCESS)
        goto done;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != RIN_VK_SUCCESS)
        goto done;
    if (synchronization2)
        vkCmdWriteTimestamp2(command_buffer, stage, query_pool, 0u);
    else
        vkCmdWriteTimestamp(command_buffer, stage, query_pool, 0u);
    end_result = vkEndCommandBuffer(command_buffer);
    result = expected_success ? end_result == RIN_VK_SUCCESS
                              : end_result != RIN_VK_SUCCESS;

done:
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    if (query_pool != 0u) vkDestroyQueryPool(device, query_pool, NULL);
    return result;
}

static int test_timestamp_queue_family_stage_validation(RinVkDevice device) {
    return record_timestamp_queue_family_case(
               device, 0u, RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 1, 1) &&
           record_timestamp_queue_family_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_2_COPY_BIT, 1, 0) &&
           record_timestamp_queue_family_case(
               device, 0u, RIN_VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1) &&
           record_timestamp_queue_family_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0) &&
           record_timestamp_queue_family_case(
               device, 1u, RIN_VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 1,
               0);
}

static RinVkResult record_process_wait_barrier(
        RinVkDevice device, TestRecordedBarrier* recorded) {
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkMemoryBarrier2 barriers[2];
    RinVkDependencyInfo dependency;
    RinVkResult result;

    if (!device || !recorded) return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(recorded, 0, sizeof(*recorded));
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = 0u;
    result = vkCreateCommandPool(device, &pool_info, NULL, &recorded->pool);
    if (result != RIN_VK_SUCCESS) return result;
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = recorded->pool;
    allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1u;
    result = vkAllocateCommandBuffers(device, &allocate_info,
                                      &recorded->command_buffer);
    if (result != RIN_VK_SUCCESS) return result;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(recorded->command_buffer, &begin_info);
    if (result != RIN_VK_SUCCESS) return result;

    memset(barriers, 0, sizeof(barriers));
    barriers[0].sType = RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barriers[0].srcStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[0].srcAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barriers[0].dstStageMask = RIN_VK_PIPELINE_STAGE_2_HOST_BIT;
    barriers[0].dstAccessMask = RIN_VK_ACCESS_2_HOST_READ_BIT;
    barriers[1].sType = RIN_VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barriers[1].srcStageMask = RIN_VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    barriers[1].srcAccessMask = RIN_VK_ACCESS_2_SHADER_WRITE_BIT;
    barriers[1].dstStageMask = RIN_VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barriers[1].dstAccessMask = RIN_VK_ACCESS_2_SHADER_READ_BIT;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 2u;
    dependency.pMemoryBarriers = barriers;
    vkCmdPipelineBarrier2(recorded->command_buffer, &dependency);
    return vkEndCommandBuffer(recorded->command_buffer);
}

static RinVkResult test_process_submit_wait_completion(
        RinVkDevice device, RinVkQueue producer_queue,
        RinVkQueue consumer_queue, TestProductAdapters* adapters) {
    RinVkSemaphoreCreateInfo semaphore_info;
    RinVkSemaphore semaphore = 0u;
    RinVkSemaphore timeline_semaphore = 0u;
    RinVkSemaphoreTypeCreateInfo semaphore_type_info;
    TestRecordedBarrier producer;
    TestRecordedBarrier consumer;
    RinVkSemaphoreSubmitInfo signal_infos[2];
    RinVkSemaphoreSubmitInfo wait_infos[2];
    RinVkCommandBufferSubmitInfo command_buffer_info;
    RinVkSubmitInfo2 submit;
    const uint32_t submits_before = adapters->canonical_submit_calls;
    const uint32_t barriers_before =
        adapters->canonical_graphics_barrier_count;
    const uint32_t opaque_prepares_before =
        g_opaque_process_prepare_calls;
    const uint32_t opaque_submits_before = g_opaque_process_submit_calls;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;

    memset(&producer, 0, sizeof(producer));
    memset(&consumer, 0, sizeof(consumer));
    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device, &semaphore_info, NULL, &semaphore) !=
            RIN_VK_SUCCESS)
        goto done;
    memset(&semaphore_type_info, 0, sizeof(semaphore_type_info));
    semaphore_type_info.sType =
        RIN_VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    semaphore_type_info.semaphoreType = RIN_VK_SEMAPHORE_TYPE_TIMELINE;
    semaphore_info.pNext = &semaphore_type_info;
    if (vkCreateSemaphore(device, &semaphore_info, NULL,
                          &timeline_semaphore) != RIN_VK_SUCCESS ||
        record_process_wait_barrier(device, &producer) != RIN_VK_SUCCESS ||
        record_process_wait_barrier(device, &consumer) != RIN_VK_SUCCESS)
        goto done;

    adapters->deferred_completion_value = 0u;
    adapters->defer_next_process_completion = 1u;
    memset(&command_buffer_info, 0, sizeof(command_buffer_info));
    command_buffer_info.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_info.commandBuffer = producer.command_buffer;
    command_buffer_info.deviceMask = 1u;
    memset(signal_infos, 0, sizeof(signal_infos));
    signal_infos[0].sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_infos[0].semaphore = semaphore;
    signal_infos[0].stageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signal_infos[1].sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_infos[1].semaphore = timeline_semaphore;
    signal_infos[1].value = 1u;
    signal_infos[1].stageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1u;
    submit.pCommandBufferInfos = &command_buffer_info;
    submit.signalSemaphoreInfoCount = 2u;
    submit.pSignalSemaphoreInfos = signal_infos;
    result = vkQueueSubmit2(producer_queue, 1u, &submit, 0u);
    if (result != RIN_VK_SUCCESS ||
        adapters->canonical_submit_calls != submits_before + 1u ||
        g_opaque_process_prepare_calls != opaque_prepares_before ||
        g_opaque_process_submit_calls != opaque_submits_before) {
        fprintf(stderr,
                "process wait producer submit: result=%d canonical=%u expected=%u\n",
                result, adapters->canonical_submit_calls,
                submits_before + 1u);
        goto done;
    }
    if (adapters->canonical_graphics_barrier_count != barriers_before + 2u ||
        adapters->canonical_last_graphics_barrier.source_stage !=
            (RIN_GPU_PIPELINE_STAGE_VERTEX_INPUT |
             RIN_GPU_PIPELINE_STAGE_VERTEX_SHADER |
             RIN_GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS |
             RIN_GPU_PIPELINE_STAGE_FRAGMENT_SHADER |
             RIN_GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS |
             RIN_GPU_PIPELINE_STAGE_COLOR_OUTPUT) ||
        adapters->canonical_last_graphics_barrier.source_access !=
            RIN_GPU_RESOURCE_WRITE ||
        adapters->canonical_last_graphics_barrier.destination_stage !=
            (RIN_GPU_PIPELINE_STAGE_VERTEX_INPUT |
             RIN_GPU_PIPELINE_STAGE_VERTEX_SHADER |
             RIN_GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS |
             RIN_GPU_PIPELINE_STAGE_FRAGMENT_SHADER |
             RIN_GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS |
             RIN_GPU_PIPELINE_STAGE_COLOR_OUTPUT) ||
        adapters->canonical_last_graphics_barrier.destination_access !=
            RIN_GPU_RESOURCE_READ) {
        fprintf(stderr,
                "public sync2 shader barrier scope mismatch at process submit: "
                "count=%u before=%u last=%llu/%llu->%llu/%llu "
                "expected=%llu/%llu->%llu/%llu\n",
                adapters->canonical_graphics_barrier_count,
                barriers_before,
                (unsigned long long)adapters->canonical_last_graphics_barrier.source_stage,
                (unsigned long long)adapters->canonical_last_graphics_barrier.source_access,
                (unsigned long long)adapters->canonical_last_graphics_barrier.destination_stage,
                (unsigned long long)adapters->canonical_last_graphics_barrier.destination_access,
                (unsigned long long)(RIN_GPU_PIPELINE_STAGE_VERTEX_INPUT |
                    RIN_GPU_PIPELINE_STAGE_VERTEX_SHADER |
                    RIN_GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS |
                    RIN_GPU_PIPELINE_STAGE_FRAGMENT_SHADER |
                    RIN_GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS |
                    RIN_GPU_PIPELINE_STAGE_COLOR_OUTPUT),
                (unsigned long long)RIN_GPU_RESOURCE_WRITE,
                (unsigned long long)(RIN_GPU_PIPELINE_STAGE_VERTEX_INPUT |
                    RIN_GPU_PIPELINE_STAGE_VERTEX_SHADER |
                    RIN_GPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS |
                    RIN_GPU_PIPELINE_STAGE_FRAGMENT_SHADER |
                    RIN_GPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS |
                    RIN_GPU_PIPELINE_STAGE_COLOR_OUTPUT),
                (unsigned long long)RIN_GPU_RESOURCE_READ);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    if (adapters->deferred_completion_value == 0u ||
        adapters->deferred_completion_queue_id >=
            adapters->software->queue_count ||
        adapters->software->completed_values[
            adapters->deferred_completion_queue_id] >=
            adapters->deferred_completion_value) {
        fprintf(stderr, "process wait producer completion was not deferred\n");
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }

    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    memset(wait_infos, 0, sizeof(wait_infos));
    wait_infos[0].sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_infos[0].semaphore = semaphore;
    wait_infos[0].stageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    wait_infos[1].sType = RIN_VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_infos[1].semaphore = timeline_semaphore;
    wait_infos[1].value = 1u;
    wait_infos[1].stageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    command_buffer_info.commandBuffer = consumer.command_buffer;
    submit.waitSemaphoreInfoCount = 2u;
    submit.pWaitSemaphoreInfos = wait_infos;
    submit.commandBufferInfoCount = 1u;
    submit.pCommandBufferInfos = &command_buffer_info;
    result = vkQueueSubmit2(consumer_queue, 1u, &submit, 0u);
    if (result != RIN_VK_SUCCESS ||
        adapters->canonical_submit_calls != submits_before + 1u ||
        g_opaque_process_prepare_calls != opaque_prepares_before ||
        g_opaque_process_submit_calls != opaque_submits_before) {
        fprintf(stderr,
                "process wait consumer enqueue: result=%d canonical=%u expected=%u\n",
                result, adapters->canonical_submit_calls,
                submits_before + 1u);
        goto done;
    }
    if (adapters->software->completed_values[
            adapters->deferred_completion_queue_id] >=
        adapters->deferred_completion_value) {
        fprintf(stderr, "process wait producer completion was not deferred\n");
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    adapters->software->completed_values[
        adapters->deferred_completion_queue_id] =
        adapters->deferred_completion_value;
    result = vkQueueWaitIdle(consumer_queue);
    if (result != RIN_VK_SUCCESS ||
        adapters->canonical_submit_calls != submits_before + 2u ||
        g_opaque_process_prepare_calls != opaque_prepares_before ||
        g_opaque_process_submit_calls != opaque_submits_before) {
        fprintf(stderr,
                "process wait consumer completion: result=%d canonical=%u expected=%u\n",
                result, adapters->canonical_submit_calls,
                submits_before + 2u);
        goto done;
    }
    result = RIN_VK_SUCCESS;

done:
    if (producer.pool != NULL)
        vkDestroyCommandPool(device, producer.pool, NULL);
    if (consumer.pool != NULL)
        vkDestroyCommandPool(device, consumer.pool, NULL);
    if (timeline_semaphore != 0u)
        vkDestroySemaphore(device, timeline_semaphore, NULL);
    if (semaphore != 0u) vkDestroySemaphore(device, semaphore, NULL);
    return result;
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
    profile->queue_family_count = 2u;
    profile->queue_families[0].flags = RIN_GPU_VK_QUEUE_GRAPHICS |
                                       RIN_GPU_VK_QUEUE_COMPUTE |
                                       RIN_GPU_VK_QUEUE_TRANSFER;
    profile->queue_families[0].queue_count = 2u;
    profile->queue_families[0].timestamp_valid_bits = 64u;
    profile->queue_families[1].flags = RIN_GPU_VK_QUEUE_TRANSFER;
    profile->queue_families[1].queue_count = 1u;
    profile->queue_families[1].timestamp_valid_bits = 0u;
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

static RinVkResult record_and_submit_buffer_copy(
        RinVkDevice device, RinVkQueue queue,
        TestProductAdapters* product_adapters) {
    const uint64_t buffer_size = 64u;
    const uint64_t copy_size = 16u;
    const uint64_t source_offset = 8u;
    const uint64_t destination_offset = 24u;
    const uint8_t source_bytes[16] = {
        0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
        0x99u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xf0u, 0x0fu
    };
    RinVkBufferCreateInfo buffer_info;
    RinVkBuffer source_buffer = 0u;
    RinVkBuffer destination_buffer = 0u;
    RinVkMemoryRequirements source_requirements;
    RinVkMemoryRequirements destination_requirements;
    RinVkMemoryAllocateInfo allocation_info;
    RinVkDeviceMemory source_memory = 0u;
    RinVkDeviceMemory destination_memory = 0u;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = 0u;
    RinVkCommandBufferAllocateInfo command_allocate_info;
    RinVkCommandBuffer command_buffer = 0u;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkBufferCopy copy;
    RinVkSubmitInfo submit;
    RinVkFenceCreateInfo fence_info;
    RinVkFence fence = 0u;
    const uint32_t submits_before =
        product_adapters ? product_adapters->canonical_submit_calls : 0u;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;
    void* mapped = NULL;

    if (!device || !queue || !product_adapters)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateBuffer(device, &buffer_info, NULL, &source_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkCreateBuffer(device, &buffer_info, NULL, &destination_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    vkGetBufferMemoryRequirements(device, source_buffer,
                                  &source_requirements);
    vkGetBufferMemoryRequirements(device, destination_buffer,
                                  &destination_requirements);
    if ((source_requirements.memoryTypeBits & (UINT32_C(1) << 1u)) == 0u ||
        (destination_requirements.memoryTypeBits &
         (UINT32_C(1) << 1u)) == 0u) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }
    memset(&allocation_info, 0, sizeof(allocation_info));
    allocation_info.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.memoryTypeIndex = 1u;
    allocation_info.allocationSize = source_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL, &source_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    allocation_info.allocationSize = destination_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL,
                              &destination_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindBufferMemory(device, source_buffer, source_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindBufferMemory(device, destination_buffer,
                                destination_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkMapMemory(device, source_memory, 0u, buffer_size, 0u, &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(mapped, 0u, (size_t)buffer_size);
    memcpy((uint8_t*)mapped + source_offset, source_bytes, sizeof(source_bytes));
    vkUnmapMemory(device, source_memory);
    mapped = NULL;
    result = vkMapMemory(device, destination_memory, 0u, buffer_size, 0u,
                         &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(mapped, 0xa5, (size_t)buffer_size);
    vkUnmapMemory(device, destination_memory);
    mapped = NULL;

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = 0u;
    result = vkCreateCommandPool(device, &pool_info, NULL, &pool);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&command_allocate_info, 0, sizeof(command_allocate_info));
    command_allocate_info.sType =
        RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_allocate_info.commandPool = pool;
    command_allocate_info.level = RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_allocate_info.commandBufferCount = 1u;
    result = vkAllocateCommandBuffers(device, &command_allocate_info,
                                      &command_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != RIN_VK_SUCCESS) goto done;
    copy.srcOffset = source_offset;
    copy.dstOffset = destination_offset;
    copy.size = copy_size;
    vkCmdCopyBuffer(command_buffer, source_buffer, destination_buffer, 1u,
                    &copy);
    result = vkEndCommandBuffer(command_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = vkCreateFence(device, &fence_info, NULL, &fence);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &command_buffer;
    result = vkQueueSubmit(queue, 1u, &submit, fence);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkWaitForFences(device, 1u, &fence, 1u, UINT64_MAX);
    if (result != RIN_VK_SUCCESS) goto done;
    if (product_adapters->canonical_submit_calls != submits_before + 1u ||
        product_adapters->canonical_command_count != 1u ||
        product_adapters->sdk_last_command_count != 1u ||
        product_adapters->sdk_last_commands[0].type !=
            RIN_GPU_BACKEND_COMMAND_COPY_BUFFER ||
        product_adapters->sdk_last_submit_resource_count != 2u ||
        vkMapMemory(device, destination_memory, 0u, buffer_size, 0u,
                    &mapped) != RIN_VK_SUCCESS) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    {
        const RinGpuBackendBufferCopyV1* submitted_copy =
            &product_adapters->sdk_last_commands[0].value.buffer_copy;
        const TestResourceBinding* source_binding = test_find_resource_binding(
            product_adapters, submitted_copy->source_cookie);
        const TestResourceBinding* destination_binding =
            test_find_resource_binding(product_adapters,
                                       submitted_copy->destination_cookie);
        uint32_t source_lease_found = 0u;
        uint32_t destination_lease_found = 0u;
        if (!source_binding || !destination_binding) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
        for (uint32_t resource_index = 0u; resource_index < 2u;
             ++resource_index) {
            const RinVulkanProductResourceV1* lease =
                &product_adapters->sdk_last_submit_resources[resource_index];
            if (lease->allocation_handle == source_binding->allocation_handle &&
                lease->required_gpu_access ==
                    RIN_VULKAN_PRODUCT_MEMORY_GPU_READ)
                source_lease_found = 1u;
            if (lease->allocation_handle ==
                    destination_binding->allocation_handle &&
                lease->required_gpu_access ==
                    RIN_VULKAN_PRODUCT_MEMORY_GPU_WRITE)
                destination_lease_found = 1u;
        }
        if (!source_lease_found || !destination_lease_found) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
    }
    if (memcmp((const uint8_t*)mapped + destination_offset, source_bytes,
               sizeof(source_bytes)) != 0 ||
        ((const uint8_t*)mapped)[0] != 0xa5u ||
        ((const uint8_t*)mapped)[destination_offset + copy_size] != 0xa5u) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    result = RIN_VK_SUCCESS;

done:
    if (result != RIN_VK_SUCCESS)
        fprintf(stderr,
                "native-window buffer-copy fixture failed: result=%d "
                "sdk_submits=%u canonical_submits=%u command_count=%u "
                "resource_count=%u\n",
                result, product_adapters->sdk_process_submit_calls,
                product_adapters->canonical_submit_calls,
                product_adapters->sdk_last_command_count,
                product_adapters->sdk_last_submit_resource_count);
    if (mapped) vkUnmapMemory(device, destination_memory != 0u
                                         ? destination_memory
                                         : source_memory);
    if (fence != 0u) vkDestroyFence(device, fence, NULL);
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    if (source_buffer != 0u) vkDestroyBuffer(device, source_buffer, NULL);
    if (destination_buffer != 0u)
        vkDestroyBuffer(device, destination_buffer, NULL);
    if (source_memory != 0u) vkFreeMemory(device, source_memory, NULL);
    if (destination_memory != 0u)
        vkFreeMemory(device, destination_memory, NULL);
    return result;
}

static RinVkResult record_buffer_image_roundtrip(
        RinVkDevice device, RinVkQueue queue,
        TestProductAdapters* product_adapters) {
    static const uint8_t copied_texels[16] = {
        0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
        0x99u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xf0u, 0x0fu
    };
    const uint64_t upload_size = 64u;
    const uint64_t readback_size = 64u;
    RinVkImageCreateInfo image_info;
    RinVkImage image = 0u;
    RinVkImage copy_destination = 0u;
    RinVkMemoryRequirements image_requirements;
    RinVkMemoryRequirements copy_destination_requirements;
    RinVkBufferCreateInfo buffer_info;
    RinVkBuffer upload_buffer = 0u;
    RinVkBuffer readback_buffer = 0u;
    RinVkMemoryRequirements upload_requirements;
    RinVkMemoryRequirements readback_requirements;
    RinVkMemoryAllocateInfo allocation_info;
    RinVkDeviceMemory image_memory = 0u;
    RinVkDeviceMemory copy_destination_memory = 0u;
    RinVkDeviceMemory upload_memory = 0u;
    RinVkDeviceMemory readback_memory = 0u;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = 0u;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = 0u;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkImageMemoryBarrier2 barrier;
    RinVkDependencyInfo dependency;
    RinVkBufferImageCopy region;
    RinVkImageCopy image_copy;
    RinVkImageBlit image_blit;
    RinVkSubmitInfo submit;
    RinVkFenceCreateInfo fence_info;
    RinVkFence fence = 0u;
    void* mapped = NULL;
    RinVkDeviceMemory mapped_memory = 0u;
    uint32_t submits_before;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;

    if (!device || !queue || !product_adapters)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    memset(&image_info, 0, sizeof(image_info));
    image_info.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = RIN_VK_IMAGE_TYPE_2D;
    image_info.format = RIN_VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = 4u;
    image_info.extent.height = 3u;
    image_info.extent.depth = 1u;
    image_info.mipLevels = 1u;
    image_info.arrayLayers = 1u;
    image_info.samples = RIN_VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = RIN_VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateImage(device, &image_info, NULL, &image);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkCreateImage(device, &image_info, NULL, &copy_destination);
    if (result != RIN_VK_SUCCESS) goto done;
    vkGetImageMemoryRequirements(device, image, &image_requirements);
    vkGetImageMemoryRequirements(device, copy_destination,
                                 &copy_destination_requirements);
    if ((image_requirements.memoryTypeBits & UINT32_C(1)) == 0u ||
        (copy_destination_requirements.memoryTypeBits & UINT32_C(1)) == 0u ||
        image_requirements.size != copy_destination_requirements.size) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }

    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = upload_size;
    buffer_info.usage = RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateBuffer(device, &buffer_info, NULL, &upload_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    buffer_info.size = readback_size;
    buffer_info.usage = RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    result = vkCreateBuffer(device, &buffer_info, NULL, &readback_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    vkGetBufferMemoryRequirements(device, upload_buffer, &upload_requirements);
    vkGetBufferMemoryRequirements(device, readback_buffer,
                                  &readback_requirements);
    if ((upload_requirements.memoryTypeBits & (UINT32_C(1) << 1u)) == 0u ||
        (readback_requirements.memoryTypeBits & (UINT32_C(1) << 1u)) == 0u) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }

    memset(&allocation_info, 0, sizeof(allocation_info));
    allocation_info.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.memoryTypeIndex = 0u;
    allocation_info.allocationSize = image_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL, &image_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    allocation_info.allocationSize = copy_destination_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL,
                              &copy_destination_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    allocation_info.memoryTypeIndex = 1u;
    allocation_info.allocationSize = upload_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL, &upload_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    allocation_info.allocationSize = readback_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL,
                              &readback_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindImageMemory(device, image, image_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindImageMemory(device, copy_destination,
                               copy_destination_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindBufferMemory(device, upload_buffer, upload_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindBufferMemory(device, readback_buffer, readback_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;

    result = vkMapMemory(device, upload_memory, 0u, upload_size, 0u, &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    mapped_memory = upload_memory;
    memset(mapped, 0xcc, (size_t)upload_size);
    memcpy((uint8_t*)mapped + 4u, copied_texels, 8u);
    memcpy((uint8_t*)mapped + 16u, copied_texels + 8u, 8u);
    vkUnmapMemory(device, upload_memory);
    mapped = NULL;
    mapped_memory = 0u;
    result = vkMapMemory(device, readback_memory, 0u, readback_size, 0u,
                         &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    mapped_memory = readback_memory;
    memset(mapped, 0xa5, (size_t)readback_size);
    vkUnmapMemory(device, readback_memory);
    mapped = NULL;
    mapped_memory = 0u;

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
    barrier.subresourceRange.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1u;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    barrier.image = copy_destination;
    vkCmdPipelineBarrier2(command_buffer, &dependency);

    memset(&region, 0, sizeof(region));
    region.bufferOffset = 4u;
    region.bufferRowLength = 3u;
    region.bufferImageHeight = 3u;
    region.imageSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageOffset.x = 1;
    region.imageExtent.width = 2u;
    region.imageExtent.height = 2u;
    region.imageExtent.depth = 1u;
    vkCmdCopyBufferToImage(command_buffer, upload_buffer, image,
                           RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                           &region);

    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.image = image;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    memset(&image_copy, 0, sizeof(image_copy));
    image_copy.srcSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.srcSubresource.layerCount = 1u;
    image_copy.dstSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.dstSubresource.layerCount = 1u;
    image_copy.extent.width = 4u;
    image_copy.extent.height = 3u;
    image_copy.extent.depth = 1u;
    vkCmdCopyImage(command_buffer, image,
                   RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   copy_destination,
                   RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1u, &image_copy);
    memset(&image_blit, 0, sizeof(image_blit));
    image_blit.srcSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit.srcSubresource.layerCount = 1u;
    image_blit.dstSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit.dstSubresource.layerCount = 1u;
    image_blit.srcOffsets[1].x = 4;
    image_blit.srcOffsets[1].y = 3;
    image_blit.srcOffsets[1].z = 1;
    image_blit.dstOffsets[1].x = 4;
    image_blit.dstOffsets[1].y = 3;
    image_blit.dstOffsets[1].z = 1;
    vkCmdBlitImage(command_buffer, image,
                   RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   copy_destination,
                   RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1u, &image_blit, RIN_VK_FILTER_NEAREST);
    barrier.image = copy_destination;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    region.bufferOffset = 8u;
    vkCmdCopyImageToBuffer(command_buffer, copy_destination,
                           RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback_buffer, 1u, &region);
    result = vkEndCommandBuffer(command_buffer);
    if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "image-copy command recording failed: %d\n", result);
        goto done;
    }

    submits_before = product_adapters->canonical_submit_calls;
    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &command_buffer;
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = vkCreateFence(device, &fence_info, NULL, &fence);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkQueueSubmit(queue, 1u, &submit, fence);
    if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "image-copy queue submit failed: %d\n", result);
        goto done;
    }
    result = vkWaitForFences(device, 1u, &fence, 1u, UINT64_MAX);
    if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "image-copy completion wait failed: %d\n", result);
        goto done;
    }
    if (product_adapters->canonical_submit_calls != submits_before + 1u ||
        product_adapters->canonical_command_count != 12u) {
        fprintf(stderr,
                "image-copy canonical batch mismatch: submits %u->%u, commands=%u\n",
                submits_before, product_adapters->canonical_submit_calls,
                product_adapters->canonical_command_count);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    result = vkMapMemory(device, readback_memory, 0u, readback_size, 0u,
                         &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    mapped_memory = readback_memory;
    if (memcmp((const uint8_t*)mapped + 8u, copied_texels, 8u) != 0 ||
        memcmp((const uint8_t*)mapped + 20u, copied_texels + 8u, 8u) != 0 ||
        ((const uint8_t*)mapped)[7u] != 0xa5u ||
        ((const uint8_t*)mapped)[16u] != 0xa5u ||
        ((const uint8_t*)mapped)[19u] != 0xa5u ||
        ((const uint8_t*)mapped)[28u] != 0xa5u) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    result = RIN_VK_SUCCESS;

done:
    if (mapped) vkUnmapMemory(device, mapped_memory);
    if (fence != 0u) vkDestroyFence(device, fence, NULL);
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    if (upload_buffer != 0u) vkDestroyBuffer(device, upload_buffer, NULL);
    if (readback_buffer != 0u) vkDestroyBuffer(device, readback_buffer, NULL);
    if (image != 0u) vkDestroyImage(device, image, NULL);
    if (copy_destination != 0u)
        vkDestroyImage(device, copy_destination, NULL);
    if (upload_memory != 0u) vkFreeMemory(device, upload_memory, NULL);
    if (readback_memory != 0u) vkFreeMemory(device, readback_memory, NULL);
    if (image_memory != 0u) vkFreeMemory(device, image_memory, NULL);
    if (copy_destination_memory != 0u)
        vkFreeMemory(device, copy_destination_memory, NULL);
    return result;
}

static RinVkResult record_image_resolve_roundtrip(
        RinVkDevice device, RinVkQueue queue,
        TestProductAdapters* product_adapters, uint32_t sample_count) {
    uint8_t expected_pixel[4];
    const uint32_t width = 4u;
    const uint32_t height = 3u;
    const uint64_t destination_bytes = (uint64_t)width * height * 4u;
    uint64_t source_bytes;
    RinVkImageCreateInfo image_info;
    RinVkImage source = 0u;
    RinVkImage destination = 0u;
    RinVkMemoryRequirements source_requirements;
    RinVkMemoryRequirements destination_requirements;
    RinVkBufferCreateInfo buffer_info;
    RinVkBuffer readback_buffer = 0u;
    RinVkMemoryRequirements readback_requirements;
    RinVkMemoryAllocateInfo allocation_info;
    RinVkDeviceMemory source_memory = 0u;
    RinVkDeviceMemory destination_memory = 0u;
    RinVkDeviceMemory readback_memory = 0u;
    RinVkCommandPoolCreateInfo pool_info;
    RinVkCommandPool pool = 0u;
    RinVkCommandBufferAllocateInfo allocate_info;
    RinVkCommandBuffer command_buffer = 0u;
    RinVkCommandBufferBeginInfo begin_info;
    RinVkImageMemoryBarrier2 barrier;
    RinVkBufferMemoryBarrier2 buffer_barrier;
    RinVkDependencyInfo dependency;
    RinVkImageResolve resolve;
    RinVkBufferImageCopy readback_region;
    RinVkSubmitInfo submit;
    RinVkFenceCreateInfo fence_info;
    RinVkFence fence = 0u;
    RinGpuVulkanSoftwareAllocationV1* source_allocation;
    const TestResourceBinding* source_binding;
    void* mapped = NULL;
    uint32_t submits_before;
    uint32_t resolves_before;
    uint32_t barriers_before;
    uint64_t pixel;
    RinVkResult result = RIN_VK_ERROR_INITIALIZATION_FAILED;

    if (!device || !queue || !product_adapters)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    if (sample_count != 2u && sample_count != 4u)
        return RIN_VK_ERROR_INITIALIZATION_FAILED;
    source_bytes = destination_bytes * sample_count;
    expected_pixel[0] = (uint8_t)(5u * (sample_count + 1u));
    expected_pixel[1] = (uint8_t)(10u * (sample_count + 1u));
    expected_pixel[2] = (uint8_t)(15u * (sample_count + 1u));
    expected_pixel[3] = (uint8_t)(20u * (sample_count + 1u));
    memset(&image_info, 0, sizeof(image_info));
    image_info.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = RIN_VK_IMAGE_TYPE_2D;
    image_info.format = RIN_VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1u;
    image_info.mipLevels = 1u;
    image_info.arrayLayers = 1u;
    image_info.samples = sample_count == 2u
        ? RIN_VK_SAMPLE_COUNT_2_BIT : RIN_VK_SAMPLE_COUNT_4_BIT;
    image_info.tiling = RIN_VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateImage(device, &image_info, NULL, &source);
    if (result != RIN_VK_SUCCESS) goto done;
    image_info.samples = RIN_VK_SAMPLE_COUNT_1_BIT;
    image_info.usage = RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    result = vkCreateImage(device, &image_info, NULL, &destination);
    if (result != RIN_VK_SUCCESS) goto done;
    vkGetImageMemoryRequirements(device, source, &source_requirements);
    vkGetImageMemoryRequirements(device, destination,
                                 &destination_requirements);
    if (source_requirements.size != source_bytes ||
        destination_requirements.size != destination_bytes ||
        (source_requirements.memoryTypeBits & UINT32_C(1)) == 0u ||
        (destination_requirements.memoryTypeBits & UINT32_C(1)) == 0u) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }

    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = destination_bytes;
    buffer_info.usage = RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = RIN_VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateBuffer(device, &buffer_info, NULL, &readback_buffer);
    if (result != RIN_VK_SUCCESS) goto done;
    vkGetBufferMemoryRequirements(device, readback_buffer,
                                  &readback_requirements);
    if ((readback_requirements.memoryTypeBits & (UINT32_C(1) << 1u)) == 0u) {
        result = RIN_VK_ERROR_FEATURE_NOT_PRESENT;
        goto done;
    }

    memset(&allocation_info, 0, sizeof(allocation_info));
    allocation_info.sType = RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.memoryTypeIndex = 0u;
    allocation_info.allocationSize = source_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL, &source_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    allocation_info.allocationSize = destination_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL,
                              &destination_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    allocation_info.memoryTypeIndex = 1u;
    allocation_info.allocationSize = readback_requirements.size;
    result = vkAllocateMemory(device, &allocation_info, NULL,
                              &readback_memory);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindImageMemory(device, source, source_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindImageMemory(device, destination, destination_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkBindBufferMemory(device, readback_buffer, readback_memory, 0u);
    if (result != RIN_VK_SUCCESS) goto done;

    source_binding = test_find_resource_binding(
        product_adapters, (uint64_t)source);
    source_allocation = source_binding
        ? test_find_allocation(product_adapters->software,
                               source_binding->allocation_handle)
        : NULL;
    if (!source_allocation || !source_allocation->bytes ||
        source_allocation->size_bytes != source_bytes) {
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    for (pixel = 0u; pixel < (uint64_t)width * height; ++pixel) {
        uint32_t sample;
        for (sample = 0u; sample < sample_count; ++sample) {
            uint8_t* sample_pixel = source_allocation->bytes +
                ((uint64_t)sample * width * height + pixel) * 4u;
            sample_pixel[0] = (uint8_t)((sample + 1u) * 10u);
            sample_pixel[1] = (uint8_t)((sample + 1u) * 20u);
            sample_pixel[2] = (uint8_t)((sample + 1u) * 30u);
            sample_pixel[3] = (uint8_t)((sample + 1u) * 40u);
        }
    }
    atomic_thread_fence(memory_order_release);
    result = vkMapMemory(device, readback_memory, 0u, destination_bytes, 0u,
                         &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    memset(mapped, 0xa5, (size_t)destination_bytes);
    vkUnmapMemory(device, readback_memory);
    mapped = NULL;

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

    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = RIN_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.srcQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.image = source;
    barrier.subresourceRange.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;
    memset(&dependency, 0, sizeof(dependency));
    dependency.sType = RIN_VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1u;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    barrier.dstAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = destination;
    vkCmdPipelineBarrier2(command_buffer, &dependency);

    memset(&resolve, 0, sizeof(resolve));
    resolve.srcSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    resolve.srcSubresource.layerCount = 1u;
    resolve.dstSubresource.aspectMask = RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    resolve.dstSubresource.layerCount = 1u;
    resolve.extent.width = width;
    resolve.extent.height = height;
    resolve.extent.depth = 1u;
    vkCmdResolveImage(command_buffer, source,
                      RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination,
                      RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &resolve);
    barrier.srcAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = RIN_VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.image = destination;
    barrier.oldLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier2(command_buffer, &dependency);

    memset(&readback_region, 0, sizeof(readback_region));
    readback_region.imageSubresource.aspectMask =
        RIN_VK_IMAGE_ASPECT_COLOR_BIT;
    readback_region.imageSubresource.layerCount = 1u;
    readback_region.imageExtent.width = width;
    readback_region.imageExtent.height = height;
    readback_region.imageExtent.depth = 1u;
    vkCmdCopyImageToBuffer(command_buffer, destination,
                           RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback_buffer, 1u, &readback_region);
    memset(&buffer_barrier, 0, sizeof(buffer_barrier));
    buffer_barrier.sType = RIN_VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    buffer_barrier.srcStageMask = RIN_VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    buffer_barrier.srcAccessMask = RIN_VK_ACCESS_2_TRANSFER_WRITE_BIT;
    buffer_barrier.dstStageMask = RIN_VK_PIPELINE_STAGE_2_HOST_BIT;
    buffer_barrier.dstAccessMask = RIN_VK_ACCESS_2_HOST_READ_BIT;
    buffer_barrier.srcQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    buffer_barrier.dstQueueFamilyIndex = RIN_VK_QUEUE_FAMILY_IGNORED;
    buffer_barrier.buffer = readback_buffer;
    buffer_barrier.offset = 0u;
    buffer_barrier.size = destination_bytes;
    dependency.memoryBarrierCount = 0u;
    dependency.pMemoryBarriers = NULL;
    dependency.bufferMemoryBarrierCount = 1u;
    dependency.pBufferMemoryBarriers = &buffer_barrier;
    dependency.imageMemoryBarrierCount = 0u;
    dependency.pImageMemoryBarriers = NULL;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    result = vkEndCommandBuffer(command_buffer);
    if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "image-resolve command recording failed: %d\n",
                result);
        goto done;
    }

    submits_before = product_adapters->canonical_submit_calls;
    resolves_before = product_adapters->canonical_resolve_command_count;
    barriers_before = product_adapters->canonical_graphics_barrier_count;
    memset(&submit, 0, sizeof(submit));
    submit.sType = RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &command_buffer;
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = vkCreateFence(device, &fence_info, NULL, &fence);
    if (result != RIN_VK_SUCCESS) goto done;
    result = vkQueueSubmit(queue, 1u, &submit, fence);
    if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "image-resolve queue submit failed: %d\n", result);
        goto done;
    }
    result = vkWaitForFences(device, 1u, &fence, 1u, UINT64_MAX);
    if (result != RIN_VK_SUCCESS) {
        fprintf(stderr, "image-resolve completion wait failed: %d\n", result);
        goto done;
    }
    if (product_adapters->canonical_submit_calls != submits_before + 1u ||
        product_adapters->canonical_resolve_command_count !=
            resolves_before + 1u ||
        product_adapters->canonical_command_count != 9u ||
        product_adapters->canonical_graphics_barrier_count !=
            barriers_before + 4u ||
        product_adapters->canonical_last_graphics_barrier.source_stage !=
            RIN_GPU_PIPELINE_STAGE_COPY ||
        product_adapters->canonical_last_graphics_barrier.source_access !=
            RIN_GPU_RESOURCE_WRITE ||
        product_adapters->canonical_last_graphics_barrier.destination_stage !=
            RIN_GPU_PIPELINE_STAGE_HOST ||
        product_adapters->canonical_last_graphics_barrier.destination_access !=
            RIN_GPU_RESOURCE_READ ||
        product_adapters->canonical_last_graphics_barrier.flags != 0u ||
        product_adapters->canonical_last_graphics_barrier.reserved != 0u) {
        fprintf(stderr,
                "image-resolve canonical batch mismatch: commands=%u, resolves=%u, barriers=%u, last=%u/%u->%u/%u\n",
                product_adapters->canonical_command_count,
                product_adapters->canonical_resolve_command_count -
                    resolves_before,
                product_adapters->canonical_graphics_barrier_count -
                    barriers_before,
                product_adapters->canonical_last_graphics_barrier.source_stage,
                product_adapters->canonical_last_graphics_barrier.source_access,
                product_adapters->canonical_last_graphics_barrier.destination_stage,
                product_adapters->canonical_last_graphics_barrier.destination_access);
        result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        goto done;
    }
    result = vkMapMemory(device, readback_memory, 0u, destination_bytes, 0u,
                         &mapped);
    if (result != RIN_VK_SUCCESS) goto done;
    for (pixel = 0u; pixel < (uint64_t)width * height; ++pixel) {
        if (memcmp((const uint8_t*)mapped + pixel * sizeof(expected_pixel),
                   expected_pixel, sizeof(expected_pixel)) != 0) {
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            goto done;
        }
    }
    result = RIN_VK_SUCCESS;

done:
    if (mapped) vkUnmapMemory(device, readback_memory);
    if (fence != 0u) vkDestroyFence(device, fence, NULL);
    if (pool != NULL) vkDestroyCommandPool(device, pool, NULL);
    if (readback_buffer != 0u)
        vkDestroyBuffer(device, readback_buffer, NULL);
    if (source != 0u) vkDestroyImage(device, source, NULL);
    if (destination != 0u) vkDestroyImage(device, destination, NULL);
    if (readback_memory != 0u)
        vkFreeMemory(device, readback_memory, NULL);
    if (source_memory != 0u) vkFreeMemory(device, source_memory, NULL);
    if (destination_memory != 0u)
        vkFreeMemory(device, destination_memory, NULL);
    return result;
}

int main(void) {
    RinGpuVulkanPhysicalDeviceV2 profile;
    RinGpuVulkanRuntimeV1 runtime;
    RinGpuVulkanSoftwarePlatformV1 software;
    TestProductAdapters product_adapters;
    RinVkPhysicalDeviceProperties physical_properties;
    RinVkQueueFamilyProperties timestamp_queue_properties[2];
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
        RIN_VK_KHR_TIMELINE_SEMAPHORE_EXTENSION,
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
    TestCompositorSurface compositor;
    RinVkSurfaceCapabilitiesKHR capabilities;
    RinVkSurfaceFormatKHR surface_format;
    RinVkDeviceQueueCreateInfo queue_infos[2];
    RinVkPhysicalDeviceVulkan12Features vulkan12;
    RinVkPhysicalDeviceSynchronization2Features synchronization2;
    RinVkDeviceCreateInfo device_info;
    RinVkDevice device = NULL;
    RinVkQueue queue = NULL;
    RinVkQueue second_queue = NULL;
    RinVkQueue transfer_queue = NULL;
    const float queue_priorities[2] = {1.0f, 1.0f};
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
    uint32_t product_v6_bound = 0u;
    uint32_t product_v7_bound = 0u;
    uint32_t product_v8_bound = 0u;
    uint32_t product_v9_bound = 0u;
    uint32_t process_backend_initialized = 0u;
    uint32_t surface_created = 0u;
    uint32_t device_created = 0u;
    uint32_t swapchain_created = 0u;
    int runtime_init_result;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        result = RIN_VK_ERROR_INITIALIZATION_FAILED; \
        goto cleanup; \
    } \
} while (0)

    memset(&compositor, 0, sizeof(compositor));
    compositor.width = 4u;
    compositor.height = 4u;
    compositor.generation = 1u;
    compositor.busy_imports = 1u;
    compositor.expected_word = UINT32_C(0x6a3c17e5);

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
    memset(&software, 0, sizeof(software));
    CHECK(rin_gpu_vulkan_software_platform_init(
              &software, profile.iommu_domain_cookie, profile.device_epoch,
              profile.queue_families[0].queue_count +
                  profile.queue_families[1].queue_count,
              UINT64_C(16) * 1024u * 1024u) == RIN_VULKAN_PRODUCT_OK);
    software_initialized = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform(&software.platform) ==
          RIN_GPU_VULKAN_OK);
    product_bound = 1u;
    runtime_init_result = initialize_product_adapters(
        &product_adapters, &software);
    process_backend_initialized =
        product_adapters.process_submit_backend.state != 0u;
    CHECK(runtime_init_result == RIN_VULKAN_PRODUCT_OK);
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
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v6(
              product_adapters.v6) == RIN_GPU_VULKAN_OK);
    product_v6_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v7(
              product_adapters.v7) == RIN_GPU_VULKAN_OK);
    product_v7_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v8(
              product_adapters.v8) == RIN_GPU_VULKAN_OK);
    product_v8_bound = 1u;
    CHECK(rin_gpu_vulkan_icd_bind_product_platform_v9(
              product_adapters.v9) == RIN_GPU_VULKAN_OK);
    product_v9_bound = 1u;
    memset(&physical_properties, 0, sizeof(physical_properties));
    vkGetPhysicalDeviceProperties(physical, &physical_properties);
    CHECK(physical_properties.limits.timestampComputeAndGraphics == 1u &&
          physical_properties.limits.timestampPeriod == 1.0f);
    {
        uint32_t timestamp_queue_count = 2u;
        memset(timestamp_queue_properties, 0,
               sizeof(timestamp_queue_properties));
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical, &timestamp_queue_count, timestamp_queue_properties);
        CHECK(timestamp_queue_count == 2u &&
              timestamp_queue_properties[0].timestampValidBits == 64u &&
              timestamp_queue_properties[1].timestampValidBits == 0u);
    }

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
          capabilities.currentExtent.height == 4u);
#else
    CHECK(capabilities.currentExtent.width == compositor.width &&
          capabilities.currentExtent.height == compositor.height);
#endif
    CHECK((capabilities.supportedUsageFlags &
           RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u &&
          (capabilities.supportedUsageFlags &
           RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u);
    extension_count = 1u;
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
              physical, surface, &extension_count, &surface_format) ==
          RIN_VK_SUCCESS);
    CHECK(extension_count == 1u &&
          surface_format.format == RIN_VK_FORMAT_R8G8B8A8_UNORM);

    memset(queue_infos, 0, sizeof(queue_infos));
    queue_infos[0].sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[0].queueFamilyIndex = 0u;
    queue_infos[0].queueCount = 2u;
    queue_infos[0].pQueuePriorities = queue_priorities;
    queue_infos[1].sType = RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[1].queueFamilyIndex = 1u;
    queue_infos[1].queueCount = 1u;
    queue_infos[1].pQueuePriorities = queue_priorities;
    memset(&synchronization2, 0, sizeof(synchronization2));
    synchronization2.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2.synchronization2 = 1u;
    memset(&vulkan12, 0, sizeof(vulkan12));
    vulkan12.sType =
        RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12.pNext = &synchronization2;
    vulkan12.timelineSemaphore = 1u;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &vulkan12;
    device_info.queueCreateInfoCount = 2u;
    device_info.pQueueCreateInfos = queue_infos;
    device_info.enabledExtensionCount = 3u;
    device_info.ppEnabledExtensionNames = device_extensions;
    CHECK(vkCreateDevice(physical, &device_info, NULL, &device) ==
          RIN_VK_SUCCESS);
    device_created = 1u;
    vkGetDeviceQueue(device, 0u, 0u, &queue);
    CHECK(queue != NULL);
    vkGetDeviceQueue(device, 0u, 1u, &second_queue);
    CHECK(second_queue != NULL && second_queue != queue);
    vkGetDeviceQueue(device, 1u, 0u, &transfer_queue);
    CHECK(transfer_queue != NULL);
    CHECK(test_sync2_queue_family_stage_validation(device));
    CHECK(test_sync2_event_queue_type_validation(device));
    CHECK(test_sync2_queue_submit_stage_validation(device, transfer_queue));
    CHECK(test_timestamp_queue_family_stage_validation(device));
    {
        const RinVkResult process_wait_result =
            test_process_submit_wait_completion(
                device, queue, second_queue, &product_adapters);
        if (process_wait_result != RIN_VK_SUCCESS)
            fprintf(stderr,
                    "process-submit completion-gated wait returned %d\n",
                    process_wait_result);
        CHECK(process_wait_result == RIN_VK_SUCCESS);
    }

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

        semaphore_submit_info.stageMask =
            RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
        CHECK(vkQueueSubmit2(queue, 1u, &empty_submit, 0u) ==
              RIN_VK_ERROR_FEATURE_NOT_PRESENT);

        semaphore_submit_info.stageMask = 0u;
        empty_submit.signalSemaphoreInfoCount = 0u;
        empty_submit.pSignalSemaphoreInfos = NULL;
        empty_submit.waitSemaphoreInfoCount = 1u;
        empty_submit.pWaitSemaphoreInfos = &semaphore_submit_info;
        CHECK(vkQueueSubmit2(queue, 1u, &empty_submit, 0u) ==
              RIN_VK_SUCCESS);

        semaphore_submit_info.stageMask =
            RIN_VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
        CHECK(vkQueueSubmit2(queue, 1u, &empty_submit, 0u) ==
              RIN_VK_ERROR_FEATURE_NOT_PRESENT);

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
    CHECK(record_and_submit_buffer_copy(device, queue,
                                        &product_adapters) == RIN_VK_SUCCESS);
    CHECK(record_buffer_image_roundtrip(device, queue,
                                        &product_adapters) == RIN_VK_SUCCESS);
    CHECK(record_image_resolve_roundtrip(device, queue, &product_adapters,
                                         2u) == RIN_VK_SUCCESS);
    CHECK(record_image_resolve_roundtrip(device, queue, &product_adapters,
                                         4u) == RIN_VK_SUCCESS);
    {
        RinVkResult record_result = record_present_layout_and_clear(
            device, queue, images[image_index], acquire_semaphore,
            render_complete_semaphore, compositor.expected_word);
        if (record_result != RIN_VK_SUCCESS)
            fprintf(stderr, "native-window image submit returned %d\n",
                    record_result);
        CHECK(record_result == RIN_VK_SUCCESS);
    }
    CHECK(product_adapters.canonical_submit_calls == 7u &&
          product_adapters.canonical_command_count == 10u);
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
    {
        RinVkResult first_present_result = vkQueuePresentKHR(queue, &present);
        if (first_present_result != RIN_VK_NOT_READY)
            fprintf(stderr,
                    "first native-window present returned %d (pResults=%d, imports=%u, accepted=%u, busy=%u)\n",
                    first_present_result, present_result,
                    compositor.import_attempts, compositor.accepted_imports,
                    compositor.busy_imports);
        CHECK(first_present_result == RIN_VK_NOT_READY);
    }
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
#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST)
    CHECK(test_dispatch_fixture_frame_to_compositor(&compositor) == 0);
#endif
#endif

#if defined(RIN_VULKAN_NATIVE_WINDOW_COMPOSITOR_TEST)
    memset(&g_native_window_app_surface, 0,
           sizeof(g_native_window_app_surface));
    {
        uint8_t imported_pixels[2u * 2u * 4u];
        uint32_t imported_surface_id = 0u;
        uint32_t imported_buffer_slot = UINT32_MAX;
        uint32_t imported_front_slot = UINT32_MAX;
        uint64_t imported_surface_generation = 0u;
        uint64_t imported_frame_sequence = 0u;
        uint32_t pixel;
        int imported_pixels_nonzero = 0;
        CHECK(compositor_gpu_readback_runtime_core_reset(
                  77u, 2u, 2u, &imported_surface_id,
                  &imported_surface_generation) == 0);
        CHECK(imported_surface_id != 0u &&
              imported_surface_generation != 0u);
        g_native_window_app_surface.compositor_surface_id =
            imported_surface_id;
        g_native_window_app_surface.compositor_generation =
            imported_surface_generation;
    {
        const int app_result = rinos_native_window_surface_main();
        if (app_result != 0)
            fprintf(stderr,
                    "native-window public app failed: result=%d, imports=%u/%u, compositor=%d\n",
                    app_result,
                    g_native_window_app_surface.frame_import_calls,
                    g_native_window_app_surface.frame_import_attempts,
                    g_native_window_app_surface.last_import_result);
        CHECK(app_result == 0);
    }
    CHECK(g_native_window_app_surface.window_create_calls == 1u &&
          g_native_window_app_surface.surface_ops_calls == 1u &&
          g_native_window_app_surface.frame_import_calls == 1u &&
          g_native_window_app_surface.frame_import_attempts == 1u &&
          g_native_window_app_surface.compositor_dispatch_calls == 1u &&
          g_native_window_app_surface.window_close_calls == 1u &&
          g_native_window_app_surface.completion_callback == NULL &&
          g_native_window_app_surface.completion_context == NULL);
        imported_buffer_slot =
            g_native_window_app_surface.compositor_buffer_slot;
        CHECK(compositor_gpu_readback_runtime_core_observe(
                  imported_buffer_slot, imported_pixels,
                  sizeof(imported_pixels), &imported_front_slot,
                  &imported_frame_sequence) == 0);
        CHECK(imported_front_slot == imported_buffer_slot &&
              imported_frame_sequence == 1u);
        for (pixel = 0u; pixel < 4u; ++pixel)
            if (imported_pixels[pixel * 4u] != 0u ||
                imported_pixels[pixel * 4u + 1u] != 0u ||
                imported_pixels[pixel * 4u + 2u] != 0u ||
                imported_pixels[pixel * 4u + 3u] != 0u)
                imported_pixels_nonzero = 1;
        CHECK(imported_pixels_nonzero);
    }
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
        uint32_t unbind_calls_before_destroy;
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
        unbind_calls_before_destroy =
            product_adapters.resource_unbind_calls;
        product_adapters.fail_unbind_cookie = bound_images[0];
        product_adapters.fail_unbind_attempts = 1u;
        vkDestroySwapchainKHR(device, swapchain, NULL);
        {
            const RinVkResult retained_images_result =
                vkGetSwapchainImagesKHR(device, swapchain, &bound_image_count,
                                        bound_images);
            if (product_adapters.active_resource_count != 2u ||
                product_adapters.resource_unbind_calls !=
                    unbind_calls_before_destroy ||
                retained_images_result != RIN_VK_SUCCESS ||
                bound_image_count != 2u)
                fprintf(stderr,
                        "swapchain retry state: active=%u unbind_calls=%u fail_attempts=%u images_result=%d image_count=%u\n",
                        product_adapters.active_resource_count,
                        product_adapters.resource_unbind_calls,
                        product_adapters.fail_unbind_attempts,
                        retained_images_result, bound_image_count);
            CHECK(product_adapters.active_resource_count == 2u &&
                  product_adapters.resource_unbind_calls ==
                      unbind_calls_before_destroy &&
                  retained_images_result == RIN_VK_SUCCESS &&
                  bound_image_count == 2u);
        }
        vkDestroySwapchainKHR(device, swapchain, NULL);
        CHECK(product_adapters.active_resource_count == 0u &&
              product_adapters.resource_unbind_calls ==
                  unbind_calls_before_destroy + 2u &&
              product_adapters.fail_unbind_attempts == 0u);
        swapchain_created = 0u;
        CHECK(test_process_compute_pipeline_dispatch(
                  device, queue, &product_adapters) == 0);
        CHECK(test_process_compute_descriptor_dispatch(
                  device, queue, physical, &product_adapters) == 0);
        CHECK(test_process_timestamp_query(
                  device, queue, &product_adapters) == 0);
        CHECK(product_adapters.sdk_queue_bind_calls == software.queue_count &&
              product_adapters.sdk_process_submit_calls ==
                  product_adapters.canonical_submit_calls &&
              product_adapters.sdk_queue_release_calls == 0u &&
              product_adapters.sdk_active_queue_count ==
                  software.queue_count);
        CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v9(
                  product_adapters.v9) == RIN_GPU_VULKAN_OK);
        product_v9_bound = 0u;
        CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v8(
                  product_adapters.v8) == RIN_GPU_VULKAN_OK);
        product_v8_bound = 0u;
        CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v7(
                  product_adapters.v7) == RIN_GPU_VULKAN_OK);
        product_v7_bound = 0u;
        CHECK(rin_gpu_vulkan_icd_unbind_product_platform_v6(
                  product_adapters.v6) == RIN_GPU_VULKAN_OK);
        product_v6_bound = 0u;
        CHECK(rin_vulkan_rinos_process_submit_backend_v5_detach(
                  &product_adapters.process_submit_backend) ==
              RIN_VULKAN_PRODUCT_OK);
        process_backend_initialized = 0u;
        CHECK(product_adapters.sdk_queue_release_calls == software.queue_count &&
              product_adapters.sdk_active_queue_count == 0u);
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
    if (product_v9_bound) {
        const int unbind_result =
            rin_gpu_vulkan_icd_unbind_product_platform_v9(
                product_adapters.v9);
        if (unbind_result == RIN_GPU_VULKAN_OK) {
            product_v9_bound = 0u;
        } else {
            fprintf(stderr, "V9 process-query unbind returned %d\n",
                    unbind_result);
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (product_v8_bound) {
        const int unbind_result =
            rin_gpu_vulkan_icd_unbind_product_platform_v8(
                product_adapters.v8);
        if (unbind_result == RIN_GPU_VULKAN_OK) {
            product_v8_bound = 0u;
        } else {
            fprintf(stderr, "V8 process-compute unbind returned %d\n",
                    unbind_result);
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (product_v7_bound) {
        const int unbind_result =
            rin_gpu_vulkan_icd_unbind_product_platform_v7(
                product_adapters.v7);
        if (unbind_result == RIN_GPU_VULKAN_OK) {
            product_v7_bound = 0u;
        } else {
            fprintf(stderr, "V7 process-submit unbind returned %d\n",
                    unbind_result);
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (product_v6_bound) {
        const int unbind_result =
            rin_gpu_vulkan_icd_unbind_product_platform_v6(
                product_adapters.v6);
        if (unbind_result == RIN_GPU_VULKAN_OK) {
            product_v6_bound = 0u;
        } else {
            fprintf(stderr, "V6 process-submit unbind returned %d\n",
                    unbind_result);
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (product_v5_bound)
        (void)rin_gpu_vulkan_icd_unbind_product_platform_v5(
            &product_adapters.v5);
    if (process_backend_initialized) {
        if (product_v6_bound) {
            fprintf(stderr,
                    "process-submit queue tokens retained while V6 remains bound\n");
            result = RIN_VK_ERROR_INITIALIZATION_FAILED;
        } else {
            const int detach_result =
                rin_vulkan_rinos_process_submit_backend_v5_detach(
                    &product_adapters.process_submit_backend);
            if (detach_result != RIN_VULKAN_PRODUCT_OK) {
                fprintf(stderr,
                        "process-submit queue token cleanup returned %d\n",
                        detach_result);
                result = RIN_VK_ERROR_INITIALIZATION_FAILED;
            } else {
                process_backend_initialized = 0u;
            }
        }
    }
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
    puts("RinVulkan native-window SDK process-submit integration: PASS");
    return 0;
}
