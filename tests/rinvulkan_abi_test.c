/* SPDX-License-Identifier: MIT */
#include <rinvulkan/icd.h>
#include <rinvulkan/platform.h>

_Static_assert(sizeof(RinVkExtent2D) == 8u, "VkExtent2D ABI drift");
_Static_assert(sizeof(RinVkSurfaceCapabilitiesKHR) == 52u,
               "VkSurfaceCapabilitiesKHR ABI drift");
_Static_assert(sizeof(RinVkSurfaceFormatKHR) == 8u,
               "VkSurfaceFormatKHR ABI drift");
_Static_assert(sizeof(RinVkDisplayModeParametersKHR) == 12u,
               "VkDisplayModeParametersKHR ABI drift");
_Static_assert(sizeof(RinVkDisplayPlaneCapabilitiesKHR) == 68u,
               "VkDisplayPlaneCapabilitiesKHR ABI drift");
_Static_assert(offsetof(RinVulkanWsiPlatformV3, query_surface_support) ==
                   sizeof(RinVulkanWsiPlatformV2),
               "Vulkan WSI V3 callback prefix drift");
_Static_assert(sizeof(RinVulkanWsiPlatformV3) ==
                   (sizeof(void*) == 8u ? 184u : 144u),
               "Vulkan WSI V3 ABI drift");
_Static_assert(sizeof(RinVulkanWsiSurfacePropertiesV4) == 252u,
               "Vulkan WSI surface properties ABI drift");
_Static_assert(offsetof(RinVulkanWsiPlatformV4, query_surface_properties) ==
                   sizeof(RinVulkanWsiPlatformV3),
               "Vulkan WSI V4 callback prefix drift");
_Static_assert(sizeof(RinVulkanWsiPlatformV4) ==
                   (sizeof(void*) == 8u ? 224u : 180u),
               "Vulkan WSI V4 ABI drift");
_Static_assert(offsetof(RinVulkanProductSubmissionV2, base) == 8u,
               "Vulkan product V2 base prefix drift");
_Static_assert(offsetof(RinVulkanProductPlatformV2, base) == 8u,
               "Vulkan product platform V2 prefix drift");
_Static_assert(sizeof(RinVulkanProductSubmissionV1) == 80u,
               "Vulkan product V1 submission ABI drift");
_Static_assert(sizeof(RinVulkanProductPlatformV1) ==
                   (sizeof(void*) == 8u ? 104u : 72u),
               "Vulkan product V1 platform ABI drift");

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(RinVkDisplayPropertiesKHR) == 48u,
               "VkDisplayPropertiesKHR ABI drift");
_Static_assert(sizeof(RinVkDisplayModePropertiesKHR) == 24u,
               "VkDisplayModePropertiesKHR ABI drift");
_Static_assert(sizeof(RinVkDisplayPlanePropertiesKHR) == 16u,
               "VkDisplayPlanePropertiesKHR ABI drift");
_Static_assert(sizeof(RinVkDisplaySurfaceCreateInfoKHR) == 64u,
               "VkDisplaySurfaceCreateInfoKHR ABI drift");
_Static_assert(sizeof(RinVkSwapchainCreateInfoKHR) == 104u,
               "VkSwapchainCreateInfoKHR ABI drift");
_Static_assert(sizeof(RinVkPresentInfoKHR) == 64u,
               "VkPresentInfoKHR ABI drift");
#endif

int main(void) {
    RinVulkanProductPlatformV1 platform = {0};
    return sizeof(platform) == 104u &&
                   sizeof(RinVulkanProductStatusV1) == 128u &&
                   sizeof(RinVulkanProductReportV1) == 128u &&
                   sizeof(RinVulkanProductSubmissionWaitV1) == 24u &&
                   sizeof(RinVulkanProductSubmissionV2) == 288u &&
                   RIN_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR ==
                       1000001000 &&
                   RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR == 1000001001
               ? 0
               : 1;
}
