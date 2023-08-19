#pragma once
#include "GamePlayer.h"
#include "NPC.h"
#include "../Tool/ContinuousData.h"
#include <event2/util.h>

namespace GAME {
	void DeleteCrowd(GamePlayer* Player, void* Data);//销毁玩家
	bool TimeoutCrowd(GamePlayer* x, void* data);//超时事件

	class Crowd
	{
	public:
		Crowd(
			unsigned int size,
			VulKan::Device* Device,
			VulKan::Pipeline* Pipeline,
			VulKan::SwapChain* SwapChain,
			VulKan::RenderPass* RenderPass,
			VulKan::Sampler* Sampler,
			std::vector<VulKan::Buffer*> CameraVPMatricesBuffer
		);

		~Crowd();

		GamePlayer* GetGamePlayer(evutil_socket_t key);

		//超时销毁
		void TimeoutDetection();

		//获取所有玩家CommandBuffer
		void GetCommandBufferS(std::vector<VkCommandBuffer>* CommandBufferVector, unsigned int Format);

		//获取玩家数量（客户端是不包括自己的）
		unsigned int GetNumber() {
			return MapPlayerS->GetNumber();
		}

		//设置物理
		void SteSquarePhysics(SquarePhysics::SquarePhysics* SquarePhysics) {
			mSquarePhysics = SquarePhysics;
			MapPlayerS->SetDeleteCallback(DeleteCrowd, mSquarePhysics);//销毁时，随便把物理模拟中的也一并销毁
			MapPlayerS->SetTimeoutCallback(TimeoutCrowd, nullptr);
			MapPlayerS->SetTimeoutTime(1000);
		}

		void ReconfigurationCommandBuffer();
		
		void UpTime() {
			MapPlayerS->UpDataWholeTime();
		}

		void AddNPC(int x, int y, Labyrinth* Labyrinth);

		void NPCEvent(int Format, float time);

		void KillAll();

	private:
		//储存用来生成玩家
		unsigned int mSize = 0;
		VulKan::Device* mDevice = nullptr;
		VulKan::CommandPool* mCommandPool = nullptr;
		VulKan::Pipeline* mPipeline = nullptr;
		VulKan::SwapChain* mSwapChain = nullptr;
		VulKan::RenderPass* mRenderPass = nullptr;
		VulKan::Sampler* mSampler = nullptr;
		std::vector<VulKan::Buffer*> mCameraVPMatricesBuffer = {};

		ContinuousMap<evutil_socket_t, GamePlayer*>* MapPlayerS = nullptr;//玩家映射

		ContinuousData<NPC*>* mNPCS = nullptr;

		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;//物理
	};
}
