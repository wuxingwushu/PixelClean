#include "instance.h"
#include  "../Tool/Tool.h"
#include <algorithm>
#include <cstring>
#if defined(_WIN32)
#include <GLFW/glfw3.h>
#elif defined(__ANDROID__)
#include <vulkan/vulkan_android.h>
#endif
#include "../DebugLog.h"

#ifndef VK_API_VERSION_1_1
#define VK_API_VERSION_1_1 VK_MAKE_API_VERSION(0, 1, 1, 0)
#endif

#ifndef VK_API_VERSION_1_2
#define VK_API_VERSION_1_2 VK_MAKE_API_VERSION(0, 1, 2, 0)
#endif

#ifndef VK_API_VERSION_1_3
#define VK_API_VERSION_1_3 VK_MAKE_API_VERSION(0, 1, 3, 0)
#endif

namespace VulKan {
	//validation layer 回调函数
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pMessageData,
		void* pUserData) {

		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			LOGW("ValidationLayer: %s", pMessageData->pMessage);
			if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				if (TOOL::VulKanError) TOOL::VulKanError->warn(pMessageData->pMessage);
			}else if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				if (TOOL::VulKanError) TOOL::VulKanError->error(pMessageData->pMessage);
			}
		}
		
		return VK_FALSE;
	}

	//辅助函数			辅助创建监听功能
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* debugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

		if (func != nullptr) {//判断创建成功没
			return func(instance, pCreateInfo, pAllocator, debugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	//回收创建的监听功能
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		VkDebugUtilsMessengerEXT  debugMessenger,
		const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

		if (func != nullptr) {
			return func(instance, debugMessenger, pAllocator);
		}
	}


	Instance::Instance(bool enableValidationLayer) {
		LOGD("Instance::Instance(enableValidationLayer=%d)", enableValidationLayer);
		mEnableValidationLayer = enableValidationLayer;

		uint32_t instanceVersion = VK_API_VERSION_1_0;
		PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion = 
			reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
		if (pfnEnumerateInstanceVersion) {
			if (pfnEnumerateInstanceVersion(&instanceVersion) == VK_SUCCESS) {
				LOGI("Vulkan instance version: %u.%u.%u",
					VK_VERSION_MAJOR(instanceVersion),
					VK_VERSION_MINOR(instanceVersion),
					VK_VERSION_PATCH(instanceVersion));
			}
		}

		if (mEnableValidationLayer) {
			if (!checkValidationLayerSupport()) {
				mEnableValidationLayer = false;
				LOGW("Validation layer is not supported, disabled");
			}
		}

		printAvailableExtensions();

		std::vector<const char*> availableExtNames = getAvailableExtensionNames();

		extensions = getRequiredExtensions();
		std::vector<const char*> filteredExtensions = filterExtensions(extensions, availableExtNames);

		uint32_t apiVersions[] = {
			(instanceVersion < VK_API_VERSION_1_3) ? instanceVersion : VK_API_VERSION_1_3,
			VK_API_VERSION_1_2,
			VK_API_VERSION_1_1,
			VK_API_VERSION_1_0
		};

		struct RetryConfig {
			uint32_t apiVersion;
			std::vector<const char*> exts;
			bool useValidation;
			const char* desc;
		};

		std::vector<RetryConfig> retryConfigs;

		retryConfigs.push_back({apiVersions[0], extensions, mEnableValidationLayer, "detected API + all required extensions"});
		retryConfigs.push_back({apiVersions[0], filteredExtensions, mEnableValidationLayer, "detected API + filtered extensions"});
		retryConfigs.push_back({VK_API_VERSION_1_0, extensions, mEnableValidationLayer, "API 1.0 + all required extensions"});
		retryConfigs.push_back({VK_API_VERSION_1_0, filteredExtensions, mEnableValidationLayer, "API 1.0 + filtered extensions"});
		retryConfigs.push_back({VK_API_VERSION_1_0, filteredExtensions, false, "API 1.0 + filtered extensions + no validation"});

		VkResult result = VK_ERROR_INITIALIZATION_FAILED;
		bool created = false;

		for (size_t attempt = 0; attempt < retryConfigs.size(); attempt++) {
			const auto& cfg = retryConfigs[attempt];
			LOGI("Attempt %zu: %s (apiVersion=%u.%u.%u, %u extensions, validation=%d)",
				attempt, cfg.desc,
				VK_VERSION_MAJOR(cfg.apiVersion),
				VK_VERSION_MINOR(cfg.apiVersion),
				VK_VERSION_PATCH(cfg.apiVersion),
				(uint32_t)cfg.exts.size(), cfg.useValidation);

			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "vulkanLession";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "NO ENGINE";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = cfg.apiVersion;

			VkInstanceCreateInfo instCreateInfo = {};
			instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instCreateInfo.pApplicationInfo = &appInfo;
			instCreateInfo.enabledExtensionCount = static_cast<uint32_t>(cfg.exts.size());
			instCreateInfo.ppEnabledExtensionNames = cfg.exts.data();

			if (cfg.useValidation) {
				instCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
				instCreateInfo.ppEnabledLayerNames = validationLayers.data();
			}
			else {
				instCreateInfo.enabledLayerCount = 0;
			}

			result = vkCreateInstance(&instCreateInfo, nullptr, &mInstance);
			if (result == VK_SUCCESS) {
				LOGI("vkCreateInstance succeeded on attempt %zu: %s", attempt, cfg.desc);
				extensions = cfg.exts;
				if (!cfg.useValidation) {
					mEnableValidationLayer = false;
				}
				created = true;
				break;
			}
			else {
				LOGW("Attempt %zu failed: VkResult = %d", attempt, (int)result);
			}
		}

		if (!created) {
			LOGE("All vkCreateInstance attempts failed. Vulkan is not available on this device.");
			LOGE("  VK_ERROR_INCOMPATIBLE_DRIVER=%d, VK_ERROR_EXTENSION_NOT_PRESENT=%d, "
				 "VK_ERROR_LAYER_NOT_PRESENT=%d, VK_ERROR_INITIALIZATION_FAILED=%d",
				(int)VK_ERROR_INCOMPATIBLE_DRIVER, (int)VK_ERROR_EXTENSION_NOT_PRESENT,
				(int)VK_ERROR_LAYER_NOT_PRESENT, (int)VK_ERROR_INITIALIZATION_FAILED);
			if (TOOL::VulKanError) TOOL::VulKanError->error("Error:failed to create instance, Vulkan not available");
			throw std::runtime_error("Error:failed to create instance");
		}

		if (mEnableValidationLayer) {
			setupDebugger();
		}
	}

	Instance::~Instance() {
		if (mEnableValidationLayer) {
			DestroyDebugUtilsMessengerEXT(mInstance, mDebugger, nullptr);
		}

		vkDestroyInstance(mInstance, nullptr);
	}

	void Instance::printAvailableExtensions() {
		uint32_t extensionCount = 0;
		VkResult enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		if (enumResult != VK_SUCCESS) {
			LOGE("printAvailableExtensions: vkEnumerateInstanceExtensionProperties failed, VkResult=%d", (int)enumResult);
			return;
		}

		std::vector<VkExtensionProperties> extensions(extensionCount);
		enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		if (enumResult != VK_SUCCESS) {
			LOGE("printAvailableExtensions: vkEnumerateInstanceExtensionProperties(2nd call) failed, VkResult=%d", (int)enumResult);
			return;
		}

		LOGI("Available instance extensions (%u):", extensionCount);
		for (uint32_t i = 0; i < extensionCount; i++) {
			LOGI("  [%u] %s (spec %u.%u.%u)", i, extensions[i].extensionName,
				VK_VERSION_MAJOR(extensions[i].specVersion),
				VK_VERSION_MINOR(extensions[i].specVersion),
				VK_VERSION_PATCH(extensions[i].specVersion));
		}
	}

	std::vector<const char*> Instance::getAvailableExtensionNames() {
		std::vector<const char*> names;
		uint32_t extensionCount = 0;
		VkResult enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		if (enumResult != VK_SUCCESS || extensionCount == 0) {
			return names;
		}
		std::vector<VkExtensionProperties> props(extensionCount);
		enumResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, props.data());
		if (enumResult != VK_SUCCESS) {
			return names;
		}
		names.reserve(extensionCount);
		for (uint32_t i = 0; i < extensionCount; i++) {
			names.push_back(props[i].extensionName);
		}
		return names;
	}

	std::vector<const char*> Instance::filterExtensions(const std::vector<const char*>& required, const std::vector<const char*>& available) {
		std::vector<const char*> filtered;
		for (const auto& req : required) {
			bool found = false;
			for (const auto& avail : available) {
				if (strcmp(req, avail) == 0) {
					found = true;
					break;
				}
			}
			if (found) {
				filtered.push_back(req);
			}
			else {
				LOGW("Extension '%s' not available, skipping", req);
			}
		}
		return filtered;
	}

	std::vector<const char*> Instance::getRequiredExtensions() {
#if defined(_WIN32)
		uint32_t glfwExtensionCount = 0;

		//得到要求实例扩展
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#elif defined(__ANDROID__)
		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			"VK_KHR_android_surface"
		};
#endif

		//添加校验层扩展
		if (mEnableValidationLayer) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); //开启调试功能
		}

		return extensions;
	}

	bool Instance::checkValidationLayerSupport() {
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);//获取全部测试功能数量

		std::vector<VkLayerProperties> availableLayers(layerCount);//创建储存全部测试功能的数组
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());//获取全部测试功能的名字

		for (const auto& layerName : validationLayers) {//遍历要开启的测试功能
			bool layerFound = false;

			for (const auto& layerProp : availableLayers) {//和全部测试功能对比是否有对应的测试功能
				if (std::strcmp(layerName, layerProp.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	void Instance::setupDebugger() {
		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |	//监听那些类型
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

		createInfo.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |	//监听那些类型
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		createInfo.pfnUserCallback = debugCallBack;//设置监听的回调函数
		createInfo.pUserData = nullptr;

		if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugger) != VK_SUCCESS) {
			LOGE("Instance::setupDebugger: failed to create debugger");
			throw std::runtime_error("Error:VulKan Instance . failed to create debugger");
		}
	}
}