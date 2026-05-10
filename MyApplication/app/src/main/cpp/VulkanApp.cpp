#include "VulkanApp.h"
#include "Platform/PlatformDetection.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#if defined(PIXEL_ANDROID)
#include <android/log.h>
#include <android/native_window.h>
#define LOG_TAG "PixelClean-Demo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#elif defined(_WIN32)
#include <iostream>
#define LOGI(...) printf("[INFO] " __VA_ARGS__); printf("\n")
#define LOGD(...) printf("[DEBUG] " __VA_ARGS__); printf("\n")
#define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__); fprintf(stderr, "\n")
#define LOGW(...) printf("[WARN] " __VA_ARGS__); printf("\n")
#endif

#include <cstring>
#include <algorithm>
#include <set>

bool DemoApp::gVulkanEnableValidation = false;

static const std::vector<const char*> kDesiredValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VKAPI_ATTR VkBool32 VKAPI_CALL DemoApp::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*)
{
    LOGE("Vulkan: %s", pCallbackData->pMessage);
    return VK_FALSE;
}

DemoApp::~DemoApp()
{
    Shutdown();
}

static bool CheckValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    LOGD("Available Vulkan layers (%d):", layerCount);
    for (const auto& layer : available) {
        LOGD("  %s", layer.layerName);
    }

    for (const char* desired : kDesiredValidationLayers) {
        bool found = false;
        for (const auto& layer : available) {
            if (strcmp(desired, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOGW("Validation layer '%s' not available.", desired);
            return false;
        }
    }
    LOGI("Validation layers available.");
    return true;
}

bool DemoApp::Initialize(
#if defined(PIXEL_ANDROID)
    ANativeWindow* window
#elif defined(_WIN32)
    GLFWwindow* window
#endif
)
{
    mWindow = window;

#if defined(PIXEL_ANDROID)
    int32_t w = ANativeWindow_getWidth(mWindow);
    int32_t h = ANativeWindow_getHeight(mWindow);
    LOGI("Android window: %dx%d", w, h);
#elif defined(_WIN32)
    int w = 0, h = 0;
    glfwGetFramebufferSize(mWindow, &w, &h);
    LOGI("GLFW window: %dx%d", w, h);
#endif

    if (!CreateInstance())       { LOGE("FAILED: CreateInstance");       return false; }
    LOGI("OK: CreateInstance");
    if (!CreateSurface())        { LOGE("FAILED: CreateSurface");        return false; }
    LOGI("OK: CreateSurface");
    if (!CreateDeviceAndSwapChain()) { LOGE("FAILED: DeviceAndSwapChain"); return false; }
    LOGI("OK: DeviceAndSwapChain");
    if (!CreateRenderPass())     { LOGE("FAILED: CreateRenderPass");     return false; }
    LOGI("OK: CreateRenderPass");
    if (!CreateFramebuffers())   { LOGE("FAILED: CreateFramebuffers");   return false; }
    LOGI("OK: CreateFramebuffers");
    if (!CreateCommandResources()) { LOGE("FAILED: CreateCommandResources"); return false; }
    LOGI("OK: CreateCommandResources");
    if (!CreateSyncObjects())    { LOGE("FAILED: CreateSyncObjects");    return false; }
    LOGI("OK: CreateSyncObjects");
    if (!InitImGui())            { LOGE("FAILED: InitImGui");            return false; }
    LOGI("OK: InitImGui");

    mInitialized = true;
    LOGI("=== DemoApp initialization COMPLETE ===");
    return true;
}

bool DemoApp::CreateInstance()
{
    gVulkanEnableValidation = CheckValidationLayerSupport();

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "PixelClean Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "PixelClean";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(PIXEL_ANDROID)
        "VK_KHR_android_surface"
#elif defined(_WIN32)
        "VK_KHR_win32_surface"
#endif
    };

    if (gVulkanEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (gVulkanEnableValidation) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(kDesiredValidationLayers.size());
        createInfo.ppEnabledLayerNames = kDesiredValidationLayers.data();
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateInstance failed: %d", result);
        if (result == VK_ERROR_LAYER_NOT_PRESENT) {
            LOGW("Retrying without validation layers...");
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
            extensions.erase(std::remove(extensions.begin(), extensions.end(),
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME), extensions.end());
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
            result = vkCreateInstance(&createInfo, nullptr, &mInstance);
            if (result != VK_SUCCESS) { LOGE("Retry failed: %d", result); return false; }
            gVulkanEnableValidation = false;
        } else {
            return false;
        }
    }

    if (gVulkanEnableValidation) {
        vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");
        vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (vkCreateDebugUtilsMessengerEXT) {
            VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
            dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            dbgInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dbgInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbgInfo.pfnUserCallback = DebugCallback;
            vkCreateDebugUtilsMessengerEXT(mInstance, &dbgInfo, nullptr, &mDebugMessenger);
        }
    }

    LOGI("Instance created (validation: %s).", gVulkanEnableValidation ? "ON" : "OFF");
    return true;
}

bool DemoApp::CreateSurface()
{
#if defined(PIXEL_ANDROID)
    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = mWindow;
    VkResult result = vkCreateAndroidSurfaceKHR(mInstance, &surfaceInfo, nullptr, &mSurface);
#elif defined(_WIN32)
    VkResult result = glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface);
#endif
    if (result != VK_SUCCESS) { LOGE("CreateSurface failed: %d", result); return false; }
    LOGI("Surface created.");
    return true;
}

bool DemoApp::CreateDeviceAndSwapChain()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (deviceCount == 0) { LOGE("No GPUs with Vulkan support."); return false; }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        LOGD("GPU candidate: %s (type=%d)", props.deviceName, props.deviceType);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, qfProps.data());

        uint32_t presentFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);
            if (presentSupport && (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                mGraphicsQueueFamily = i;
                mPresentQueueFamily  = i;
                mPhysicalDevice = device;
                break;
            }
            if (presentSupport) presentFamily = i;
        }

        if (mPhysicalDevice == VK_NULL_HANDLE) {
            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    mGraphicsQueueFamily = i;
                    mPresentQueueFamily  = (presentFamily != UINT32_MAX) ? presentFamily : i;
                    mPhysicalDevice = device;
                    break;
                }
            }
        }

        if (mPhysicalDevice != VK_NULL_HANDLE) break;
    }

    if (mPhysicalDevice == VK_NULL_HANDLE) { LOGE("No suitable GPU found."); return false; }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
    LOGI("Selected GPU: %s", props.deviceName);

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    std::set<uint32_t> uniqueFamilies = {mGraphicsQueueFamily, mPresentQueueFamily};
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &queuePriority;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    deviceInfo.pQueueCreateInfos    = qcis.data();
    deviceInfo.pEnabledFeatures     = &deviceFeatures;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    VkResult result = vkCreateDevice(mPhysicalDevice, &deviceInfo, nullptr, &mDevice);
    if (result != VK_SUCCESS) { LOGE("vkCreateDevice failed: %d", result); return false; }
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, mPresentQueueFamily, 0, &mPresentQueue);

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, formats.data());

    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        mSurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
        mSurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    } else {
        mSurfaceFormat = VK_FORMAT_UNDEFINED;
        for (const auto& fmt : formats) {
            if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                mSurfaceFormat = fmt.format;
                mSurfaceColorSpace = fmt.colorSpace;
                break;
            }
        }
        if (mSurfaceFormat == VK_FORMAT_UNDEFINED) {
            mSurfaceFormat = formats[0].format;
            mSurfaceColorSpace = formats[0].colorSpace;
        }
    }
    LOGI("Surface format: %d, colorSpace: %d", mSurfaceFormat, mSurfaceColorSpace);

    mMinImageCount = (std::max)(caps.minImageCount + 1u, 3u);

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
#if defined(PIXEL_ANDROID)
        extent = {(uint32_t)ANativeWindow_getWidth(mWindow),
                  (uint32_t)ANativeWindow_getHeight(mWindow)};
#elif defined(_WIN32)
        int w, h;
        glfwGetFramebufferSize(mWindow, &w, &h);
        extent = {(uint32_t)w, (uint32_t)h};
#endif
        extent.width  = (std::max)(caps.minImageExtent.width,  (std::min)(caps.maxImageExtent.width,  extent.width));
        extent.height = (std::max)(caps.minImageExtent.height, (std::min)(caps.maxImageExtent.height, extent.height));
    }
    mSurfaceExtent = extent;

    VkSwapchainCreateInfoKHR scInfo{};
    scInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scInfo.surface          = mSurface;
    scInfo.minImageCount    = mMinImageCount;
    scInfo.imageFormat      = mSurfaceFormat;
    scInfo.imageColorSpace  = mSurfaceColorSpace;
    scInfo.imageExtent      = mSurfaceExtent;
    scInfo.imageArrayLayers = 1;
    scInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t families[] = {mGraphicsQueueFamily, mPresentQueueFamily};
    if (mGraphicsQueueFamily != mPresentQueueFamily) {
        scInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        scInfo.queueFamilyIndexCount = 2;
        scInfo.pQueueFamilyIndices   = families;
    } else {
        scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    scInfo.preTransform   = caps.currentTransform;
    scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scInfo.presentMode    = VK_PRESENT_MODE_FIFO_KHR;
    scInfo.clipped        = VK_TRUE;
    scInfo.oldSwapchain   = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(mDevice, &scInfo, nullptr, &mSwapchain);
    if (result != VK_SUCCESS) { LOGE("vkCreateSwapchainKHR failed: %d", result); return false; }

    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, nullptr);
    mSwapchainImages.resize(mImageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, mSwapchainImages.data());
    mSwapchainImageViews.resize(mImageCount);

    for (uint32_t i = 0; i < mImageCount; i++) {
        VkImageViewCreateInfo ivInfo{};
        ivInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.image                           = mSwapchainImages[i];
        ivInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivInfo.format                          = mSurfaceFormat;
        ivInfo.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY};
        ivInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivInfo.subresourceRange.baseMipLevel   = 0;
        ivInfo.subresourceRange.levelCount     = 1;
        ivInfo.subresourceRange.baseArrayLayer = 0;
        ivInfo.subresourceRange.layerCount     = 1;
        result = vkCreateImageView(mDevice, &ivInfo, nullptr, &mSwapchainImageViews[i]);
        if (result != VK_SUCCESS) { LOGE("vkCreateImageView failed: %d", result); return false; }
    }

    LOGI("Swapchain: %d images %dx%d", mImageCount, extent.width, extent.height);
    return true;
}

bool DemoApp::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = mSurfaceFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;

    VkResult result = vkCreateRenderPass(mDevice, &rpInfo, nullptr, &mRenderPass);
    if (result != VK_SUCCESS) { LOGE("vkCreateRenderPass failed: %d", result); return false; }
    return true;
}

bool DemoApp::CreateFramebuffers()
{
    mFramebuffers.resize(mImageCount);
    for (uint32_t i = 0; i < mImageCount; i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = mRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &mSwapchainImageViews[i];
        fbInfo.width           = mSurfaceExtent.width;
        fbInfo.height          = mSurfaceExtent.height;
        fbInfo.layers          = 1;
        VkResult result = vkCreateFramebuffer(mDevice, &fbInfo, nullptr, &mFramebuffers[i]);
        if (result != VK_SUCCESS) { LOGE("vkCreateFramebuffer failed: %d", result); return false; }
    }
    return true;
}

bool DemoApp::CreateCommandResources()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = mGraphicsQueueFamily;
    VkResult result = vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool);
    if (result != VK_SUCCESS) { LOGE("vkCreateCommandPool failed: %d", result); return false; }

    mCommandBuffers.resize(mImageCount);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = mCommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());
    result = vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data());
    if (result != VK_SUCCESS) { LOGE("vkAllocateCommandBuffers failed: %d", result); return false; }

    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
        {VK_DESCRIPTOR_TYPE_SAMPLER,       100},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
    };
    VkDescriptorPoolCreateInfo dpInfo{};
    dpInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpInfo.maxSets       = 200;
    dpInfo.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
    dpInfo.pPoolSizes    = sizes;
    result = vkCreateDescriptorPool(mDevice, &dpInfo, nullptr, &mDescriptorPool);
    if (result != VK_SUCCESS) { LOGE("vkCreateDescriptorPool failed: %d", result); return false; }

    return true;
}

bool DemoApp::CreateSyncObjects()
{
    mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(mDevice, &semInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            LOGE("CreateSyncObjects failed at %d", i);
            return false;
        }
    }
    return true;
}

static void CheckVkResultFn(VkResult err)
{
    if (err != VK_SUCCESS) {
        LOGE("ImGui Vulkan error: %d", err);
    }
}

bool DemoApp::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2((float)mSurfaceExtent.width, (float)mSurfaceExtent.height);
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_InitInfo initInfo;
    memset(&initInfo, 0, sizeof(initInfo));

    initInfo.ApiVersion          = VK_API_VERSION_1_2;
    initInfo.Instance            = mInstance;
    initInfo.PhysicalDevice      = mPhysicalDevice;
    initInfo.Device              = mDevice;
    initInfo.QueueFamily         = mGraphicsQueueFamily;
    initInfo.Queue               = mGraphicsQueue;
    initInfo.PipelineCache       = VK_NULL_HANDLE;
    initInfo.DescriptorPool      = VK_NULL_HANDLE;
    initInfo.DescriptorPoolSize  = 100;
    initInfo.MinImageCount       = mMinImageCount;
    initInfo.ImageCount          = mImageCount;
    initInfo.UseDynamicRendering = false;
    initInfo.PipelineInfoMain.RenderPass  = mRenderPass;
    initInfo.PipelineInfoMain.Subpass     = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator           = nullptr;
    initInfo.CheckVkResultFn     = CheckVkResultFn;
    initInfo.MinAllocationSize   = 1024 * 1024;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        LOGE("ImGui_ImplVulkan_Init failed");
        return false;
    }

    LOGI("ImGui initialized.");
    return true;
}

bool DemoApp::InitVulkanTool()
{
    LOGI("VulkanTool initialized (Library linked).");
    return true;
}

void DemoApp::Shutdown()
{
    mInitialized = false;

    if (mDevice) {
        vkDeviceWaitIdle(mDevice);

        ImGui_ImplVulkan_Shutdown();
        ImGui::DestroyContext();

        for (auto& f : mInFlightFences)          vkDestroyFence(mDevice, f, nullptr);
        for (auto& s : mRenderFinishedSemaphores) vkDestroySemaphore(mDevice, s, nullptr);
        for (auto& s : mImageAvailableSemaphores) vkDestroySemaphore(mDevice, s, nullptr);

        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

        for (auto& fb : mFramebuffers)           vkDestroyFramebuffer(mDevice, fb, nullptr);
        for (auto& iv : mSwapchainImageViews)    vkDestroyImageView(mDevice, iv, nullptr);

        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        vkDestroyDevice(mDevice, nullptr);
    }

    if (mSurface) vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

    if (mInstance) {
        if (gVulkanEnableValidation && vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        }
        vkDestroyInstance(mInstance, nullptr);
    }

    LOGI("Shutdown complete.");
}

void DemoApp::CleanupSwapchain()
{
    for (auto& fb : mFramebuffers)        vkDestroyFramebuffer(mDevice, fb, nullptr);
    for (auto& iv : mSwapchainImageViews) vkDestroyImageView(mDevice, iv, nullptr);
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
}

void DemoApp::RecreateSwapchain()
{
    vkDeviceWaitIdle(mDevice);
    CleanupSwapchain();

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

    mMinImageCount = (std::max)(caps.minImageCount + 1u, 3u);

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
#if defined(PIXEL_ANDROID)
        extent = {(uint32_t)ANativeWindow_getWidth(mWindow),
                  (uint32_t)ANativeWindow_getHeight(mWindow)};
#elif defined(_WIN32)
        int w, h;
        glfwGetFramebufferSize(mWindow, &w, &h);
        extent = {(uint32_t)w, (uint32_t)h};
#endif
        extent.width  = (std::max)(caps.minImageExtent.width,  (std::min)(caps.maxImageExtent.width,  extent.width));
        extent.height = (std::max)(caps.minImageExtent.height, (std::min)(caps.maxImageExtent.height, extent.height));
    }
    mSurfaceExtent = extent;

    VkSwapchainCreateInfoKHR scInfo{};
    scInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scInfo.surface          = mSurface;
    scInfo.minImageCount    = mMinImageCount;
    scInfo.imageFormat      = mSurfaceFormat;
    scInfo.imageColorSpace  = mSurfaceColorSpace;
    scInfo.imageExtent      = mSurfaceExtent;
    scInfo.imageArrayLayers = 1;
    scInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    scInfo.preTransform     = caps.currentTransform;
    scInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scInfo.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    scInfo.clipped          = VK_TRUE;
    scInfo.oldSwapchain     = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(mDevice, &scInfo, nullptr, &mSwapchain);
    if (result != VK_SUCCESS) { LOGE("Recreate swapchain failed: %d", result); return; }

    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, nullptr);
    mSwapchainImages.resize(mImageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, mSwapchainImages.data());
    mSwapchainImageViews.resize(mImageCount);

    for (uint32_t i = 0; i < mImageCount; i++) {
        VkImageViewCreateInfo ivInfo{};
        ivInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.image                           = mSwapchainImages[i];
        ivInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivInfo.format                          = mSurfaceFormat;
        ivInfo.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY};
        ivInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivInfo.subresourceRange.baseMipLevel   = 0;
        ivInfo.subresourceRange.levelCount     = 1;
        ivInfo.subresourceRange.baseArrayLayer = 0;
        ivInfo.subresourceRange.layerCount     = 1;
        vkCreateImageView(mDevice, &ivInfo, nullptr, &mSwapchainImageViews[i]);
    }

    CreateFramebuffers();
    mFramebufferResized = false;
    LOGI("Swapchain recreated: %dx%d", extent.width, extent.height);
}

void DemoApp::OnWindowResize()
{
    mFramebufferResized = true;
}

void DemoApp::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
    static int recordCount = 0;
    recordCount++;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue cv = {{{0.08f, 0.08f, 0.12f, 1.0f}}};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = mRenderPass;
    rpInfo.framebuffer       = mFramebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = mSurfaceExtent;
    rpInfo.clearValueCount   = 1;
    rpInfo.pClearValues      = &cv;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    {
        static bool showDemo = false;
        static int counter = 0;

        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_Once);
        if (ImGui::Begin("PixelClean Cross-Platform Demo",
                         nullptr,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Engine Status: Running");
            ImGui::SameLine();
            ImGui::Text("  Validation: %s", gVulkanEnableValidation ? "ON" : "OFF");
            ImGui::Separator();

#if defined(PIXEL_ANDROID)
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Platform: Android");
#elif defined(_WIN32)
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Platform: Windows");
#else
            ImGui::Text("Platform: Unknown");
#endif
            ImGui::Text("API:          Vulkan 1.2");
            ImGui::Text("Resolution:   %dx%d", mSurfaceExtent.width, mSurfaceExtent.height);
            ImGui::Text("Swap Images:  %d", mImageCount);
            ImGui::Text("Surface Fmt:  %d", static_cast<int>(mSurfaceFormat));
            ImGui::Separator();

            ImGui::Text("Libraries:");
            ImGui::BulletText("Vulkan/   - Core Vulkan wrapper");
            ImGui::BulletText("VulkanTool/ - Visual effects & pipelines");
            ImGui::BulletText("imgui/    - UI framework");

            ImGui::Separator();

            if (ImGui::CollapsingHeader("Performance")) {
                ImGui::Text("Present Mode: FIFO (VSync)");
                float fps = ImGui::GetIO().Framerate;
                ImGui::Text("FPS:          %.1f", fps);
                ImGui::Text("Frame Time:   %.2f ms", 1000.0f / (std::max)(fps, 1.0f));
            }

            if (ImGui::CollapsingHeader("Controls")) {
                ImGui::Checkbox("Show ImGui Demo Window", &showDemo);
                if (ImGui::Button("Click Me!")) {
                    counter++;
                }
                ImGui::SameLine();
                ImGui::Text("Clicks: %d", counter);
            }

            if (ImGui::CollapsingHeader("Platform Info")) {
#if defined(PIXEL_ANDROID)
                ImGui::Text("Build: Android NDK");
                ImGui::Text("Window: ANativeWindow");
                ImGui::Text("Surface: VK_KHR_android_surface");
#elif defined(_WIN32)
                ImGui::Text("Build: MSVC / MinGW");
                ImGui::Text("Window: GLFW");
                ImGui::Text("Surface: VK_KHR_win32_surface");
#endif
            }

            ImGui::Separator();
            if (recordCount <= 3) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
                                   "Frame #%d | Init OK | Rendering active", recordCount);
            }

            ImGui::Separator();
            static float bg[3] = {0.08f, 0.08f, 0.12f};
            ImGui::ColorEdit3("Clear Color", bg);
            cv.color.float32[0] = bg[0];
            cv.color.float32[1] = bg[1];
            cv.color.float32[2] = bg[2];
        }
        ImGui::End();

        if (showDemo) {
            ImGui::ShowDemoWindow(&showDemo);
        }
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

void DemoApp::DrawFrame()
{
    if (!mInitialized) return;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)mSurfaceExtent.width, (float)mSurfaceExtent.height);

    vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX,
                                            mImageAvailableSemaphores[mCurrentFrame],
                                            VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || mFramebufferResized) {
        RecreateSwapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("vkAcquireNextImageKHR: %d", result);
        return;
    }

    vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);
    RecordCommandBuffer(mCommandBuffers[imageIndex], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemas[]      = {mImageAvailableSemaphores[mCurrentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = waitSemas;
    submitInfo.pWaitDstStageMask  = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &mCommandBuffers[imageIndex];

    VkSemaphore sigSemas[] = {mRenderFinishedSemaphores[mCurrentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = sigSemas;

    result = vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]);
    if (result != VK_SUCCESS) { LOGE("vkQueueSubmit: %d", result); return; }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = sigSemas;

    VkSwapchainKHR swapchains[] = {mSwapchain};
    presentInfo.swapchainCount   = 1;
    presentInfo.pSwapchains      = swapchains;
    presentInfo.pImageIndices    = &imageIndex;

    result = vkQueuePresentKHR(mPresentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFramebufferResized) {
        mFramebufferResized = false;
        RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        LOGE("vkQueuePresentKHR: %d", result);
    }

    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}