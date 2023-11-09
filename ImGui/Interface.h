#pragma once
#include "GUI.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/device.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"

#include "../GlobalVariable.h"
#include "../Tool/QueueData.h"
#include "ImGuiTexture.h"

namespace GAME {

	enum InterFaceEnum_ {
		MainInterface_Enum = 0,
		ViceInterface_Enum,
		MultiplePeopleInterface_Enum,
		SetInterface_Enum
	};

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
			ImGuiTexture* imGuiTexture,
			int FormatCount
		);

		void StructureImGuiInterFace();//构建

		~ImGuiInterFace();

		unsigned int mCurrentFrame;//当前第几帧
		void InterFace(unsigned int CurrentFrame);//显示
		int GetInterfaceIndexes() { return InterfaceIndexes; }

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

		ImFont* Font; //字体
	private:
		VkDescriptorPool			g_DescriptorPool = VK_NULL_HANDLE;//给 ImGui 创建的 DescriptorPool 记得销毁
		int							g_MinImageCount = 3;
		int mFormatCount;
		VulKan::Window* mWindown{ nullptr };
		VulKan::Device* mDevice{ nullptr };
		ImGui_ImplVulkan_InitInfo mInfo;
		VulKan::RenderPass* mRenderPass;

		ImGuiTexture* mImGuiTexture{ nullptr };
		
		VulKan::CommandBuffer* mCommandBuffer;

		InterFaceEnum_ InterfaceIndexes = MainInterface_Enum;//显示那个界面
		bool InterFaceBool = true;//是否显示界面

		VulKan::CommandPool** ImGuiCommandPoolS;
		VulKan::CommandBuffer** ImGuiCommandBufferS;

		void MainInterface();//游戏主界面
		
		void ViceInterface();//游戏时的界面

		void MultiplePeopleInterface();//多人界面

		bool SetBool;
		InterFaceEnum_ PreviousLayerInterface = MainInterface_Enum;//储存上一层界面
		void SetInterface();//设置界面
	};
}
