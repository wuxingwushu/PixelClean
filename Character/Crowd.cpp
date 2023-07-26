#include "Crowd.h"

namespace GAME {

	void DeleteCrowd(GamePlayer* Player, void* Data) {
		SquarePhysics::SquarePhysics* LSquarePhysics = (SquarePhysics::SquarePhysics*)Data;
		LSquarePhysics->RemoveObjectCollision(Player->mObjectCollision);
		std::cout << "Delete Player, 释放 玩家" << std::endl;
		delete Player;
	}

	bool TimeoutCrowd(GamePlayer* x, void* data) {
		std::cout << "Timeout Player, 超时 玩家" << std::endl;
		return 1;
	}


	Crowd::Crowd(
		unsigned int size,
		VulKan::Device* Device,
		VulKan::Pipeline* Pipeline,
		VulKan::SwapChain* SwapChain,
		VulKan::RenderPass* RenderPass,
		VulKan::Sampler* Sampler,
		std::vector<VulKan::Buffer*> CameraVPMatricesBuffer
	) :
		mSize(size),
		mDevice(Device),
		mPipeline(Pipeline),
		mSwapChain(SwapChain),
		mRenderPass(RenderPass),
		mSampler(Sampler),
		mCameraVPMatricesBuffer(CameraVPMatricesBuffer)
	{
		MapPlayerS = new ContinuousMap<evutil_socket_t, GamePlayer*>(size);

		mCommandPool = new VulKan::CommandPool(mDevice);
	}

	Crowd::~Crowd()
	{

	}

	GamePlayer* Crowd::GetGamePlayer(evutil_socket_t key) {
		GamePlayer** LGamePlayer = MapPlayerS->Get(key);
		if (LGamePlayer == nullptr) {
			LGamePlayer = MapPlayerS->New(key);
			(*LGamePlayer) = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, 0, 0);
			(*LGamePlayer)->initUniformManager(
				mDevice,
				mCommandPool,
				mSwapChain->getImageCount(),
				6,
				mPipeline->DescriptorSetLayout,
				mCameraVPMatricesBuffer,
				mSampler
			);
			(*LGamePlayer)->InitCommandBuffer();
			mSquarePhysics->AddObjectCollision((*LGamePlayer)->mObjectCollision);
		}
		return (*LGamePlayer);
	}

	void Crowd::TimeoutDetection() {
		MapPlayerS->TimeoutDetection();
	}

	void Crowd::GetCommandBufferS(std::vector<VkCommandBuffer>* CommandBufferVector, unsigned int Format) {
		GamePlayer** PlayerS = MapPlayerS->GetData();
		for (size_t i = 0; i < MapPlayerS->GetNumber(); i++)
		{
			CommandBufferVector->push_back(PlayerS[i]->getCommandBuffer(Format));
		}
	}
}
