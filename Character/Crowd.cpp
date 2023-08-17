#include "Crowd.h"
#include "../GlobalVariable.h"

namespace GAME {

	void DeleteCrowd(GamePlayer* Player, void* Data) {
		SquarePhysics::SquarePhysics* LSquarePhysics = (SquarePhysics::SquarePhysics*)Data;
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
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

		mNPCS = new ContinuousData<NPC*>(100);

		mCommandPool = new VulKan::CommandPool(mDevice);
	}

	Crowd::~Crowd()
	{

	}

	GamePlayer* Crowd::GetGamePlayer(evutil_socket_t key) {
		GamePlayer** LGamePlayer = MapPlayerS->Get(key);
		if (LGamePlayer == nullptr) {//不存在玩家是创建玩家
			LGamePlayer = MapPlayerS->New(key);
			(*LGamePlayer) = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, mSquarePhysics, 0, 0);
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

			Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
		}
		return (*LGamePlayer);
	}

	void Crowd::TimeoutDetection() {
		MapPlayerS->TimeoutDetection();
		GamePlayer** PlayerS = MapPlayerS->GetData();
		for (size_t i = 0; i < MapPlayerS->GetNumber(); i++)
		{
			PlayerS[i]->UpData();//更新玩家伤痕
		}
	}

	void Crowd::GetCommandBufferS(std::vector<VkCommandBuffer>* CommandBufferVector, unsigned int Format) {
		GamePlayer** PlayerS = MapPlayerS->GetData();
		for (size_t i = 0; i < MapPlayerS->GetNumber(); i++)
		{
			CommandBufferVector->push_back(PlayerS[i]->getCommandBuffer(Format));
		}
		NPC** LNPC = mNPCS->Data();
		for (size_t i = 0; i < mNPCS->GetNumber(); i++)
		{
			CommandBufferVector->push_back(LNPC[i]->getCommandBuffer(Format));
		}
	}


	void Crowd::AddNPC(int x, int y, Labyrinth* Labyrinth) {
		GamePlayer* LGamePlayer = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, mSquarePhysics, x, y);
		LGamePlayer->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			6,
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		LGamePlayer->InitCommandBuffer();

		NPC* LNPC = new NPC(LGamePlayer, Labyrinth);
		mNPCS->add(LNPC);
	}

	void Crowd::NPCEvent(int Format, float time) {
		NPC** LNPC = mNPCS->Data();
		for (size_t i = 0; i < mNPCS->GetNumber(); i++)
		{
			LNPC[i]->Event(Format, time);
		}
	}
}
