#pragma once

typedef void GLFWwindow;
typedef void GLFWmonitor;

#define GLFW_TRUE  1
#define GLFW_FALSE 0
#define GLFW_PRESS 1
#define GLFW_RELEASE 0
#define GLFW_NO_API 0
#define GLFW_CLIENT_API 0x00022001
#define GLFW_RESIZABLE 0x00020003
#define GLFW_FLOATING 0x00020007
#define GLFW_DONT_CARE -1

#define GLFW_CURSOR_NORMAL   0x00034001
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_MOUSE_BUTTON_LEFT   0
#define GLFW_MOUSE_BUTTON_RIGHT  1

#define GLFW_KEY_SPACE        32
#define GLFW_KEY_GRAVE_ACCENT 96
#define GLFW_KEY_ESCAPE       256
#define GLFW_KEY_SLASH        47
#define GLFW_KEY_1            49
#define GLFW_KEY_2            50
#define GLFW_KEY_C            67
#define GLFW_KEY_W            87
#define GLFW_KEY_S            83
#define GLFW_KEY_A            65
#define GLFW_KEY_D            68

#define VK_NULL_HANDLE 0

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>

static inline int glfwInit(void) { return GLFW_TRUE; }
static inline void glfwTerminate(void) {}
static inline void glfwPollEvents(void) {}
static inline void glfwWaitEvents(void) {}
static inline int glfwWindowShouldClose(GLFWwindow*) { return 0; }
static inline void glfwDestroyWindow(GLFWwindow*) {}
static inline void glfwWindowHint(int, int) {}
static inline void glfwSetWindowUserPointer(GLFWwindow*, void*) {}
static inline void* glfwGetWindowUserPointer(GLFWwindow*) { return nullptr; }
static inline void glfwSetFramebufferSizeCallback(GLFWwindow*, void(*)()) {}
static inline void glfwSetScrollCallback(GLFWwindow*, void(*)()) {}
static inline void glfwSetCursorPosCallback(GLFWwindow*, void(*)()) {}
static inline void glfwSetInputMode(GLFWwindow*, int, int) {}
static inline int glfwGetKey(GLFWwindow*, int) { return GLFW_RELEASE; }
static inline int glfwGetMouseButton(GLFWwindow*, int) { return GLFW_RELEASE; }
static inline double glfwGetTime(void) { return 0.0; }
static inline GLFWmonitor* glfwGetPrimaryMonitor(void) { return nullptr; }
static inline void glfwSetWindowMonitor(GLFWwindow*, GLFWmonitor*, int, int, int, int, int) {}

static inline void glfwGetWindowSize(GLFWwindow*, int* width, int* height) {
    *width = 1920; *height = 1080;
}
static inline void glfwGetFramebufferSize(GLFWwindow*, int* width, int* height) {
    *width = 1920; *height = 1080;
}
static inline void glfwGetCursorPos(GLFWwindow*, double* x, double* y) {
    *x = 0.0; *y = 0.0;
}

typedef VkResult (*PFN_vkCreateDebugUtilsMessengerEXT)(
    VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
typedef void (*PFN_vkDestroyDebugUtilsMessengerEXT)(
    VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);