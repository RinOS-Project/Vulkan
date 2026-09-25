/* SPDX-License-Identifier: MIT */
#ifndef RINVULKAN_PUBLIC_ICD_H
#define RINVULKAN_PUBLIC_ICD_H

#include <stddef.h>
#include <stdint.h>

#include <rinvulkan/platform.h>
#include <rinvulkan/runtime.h>

#if defined(_WIN32) && !defined(_WIN64)
#define RIN_VKAPI_CALL __stdcall
#else
#define RIN_VKAPI_CALL
#endif

#if defined(_WIN32)
#define RIN_VKAPI_ATTR __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define RIN_VKAPI_ATTR __attribute__((visibility("default")))
#else
#define RIN_VKAPI_ATTR
#endif

#define RIN_VK_ICD_LOADER_MAGIC UINT32_C(0x01cdc0de)
#define RIN_VK_ICD_INTERFACE_VERSION 5u
#define RIN_VK_MAX_EXTENSION_NAME_SIZE 256u
#define RIN_VK_MAX_DESCRIPTION_SIZE 256u

#define RIN_VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define RIN_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define RIN_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define RIN_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#define RIN_VK_STRUCTURE_TYPE_SUBMIT_INFO 4
#define RIN_VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO 5
#define RIN_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO 8
#define RIN_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 9
#define RIN_VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 12
#define RIN_VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO 14
#define RIN_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 39
#define RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40
#define RIN_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 42
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES 50
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES 51
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES 52
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES 53
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES 54
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 1000059000
#define RIN_VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 1000059001

#define RIN_VK_SUCCESS 0
#define RIN_VK_NOT_READY 1
#define RIN_VK_TIMEOUT 2
#define RIN_VK_INCOMPLETE 5
#define RIN_VK_ERROR_OUT_OF_HOST_MEMORY (-1)
#define RIN_VK_ERROR_OUT_OF_DEVICE_MEMORY (-2)
#define RIN_VK_ERROR_INITIALIZATION_FAILED (-3)
#define RIN_VK_ERROR_DEVICE_LOST (-4)
#define RIN_VK_ERROR_LAYER_NOT_PRESENT (-6)
#define RIN_VK_ERROR_EXTENSION_NOT_PRESENT (-7)
#define RIN_VK_ERROR_FEATURE_NOT_PRESENT (-8)
#define RIN_VK_ERROR_INCOMPATIBLE_DRIVER (-9)
#define RIN_VK_ERROR_TOO_MANY_OBJECTS (-10)
#define RIN_VK_ERROR_UNKNOWN (-13)

#define RIN_VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 0x00000001u
#define RIN_VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 0x00000002u
#define RIN_VK_COMMAND_POOL_CREATE_PROTECTED_BIT 0x00000004u
#define RIN_VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT 0x00000001u
#define RIN_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x00000001u
#define RIN_VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT 0x00000002u
#define RIN_VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT 0x00000004u
#define RIN_VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT 0x00000001u
#define RIN_VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
#define RIN_VK_COMMAND_BUFFER_LEVEL_SECONDARY 1

typedef int32_t RinVkResult;
typedef int32_t RinVkStructureType;
typedef int32_t RinVkPhysicalDeviceType;
typedef void (RIN_VKAPI_CALL *RinVkVoidFunction)(void);
typedef struct RinVkInstance_T* RinVkInstance;
typedef struct RinVkPhysicalDevice_T* RinVkPhysicalDevice;
typedef struct RinVkDevice_T* RinVkDevice;
typedef struct RinVkQueue_T* RinVkQueue;
#if UINTPTR_MAX == UINT64_MAX
typedef struct RinVkCommandPool_T* RinVkCommandPool;
#else
typedef uint64_t RinVkCommandPool;
#endif
typedef struct RinVkCommandBuffer_T* RinVkCommandBuffer;
typedef uint64_t RinVkBuffer;
typedef uint64_t RinVkImage;
typedef uint64_t RinVkDeviceMemory;
typedef uint64_t RinVkFence;
typedef uint64_t RinVkSemaphore;

typedef struct RinVkApplicationInfo {
    RinVkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} RinVkApplicationInfo;

typedef struct RinVkInstanceCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    const RinVkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
} RinVkInstanceCreateInfo;

typedef struct RinVkExtensionProperties {
    char extensionName[RIN_VK_MAX_EXTENSION_NAME_SIZE];
    uint32_t specVersion;
} RinVkExtensionProperties;

typedef struct RinVkLayerProperties {
    char layerName[RIN_VK_MAX_EXTENSION_NAME_SIZE];
    uint32_t specVersion;
    uint32_t implementationVersion;
    char description[RIN_VK_MAX_DESCRIPTION_SIZE];
} RinVkLayerProperties;

typedef struct RinVkPhysicalDeviceFeatures {
    uint32_t robustBufferAccess;
    uint32_t fullDrawIndexUint32;
    uint32_t imageCubeArray;
    uint32_t independentBlend;
    uint32_t geometryShader;
    uint32_t tessellationShader;
    uint32_t sampleRateShading;
    uint32_t dualSrcBlend;
    uint32_t logicOp;
    uint32_t multiDrawIndirect;
    uint32_t drawIndirectFirstInstance;
    uint32_t depthClamp;
    uint32_t depthBiasClamp;
    uint32_t fillModeNonSolid;
    uint32_t depthBounds;
    uint32_t wideLines;
    uint32_t largePoints;
    uint32_t alphaToOne;
    uint32_t multiViewport;
    uint32_t samplerAnisotropy;
    uint32_t textureCompressionETC2;
    uint32_t textureCompressionASTC_LDR;
    uint32_t textureCompressionBC;
    uint32_t occlusionQueryPrecise;
    uint32_t pipelineStatisticsQuery;
    uint32_t vertexPipelineStoresAndAtomics;
    uint32_t fragmentStoresAndAtomics;
    uint32_t shaderTessellationAndGeometryPointSize;
    uint32_t shaderImageGatherExtended;
    uint32_t shaderStorageImageExtendedFormats;
    uint32_t shaderStorageImageMultisample;
    uint32_t shaderStorageImageReadWithoutFormat;
    uint32_t shaderStorageImageWriteWithoutFormat;
    uint32_t shaderUniformBufferArrayDynamicIndexing;
    uint32_t shaderSampledImageArrayDynamicIndexing;
    uint32_t shaderStorageBufferArrayDynamicIndexing;
    uint32_t shaderStorageImageArrayDynamicIndexing;
    uint32_t shaderClipDistance;
    uint32_t shaderCullDistance;
    uint32_t shaderFloat64;
    uint32_t shaderInt64;
    uint32_t shaderInt16;
    uint32_t shaderResourceResidency;
    uint32_t shaderResourceMinLod;
    uint32_t sparseBinding;
    uint32_t sparseResidencyBuffer;
    uint32_t sparseResidencyImage2D;
    uint32_t sparseResidencyImage3D;
    uint32_t sparseResidency2Samples;
    uint32_t sparseResidency4Samples;
    uint32_t sparseResidency8Samples;
    uint32_t sparseResidency16Samples;
    uint32_t sparseResidencyAliased;
    uint32_t variableMultisampleRate;
    uint32_t inheritedQueries;
} RinVkPhysicalDeviceFeatures;

typedef struct RinVkPhysicalDeviceFeatures2 {
    RinVkStructureType sType;
    void* pNext;
    RinVkPhysicalDeviceFeatures features;
} RinVkPhysicalDeviceFeatures2;

typedef struct RinVkPhysicalDeviceVulkan12Features {
    RinVkStructureType sType;
    void* pNext;
    uint32_t samplerMirrorClampToEdge;
    uint32_t drawIndirectCount;
    uint32_t storageBuffer8BitAccess;
    uint32_t uniformAndStorageBuffer8BitAccess;
    uint32_t storagePushConstant8;
    uint32_t shaderBufferInt64Atomics;
    uint32_t shaderSharedInt64Atomics;
    uint32_t shaderFloat16;
    uint32_t shaderInt8;
    uint32_t descriptorIndexing;
    uint32_t shaderInputAttachmentArrayDynamicIndexing;
    uint32_t shaderUniformTexelBufferArrayDynamicIndexing;
    uint32_t shaderStorageTexelBufferArrayDynamicIndexing;
    uint32_t shaderUniformBufferArrayNonUniformIndexing;
    uint32_t shaderSampledImageArrayNonUniformIndexing;
    uint32_t shaderStorageBufferArrayNonUniformIndexing;
    uint32_t shaderStorageImageArrayNonUniformIndexing;
    uint32_t shaderInputAttachmentArrayNonUniformIndexing;
    uint32_t shaderUniformTexelBufferArrayNonUniformIndexing;
    uint32_t shaderStorageTexelBufferArrayNonUniformIndexing;
    uint32_t descriptorBindingUniformBufferUpdateAfterBind;
    uint32_t descriptorBindingSampledImageUpdateAfterBind;
    uint32_t descriptorBindingStorageImageUpdateAfterBind;
    uint32_t descriptorBindingStorageBufferUpdateAfterBind;
    uint32_t descriptorBindingUniformTexelBufferUpdateAfterBind;
    uint32_t descriptorBindingStorageTexelBufferUpdateAfterBind;
    uint32_t descriptorBindingUpdateUnusedWhilePending;
    uint32_t descriptorBindingPartiallyBound;
    uint32_t descriptorBindingVariableDescriptorCount;
    uint32_t runtimeDescriptorArray;
    uint32_t samplerFilterMinmax;
    uint32_t scalarBlockLayout;
    uint32_t imagelessFramebuffer;
    uint32_t uniformBufferStandardLayout;
    uint32_t shaderSubgroupExtendedTypes;
    uint32_t separateDepthStencilLayouts;
    uint32_t hostQueryReset;
    uint32_t timelineSemaphore;
    uint32_t bufferDeviceAddress;
    uint32_t bufferDeviceAddressCaptureReplay;
    uint32_t bufferDeviceAddressMultiDevice;
    uint32_t vulkanMemoryModel;
    uint32_t vulkanMemoryModelDeviceScope;
    uint32_t vulkanMemoryModelAvailabilityVisibilityChains;
    uint32_t shaderOutputViewportIndex;
    uint32_t shaderOutputLayer;
    uint32_t subgroupBroadcastDynamicId;
} RinVkPhysicalDeviceVulkan12Features;

typedef struct RinVkPhysicalDeviceVulkan13Features {
    RinVkStructureType sType;
    void* pNext;
    uint32_t robustImageAccess;
    uint32_t inlineUniformBlock;
    uint32_t descriptorBindingInlineUniformBlockUpdateAfterBind;
    uint32_t pipelineCreationCacheControl;
    uint32_t privateData;
    uint32_t shaderDemoteToHelperInvocation;
    uint32_t shaderTerminateInvocation;
    uint32_t subgroupSizeControl;
    uint32_t computeFullSubgroups;
    uint32_t synchronization2;
    uint32_t textureCompressionASTC_HDR;
    uint32_t shaderZeroInitializeWorkgroupMemory;
    uint32_t dynamicRendering;
    uint32_t shaderIntegerDotProduct;
    uint32_t maintenance4;
} RinVkPhysicalDeviceVulkan13Features;

typedef struct RinVkPhysicalDeviceLimits {
    uint32_t maxImageDimension1D;
    uint32_t maxImageDimension2D;
    uint32_t maxImageDimension3D;
    uint32_t maxImageDimensionCube;
    uint32_t maxImageArrayLayers;
    uint32_t maxTexelBufferElements;
    uint32_t maxUniformBufferRange;
    uint32_t maxStorageBufferRange;
    uint32_t maxPushConstantsSize;
    uint32_t maxMemoryAllocationCount;
    uint32_t maxSamplerAllocationCount;
    uint64_t bufferImageGranularity;
    uint64_t sparseAddressSpaceSize;
    uint32_t maxBoundDescriptorSets;
    uint32_t maxPerStageDescriptorSamplers;
    uint32_t maxPerStageDescriptorUniformBuffers;
    uint32_t maxPerStageDescriptorStorageBuffers;
    uint32_t maxPerStageDescriptorSampledImages;
    uint32_t maxPerStageDescriptorStorageImages;
    uint32_t maxPerStageDescriptorInputAttachments;
    uint32_t maxPerStageResources;
    uint32_t maxDescriptorSetSamplers;
    uint32_t maxDescriptorSetUniformBuffers;
    uint32_t maxDescriptorSetUniformBuffersDynamic;
    uint32_t maxDescriptorSetStorageBuffers;
    uint32_t maxDescriptorSetStorageBuffersDynamic;
    uint32_t maxDescriptorSetSampledImages;
    uint32_t maxDescriptorSetStorageImages;
    uint32_t maxDescriptorSetInputAttachments;
    uint32_t maxVertexInputAttributes;
    uint32_t maxVertexInputBindings;
    uint32_t maxVertexInputAttributeOffset;
    uint32_t maxVertexInputBindingStride;
    uint32_t maxVertexOutputComponents;
    uint32_t maxTessellationGenerationLevel;
    uint32_t maxTessellationPatchSize;
    uint32_t maxTessellationControlPerVertexInputComponents;
    uint32_t maxTessellationControlPerVertexOutputComponents;
    uint32_t maxTessellationControlPerPatchOutputComponents;
    uint32_t maxTessellationControlTotalOutputComponents;
    uint32_t maxTessellationEvaluationInputComponents;
    uint32_t maxTessellationEvaluationOutputComponents;
    uint32_t maxGeometryShaderInvocations;
    uint32_t maxGeometryInputComponents;
    uint32_t maxGeometryOutputComponents;
    uint32_t maxGeometryOutputVertices;
    uint32_t maxGeometryTotalOutputComponents;
    uint32_t maxFragmentInputComponents;
    uint32_t maxFragmentOutputAttachments;
    uint32_t maxFragmentDualSrcAttachments;
    uint32_t maxFragmentCombinedOutputResources;
    uint32_t maxComputeSharedMemorySize;
    uint32_t maxComputeWorkGroupCount[3];
    uint32_t maxComputeWorkGroupInvocations;
    uint32_t maxComputeWorkGroupSize[3];
    uint32_t subPixelPrecisionBits;
    uint32_t subTexelPrecisionBits;
    uint32_t mipmapPrecisionBits;
    uint32_t maxDrawIndexedIndexValue;
    uint32_t maxDrawIndirectCount;
    float maxSamplerLodBias;
    float maxSamplerAnisotropy;
    uint32_t maxViewports;
    uint32_t maxViewportDimensions[2];
    float viewportBoundsRange[2];
    uint32_t viewportSubPixelBits;
    size_t minMemoryMapAlignment;
    uint64_t minTexelBufferOffsetAlignment;
    uint64_t minUniformBufferOffsetAlignment;
    uint64_t minStorageBufferOffsetAlignment;
    int32_t minTexelOffset;
    uint32_t maxTexelOffset;
    int32_t minTexelGatherOffset;
    uint32_t maxTexelGatherOffset;
    float minInterpolationOffset;
    float maxInterpolationOffset;
    uint32_t subPixelInterpolationOffsetBits;
    uint32_t maxFramebufferWidth;
    uint32_t maxFramebufferHeight;
    uint32_t maxFramebufferLayers;
    uint32_t framebufferColorSampleCounts;
    uint32_t framebufferDepthSampleCounts;
    uint32_t framebufferStencilSampleCounts;
    uint32_t framebufferNoAttachmentsSampleCounts;
    uint32_t maxColorAttachments;
    uint32_t sampledImageColorSampleCounts;
    uint32_t sampledImageIntegerSampleCounts;
    uint32_t sampledImageDepthSampleCounts;
    uint32_t sampledImageStencilSampleCounts;
    uint32_t storageImageSampleCounts;
    uint32_t maxSampleMaskWords;
    uint32_t timestampComputeAndGraphics;
    float timestampPeriod;
    uint32_t maxClipDistances;
    uint32_t maxCullDistances;
    uint32_t maxCombinedClipAndCullDistances;
    uint32_t discreteQueuePriorities;
    float pointSizeRange[2];
    float lineWidthRange[2];
    float pointSizeGranularity;
    float lineWidthGranularity;
    uint32_t strictLines;
    uint32_t standardSampleLocations;
    uint64_t optimalBufferCopyOffsetAlignment;
    uint64_t optimalBufferCopyRowPitchAlignment;
    uint64_t nonCoherentAtomSize;
} RinVkPhysicalDeviceLimits;

typedef struct RinVkPhysicalDeviceSparseProperties {
    uint32_t residencyStandard2DBlockShape;
    uint32_t residencyStandard2DMultisampleBlockShape;
    uint32_t residencyStandard3DBlockShape;
    uint32_t residencyAlignedMipSize;
    uint32_t residencyNonResidentStrict;
} RinVkPhysicalDeviceSparseProperties;

typedef struct RinVkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    RinVkPhysicalDeviceType deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
    RinVkPhysicalDeviceLimits limits;
    RinVkPhysicalDeviceSparseProperties sparseProperties;
} RinVkPhysicalDeviceProperties;

typedef struct RinVkPhysicalDeviceProperties2 {
    RinVkStructureType sType;
    void* pNext;
    RinVkPhysicalDeviceProperties properties;
} RinVkPhysicalDeviceProperties2;

typedef struct RinVkPhysicalDeviceVulkan11Properties {
    RinVkStructureType sType;
    void* pNext;
    uint8_t deviceUUID[16];
    uint8_t driverUUID[16];
    uint8_t deviceLUID[8];
    uint32_t deviceNodeMask;
    uint32_t deviceLUIDValid;
    uint32_t subgroupSize;
    uint32_t subgroupSupportedStages;
    uint32_t subgroupSupportedOperations;
    uint32_t subgroupQuadOperationsInAllStages;
    int32_t pointClippingBehavior;
    uint32_t maxMultiviewViewCount;
    uint32_t maxMultiviewInstanceIndex;
    uint32_t protectedNoFault;
    uint32_t maxPerSetDescriptors;
    uint64_t maxMemoryAllocationSize;
} RinVkPhysicalDeviceVulkan11Properties;

typedef struct RinVkConformanceVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t subminor;
    uint8_t patch;
} RinVkConformanceVersion;

typedef struct RinVkPhysicalDeviceVulkan12Properties {
    RinVkStructureType sType;
    void* pNext;
    int32_t driverID;
    char driverName[256];
    char driverInfo[256];
    RinVkConformanceVersion conformanceVersion;
    int32_t denormBehaviorIndependence;
    int32_t roundingModeIndependence;
    uint32_t shaderSignedZeroInfNanPreserveFloat16;
    uint32_t shaderSignedZeroInfNanPreserveFloat32;
    uint32_t shaderSignedZeroInfNanPreserveFloat64;
    uint32_t shaderDenormPreserveFloat16;
    uint32_t shaderDenormPreserveFloat32;
    uint32_t shaderDenormPreserveFloat64;
    uint32_t shaderDenormFlushToZeroFloat16;
    uint32_t shaderDenormFlushToZeroFloat32;
    uint32_t shaderDenormFlushToZeroFloat64;
    uint32_t shaderRoundingModeRTEFloat16;
    uint32_t shaderRoundingModeRTEFloat32;
    uint32_t shaderRoundingModeRTEFloat64;
    uint32_t shaderRoundingModeRTZFloat16;
    uint32_t shaderRoundingModeRTZFloat32;
    uint32_t shaderRoundingModeRTZFloat64;
    uint32_t maxUpdateAfterBindDescriptorsInAllPools;
    uint32_t shaderUniformBufferArrayNonUniformIndexingNative;
    uint32_t shaderSampledImageArrayNonUniformIndexingNative;
    uint32_t shaderStorageBufferArrayNonUniformIndexingNative;
    uint32_t shaderStorageImageArrayNonUniformIndexingNative;
    uint32_t shaderInputAttachmentArrayNonUniformIndexingNative;
    uint32_t robustBufferAccessUpdateAfterBind;
    uint32_t quadDivergentImplicitLod;
    uint32_t maxPerStageDescriptorUpdateAfterBindSamplers;
    uint32_t maxPerStageDescriptorUpdateAfterBindUniformBuffers;
    uint32_t maxPerStageDescriptorUpdateAfterBindStorageBuffers;
    uint32_t maxPerStageDescriptorUpdateAfterBindSampledImages;
    uint32_t maxPerStageDescriptorUpdateAfterBindStorageImages;
    uint32_t maxPerStageDescriptorUpdateAfterBindInputAttachments;
    uint32_t maxPerStageUpdateAfterBindResources;
    uint32_t maxDescriptorSetUpdateAfterBindSamplers;
    uint32_t maxDescriptorSetUpdateAfterBindUniformBuffers;
    uint32_t maxDescriptorSetUpdateAfterBindUniformBuffersDynamic;
    uint32_t maxDescriptorSetUpdateAfterBindStorageBuffers;
    uint32_t maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
    uint32_t maxDescriptorSetUpdateAfterBindSampledImages;
    uint32_t maxDescriptorSetUpdateAfterBindStorageImages;
    uint32_t maxDescriptorSetUpdateAfterBindInputAttachments;
    uint32_t supportedDepthResolveModes;
    uint32_t supportedStencilResolveModes;
    uint32_t independentResolveNone;
    uint32_t independentResolve;
    uint32_t filterMinmaxSingleComponentFormats;
    uint32_t filterMinmaxImageComponentMapping;
    uint64_t maxTimelineSemaphoreValueDifference;
    uint32_t framebufferIntegerColorSampleCounts;
} RinVkPhysicalDeviceVulkan12Properties;

typedef struct RinVkPhysicalDeviceVulkan13Properties {
    RinVkStructureType sType;
    void* pNext;
    uint32_t minSubgroupSize;
    uint32_t maxSubgroupSize;
    uint32_t maxComputeWorkgroupSubgroups;
    uint32_t requiredSubgroupSizeStages;
    uint32_t maxInlineUniformBlockSize;
    uint32_t maxPerStageDescriptorInlineUniformBlocks;
    uint32_t maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks;
    uint32_t maxDescriptorSetInlineUniformBlocks;
    uint32_t maxDescriptorSetUpdateAfterBindInlineUniformBlocks;
    uint32_t maxInlineUniformTotalSize;
    uint32_t integerDotProduct8BitUnsignedAccelerated;
    uint32_t integerDotProduct8BitSignedAccelerated;
    uint32_t integerDotProduct8BitMixedSignednessAccelerated;
    uint32_t integerDotProduct4x8BitPackedUnsignedAccelerated;
    uint32_t integerDotProduct4x8BitPackedSignedAccelerated;
    uint32_t integerDotProduct4x8BitPackedMixedSignednessAccelerated;
    uint32_t integerDotProduct16BitUnsignedAccelerated;
    uint32_t integerDotProduct16BitSignedAccelerated;
    uint32_t integerDotProduct16BitMixedSignednessAccelerated;
    uint32_t integerDotProduct32BitUnsignedAccelerated;
    uint32_t integerDotProduct32BitSignedAccelerated;
    uint32_t integerDotProduct32BitMixedSignednessAccelerated;
    uint32_t integerDotProduct64BitUnsignedAccelerated;
    uint32_t integerDotProduct64BitSignedAccelerated;
    uint32_t integerDotProduct64BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating8BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating8BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating16BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating16BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating32BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating32BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated;
    uint32_t integerDotProductAccumulatingSaturating64BitUnsignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating64BitSignedAccelerated;
    uint32_t integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated;
    uint64_t storageTexelBufferOffsetAlignmentBytes;
    uint32_t storageTexelBufferOffsetSingleTexelAlignment;
    uint64_t uniformTexelBufferOffsetAlignmentBytes;
    uint32_t uniformTexelBufferOffsetSingleTexelAlignment;
    uint64_t maxBufferSize;
} RinVkPhysicalDeviceVulkan13Properties;

typedef struct RinVkExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} RinVkExtent3D;

typedef struct RinVkQueueFamilyProperties {
    uint32_t queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    RinVkExtent3D minImageTransferGranularity;
} RinVkQueueFamilyProperties;

typedef struct RinVkMemoryType {
    uint32_t propertyFlags;
    uint32_t heapIndex;
} RinVkMemoryType;

typedef struct RinVkMemoryHeap {
    uint64_t size;
    uint32_t flags;
    uint32_t reserved;
} RinVkMemoryHeap;

typedef struct RinVkPhysicalDeviceMemoryProperties {
    uint32_t memoryTypeCount;
    RinVkMemoryType memoryTypes[32];
    uint32_t memoryHeapCount;
    RinVkMemoryHeap memoryHeaps[16];
} RinVkPhysicalDeviceMemoryProperties;

typedef struct RinVkDeviceQueueCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    const float* pQueuePriorities;
} RinVkDeviceQueueCreateInfo;

typedef struct RinVkDeviceCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueCreateInfoCount;
    const RinVkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const RinVkPhysicalDeviceFeatures* pEnabledFeatures;
} RinVkDeviceCreateInfo;

typedef struct RinVkCommandPoolCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueFamilyIndex;
} RinVkCommandPoolCreateInfo;

typedef struct RinVkCommandBufferAllocateInfo {
    RinVkStructureType sType;
    const void* pNext;
    RinVkCommandPool commandPool;
    int32_t level;
    uint32_t commandBufferCount;
} RinVkCommandBufferAllocateInfo;

typedef struct RinVkCommandBufferBeginInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    const void* pInheritanceInfo;
} RinVkCommandBufferBeginInfo;

typedef struct RinVkSubmitInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const uint64_t* pWaitSemaphores;
    const uint32_t* pWaitDstStageMask;
    uint32_t commandBufferCount;
    const RinVkCommandBuffer* pCommandBuffers;
    uint32_t signalSemaphoreCount;
    const uint64_t* pSignalSemaphores;
} RinVkSubmitInfo;

typedef struct RinVkFenceCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
} RinVkFenceCreateInfo;

typedef struct RinVkSemaphoreCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
} RinVkSemaphoreCreateInfo;

typedef struct RinVkMemoryAllocateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint64_t allocationSize;
    uint32_t memoryTypeIndex;
} RinVkMemoryAllocateInfo;

typedef struct RinVkBufferCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint64_t size;
    uint32_t usage;
    uint32_t sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
} RinVkBufferCreateInfo;

typedef struct RinVkImageCreateInfo {
    RinVkStructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t imageType;
    int32_t format;
    RinVkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    uint32_t samples;
    uint32_t tiling;
    uint32_t usage;
    uint32_t sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
} RinVkImageCreateInfo;

typedef struct RinVkMemoryRequirements {
    uint64_t size;
    uint64_t alignment;
    uint32_t memoryTypeBits;
} RinVkMemoryRequirements;

typedef struct RinVkBufferCopy {
    uint64_t srcOffset;
    uint64_t dstOffset;
    uint64_t size;
} RinVkBufferCopy;

typedef struct RinVkImageSubresourceLayers {
    uint32_t aspectMask;
    uint32_t mipLevel;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
} RinVkImageSubresourceLayers;

typedef struct RinVkOffset3D {
    int32_t x;
    int32_t y;
    int32_t z;
} RinVkOffset3D;

typedef struct RinVkBufferImageCopy {
    uint64_t bufferOffset;
    uint32_t bufferRowLength;
    uint32_t bufferImageHeight;
    RinVkImageSubresourceLayers imageSubresource;
    RinVkOffset3D imageOffset;
    RinVkExtent3D imageExtent;
} RinVkBufferImageCopy;

typedef struct RinVkImageCopy {
    RinVkImageSubresourceLayers srcSubresource;
    RinVkOffset3D srcOffset;
    RinVkImageSubresourceLayers dstSubresource;
    RinVkOffset3D dstOffset;
    RinVkExtent3D extent;
} RinVkImageCopy;

typedef union RinVkClearColorValue {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
} RinVkClearColorValue;

typedef struct RinVkImageSubresourceRange {
    uint32_t aspectMask;
    uint32_t baseMipLevel;
    uint32_t levelCount;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
} RinVkImageSubresourceRange;

#define RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT UINT32_C(0x00000001)
#define RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT UINT32_C(0x00000002)
#define RIN_VK_BUFFER_USAGE_KNOWN \
    (RIN_VK_BUFFER_USAGE_TRANSFER_SRC_BIT | RIN_VK_BUFFER_USAGE_TRANSFER_DST_BIT)
#define RIN_VK_IMAGE_TYPE_2D 1u
#define RIN_VK_FORMAT_R8G8B8A8_UNORM 37
#define RIN_VK_IMAGE_TILING_OPTIMAL 0u
#define RIN_VK_SAMPLE_COUNT_1_BIT 1u
#define RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT UINT32_C(0x00000001)
#define RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT UINT32_C(0x00000002)
#define RIN_VK_IMAGE_USAGE_KNOWN \
    (RIN_VK_IMAGE_USAGE_TRANSFER_SRC_BIT | RIN_VK_IMAGE_USAGE_TRANSFER_DST_BIT)
#define RIN_VK_IMAGE_ASPECT_COLOR_BIT UINT32_C(0x00000001)
#define RIN_VK_IMAGE_LAYOUT_GENERAL UINT32_C(1)
#define RIN_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL UINT32_C(6)
#define RIN_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL UINT32_C(7)
#define RIN_VK_SHARING_MODE_EXCLUSIVE 0u
#define RIN_VK_FENCE_CREATE_SIGNALED_BIT UINT32_C(0x00000001)
#define RIN_VK_FENCE_CREATE_KNOWN RIN_VK_FENCE_CREATE_SIGNALED_BIT
#define RIN_VK_SEMAPHORE_CREATE_KNOWN 0u

#if UINTPTR_MAX == UINT64_MAX
#define RIN_VK_APPLICATION_INFO_SIZE 48u
#define RIN_VK_INSTANCE_CREATE_INFO_SIZE 64u
#define RIN_VK_FENCE_CREATE_INFO_SIZE 24u
#define RIN_VK_SEMAPHORE_CREATE_INFO_SIZE 24u
#define RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE 40u
#define RIN_VK_DEVICE_CREATE_INFO_SIZE 72u
#define RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE 24u
#define RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE 32u
#define RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE 32u
#define RIN_VK_SUBMIT_INFO_SIZE 72u
#define RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE 504u
#define RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE 824u
#define RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET 304u
#define RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET 424u
#define RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET 296u
#define RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET 800u
#define RIN_VK_PHYSICAL_FEATURES_2_SIZE 240u
#define RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET 16u
#define RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE 208u
#define RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE 80u
#define RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET 52u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET 164u
#define RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET 168u
#define RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET 52u
#define RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET 64u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET 72u
#define RIN_VK_PHYSICAL_PROPERTIES_2_SIZE 840u
#define RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET 16u
#define RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE 112u
#define RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET 104u
#define RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE 736u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET 720u
#define RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE 216u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET 208u
#else
#define RIN_VK_APPLICATION_INFO_SIZE 28u
#define RIN_VK_INSTANCE_CREATE_INFO_SIZE 32u
#define RIN_VK_FENCE_CREATE_INFO_SIZE 12u
#define RIN_VK_SEMAPHORE_CREATE_INFO_SIZE 12u
#define RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE 24u
#define RIN_VK_DEVICE_CREATE_INFO_SIZE 40u
#define RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE 16u
#define RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE 24u
#define RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE 16u
#define RIN_VK_SUBMIT_INFO_SIZE 36u
#define RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE 488u
#define RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE 800u
#define RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET 296u
#define RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET 412u
#define RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET 292u
#define RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET 780u
#define RIN_VK_PHYSICAL_FEATURES_2_SIZE 228u
#define RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET 8u
#define RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE 196u
#define RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE 68u
#define RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET 44u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET 156u
#define RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET 160u
#define RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET 44u
#define RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET 56u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET 64u
#define RIN_VK_PHYSICAL_PROPERTIES_2_SIZE 808u
#define RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET 8u
#define RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE 100u
#define RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET 92u
#define RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE 724u
#define RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET 712u
#define RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE 200u
#define RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET 192u
#endif

#if defined(__cplusplus)
static_assert(sizeof(RinVkApplicationInfo) == RIN_VK_APPLICATION_INFO_SIZE,
              "Vulkan application info ABI drift");
static_assert(sizeof(RinVkInstanceCreateInfo) ==
                  RIN_VK_INSTANCE_CREATE_INFO_SIZE,
              "Vulkan instance create ABI drift");
static_assert(sizeof(RinVkExtensionProperties) == 260u,
              "Vulkan extension properties ABI drift");
static_assert(sizeof(RinVkLayerProperties) == 520u,
              "Vulkan layer properties ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceFeatures) == 220u,
              "Vulkan physical features ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceFeatures2) ==
                  RIN_VK_PHYSICAL_FEATURES_2_SIZE,
              "Vulkan physical features2 ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceFeatures2, features) ==
                  RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET,
              "Vulkan physical features2 offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan12Features) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE,
              "Vulkan 1.2 physical features ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan13Features) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE,
              "Vulkan 1.3 physical features ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                       descriptorIndexing) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET,
              "Vulkan 1.2 descriptor-indexing offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                       timelineSemaphore) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET,
              "Vulkan 1.2 timeline-semaphore offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                       bufferDeviceAddress) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET,
              "Vulkan 1.2 buffer-device-address offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                       synchronization2) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET,
              "Vulkan 1.3 synchronization2 offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                       dynamicRendering) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET,
              "Vulkan 1.3 dynamic-rendering offset drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                       maintenance4) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET,
              "Vulkan 1.3 maintenance4 offset drift");
static_assert(sizeof(RinVkPhysicalDeviceLimits) ==
                  RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE,
              "Vulkan physical limits ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceSparseProperties) == 20u,
              "Vulkan sparse properties ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceProperties) ==
                  RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE,
              "Vulkan physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceLimits, minMemoryMapAlignment) ==
                  RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET,
              "Vulkan physical limits map-alignment offset drift");
static_assert(offsetof(RinVkPhysicalDeviceLimits, timestampPeriod) ==
                  RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET,
              "Vulkan physical limits timestamp offset drift");
static_assert(offsetof(RinVkPhysicalDeviceProperties, limits) ==
                  RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET,
              "Vulkan physical properties limits offset drift");
static_assert(offsetof(RinVkPhysicalDeviceProperties, sparseProperties) ==
                  RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET,
              "Vulkan physical properties sparse offset drift");
static_assert(sizeof(RinVkPhysicalDeviceProperties2) ==
                  RIN_VK_PHYSICAL_PROPERTIES_2_SIZE,
              "Vulkan physical properties2 ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceProperties2, properties) ==
                  RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET,
              "Vulkan physical properties2 offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan11Properties) ==
                  RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE,
              "Vulkan 1.1 physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan11Properties,
                       maxMemoryAllocationSize) ==
                  RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET,
              "Vulkan 1.1 maximum allocation offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan12Properties) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE,
              "Vulkan 1.2 physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan12Properties,
                       maxTimelineSemaphoreValueDifference) ==
                  RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET,
              "Vulkan 1.2 timeline limit offset drift");
static_assert(sizeof(RinVkPhysicalDeviceVulkan13Properties) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE,
              "Vulkan 1.3 physical properties ABI drift");
static_assert(offsetof(RinVkPhysicalDeviceVulkan13Properties,
                       maxBufferSize) ==
                  RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET,
              "Vulkan 1.3 maximum buffer offset drift");
static_assert(sizeof(RinVkQueueFamilyProperties) == 24u,
              "Vulkan queue family properties ABI drift");
static_assert(sizeof(RinVkPhysicalDeviceMemoryProperties) == 520u,
              "Vulkan memory properties ABI drift");
static_assert(sizeof(RinVkDeviceQueueCreateInfo) ==
                  RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE,
              "Vulkan device queue create ABI drift");
static_assert(sizeof(RinVkDeviceCreateInfo) ==
                  RIN_VK_DEVICE_CREATE_INFO_SIZE,
              "Vulkan device create ABI drift");
static_assert(sizeof(RinVkCommandPoolCreateInfo) ==
                  RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE,
              "Vulkan command-pool create ABI drift");
static_assert(sizeof(RinVkCommandBufferAllocateInfo) ==
                  RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE,
              "Vulkan command-buffer allocate ABI drift");
static_assert(sizeof(RinVkCommandBufferBeginInfo) ==
                  RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE,
              "Vulkan command-buffer begin ABI drift");
static_assert(sizeof(RinVkSubmitInfo) == RIN_VK_SUBMIT_INFO_SIZE,
              "Vulkan submit ABI drift");
static_assert(sizeof(RinVkFenceCreateInfo) == RIN_VK_FENCE_CREATE_INFO_SIZE,
              "Vulkan fence-create ABI drift");
static_assert(sizeof(RinVkSemaphoreCreateInfo) ==
                  RIN_VK_SEMAPHORE_CREATE_INFO_SIZE,
              "Vulkan semaphore-create ABI drift");
static_assert(sizeof(RinVkBufferCopy) == 24u,
              "Vulkan buffer-copy ABI drift");
static_assert(sizeof(RinVkCommandPool) == 8u,
              "Vulkan command-pool handle ABI drift");
static_assert(sizeof(RinVkCommandBuffer) == sizeof(void*),
              "Vulkan command-buffer handle ABI drift");
#else
_Static_assert(sizeof(RinVkApplicationInfo) == RIN_VK_APPLICATION_INFO_SIZE,
               "Vulkan application info ABI drift");
_Static_assert(sizeof(RinVkInstanceCreateInfo) ==
                   RIN_VK_INSTANCE_CREATE_INFO_SIZE,
               "Vulkan instance create ABI drift");
_Static_assert(sizeof(RinVkExtensionProperties) == 260u,
               "Vulkan extension properties ABI drift");
_Static_assert(sizeof(RinVkLayerProperties) == 520u,
               "Vulkan layer properties ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceFeatures) == 220u,
               "Vulkan physical features ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceFeatures2) ==
                   RIN_VK_PHYSICAL_FEATURES_2_SIZE,
               "Vulkan physical features2 ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceFeatures2, features) ==
                   RIN_VK_PHYSICAL_FEATURES_2_FEATURES_OFFSET,
               "Vulkan physical features2 offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan12Features) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_FEATURES_SIZE,
               "Vulkan 1.2 physical features ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan13Features) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_FEATURES_SIZE,
               "Vulkan 1.3 physical features ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                        descriptorIndexing) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_DESCRIPTOR_OFFSET,
               "Vulkan 1.2 descriptor-indexing offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                        timelineSemaphore) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_OFFSET,
               "Vulkan 1.2 timeline-semaphore offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Features,
                        bufferDeviceAddress) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_ADDRESS_OFFSET,
               "Vulkan 1.2 buffer-device-address offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                        synchronization2) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_SYNC_OFFSET,
               "Vulkan 1.3 synchronization2 offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                        dynamicRendering) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_RENDER_OFFSET,
               "Vulkan 1.3 dynamic-rendering offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Features,
                        maintenance4) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_MAINTENANCE_OFFSET,
               "Vulkan 1.3 maintenance4 offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceLimits) ==
                   RIN_VK_PHYSICAL_DEVICE_LIMITS_SIZE,
               "Vulkan physical limits ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceSparseProperties) == 20u,
               "Vulkan sparse properties ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceProperties) ==
                   RIN_VK_PHYSICAL_DEVICE_PROPERTIES_SIZE,
               "Vulkan physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceLimits, minMemoryMapAlignment) ==
                   RIN_VK_PHYSICAL_LIMITS_MIN_MAP_OFFSET,
               "Vulkan physical limits map-alignment offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceLimits, timestampPeriod) ==
                   RIN_VK_PHYSICAL_LIMITS_TIMESTAMP_OFFSET,
               "Vulkan physical limits timestamp offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceProperties, limits) ==
                   RIN_VK_PHYSICAL_PROPERTIES_LIMITS_OFFSET,
               "Vulkan physical properties limits offset drift");
_Static_assert(offsetof(RinVkPhysicalDeviceProperties, sparseProperties) ==
                   RIN_VK_PHYSICAL_PROPERTIES_SPARSE_OFFSET,
               "Vulkan physical properties sparse offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceProperties2) ==
                   RIN_VK_PHYSICAL_PROPERTIES_2_SIZE,
               "Vulkan physical properties2 ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceProperties2, properties) ==
                   RIN_VK_PHYSICAL_PROPERTIES_2_PROPERTIES_OFFSET,
               "Vulkan physical properties2 offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan11Properties) ==
                   RIN_VK_PHYSICAL_VULKAN_1_1_PROPERTIES_SIZE,
               "Vulkan 1.1 physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan11Properties,
                        maxMemoryAllocationSize) ==
                   RIN_VK_PHYSICAL_VULKAN_1_1_MAX_ALLOCATION_OFFSET,
               "Vulkan 1.1 maximum allocation offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan12Properties) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_PROPERTIES_SIZE,
               "Vulkan 1.2 physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan12Properties,
                        maxTimelineSemaphoreValueDifference) ==
                   RIN_VK_PHYSICAL_VULKAN_1_2_TIMELINE_LIMIT_OFFSET,
               "Vulkan 1.2 timeline limit offset drift");
_Static_assert(sizeof(RinVkPhysicalDeviceVulkan13Properties) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_PROPERTIES_SIZE,
               "Vulkan 1.3 physical properties ABI drift");
_Static_assert(offsetof(RinVkPhysicalDeviceVulkan13Properties,
                        maxBufferSize) ==
                   RIN_VK_PHYSICAL_VULKAN_1_3_MAX_BUFFER_OFFSET,
               "Vulkan 1.3 maximum buffer offset drift");
_Static_assert(sizeof(RinVkQueueFamilyProperties) == 24u,
               "Vulkan queue family properties ABI drift");
_Static_assert(sizeof(RinVkPhysicalDeviceMemoryProperties) == 520u,
               "Vulkan memory properties ABI drift");
_Static_assert(sizeof(RinVkDeviceQueueCreateInfo) ==
                   RIN_VK_DEVICE_QUEUE_CREATE_INFO_SIZE,
               "Vulkan device queue create ABI drift");
_Static_assert(sizeof(RinVkDeviceCreateInfo) ==
                   RIN_VK_DEVICE_CREATE_INFO_SIZE,
               "Vulkan device create ABI drift");
_Static_assert(sizeof(RinVkCommandPoolCreateInfo) ==
                   RIN_VK_COMMAND_POOL_CREATE_INFO_SIZE,
               "Vulkan command-pool create ABI drift");
_Static_assert(sizeof(RinVkCommandBufferAllocateInfo) ==
                   RIN_VK_COMMAND_BUFFER_ALLOCATE_INFO_SIZE,
               "Vulkan command-buffer allocate ABI drift");
_Static_assert(sizeof(RinVkCommandBufferBeginInfo) ==
                   RIN_VK_COMMAND_BUFFER_BEGIN_INFO_SIZE,
               "Vulkan command-buffer begin ABI drift");
_Static_assert(sizeof(RinVkSubmitInfo) == RIN_VK_SUBMIT_INFO_SIZE,
               "Vulkan submit ABI drift");
_Static_assert(sizeof(RinVkFenceCreateInfo) == RIN_VK_FENCE_CREATE_INFO_SIZE,
               "Vulkan fence-create ABI drift");
_Static_assert(sizeof(RinVkSemaphoreCreateInfo) ==
                   RIN_VK_SEMAPHORE_CREATE_INFO_SIZE,
               "Vulkan semaphore-create ABI drift");
_Static_assert(sizeof(RinVkBufferCopy) == 24u,
               "Vulkan buffer-copy ABI drift");
_Static_assert(sizeof(RinVkCommandPool) == 8u,
               "Vulkan command-pool handle ABI drift");
_Static_assert(sizeof(RinVkCommandBuffer) == sizeof(void*),
               "Vulkan command-buffer handle ABI drift");
#endif

int rin_gpu_vulkan_icd_bind_runtime(RinGpuVulkanRuntimeV1* runtime);
int rin_gpu_vulkan_icd_unbind_runtime(RinGpuVulkanRuntimeV1* runtime);
/* Resource operations are enabled only after binding the real RinGPU product
 * owner.  The binding verifies IOMMU/domain epoch per logical device before
 * every allocation; a missing or stale owner is never treated as software
 * memory. */
int rin_gpu_vulkan_icd_bind_product_platform(
    RinVulkanProductPlatformV1* platform);
int rin_gpu_vulkan_icd_unbind_product_platform(
    RinVulkanProductPlatformV1* platform);
/* Product completion is driven by the RinGPU service loop, not by a forged
 * synchronous Vulkan wait.  It retires only work reported complete by the
 * product runtime and leaves still-pending command buffers leased. */
RinVkResult rin_gpu_vulkan_icd_maintain(RinVkDevice device);

RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* version);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vk_icdGetInstanceProcAddr(RinVkInstance instance, const char* name);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vk_icdGetPhysicalDeviceProcAddr(RinVkInstance instance, const char* name);

RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateInstanceVersion(uint32_t* api_version);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateInstanceExtensionProperties(
    const char* layer_name, uint32_t* property_count,
    RinVkExtensionProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateDeviceExtensionProperties(
    RinVkPhysicalDevice physical_device, const char* layer_name,
    uint32_t* property_count, RinVkExtensionProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumerateInstanceLayerProperties(uint32_t* property_count,
                                   RinVkLayerProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateInstance(const RinVkInstanceCreateInfo* create_info,
                 const void* allocator, RinVkInstance* instance_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyInstance(RinVkInstance instance, const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEnumeratePhysicalDevices(RinVkInstance instance,
                           uint32_t* physical_device_count,
                           RinVkPhysicalDevice* physical_devices);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceFeatures(RinVkPhysicalDevice physical_device,
                            RinVkPhysicalDeviceFeatures* features);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceFeatures2(RinVkPhysicalDevice physical_device,
                             RinVkPhysicalDeviceFeatures2* features);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceProperties(RinVkPhysicalDevice physical_device,
                              RinVkPhysicalDeviceProperties* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceProperties2(
    RinVkPhysicalDevice physical_device,
    RinVkPhysicalDeviceProperties2* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceQueueFamilyProperties(
    RinVkPhysicalDevice physical_device, uint32_t* property_count,
    RinVkQueueFamilyProperties* properties);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetPhysicalDeviceMemoryProperties(
    RinVkPhysicalDevice physical_device,
    RinVkPhysicalDeviceMemoryProperties* properties);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateDevice(RinVkPhysicalDevice physical_device,
               const RinVkDeviceCreateInfo* create_info,
               const void* allocator, RinVkDevice* device_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyDevice(RinVkDevice device, const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetDeviceQueue(RinVkDevice device, uint32_t queue_family_index,
                 uint32_t queue_index, RinVkQueue* queue_out);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkDeviceWaitIdle(RinVkDevice device);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueueWaitIdle(RinVkQueue queue);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateFence(RinVkDevice device, const RinVkFenceCreateInfo* create_info,
              const void* allocator, RinVkFence* fence_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyFence(RinVkDevice device, RinVkFence fence, const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetFences(RinVkDevice device, uint32_t fence_count,
              const RinVkFence* fences);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkGetFenceStatus(RinVkDevice device, RinVkFence fence);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkWaitForFences(RinVkDevice device, uint32_t fence_count,
                const RinVkFence* fences, uint32_t wait_all,
                uint64_t timeout);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateSemaphore(RinVkDevice device,
                  const RinVkSemaphoreCreateInfo* create_info,
                  const void* allocator, RinVkSemaphore* semaphore_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroySemaphore(RinVkDevice device, RinVkSemaphore semaphore,
                   const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateCommandPool(RinVkDevice device,
                    const RinVkCommandPoolCreateInfo* create_info,
                    const void* allocator, RinVkCommandPool* pool_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyCommandPool(RinVkDevice device, RinVkCommandPool command_pool,
                     const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetCommandPool(RinVkDevice device, RinVkCommandPool command_pool,
                   uint32_t flags);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkAllocateCommandBuffers(
    RinVkDevice device, const RinVkCommandBufferAllocateInfo* allocate_info,
    RinVkCommandBuffer* command_buffers);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkFreeCommandBuffers(RinVkDevice device, RinVkCommandPool command_pool,
                     uint32_t command_buffer_count,
                     const RinVkCommandBuffer* command_buffers);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkBeginCommandBuffer(RinVkCommandBuffer command_buffer,
                     const RinVkCommandBufferBeginInfo* begin_info);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkEndCommandBuffer(RinVkCommandBuffer command_buffer);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkResetCommandBuffer(RinVkCommandBuffer command_buffer, uint32_t flags);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyBuffer(RinVkCommandBuffer command_buffer, RinVkBuffer src_buffer,
                RinVkBuffer dst_buffer, uint32_t region_count,
                const RinVkBufferCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyImage(RinVkCommandBuffer command_buffer, RinVkImage src_image,
               uint32_t src_image_layout, RinVkImage dst_image,
               uint32_t dst_image_layout, uint32_t region_count,
               const RinVkImageCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyBufferToImage(RinVkCommandBuffer command_buffer,
                       RinVkBuffer src_buffer, RinVkImage dst_image,
                       uint32_t dst_image_layout, uint32_t region_count,
                       const RinVkBufferImageCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdCopyImageToBuffer(RinVkCommandBuffer command_buffer,
                        RinVkImage src_image, uint32_t src_image_layout,
                        RinVkBuffer dst_buffer, uint32_t region_count,
                        const RinVkBufferImageCopy* regions);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkCmdClearColorImage(RinVkCommandBuffer command_buffer, RinVkImage image,
                     uint32_t image_layout,
                     const RinVkClearColorValue* color,
                     uint32_t range_count,
                     const RinVkImageSubresourceRange* ranges);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkQueueSubmit(RinVkQueue queue, uint32_t submit_count,
              const RinVkSubmitInfo* submits, uint64_t fence);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkAllocateMemory(RinVkDevice device,
                 const RinVkMemoryAllocateInfo* allocate_info,
                 const void* allocator, RinVkDeviceMemory* memory_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkFreeMemory(RinVkDevice device, RinVkDeviceMemory memory,
             const void* allocator);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateBuffer(RinVkDevice device, const RinVkBufferCreateInfo* create_info,
               const void* allocator, RinVkBuffer* buffer_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyBuffer(RinVkDevice device, RinVkBuffer buffer,
                const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetBufferMemoryRequirements(RinVkDevice device, RinVkBuffer buffer,
                              RinVkMemoryRequirements* requirements);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkBindBufferMemory(RinVkDevice device, RinVkBuffer buffer,
                   RinVkDeviceMemory memory, uint64_t memory_offset);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkCreateImage(RinVkDevice device, const RinVkImageCreateInfo* create_info,
              const void* allocator, RinVkImage* image_out);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkDestroyImage(RinVkDevice device, RinVkImage image, const void* allocator);
RIN_VKAPI_ATTR void RIN_VKAPI_CALL
vkGetImageMemoryRequirements(RinVkDevice device, RinVkImage image,
                             RinVkMemoryRequirements* requirements);
RIN_VKAPI_ATTR RinVkResult RIN_VKAPI_CALL
vkBindImageMemory(RinVkDevice device, RinVkImage image,
                  RinVkDeviceMemory memory, uint64_t memory_offset);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vkGetDeviceProcAddr(RinVkDevice device, const char* name);
RIN_VKAPI_ATTR RinVkVoidFunction RIN_VKAPI_CALL
vkGetInstanceProcAddr(RinVkInstance instance, const char* name);

#endif
