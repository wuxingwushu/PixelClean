#pragma once

#include "base.h"
#include "Vulkan/instance.h"
#include "Vulkan/device.h"
#include "Vulkan/Window.h"
#include "Vulkan/windowSurface.h"
#include "Vulkan/swapChain.h"
#include "Vulkan/shader.h"
#include "Vulkan/pipeline.h"
#include "Vulkan/renderPass.h"
#include "Vulkan/commandPool.h"
#include "VulKan/commandBuffer.h"
#include "VulKan/semaphore.h"
#include "VulKan/fence.h"
#include "Vulkan/buffer.h"

#include "Camera.h"

#include "SoundEffect/SoundEffect.h"

#include "Character/GamePlayer.h"

#include "ImGui/Interface.h"

#include "Labyrinth/Labyrinth.h"

#include "Arms/ParticleSystem.h"
#include "Arms/Arms.h"

#include "Physics/SquarePhysics.h"

#include "Character/Crowd.h"

#include "VulkanTool/PipelineS.h"

#include "Labyrinth/Dungeon.h"

#include "VulkanTool/VisualEffect.h"
#include "VulkanTool/AuxiliaryVision.h"
#include "Tool/JPS.h"
#include "Tool/AStar.h"

namespace GAME {
	class Application{
	public:
		Application();

		~Application() = default;

		//总初始化
		void run(VulKan::Window* w);
		//鼠标事件
		void onMouseMove(double xpos, double ypos);
		//键盘事件
		void onKeyDown(CAMERA_MOVE moveDirection);

		//鼠标滚轮事件
		void SetCameraZ(int z) {
			m_position.z += z * 10;
			mCamera.update();
		}

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
		//加载游戏内容
		void LoadingGame();
		//卸载游戏内容
		void UninstallGame();


		//游戏循环事件
		void GameLoop();
		void GameCommandBuffers(unsigned int Format_i, VkCommandBufferInheritanceInfo info);
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


		
		



		

	


	//多线程录制二级指令
	private:
		std::vector<VkCommandBuffer>		ThreadCommandBufferS;//把录制好的二级指令存放在数组中统一绑定在主指令缓存中

	private:
		int mCurrentFrame{ 0 };//当前是渲染哪一张GPU画布
		VulKan::Window* mWindow{ nullptr };//创建窗口
		VulKan::Instance* mInstance{ nullptr };//实列化需要的VulKan功能APi
		VulKan::Device* mDevice{ nullptr };//获取电脑的硬件设备
		VulKan::WindowSurface* mWindowSurface{ nullptr };//获取你在什么平台运行调用不同的API（比如：Window，Android）
		VulKan::SwapChain* mSwapChain{ nullptr };//设置VulKan的工作细节
		VulKan::RenderPass* mRenderPass{ nullptr };//设置GPU画布
		//VulKan::Pipeline* mPipeline{ nullptr };//设置渲染管线
		VulKan::CommandPool* mCommandPool{ nullptr };//设置渲染指令的工作
		VulKan::Sampler* mSampler{ nullptr };//图片采样器
		std::vector<VulKan::CommandBuffer*> mCommandBuffers{};//主录入渲染指令

		VulKan::PipelineS* mPipelineS{ nullptr }; //渲染管线集合

		//ImGui
		VulKan::CommandPool* mImGuuiCommandPool{ nullptr };
		VulKan::CommandBuffer* mImGuuiCommandBuffers{ nullptr };
		VulKan::RenderPass* mImGuuiRenderPass{ nullptr };







		std::vector<VulKan::Semaphore*> mImageAvailableSemaphores{};//图片显示完毕信号量
		std::vector<VulKan::Semaphore*> mRenderFinishedSemaphores{};//图片渲染完成信号量
		std::vector<VulKan::Fence*> mFences{};//控制管线工作，比如（下一个管线需要上一个管线的图片，那就等上一个管线图片输入进来才开始工作）


		glm::vec2 PlayerForce{};//玩家移动受力
		GamePlayer* mGamePlayer{ nullptr };//玩家
		double CursorPosX = 0, CursorPosY = 0;//光标位置
	public:
		unsigned int AttackType = 0;//攻击模式
	private:

		Labyrinth* mLabyrinth = nullptr;//迷宫
		ParticleSystem* mParticleSystem = nullptr;//粒子系统
		
		
		VPMatrices	mVPMatrices{};//玩家变换矩阵（位置 角度）
		Camera      mCamera{};//定义的相机
		std::vector<VulKan::Buffer*> mCameraVPMatricesBuffer{};//GPU用的玩家变换矩阵（位置 角度）

	public:
		ImGuiInterFace* InterFace = nullptr; // ImGui 游戏界面都写这里面

		
		Arms* mArms = nullptr;//武器
		Crowd* mCrowd = nullptr;//玩家群系统

		ParticlesSpecialEffect* mParticlesSpecialEffect = nullptr;//粒子特效
		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;//物理系统


		//测试
		//GifPipeline* mGifPipeline = nullptr;//GIF渲染管线
		//GIF* mGIF = nullptr;//GIF
		Dungeon* mDungeon = nullptr;//无限地图

		VulKan::VisualEffect* mVisualEffect{ nullptr };//视觉效果
		VulKan::AuxiliaryVision* mAuxiliaryVision{ nullptr };//辅助视觉
		JPS* JPSPathfinding = nullptr;
		AStar* AStarPathfinding = nullptr;

		//GIF库
		TextureLibrary* mTextureLibrary{ nullptr };
	};
}