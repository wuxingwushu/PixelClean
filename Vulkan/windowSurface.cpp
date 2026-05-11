#include "windowSurface.h"

namespace VulKan {

WindowSurface::WindowSurface(Instance* instance, 
#if defined(_WIN32)
    Window* window
#elif defined(__ANDROID__)
    Window* window
#endif
) {
    mInstance = instance;
#if defined(_WIN32)
    if (glfwCreateWindowSurface(instance->getInstance(), window->getWindow(), nullptr, &mWindowSurface) != VK_SUCCESS) {
        throw std::runtime_error("Error: failed to create surface");
    }
#elif defined(__ANDROID__)
    // Android: 使用 vkCreateAndroidSurfaceKHR
    // 需要从 JNI 层获取 ANativeWindow*，通过 VkAndroidSurfaceCreateInfoKHR 创建
    throw std::runtime_error("Error: Android surface not yet supported in WindowSurface");
#endif
}

WindowSurface::~WindowSurface() {
    vkDestroySurfaceKHR(mInstance->getInstance(), mWindowSurface, nullptr);
}

}