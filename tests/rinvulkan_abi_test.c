/* SPDX-License-Identifier: MIT */
#include <rinvulkan/icd.h>
#include <rinvulkan/platform.h>

int main(void) {
    RinVulkanProductPlatformV1 platform = {0};
    return sizeof(platform) == 104u &&
                   sizeof(RinVulkanProductStatusV1) == 128u &&
                   sizeof(RinVulkanProductReportV1) == 128u
               ? 0
               : 1;
}
