#pragma once

#include "Camera.h"

#include "SoundEffect/SoundEffect.h"

#include "Labyrinth/Labyrinth.h"

#include "Labyrinth/Dungeon.h"

#include "GameMods/Configuration.h"
#include "GameMods/UnlimitednessMapMods.h"
#include "GameMods/MazeMods.h"


namespace GAME {
	class Application : public Configuration {
	public:
		Application();

		~Application() = default;

		//总初始化
		void run();

		//获取对于游戏
		GameMods* GetGame(GameModsEnum Game);

		void DeleteGame(GameModsEnum Game);

		//重建交换链:  当窗口大小发生变化的时候，交换链也要发生变化，Frame View Pipeline RenderPass Sync
		void recreateSwapChain();

	private:
		//窗口的初始化
		void initWindow();

		//初始化Vulkan
		void initVulkan();

		//初始化ImGui
		void initImGui();

		//初始化游戏
		void initGame();

		//主循环main
		void mainLoop();

		/*************************************************************************************/
		//游戏循环事件
		void GameLoop();
		/*************************************************************************************/

		//渲染一帧画面
		void Render();

		//回收资源
		void cleanUp();

	private:
		//创建渲染管线
		void createPipeline();
		//设置GPU画布
		void createRenderPass();
		//录入渲染指令
		void createCommandBuffers(unsigned int i);
		//创建信号量（用于渲染同步），和创建Fence（用于不重复提交指令）
		void createSyncObjects();

	private:
		//ImGui
		VulKan::CommandPool* mImGuuiCommandPool{ nullptr };
		VulKan::CommandBuffer* mImGuuiCommandBuffers{ nullptr };
		VulKan::RenderPass* mImGuuiRenderPass{ nullptr };



		std::vector<VulKan::Semaphore*> mImageAvailableSemaphores{};//图片显示完毕信号量
		std::vector<VulKan::Semaphore*> mRenderFinishedSemaphores{};//图片渲染完成信号量
		std::vector<VulKan::Fence*> mFences{};//控制管线工作，比如（下一个管线需要上一个管线的图片，那就等上一个管线图片输入进来才开始工作）


		std::vector<VkCommandBuffer> ThreadCommandBufferS;//把录制好的二级指令存放在数组中统一绑定在主指令缓存中
		int mCurrentFrame{ 0 };//当前是渲染哪一张GPU画布
		
		GameMods* mGameMods = nullptr;
	};
}
