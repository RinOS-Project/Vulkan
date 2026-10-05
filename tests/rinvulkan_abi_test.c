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
                   RIN_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR ==
                       1000001000 &&
                   RIN_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR == 1000001001
               ? 0
               : 1;
}
