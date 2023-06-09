#include "GUI.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/device.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"

namespace GAME {
	class ImGuiInterFace
	{
	public:
		ImGuiInterFace(
			VulKan::Device* device, 
			VulKan::Window* Win, 
			ImGui_ImplVulkan_InitInfo Info, 
			VulKan::RenderPass* Pass,
			VulKan::CommandBuffer* commandbuffer,
			int FormatCount
		);

		~ImGuiInterFace();

		void InterFace();

		void ImGuiShowFPS();
		void ImGuiShowTiming();

		bool GetInterFaceBool() {
			return InterFaceBool;
		}

		void SetInterFaceBool() {
			InterFaceBool = !InterFaceBool;
		}

		VkCommandBuffer GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info);

	private:
		VkDescriptorPool			g_DescriptorPool = VK_NULL_HANDLE;//给 ImGui 创建的 DescriptorPool 记得销毁
		int							g_MinImageCount = 3;
		int mFormatCount;
		VulKan::Window* mWindown{ nullptr };
		VulKan::Device* mDevice{ nullptr };

		int InterfaceIndexes = 0;
		bool InterFaceBool = false;

		VulKan::CommandPool** ImGuiCommandPoolS;
		VulKan::CommandBuffer** ImGuiCommandBufferS;

		void SetInterFace(int fi) {
			InterfaceIndexes = fi;
		}

		void MainInterface();

		void ViceInterface();

		
	};
}