#include "Crowd.h"
#include "../GlobalVariable.h"
#include "../BlockS/PixelS.h"

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


	bool NPCTimeoutCrowd(NPC* x, void* data) {
		std::cout << "Timeout Player, 超时 NPC" << std::endl;
		Crowd* LCrowd = (Crowd*)data;
		LCrowd->GetRoleSynchronizationData()->Delete(x->GetKey());
		delete x;
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
		return 1;
	}

	Crowd::Crowd(
		unsigned int size,
		VulKan::Device* Device,
		VulKan::Pipeline* Pipeline,
		VulKan::SwapChain* SwapChain,
		VulKan::RenderPass* RenderPass,
		VulKan::Sampler* Sampler,
		std::vector<VulKan::Buffer*> CameraVPMatricesBuffer,
		Labyrinth* labyrinth
	) :
		mSize(size),
		mDevice(Device),
		mPipeline(Pipeline),
		mSwapChain(SwapChain),
		mRenderPass(RenderPass),
		mSampler(Sampler),
		mCameraVPMatricesBuffer(CameraVPMatricesBuffer),
		mLabyrinth(labyrinth)
	{
		NPCID = size;
		MapPlayerS = new ContinuousMap<evutil_socket_t, GamePlayer*>(size, ContinuousMap_Timeout);

		mNPCS = new ContinuousMap<evutil_socket_t, NPC*>(100, ContinuousMap_Timeout);
		mNPCS->SetTimeoutTime(500);
		mNPCS->SetTimeoutCallback(NPCTimeoutCrowd, this);
		mNPCSynchronizationData = new ContinuousMap<evutil_socket_t, RoleSynchronizationData>(100, ContinuousMap_Timeout | ContinuousMap_Pointer);
		mNPCSynchronizationData->SetPointerCallback(PointerGamePlayer);//（调整储存连续时，同时更新引用者）更新指针

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

	void Crowd::ReconfigurationCommandBuffer() {
		GamePlayer** LGamePlayer = MapPlayerS->GetData();
		for (size_t i = 0; i < MapPlayerS->GetNumber(); i++)
		{
			LGamePlayer[i]->InitCommandBuffer();
		}
		NPC** LNPC = mNPCS->GetData();
		for (size_t i = 0; i < mNPCS->GetNumber(); i++)
		{
			LNPC[i]->InitCommandBuffer();
		}
	}

	void Crowd::GetCommandBufferS(std::vector<VkCommandBuffer>* CommandBufferVector, unsigned int Format) {
		GamePlayer** PlayerS = MapPlayerS->GetData();
		for (size_t i = 0; i < MapPlayerS->GetNumber(); i++)
		{
			CommandBufferVector->push_back(PlayerS[i]->getCommandBuffer(Format));
		}
		NPC** LNPC = mNPCS->GetData();
		for (size_t i = 0; i < mNPCS->GetNumber(); i++)
		{
			CommandBufferVector->push_back(LNPC[i]->getCommandBuffer(Format));
		}
	}

	NPC* Crowd::GetNPC(evutil_socket_t key) {
		NPC** LGamePlayer = mNPCS->Get(key);
		if (LGamePlayer == nullptr) {//不存NPC
			GamePlayer* LGamePlayer = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, mSquarePhysics, 0, 0);
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
			RoleSynchronizationData* LRole = mNPCSynchronizationData->New(key);
			LRole->Key = key;
			LRole->mBufferEventSingleData = new BufferEventSingleData(100);
			LGamePlayer->SetRoleSynchronizationData(LRole);
			NPC** LNPC = mNPCS->New(key);
			*LNPC = new NPC(LGamePlayer, mLabyrinth, wArms);

			mNPCSynchronizationData->SetPointerData(key, LGamePlayer);

			Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
			return *LNPC;
		}
		return *LGamePlayer;
	}


	void Crowd::AddNPC(int x, int y) {
		GamePlayer* LGamePlayer = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, mSquarePhysics, x, y);
		LGamePlayer->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			(rand() % TextureNumber),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		LGamePlayer->InitCommandBuffer();
		RoleSynchronizationData* LRole = mNPCSynchronizationData->New(NPCID);
		LRole->Key = NPCID;
		LRole->mBufferEventSingleData = new BufferEventSingleData(100);
		LGamePlayer->SetRoleSynchronizationData(LRole);
		NPC** LNPC = mNPCS->New(NPCID);
		*LNPC = new NPC(LGamePlayer, mLabyrinth, wArms);

		mNPCSynchronizationData->SetPointerData(NPCID, LGamePlayer);

		NPCID++;
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
	}

	void Crowd::NPCEvent(int Format, float time) {
		NPC** LNPC = mNPCS->GetData();
		for (size_t i = 0; i < mNPCS->GetNumber(); i++)
		{
			if (LNPC[i]->GetDeathInBattle()) {
				Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
				evutil_socket_t K = LNPC[i]->GetKey();
				delete LNPC[i];
				mNPCS->Delete(K);
				mNPCSynchronizationData->Delete(K);
				i--;
				continue;
			}
			LNPC[i]->Event(Format, time);
		}
	}

	void Crowd::KillAll() {
		NPC** LNPC = mNPCS->GetData();
		int NumberLNPC = mNPCS->GetNumber();
		evutil_socket_t LNPCkey;
		for (size_t i = 0; i < NumberLNPC; i++)
		{
			LNPCkey = LNPC[i]->GetKey();
			delete LNPC[i];
			mNPCS->Delete(LNPCkey);
			mNPCSynchronizationData->Delete(LNPCkey);
		}
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
	}
}
