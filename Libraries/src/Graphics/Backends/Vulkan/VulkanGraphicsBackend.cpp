#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "Graphics/Backends/Vulkan/VulkanContext.h"
#include "Graphics/Backends/Vulkan/VulkanGraphicsDevice.h"
#include "Logger.h"

namespace
{

#ifdef DEBUG
constexpr bool kEnableValidation = true;
#else
constexpr bool kEnableValidation = false;
#endif

constexpr const char *kValidationLayerName = "VK_LAYER_KHRONOS_validation";

bool ValidationLayerAvailable()
{
    std::uint32_t layerCount = 0;
    if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS || layerCount == 0)
    {
        return false;
    }
    std::vector<VkLayerProperties> layers(layerCount);
    if (vkEnumerateInstanceLayerProperties(&layerCount, layers.data()) != VK_SUCCESS)
    {
        return false;
    }
    return std::any_of(layers.begin(), layers.end(),
                       [](const VkLayerProperties &layer) { return std::string(layer.layerName) == kValidationLayerName; });
}

bool InstanceExtensionAvailable(const char *name)
{
    std::uint32_t extensionCount = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != VK_SUCCESS || extensionCount == 0)
    {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) != VK_SUCCESS)
    {
        return false;
    }
    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties &extension) { return std::string(extension.extensionName) == name; });
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void * /*userData*/)
{
    if (callbackData == nullptr || callbackData->pMessage == nullptr)
    {
        return VK_FALSE;
    }
    const std::string message = std::string("[Vulkan validation] ") + callbackData->pMessage;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
    {
        LOG_ERROR(1, message);
    }
    else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
    {
        LOG_INFO(message);
    }
    else
    {
        LOG_TRACE(message);
    }
    return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &info)
{
    info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = &DebugMessengerCallback;
}

GraphicsCapabilities MakeAdvertisedCapabilities()
{
    return {.api = GraphicsAPI::Vulkan,
            .supportsRuntimeShaderCompilation = false,
            .supportsComputeShaders = true,
            .supportsGeometryShaders = true,
            .supportsTessellationShaders = true,
            .supportsFramebufferBlit = true,
            .supportsWireframeRendering = true,
            .supportsWindowPresentation = true,
            .supportsRayTracingPipelines = true,
            .supportsAccelerationStructures = true,
            .supportsRayQueries = true,
            .supportsTemporalUpscaling = true,
            .supportsFrameGeneration = true,
            .maxColorAttachments = 8,
            .maxAccelerationStructureInstances = 1024};
}

bool HasExtension(const std::vector<VkExtensionProperties> &extensions, const char *name)
{
    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties &extension) { return std::string(extension.extensionName) == name; });
}

int ScorePhysicalDevice(const VkPhysicalDeviceProperties &properties)
{
    int score = 0;
    switch (properties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 500;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 250;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 100;
            break;
        default:
            break;
    }

    score += static_cast<int>(properties.limits.maxImageDimension2D);
    return score;
}

} // namespace

GraphicsAPI VulkanGraphicsBackend::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

bool VulkanGraphicsBackend::Initialize()
{
    if (initialized)
    {
        return true;
    }

    if (!glfwInit())
    {
        LOG_ERROR(1, "Failed to initialize GLFW for Vulkan backend bootstrap");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    context = std::make_shared<VulkanBackendContext>();
    context->capabilities = capabilities;

    if (!CreateInstance())
    {
        LOG_ERROR(1, "[Vulkan] Backend Initialize: CreateInstance failed");
        Shutdown();
        return false;
    }

    if (!SelectPhysicalDevice())
    {
        LOG_ERROR(1, "[Vulkan] Backend Initialize: SelectPhysicalDevice failed");
        Shutdown();
        return false;
    }

    capabilities = context->capabilities;
    initialized = true;
    return true;
}

void VulkanGraphicsBackend::Shutdown() noexcept
{
    if (context != nullptr)
    {
        if (context->debugMessenger != VK_NULL_HANDLE && context->instance != VK_NULL_HANDLE)
        {
            const auto destroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugMessenger != nullptr)
            {
                destroyDebugMessenger(context->instance, context->debugMessenger, nullptr);
            }
            context->debugMessenger = VK_NULL_HANDLE;
        }
        if (context->instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(context->instance, nullptr);
        }
        context.reset();
    }

    if (initialized)
    {
        glfwTerminate();
    }

    initialized = false;
}

bool VulkanGraphicsBackend::IsAvailable() const noexcept { return initialized || context != nullptr || capabilities.api == GraphicsAPI::Vulkan; }

std::string VulkanGraphicsBackend::DescribeAvailability() const
{
    if (context != nullptr && context->physicalDevice != VK_NULL_HANDLE)
    {
        return "Vulkan backend is available on device " + context->deviceName + ".";
    }

    return "Vulkan backend requires a Vulkan loader and a compatible physical device.";
}

const GraphicsCapabilities &VulkanGraphicsBackend::GetCapabilities() const noexcept { return capabilities; }

std::unique_ptr<IGraphicsDevice> VulkanGraphicsBackend::CreateDevice(const GraphicsDeviceCreateInfo &createInfo) const
{
    static_cast<void>(createInfo);
    auto *mutableBackend = const_cast<VulkanGraphicsBackend *>(this);
    if ((mutableBackend->context == nullptr || mutableBackend->context->physicalDevice == VK_NULL_HANDLE) && !mutableBackend->Initialize())
    {
        return nullptr;
    }

    return std::make_unique<VulkanGraphicsDevice>(mutableBackend->context, mutableBackend->capabilities);
}

bool VulkanGraphicsBackend::CreateInstance()
{
    std::uint32_t requiredExtensionCount = 0;
    const char **requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    if (requiredExtensions == nullptr || requiredExtensionCount == 0)
    {
        LOG_ERROR(1, "GLFW did not report any required Vulkan instance extensions");
        return false;
    }

    context->enabledInstanceExtensions.assign(requiredExtensions, requiredExtensions + requiredExtensionCount);

    const bool wantValidation = kEnableValidation && ValidationLayerAvailable();
    const bool wantDebugUtils = wantValidation && InstanceExtensionAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (wantDebugUtils)
    {
        context->enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    if (wantValidation)
    {
        context->enabledInstanceLayers.push_back(kValidationLayerName);
    }
    if (kEnableValidation)
    {
        LOG_INFO(std::string("[Vulkan] Validation layer available=") + (wantValidation ? "yes" : "no") +
                 " debugUtils=" + (wantDebugUtils ? "yes" : "no"));
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ProceduralGeneration";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "ProceduralGeneration";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(context->enabledInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = context->enabledInstanceExtensions.data();
    createInfo.enabledLayerCount = static_cast<std::uint32_t>(context->enabledInstanceLayers.size());
    createInfo.ppEnabledLayerNames = context->enabledInstanceLayers.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (wantDebugUtils)
    {
        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &context->instance);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(result), "Failed to create Vulkan instance");
        context->instance = VK_NULL_HANDLE;
        return false;
    }

    if (wantDebugUtils)
    {
        const auto createDebugMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createDebugMessenger != nullptr)
        {
            createDebugMessenger(context->instance, &debugCreateInfo, nullptr, &context->debugMessenger);
        }
    }

    return true;
}

bool VulkanGraphicsBackend::SelectPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(context->instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0)
    {
        LOG_ERROR(1, "No Vulkan physical devices are available");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices.data()) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to enumerate Vulkan physical devices");
        return false;
    }

    int bestScore = std::numeric_limits<int>::min();
    for (VkPhysicalDevice device : devices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        std::uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
        for (std::uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                graphicsQueueFamilyIndex = i;
                break;
            }
        }

        if (graphicsQueueFamilyIndex == UINT32_MAX)
        {
            continue;
        }

        const int score = ScorePhysicalDevice(properties);
        if (score > bestScore)
        {
            bestScore = score;
            context->physicalDevice = device;
            context->physicalDeviceProperties = properties;
            context->graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
            context->deviceName = properties.deviceName;
        }
    }

    if (context->physicalDevice == VK_NULL_HANDLE)
    {
        LOG_ERROR(1, "Failed to select a Vulkan physical device with graphics support");
        return false;
    }

    vkGetPhysicalDeviceMemoryProperties(context->physicalDevice, &context->memoryProperties);

    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(context->physicalDevice, &features);

    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(context->physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(context->physicalDevice, nullptr, &extensionCount, extensions.data());

    const bool hasSwapchain = HasExtension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    const bool hasAccelerationStructure = HasExtension(extensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    const bool hasRayTracingPipeline = HasExtension(extensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    const bool hasRayQuery = HasExtension(extensions, VK_KHR_RAY_QUERY_EXTENSION_NAME);
    const bool hasDeferredHostOperations = HasExtension(extensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    const bool hasBufferDeviceAddress = HasExtension(extensions, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    context->enabledDeviceExtensions.clear();
    if (hasSwapchain)
    {
        context->enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (hasAccelerationStructure && hasDeferredHostOperations && hasBufferDeviceAddress)
    {
        context->enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        context->enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        context->enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }
    if (hasRayTracingPipeline && hasAccelerationStructure && hasDeferredHostOperations)
    {
        context->enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    }
    if (hasRayQuery && hasAccelerationStructure)
    {
        context->enabledDeviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }

    context->capabilities = MakeAdvertisedCapabilities();
    context->capabilities.supportsGeometryShaders = features.geometryShader == VK_TRUE;
    context->capabilities.supportsTessellationShaders = features.tessellationShader == VK_TRUE;
    context->capabilities.supportsWireframeRendering = features.fillModeNonSolid == VK_TRUE;
    context->capabilities.supportsWindowPresentation = hasSwapchain;
    context->capabilities.maxColorAttachments = context->physicalDeviceProperties.limits.maxColorAttachments;

    return true;
}
