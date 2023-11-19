#pragma once
#include <vector>//动态数组
#include <array>//静态数组
#include <map>
#include <string>//字符串
#include <optional>
#include <set>
#include <fstream>
#include <vulkan/vulkan.h>//VulKan API

//开启的测试模式
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"//测试类型
};

namespace VulKan {
	
	class Instance {
	public:
		Instance(bool enableValidationLayer);

		~Instance();



		//得到符合要求实例扩展
		std::vector<const char*> getRequiredExtensions();



		//检查验证层是否支持
		bool checkValidationLayerSupport();
		//设置调试器的返回那些信息
		void setupDebugger();
		//判断是否开启了检测
		inline bool getEnableValidationLayer() const noexcept { return mEnableValidationLayer; }

		

		//获取VulKan的实列
		[[nodiscard]] inline VkInstance getInstance() const noexcept { return mInstance; }

		[[nodiscard]] inline std::vector<const char*> getextensions() const noexcept { return extensions; }

		

	private:
		//打印所以扩展名
		void printAvailableExtensions();

	private:
		VkInstance mInstance{ VK_NULL_HANDLE };//实列指针
		bool mEnableValidationLayer{ false };//是否开启了校验层
		VkDebugUtilsMessengerEXT mDebugger{ VK_NULL_HANDLE };//调试信使指针


		std::vector<const char*> extensions;
	};
}