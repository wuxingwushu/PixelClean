#pragma once
#include "GUI.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/device.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"

#include "../GlobalVariable.h"
#include "../Tool/QueueData.h"

namespace GAME {
	class ImGuiInterFace
	{
		struct ChatBoxStr {
			std::string str;
			clock_t timr;
		};
	public:
		ImGuiInterFace(
			VulKan::Device* device, 
			VulKan::Window* Win, 
			ImGui_ImplVulkan_InitInfo Info, 
			VulKan::RenderPass* Pass,
			VulKan::CommandBuffer* commandbuffer,
			int FormatCount
		);

		void StructureImGuiInterFace();//构建

		~ImGuiInterFace();

		void InterFace();//显示

		void ImGuiShowFPS();//显示
		void ImGuiShowTiming();


		bool ConsoleFocusHere = true;
		void ConsoleInterface();//控制台

		QueueData<ChatBoxStr>* mChatBoxStr;
		void DisplayTextS();

		bool GetInterFaceBool() {//获取显示状态
			return InterFaceBool;
		}

		void SetInterFaceBool() {//显示状态取反
			InterFaceBool = !InterFaceBool;
		}

		VkCommandBuffer GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info);

		bool GetMultiplePeople() { return StartMultiPlayerGames; }
		bool GetServerOrClient() { return ServerOrClient; }

	private:
		VkDescriptorPool			g_DescriptorPool = VK_NULL_HANDLE;//给 ImGui 创建的 DescriptorPool 记得销毁
		int							g_MinImageCount = 3;
		int mFormatCount;
		VulKan::Window* mWindown{ nullptr };
		VulKan::Device* mDevice{ nullptr };
		ImGui_ImplVulkan_InitInfo mInfo;
		VulKan::RenderPass* mRenderPass;
		ImFont* Font; //字体
		VulKan::CommandBuffer* mCommandBuffer;

		int InterfaceIndexes = 0;//显示那个界面
		bool InterFaceBool = true;//是否显示界面

		VulKan::CommandPool** ImGuiCommandPoolS;
		VulKan::CommandBuffer** ImGuiCommandBufferS;

		void SetInterFace(int fi) {//设置显示界面
			InterfaceIndexes = fi;
		}

		void MainInterface();//游戏主界面
		
		void ViceInterface();//游戏时的界面

		bool StartMultiPlayerGames = false;//是否为多人模式
		bool ServerOrClient;//是服务器还是客户端
		void MultiplePeopleInterface();//多人界面

		bool SetBool;
		int PreviousLayerInterface = 0;//储存上一层界面
		void SetInterface();//设置界面
	};
}
