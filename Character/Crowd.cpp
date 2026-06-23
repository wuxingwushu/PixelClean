#include "Crowd.h"
#include "../DebugLog.h"
#include "../GlobalVariable.h"
#include "../BlockS/PixelS.h"

namespace GAME {

	void DeleteCrowd(GamePlayer* Player, void* Data) {
		PhysicsBlock::PhysicsWorld* LPhysicsWorld = (PhysicsBlock::PhysicsWorld*)Data;
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
		std::cout << "Delete Player, 释放 玩家" << std::endl;
		delete Player;
	}

	bool TimeoutCrowd(GamePlayer* x, void* data) {
		std::cout << "Timeout Player, 超时 玩家" << std::endl;
		return 1;
	}


	bool NPCTimeoutCrowd(NPCGOBT* x, void* data) {
		std::cout << "Timeout Player, 超时 NPC" << std::endl;
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
		PathfindingDecorator* pathfinding
	) :
		mSize(size),
		mDevice(Device),
		mPipeline(Pipeline),
		mSwapChain(SwapChain),
		mRenderPass(RenderPass),
		mSampler(Sampler),
		mCameraVPMatricesBuffer(CameraVPMatricesBuffer),
		wPathfinding(pathfinding)
	{
		NPCID = size;
		MapPlayerS = new ContinuousMap<evutil_socket_t, GamePlayer*>(size, ContinuousMap_Timeout);

		mNPCS = new ContinuousMap<evutil_socket_t, NPCGOBT*>(100, ContinuousMap_Timeout);
		mNPCS->SetTimeoutTime(500);
		mNPCS->SetTimeoutCallback(NPCTimeoutCrowd, this);

		mCommandPool = new VulKan::CommandPool(mDevice);
	}

	Crowd::~Crowd()
	{
		LOGD("Crowd::~Crowd() called");
		for (auto i : *MapPlayerS)
		{
			delete i;
		}
		for (auto i : *mNPCS)
		{
			delete i;
		}
		delete MapPlayerS;
		delete mNPCS;

		delete mCommandPool;
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
		for (size_t i = 0; i < MapPlayerS->GetNumber(); ++i)
		{
			PlayerS[i]->UpData();//更新玩家伤痕
		}
	}

	void Crowd::ReconfigurationCommandBuffer() {
		for (auto i : *MapPlayerS)
		{
			i->InitCommandBuffer();
		}
		for (auto i : *mNPCS)
		{
			i->InitCommandBuffer();
		}
	}

	void Crowd::GetCommandBufferS(std::vector<VkCommandBuffer>* CommandBufferVector, unsigned int Format) {
		for (auto i : *MapPlayerS)
		{
			CommandBufferVector->push_back(i->getCommandBuffer(Format));
		}
		for (auto i : *mNPCS)
		{
			CommandBufferVector->push_back(i->getCommandBuffer(Format));
		}
	}

	NPCGOBT* Crowd::GetNPC(evutil_socket_t key) {
		NPCGOBT** LGamePlayer = mNPCS->Get(key);
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
			LGamePlayer->SetKey(key);
			NPCGOBT** LNPC = mNPCS->New(key);
			*LNPC = new NPCGOBT(LGamePlayer, wPathfinding, wArms);

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
		LGamePlayer->SetKey(NPCID);
		// 注册子弹击中该 NPC 时的销毁处理（让玩家子弹能命中并伤害 NPC）
		if (wArms) {
			wArms->RegisterTankBulletHandler(LGamePlayer);
		}
		NPCGOBT** LNPC = mNPCS->New(NPCID);
		*LNPC = new NPCGOBT(LGamePlayer, wPathfinding, wArms);

		NPCID++;
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
	}

	void Crowd::NPCEvent(int Format, float time) {
		NPCGOBT** LNPC = mNPCS->GetData();
		for (size_t i = 0; i < mNPCS->GetNumber(); ++i)
		{
			if (LNPC[i]->GetDeathInBattle()) {
				Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
				evutil_socket_t K = LNPC[i]->GetKey();
				delete LNPC[i];
				mNPCS->Delete(K);
				i--;
				// 通知上层有 NPC 被击杀（用于计分）
				if (mOnNPCKilled) mOnNPCKilled();
				continue;
			}
			LNPC[i]->Event(Format, time);
		}
	}

	void Crowd::RespawnNPC(int x, int y) {
		AddNPC(x, y);
	}

	void Crowd::KillAll() {
		evutil_socket_t LNPCkey;
		for (auto i : *mNPCS)
		{
			LNPCkey = i->GetKey();
			delete i;
			mNPCS->Delete(LNPCkey);
		}
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer
	}
}
