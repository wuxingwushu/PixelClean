#pragma once
#include "../Vulkan/instance.h"
#include "../Vulkan/device.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/windowSurface.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/shader.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../VulKan/commandBuffer.h"
#include "../VulKan/semaphore.h"
#include "../VulKan/fence.h"
#include "../Vulkan/buffer.h"
#include "../Vulkan/sampler.h"
#include "../VulkanTool/PipelineS.h"

#include "../ImGui/Interface.h"
#include "../Camera.h"
#include "../GlobalStructural.h"
#include "../Arms/ParticleSystem.h"
#include "../Character/GamePlayer.h"
#include "../Arms/Arms.h"
#include "../Character/Crowd.h"
#include "../VulkanTool/VisualEffect.h"
#include "../VulkanTool/AuxiliaryVision.h"
#include "../Tool/AStar.h"
#include "../VulkanTool/TextureLibrary.h"
#include "../Character/DamagePrompt.h"

namespace GAME {

	class Configuration
	{
	public:
		Configuration() {}
		~Configuration() {}

		//int mCurrentFrame{ 0 };//当前是渲染哪一张GPU画布
		VulKan::Window* mWindow{ nullptr };//创建窗口
		VulKan::Instance* mInstance{ nullptr };//实列化需要的VulKan功能APi
		VulKan::Device* mDevice{ nullptr };//获取电脑的硬件设备
		VulKan::WindowSurface* mWindowSurface{ nullptr };//获取你在什么平台运行调用不同的API（比如：Window，Android）
		VulKan::SwapChain* mSwapChain{ nullptr };//设置VulKan的工作细节
		VulKan::RenderPass* mRenderPass{ nullptr };//设置GPU画布
		VulKan::CommandPool* mCommandPool{ nullptr };//设置渲染指令的工作
		VulKan::Sampler* mSampler{ nullptr };//图片采样器
		VulKan::PipelineS* mPipelineS{ nullptr }; //渲染管线集合
		std::vector<VulKan::CommandBuffer*> mCommandBuffers{};//主录入渲染指令
		std::vector<VkCommandBuffer>*		wThreadCommandBufferS = nullptr;//把录制好的二级指令存放在数组中统一绑定在主指令缓存中



		double CursorPosX = 0, CursorPosY = 0;//光标位置
		glm::vec2 PlayerForce{};//玩家移动受力
		unsigned int AttackType = 0;//攻击模式
		Camera*      mCamera{ nullptr };//定义的相机
		ImGuiInterFace* InterFace = nullptr; // ImGui 游戏界面都写这里面
		TextureLibrary* mGIFTextureLibrary{ nullptr };//GIF库
		TextureLibrary* mTextureLibrary{ nullptr };//素材库
		std::vector<VulKan::Buffer*> mCameraVPMatricesBuffer{};//GPU用的玩家变换矩阵（位置 角度）
		ParticleSystem* mParticleSystem = nullptr;//粒子系统
		ParticlesSpecialEffect* mParticlesSpecialEffect = nullptr;//粒子特效
		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;//物理系统
		Arms* mArms = nullptr;//武器




		GamePlayer* mGamePlayer{ nullptr };//玩家
		Crowd* mCrowd = nullptr;//玩家群系统
		VulKan::VisualEffect* mVisualEffect{ nullptr };//视觉效果
		VulKan::AuxiliaryVision* mAuxiliaryVision{ nullptr };//辅助视觉
		DamagePrompt* mDamagePrompt{ nullptr };//受伤提示
		JPS* JPSPathfinding = nullptr;
		AStar* AStarPathfinding = nullptr;
	};

}