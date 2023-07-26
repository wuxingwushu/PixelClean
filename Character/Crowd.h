#pragma once
#include "GamePlayer.h"
#include <event2/util.h>

namespace GAME {
	void DeleteCrowd(GamePlayer* Player, void* Data);
	bool TimeoutCrowd(GamePlayer* x, void* data);

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

		void TimeoutDetection();

		void GetCommandBufferS(std::vector<VkCommandBuffer>* CommandBufferVector, unsigned int Format);

		unsigned int GetNumber() {
			return MapPlayerS->GetNumber();
		}

		void SteSquarePhysics(SquarePhysics::SquarePhysics* SquarePhysics) {
			mSquarePhysics = SquarePhysics;
			MapPlayerS->SetDeleteCallback(DeleteCrowd, mSquarePhysics);
			MapPlayerS->SetTimeoutCallback(TimeoutCrowd, nullptr);
		}

	private:
		unsigned int mSize = 0;
		VulKan::Device* mDevice = nullptr;
		VulKan::CommandPool* mCommandPool = nullptr;
		VulKan::Pipeline* mPipeline = nullptr;
		VulKan::SwapChain* mSwapChain = nullptr;
		VulKan::RenderPass* mRenderPass = nullptr;
		VulKan::Sampler* mSampler = nullptr;
		std::vector<VulKan::Buffer*> mCameraVPMatricesBuffer = {};

		ContinuousMap<evutil_socket_t, GamePlayer*>* MapPlayerS;

		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;
	};
}
