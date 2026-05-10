#pragma once

#include "Platform/PlatformDetection.h"

#if defined(PIXEL_ANDROID)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/native_window.h>
#elif defined(_WIN32)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <vector>
#include <string>
#include <atomic>

namespace VulKan {
    class Instance;
    class WindowSurface;
    class Device;
    class SwapChain;
    class RenderPass;
    class CommandPool;
    class CommandBuffer;
    class DescriptorPool;
    class DescriptorSet;
    class DescriptorSetLayout;
}

class DemoApp {
public:
    DemoApp()  = default;
    ~DemoApp();

    bool Initialize(
    #if defined(PIXEL_ANDROID)
        ANativeWindow* window
    #elif defined(_WIN32)
        GLFWwindow* window
    #endif
    );
    void Shutdown();
    void DrawFrame();
    void OnWindowResize();
    bool IsInitialized() const { return mInitialized; }

private:
    bool CreateInstance();
    bool CreateSurface();
    bool CreateDeviceAndSwapChain();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandResources();
    bool CreateSyncObjects();
    bool InitImGui();
    bool InitVulkanTool();

    void RecreateSwapchain();
    void CleanupSwapchain();
    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

#if defined(PIXEL_ANDROID)
    ANativeWindow* mWindow = nullptr;
#elif defined(_WIN32)
    GLFWwindow* mWindow = nullptr;
#endif

    VkInstance               mInstance           = VK_NULL_HANDLE;
    VkPhysicalDevice         mPhysicalDevice     = VK_NULL_HANDLE;
    VkDevice                 mDevice             = VK_NULL_HANDLE;
    VkSurfaceKHR             mSurface            = VK_NULL_HANDLE;
    VkSwapchainKHR           mSwapchain          = VK_NULL_HANDLE;
    VkRenderPass             mRenderPass         = VK_NULL_HANDLE;
    VkDescriptorPool         mDescriptorPool     = VK_NULL_HANDLE;
    VkCommandPool            mCommandPool        = VK_NULL_HANDLE;
    VkQueue                  mGraphicsQueue      = VK_NULL_HANDLE;
    VkQueue                  mPresentQueue       = VK_NULL_HANDLE;

    VulKan::Device*          mDeviceWrapper      = nullptr;
    VulKan::SwapChain*       mSwapchainWrapper   = nullptr;

    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<VkCommandBuffer> mCommandBuffers;

    static constexpr int     MAX_FRAMES_IN_FLIGHT = 3;
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence>     mInFlightFences;

    VkFormat                 mSurfaceFormat      = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR          mSurfaceColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D               mSurfaceExtent      = {0, 0};
    uint32_t                 mGraphicsQueueFamily = 0;
    uint32_t                 mPresentQueueFamily = 0;
    uint32_t                 mImageCount         = 0;
    uint32_t                 mMinImageCount      = 0;
    uint32_t                 mCurrentFrame       = 0;

    std::atomic<bool>        mFramebufferResized{false};
    bool                     mInitialized = false;

    static bool              gVulkanEnableValidation;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT  = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    VkDebugUtilsMessengerEXT            mDebugMessenger                 = VK_NULL_HANDLE;
};