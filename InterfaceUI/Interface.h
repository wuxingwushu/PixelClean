#pragma once
#include <chrono>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "GUI.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/device.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"

#include "../GlobalVariable.h"
#include "../Tool/Queue.h"
#include "ImGuiTexture.h"
#include "../VulkanTool/ShaderTexture.h"

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
			std::chrono::steady_clock::time_point timr;
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

		Queue<ChatBoxStr>* mChatBoxStr;
		void DisplayTextS();

		bool GetInterFaceBool() {//获取显示状态
			return InterFaceBool;
		}

		void SetInterFaceBool() {//显示状态取反
			InterFaceBool = !InterFaceBool;
		}

		// 缓存的 UI 缩放值，在 InterFace() 入口处更新，避免每帧各函数重复计算
		float mUIScale{ 1.0f };

		VkCommandBuffer GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info);

		VulKan::ShaderTexture* mShaderTexture{ nullptr };

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

		// 每个交换链图像一个二级 CB，每帧由 GetCommandBuffer 重新录制。
		// 不能缓存复用：ImGui 的悬停/按下等状态变化只体现在顶点颜色中，
		// 数量不变时复用旧 CB 会导致 UI 画面停留在录制那一刻。
		VulKan::CommandPool** ImGuiCommandPoolS;
		VulKan::CommandBuffer** ImGuiCommandBufferS;

		// 公共 UI 布局参数（消除三个界面函数中重复的 scale/btnW/btnH 计算）
		struct ButtonLayout {
			float scale;
			float btnScale;
			float btnW;
			float btnH;
			float spacing;
			float normalFontScale;
		};
		ButtonLayout GetButtonLayout() const;

		// 开始一个全屏无装饰面板（DrawBackground + Begin + SetWindowPos + SetWindowSize）
		// 调用者完成内容后需自行调用 ImGui::End()
		void BeginFullscreenPanel(const char* name, float fontScale);

		void MainInterface();//游戏主界面
		
		void ViceInterface();//游戏时的界面

		void MultiplePeopleInterface();//多人界面

		bool SetBool;
		InterFaceEnum_ PreviousLayerInterface = MainInterface_Enum;//储存上一层界面
		void SetInterface();//设置界面

		bool ShowLogViewer = false;
		void LogViewerInterface();//错误日志查看器
		std::string ReadLogFile(const std::string& filePath);//读取日志文件
	};
}
