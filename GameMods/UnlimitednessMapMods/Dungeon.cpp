#include "Dungeon.h"

#include "../../VulkanTool/PixelTexture.h"
#include "../../Tool/Tool.h"

namespace GAME {

	void Destroy(int* P, int x, int y, bool Bool) {
		P[y * 16 + x] = *((int*)&pixelS[20][(y * 16 + x) * 4]);
	}

	void DungeonDestroy(int x, int y, bool Bool, SquarePhysics::ObjectDecorator* Object, void* wClass) {
		DungeonDestroyStruct* LSDungeon = (DungeonDestroyStruct*)wClass;
		int* LSP = (int*)LSDungeon->wTextureAndBuffer->mPixelTexture->getHOSTImagePointer();
		Destroy(LSP, x, y, Bool);
		for (size_t ix = 0; ix < LSDungeon->wDungeon->mNumberX; ++ix)
		{
			for (size_t iy = 0; iy < LSDungeon->wDungeon->mNumberY; ++iy)
			{
				if (LSDungeon->wDungeon->mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mModel == LSDungeon->wTextureAndBuffer) {
					LSDungeon->wDungeon->PixelWallNumberReduce(x + (ix * 16), y + (iy * 16));
				}
			}
		}
		LSDungeon->wTextureAndBuffer->mPixelTexture->endHOSTImagePointer();
		LSDungeon->wTextureAndBuffer->mPixelTexture->UpDataImage();
		unsigned int dian = ((LSDungeon->wTextureAndBuffer->MistPointerY * LSDungeon->wDungeon->mSquareSideLength + y) * LSDungeon->wDungeon->mNumberX * LSDungeon->wDungeon->mSquareSideLength + (LSDungeon->wTextureAndBuffer->MistPointerX * LSDungeon->wDungeon->mSquareSideLength + x));
		int* P = (int*)LSDungeon->wDungeon->WallBool->getupdateBufferByMap();
		P[dian] = 0;
		LSDungeon->wDungeon->WallBool->endupdateBufferByMap();
		unsigned char* TP = (unsigned char*)LSDungeon->wDungeon->WarfareMist->getHOSTImagePointer();
		for (size_t i = 0; i < 4; ++i)
		{
			TP[dian * 4 + i] = pixelS[20][(y * 16 + x) * 4 + i] * (i == 3 ? 1 : 0.3f);
		}
		LSDungeon->wDungeon->WarfareMist->endHOSTImagePointer();
	}

	void GenerateBlock(SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* mT, int x, int y, void* Data) {
		GAME::Dungeon* LDungeon = (GAME::Dungeon*)Data;
		ObjectUniformGIF* LUniform = (ObjectUniformGIF*)mT->mModel->mBufferS->getupdateBufferByMap();
		mT->mModel->Type = LDungeon->GetNoise(x, y);
		if (mT->mModel->Type == 20) {
			GAME::TextureLibrary::TextureToUVInfo TUinfo = LDungeon->wTextureLibrary->GetTextureUV("004_24");
			mT->mModel->mGIFDescriptorSet = LDungeon->AddGIF(mT->mModel->mBufferS, TUinfo.mTexture, TUinfo.mUV);
			LUniform->chuang = 1.0f / TUinfo.X;
		}
		LUniform->mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x * 16, y * 16, 0.0f));//位移矩阵
		mT->mModel->mBufferS->endupdateBufferByMap();
		bool CollisionBool = false;
		if (mT->mModel->Type > (TextureNumber / 2)) {
			CollisionBool = true;
		}
		mT->mModel->mPixelTexture->updateBufferByMap((void*)pixelS[mT->mModel->Type], 16 * 16 * 4);
		for (size_t ix = 0; ix < LDungeon->mSquareSideLength; ++ix)
		{
			for (size_t iy = 0; iy < LDungeon->mSquareSideLength; ++iy)
			{
				for (size_t i = 0; i < 4; ++i)
				{
					LDungeon->LSMistPointer[((mT->mModel->MistPointerY * 16 + ix) * 16 * LDungeon->mNumberX * 4) + (mT->mModel->MistPointerX * 16 * 4) + (iy * 4) + i] = pixelS[mT->mModel->Type][(ix * 16 * 4) + (iy * 4) + i] * (i == 3 ? 1 : 0.3f);
				}
				LDungeon->LSPointer[((mT->mModel->MistPointerY * 16 + ix) * 16 * LDungeon->mNumberX) + (mT->mModel->MistPointerX * 16) + iy] = CollisionBool;
			}
		}
		for (size_t ix = 0; ix < LDungeon->mSquareSideLength; ++ix)
		{
			for (size_t iy = 0; iy < LDungeon->mSquareSideLength; ++iy)
			{
				mT->mGridDecorator.at({ ix,iy })->Collision = CollisionBool;
			}
		}
	}

	Dungeon::Dungeon(VulKan::Device* device, unsigned int X, unsigned int Y, SquarePhysics::SquarePhysics* squarePhysics, float GamePlayerX, float GamePlayerY)
		:wDevice(device),
		mNumberX(X),
		mNumberY(Y),
		wSquarePhysics(squarePhysics)
	{
		mPerlinNoise = new PerlinNoise();

		mMoveTerrain = new SquarePhysics::MoveTerrain<TextureAndBuffer>(mNumberX, mNumberY, mSquareSideLength, 1);
		mMoveTerrain->SetPos(GamePlayerX, GamePlayerY);
		mMoveTerrain->SetOrigin(mNumberX / 2, mNumberY / 2);
		mMoveTerrain->SetCallback(
			[](SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* mT, int x, int y, void* Data) {
				GAME::Dungeon* LDungeon = (GAME::Dungeon*)Data;
				if (LDungeon->Pointerkaiguan) {//开启HOST指针
					LDungeon->Pointerkaiguan = false;
					TOOL::mTimer->MomentTiming(u8"地图移动生成耗时 ");
					LDungeon->LSMistPointer = (unsigned char*)LDungeon->WarfareMist->getHOSTImagePointer();
					LDungeon->LSPointer = (int*)LDungeon->WallBool->getupdateBufferByMap();
				}
				//std::cout << x << " - " << y << " - " << mT->mModel->mBufferS << std::endl;
				LDungeon->MultithreadingGenerate.push_back(TOOL::mThreadPool->enqueue(&GenerateBlock, mT, x, y, Data));
				LDungeon->MultithreadingPixelTexture.push_back(mT->mModel->mPixelTexture);
			},
			this,
			[](SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* mT, void* Data) {
				GAME::Dungeon* LDungeon = (GAME::Dungeon*)Data;
				if (mT->mModel->Type == 20) {
					LDungeon->popGIF({ mT->mModel->mGIFDescriptorSet, nullptr, nullptr });
				}
			},
			this
			);
		wSquarePhysics->SetFixedSizeTerrain(mMoveTerrain);

		const float Positions[] = {
			-0.001f, -0.001f, 0.0f,
			16.001f, -0.001f, 0.0f,
			16.001f, 16.001f, 0.0f,
			-0.001f, 16.001f, 0.0f,
		};

		const float UVs[] = {
			0.01f,0.01f,
			0.99f,0.01f,
			0.99f,0.99f,
			0.01f,0.99f,
		};

		const unsigned int IndexDatas[] = {
			0,1,2,
			2,3,0,
		};

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(wDevice, 12 * sizeof(float), (void*)Positions);
		mUVBuffer = VulKan::Buffer::createVertexBuffer(wDevice, 8 * sizeof(float), (void*)UVs);
		mIndexBuffer = VulKan::Buffer::createIndexBuffer(wDevice, 6 * sizeof(unsigned int), (void*)IndexDatas);
		mIndexDatasSize = 6;

		double UVx = 1.0f / mNumberX, UVy = 1.0f / mNumberY;
		double UVpianX = UVx * 0.01f, UVpianY = UVy * 0.01f;

		mMistUVBuffer = new VulKan::Buffer * *[mNumberX];
		mTextureAndBuffer = new TextureAndBuffer * [mNumberX];
		for (size_t ix = 0; ix < mNumberX; ++ix)
		{
			mMistUVBuffer[ix] = new VulKan::Buffer * [mNumberY];
			mTextureAndBuffer[ix] = new TextureAndBuffer[mNumberY];
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				float MistUVs[] = {
					float(UVx * ix + UVpianX),	float(UVy * iy + UVpianY),
					float(UVx * (ix + 1) - UVpianX),	float(UVy * iy + UVpianY),
					float(UVx * (ix + 1) - UVpianX),	float(UVy * (iy + 1) - UVpianY),
					float(UVx * ix + UVpianX),	float(UVy * (iy + 1) - UVpianY),
				};
				mTextureAndBuffer[ix][iy].MistPointerX = ix;
				mTextureAndBuffer[ix][iy].MistPointerY = iy;
				mMistUVBuffer[ix][iy] = VulKan::Buffer::createVertexBuffer(wDevice, 8 * sizeof(float), (void*)MistUVs);
				mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mModel = &mTextureAndBuffer[ix][iy];
			}
		}
	}

	Dungeon::~Dungeon()
	{
		DeleteMist();

		/******* GIF *******/

		for (size_t i = 0; i < mBlockData->GetApplyNumber(); i++)
		{
			for (size_t id = 0; id < mBlockData->GetBlockDataS()[i]->mNumber; id++)
			{
				delete mBlockData->GetBlockDataS()[i]->DataS[id].mDescriptorSet;
			}
		}
		delete mBlockData;
		
		
		for (size_t i = 0; i < mGIFBlockNumber; i++)
		{
			for (size_t id = 0; id < wFrameCount; id++)
			{
				delete mGIFBlockCommandBuffer[i].mCommandBuffer[id];
			}
			delete mGIFBlockCommandBuffer[i].mCommandBuffer;
			delete mGIFBlockCommandBuffer[i].mMutex;
			delete mGIFBlockCommandBuffer[i].mGIFDescriptorPool;
			delete mGIFCommandPool[i];
		}
		delete mGIFBlockCommandBuffer;
		delete mGIFCommandPool;


		/*******************/

		delete mMoveTerrain;

		delete mPositionBuffer;
		delete mUVBuffer;
		delete mIndexBuffer;

		delete mPerlinNoise;

		for (size_t ix = 0; ix < mNumberX; ++ix)
		{
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				delete mMistDescriptorSet[ix][iy];
				delete mMistUVBuffer[ix][iy];
				delete mDescriptorSet[ix][iy];

				delete mTextureAndBuffer[ix][iy].mBufferS;
				delete mTextureAndBuffer[ix][iy].mPixelTexture;
			}
			delete mMistDescriptorSet[ix];
			delete mMistUVBuffer[ix];
			delete mDescriptorSet[ix];

			delete mDungeonDestroyStruct[ix];
			delete mTextureAndBuffer[ix];
		}
		delete mMistDescriptorSet;
		delete mMistUVBuffer;
		delete mDescriptorSet;

		delete mDungeonDestroyStruct;
		delete mTextureAndBuffer;


		for (size_t i = 0; i < wFrameCount; ++i)
		{
			delete mMistCommandBuffer[i];
			delete mCommandBuffer[i];
		}
		delete mMistCommandBuffer;
		delete mCommandBuffer;

		delete WarfareMist;
		delete mDescriptorPool;
		delete mCommandPool;
	}

	void Dungeon::GIFCommandBuffer(BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT* Pointer) {
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = wRenderPass->getRenderPass();

		for (size_t id = 0; id < wFrameCount; ++id)
		{
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(id);
			VulKan::CommandBuffer* CommandBufferL = Pointer->mHandle->mCommandBuffer[id];

			CommandBufferL->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			CommandBufferL->bindGraphicPipeline(wGIFPipeline->getPipeline());//获得渲染管线
			CommandBufferL->bindIndexBuffer(mIndexBuffer->getBuffer());//获得顶点索引
			for (size_t idd = 0; idd < Pointer->mNumber; idd++)
			{
				CommandBufferL->bindVertexBuffer({ mPositionBuffer->getBuffer(), Pointer->DataS[idd].mBufferUV->getBuffer() });//获取顶点数据，UV值
				CommandBufferL->bindDescriptorSet(wGIFPipeline->getLayout(), Pointer->DataS[idd].mDescriptorSet->getDescriptorSet(id));//获得 模型位置数据， 贴图数据，……
				CommandBufferL->drawIndex(mIndexDatasSize);//获取绘画物体的顶点个数
			}
			CommandBufferL->end();
		}
	}

	void Dungeon::UpDataGIFCommandBuffer() {
		BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT** PointerInfo = mBlockData->GetBlockDataS();
		BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT* Pointer;
		for (size_t i = 0; i < mBlockData->GetApplyNumber(); i++)
		{
			Pointer = *PointerInfo;
			++PointerInfo;
			
			if (Pointer->mNumber != 0) {
				if (Pointer->mHandle->State == GIFCommandBufferState::RequestUpdate) {
					MultithreadingGenerate.push_back(TOOL::mThreadPool->enqueue(&Dungeon::GIFCommandBuffer, this, Pointer));
				}
				Pointer->mHandle->State = GIFCommandBufferState::Enabled;
			}
			else {
				Pointer->mHandle->State = GIFCommandBufferState::NotEnabled;
			}
		}
	}

	VulKan::DescriptorSet* Dungeon::AddGIF(VulKan::Buffer* buffer, VulKan::Texture* texture, VulKan::Buffer* bufferUV) {
		std::vector<VulKan::UniformParameter*> mUniformParams;
		VulKan::UniformParameter vpParam;
		vpParam.mBinding = 0;
		vpParam.mCount = 1;
		vpParam.mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam.mSize = sizeof(VPMatrices);
		vpParam.mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam.mBuffers = wVPMstdBuffer;
		mUniformParams.push_back(&vpParam);

		VulKan::UniformParameter objectParam;
		objectParam.mBinding = 1;
		objectParam.mCount = 1;
		objectParam.mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam.mSize = sizeof(ObjectUniformGIF);
		objectParam.mStage = VK_SHADER_STAGE_VERTEX_BIT;
		objectParam.mBuffers.resize(wFrameCount);
		for (size_t i = 0; i < wFrameCount; i++)
		{
			objectParam.mBuffers[i] = buffer;
		}
		mUniformParams.push_back(&objectParam);

		VulKan::UniformParameter textureParam;
		textureParam.mBinding = 2;
		textureParam.mCount = 1;
		textureParam.mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam.mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam.mTexture = texture;
		mUniformParams.push_back(&textureParam);

		BlockData<BlockGifData, BlockCommandBuffer>::BlockDataInfo info = mBlockData->DelayRegistration();
		info.mBlockDataT->DataS[info.mIndex].mDescriptorSet = new VulKan::DescriptorSet(wDevice, mUniformParams, wDescriptorSetLayout, info.mBlockDataT->mHandle->mGIFDescriptorPool, wFrameCount, info.mBlockDataT->mHandle->mMutex);
		mBlockData->Registration(info);
		info.mBlockDataT->DataS[info.mIndex].mBuffer = buffer;
		info.mBlockDataT->DataS[info.mIndex].mBufferUV = bufferUV;
		info.mBlockDataT->mHandle->State = GIFCommandBufferState::RequestUpdate;
		return info.mBlockDataT->DataS[info.mIndex].mDescriptorSet;
	}

	void Dungeon::popGIF(BlockGifData data) {
		mBlockData->DelayPop(data);
	}

	void GIFAdd(Dungeon::BlockGifData* data, Dungeon::BlockCommandBuffer* H, void* Class) {

	}

	void GIFPop(Dungeon::BlockGifData* data, Dungeon::BlockCommandBuffer* H, void* Class) {
		delete data->mDescriptorSet;
		data->mDescriptorSet = nullptr;
		H->State = Dungeon::GIFCommandBufferState::RequestUpdate;
	}

	//初始化描述符
	void Dungeon::initUniformManager(
		int frameCount, //GPU画布的数量
		const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
		std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
		VulKan::Sampler* sampler,//图片采样器
		TextureLibrary* textureLibrary
	) {
		wFrameCount = frameCount;
		wVPMstdBuffer = VPMstdBuffer;
		wDescriptorSetLayout = mDescriptorSetLayout;
		wTextureLibrary = textureLibrary;
		mCommandPool = new VulKan::CommandPool(wDevice);
		mCommandBuffer = new VulKan::CommandBuffer * [wFrameCount];
		mMistCommandBuffer = new VulKan::CommandBuffer * [wFrameCount];
		for (size_t i = 0; i < wFrameCount; ++i)
		{
			mMistCommandBuffer[i] = new VulKan::CommandBuffer(wDevice, mCommandPool, true);
			mCommandBuffer[i] = new VulKan::CommandBuffer(wDevice, mCommandPool, true);
		}


		InitMist();

		std::vector<VulKan::UniformParameter*> mUniformParams;
		VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
		vpParam->mBinding = 0;
		vpParam->mCount = 1;
		vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam->mSize = sizeof(VPMatrices);
		vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam->mBuffers = VPMstdBuffer;
		mUniformParams.push_back(vpParam);

		VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
		objectParam->mBinding = 1;
		objectParam->mCount = 1;
		objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam->mSize = sizeof(ObjectUniformGIF);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		objectParam->mBuffers.resize(wFrameCount);
		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam->mPixelTexture = nullptr;
		mUniformParams.push_back(textureParam);

		//GIF
		mGIFBlockNumber = ((mNumberX * mNumberY) / 10) + (((mNumberX * mNumberY) % 10) != 0 ? 1 : 0);
		mGIFCommandPool = new VulKan::CommandPool * [mGIFBlockNumber];
		mGIFBlockCommandBuffer = new BlockCommandBuffer[mGIFBlockNumber];
		for (size_t i = 0; i < mGIFBlockNumber; ++i)
		{
			mGIFCommandPool[i] = new VulKan::CommandPool(wDevice);
			mGIFBlockCommandBuffer[i].mGIFDescriptorPool = new VulKan::DescriptorPool(wDevice);
			mGIFBlockCommandBuffer[i].mGIFDescriptorPool->build(mUniformParams, wFrameCount, 10);
			mGIFBlockCommandBuffer[i].mCommandBuffer = new VulKan::CommandBuffer * [wFrameCount];
			mGIFBlockCommandBuffer[i].mMutex = new std::mutex;
			for (size_t id = 0; id < wFrameCount; ++id)
			{
				mGIFBlockCommandBuffer[i].mCommandBuffer[id] = new VulKan::CommandBuffer(wDevice, mGIFCommandPool[i], true);
			}
		}

		mBlockData = new BlockData<BlockGifData, BlockCommandBuffer>(mNumberX * mNumberY, 10, mGIFBlockCommandBuffer);
		mBlockData->SetCallback(GIFAdd, GIFPop, this);

		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(wDevice);
		mDescriptorPool->build(mUniformParams, wFrameCount, mNumberX * mNumberY * 2);
		ObjectUniformGIF mUniform;
		mUniform.chuang = 1.0f / 24;
		//int(*arr2D)[5] = reinterpret_cast<int(*)[5]>(shucs);
		mDescriptorSet = new VulKan::DescriptorSet * *[mNumberX];
		mMistDescriptorSet = new VulKan::DescriptorSet * *[mNumberX];
		WarfareMist = new VulKan::PixelTexture(wDevice, mCommandPool, nullptr, mNumberX * 16, mNumberY * 16, 4, sampler);
		unsigned char* WarfareMistPointer = (unsigned char*)WarfareMist->getHOSTImagePointer();
		int* WallBoolPointer = (int*)WallBool->getupdateBufferByMap();
		mDungeonDestroyStruct = new DungeonDestroyStruct * [mNumberX];
		short* qiangshulaingData = new short[16 * 16 * mNumberX * mNumberY];
		for (int ix = 0; ix < mNumberX; ++ix)
		{
			mDungeonDestroyStruct[ix] = new DungeonDestroyStruct[mNumberY];
			mDescriptorSet[ix] = new VulKan::DescriptorSet * [mNumberY];
			mMistDescriptorSet[ix] = new VulKan::DescriptorSet * [mNumberY];
			for (int iy = 0; iy < mNumberY; ++iy)
			{
				mTextureAndBuffer[ix][iy].Type = GetNoise(ix - mMoveTerrain->Origin.x + mMoveTerrain->GetGridSPosX(), iy - mMoveTerrain->Origin.y + mMoveTerrain->GetGridSPosY());

				mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3((ix - mMoveTerrain->Origin.x + mMoveTerrain->GetGridSPosX()) * 16, (iy - mMoveTerrain->Origin.y + mMoveTerrain->GetGridSPosY()) * 16, 0));//位移矩阵
				mTextureAndBuffer[ix][iy].mBufferS = VulKan::Buffer::createUniformBuffer(wDevice, sizeof(ObjectUniformGIF));
				mTextureAndBuffer[ix][iy].mBufferS->updateBufferByMap((void*)&mUniform, sizeof(ObjectUniformGIF));
				for (size_t i = 0; i < wFrameCount; ++i)
				{
					objectParam->mBuffers[i] = mTextureAndBuffer[ix][iy].mBufferS;
				}
				
				mTextureAndBuffer[ix][iy].mPixelTexture = new VulKan::PixelTexture(wDevice, mCommandPool, pixelS[mTextureAndBuffer[ix][iy].Type], 16, 16, 4, sampler);

				bool CollisionBool = false;
				if (mTextureAndBuffer[ix][iy].Type > (TextureNumber / 2)) {
					CollisionBool = true;
				}
				if (mTextureAndBuffer[ix][iy].Type == 20) {
					GAME::TextureLibrary::TextureToUVInfo TUinfo = wTextureLibrary->GetTextureUV("004_24");
					mTextureAndBuffer[ix][iy].mGIFDescriptorSet = AddGIF(mTextureAndBuffer[ix][iy].mBufferS, TUinfo.mTexture, TUinfo.mUV);
				}
				for (size_t ixx = 0; ixx < mSquareSideLength; ++ixx)
				{
					for (size_t iyy = 0; iyy < mSquareSideLength; ++iyy)
					{
						for (size_t i = 0; i < 4; ++i)
						{
							WarfareMistPointer[((iy * 16 + ixx) * 16 * mNumberX * 4) + (ix * 16 * 4) + (iyy * 4) + i] = pixelS[mTextureAndBuffer[ix][iy].Type][(ixx * 16 * 4) + (iyy * 4) + i] * (i == 3 ? 1 : 0.3f);
						}
						WallBoolPointer[((((iy * 16) + ixx) * mNumberX * 16) + (ix * 16) + iyy)] = CollisionBool;
						mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mGridDecorator.at({ ixx,iyy })->Collision = CollisionBool;
					}
				}

				mTextureAndBuffer[ix][iy].PixelWallNumber = qiangshulaingData;
				qiangshulaingData += 16 * 16;

				mDungeonDestroyStruct[ix][iy] = { &mTextureAndBuffer[ix][iy], this };
				mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mGridDecorator.SetCollisionCallback(DungeonDestroy, &mDungeonDestroyStruct[ix][iy]);
				textureParam->mPixelTexture = mTextureAndBuffer[ix][iy].mPixelTexture;
				mDescriptorSet[ix][iy] = new VulKan::DescriptorSet(wDevice, mUniformParams, mDescriptorSetLayout, mDescriptorPool, wFrameCount);
				textureParam->mPixelTexture = WarfareMist;
				mMistDescriptorSet[ix][iy] = new VulKan::DescriptorSet(wDevice, mUniformParams, mDescriptorSetLayout, mDescriptorPool, wFrameCount);
			}
		}
		WallBool->endupdateBufferByMap();
		WarfareMist->endHOSTImagePointer();
		WarfareMist->UpDataImage();

		for (auto i : mUniformParams) {
			delete i;
		}

		for (size_t ix = 0; ix < mNumberX; ++ix)
		{
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				TOOL::mThreadPool->enqueue(&Dungeon::BlockPixelWallNumber, this, ix, iy);
				//BlockPixelWallNumber(ix, iy);
			}
		}
	}

	void Dungeon::initCommandBuffer() {
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = wRenderPass->getRenderPass();

		BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT** PointerInfo = mBlockData->GetBlockDataS();
		BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT* Pointer;
		for (size_t i = 0; i < mBlockData->GetApplyNumber(); i++)
		{
			Pointer = *PointerInfo;
			++PointerInfo;

			if (Pointer->mNumber != 0) {
				MultithreadingGenerate.push_back(TOOL::mThreadPool->enqueue(&Dungeon::GIFCommandBuffer, this, Pointer));
				Pointer->mHandle->State = GIFCommandBufferState::Enabled;
			}
			else {
				Pointer->mHandle->State = GIFCommandBufferState::NotEnabled;
			}
		}

		for (size_t i = 0; i < wFrameCount; ++i)
		{
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mCommandBuffer[i]->bindGraphicPipeline(wPipeline->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer({ mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() });//获取顶点数据，UV值
			mCommandBuffer[i]->bindIndexBuffer(mIndexBuffer->getBuffer());//获得顶点索引
			for (size_t ix = 0; ix < mNumberX; ++ix)
			{
				for (size_t iy = 0; iy < mNumberY; ++iy)
				{
					mCommandBuffer[i]->bindDescriptorSet(wPipeline->getLayout(), mDescriptorSet[ix][iy]->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
					mCommandBuffer[i]->drawIndex(mIndexDatasSize);//获取绘画物体的顶点个数
				}
			}
			mCommandBuffer[i]->end();

			mMistCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mMistCommandBuffer[i]->bindGraphicPipeline(wPipeline->getPipeline());//获得渲染管线
			mMistCommandBuffer[i]->bindIndexBuffer(mIndexBuffer->getBuffer());//获得顶点索引
			for (size_t ix = 0; ix < mNumberX; ++ix)
			{
				for (size_t iy = 0; iy < mNumberY; ++iy)
				{
					mMistCommandBuffer[i]->bindVertexBuffer({ mPositionBuffer->getBuffer(), mMistUVBuffer[ix][iy]->getBuffer() });//获取顶点数据，UV值
					mMistCommandBuffer[i]->bindDescriptorSet(wPipeline->getLayout(), mMistDescriptorSet[ix][iy]->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
					mMistCommandBuffer[i]->drawIndex(mIndexDatasSize);//获取绘画物体的顶点个数
				}
			}
			mMistCommandBuffer[i]->end();
		}

		for (auto& i : MultithreadingGenerate)//等待全部线程任务结束
		{
			i.wait();
		}
		MultithreadingGenerate.clear();//清空
	}

	void Dungeon::Destroy(int x, int y, bool Bool) {
		LSPointer[x * mSquareSideLength + y] = 0;
	}

	void Dungeon::InitMist() {
		wymiwustruct.size = 1800;
		wymiwustruct.y_size = mNumberX * 16;
		wymiwustruct.x_size = mNumberY * 16;
		unsigned char Mist[4] = { 255,0,0,255 };
		wymiwustruct.Color = *(int*)Mist;

		//创建迷雾计算结果储存的缓存
		jihsuanTP = new VulKan::Buffer(wDevice, (mNumberX * mNumberY * 16 * 16 * 4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);
		//创建迷雾原数据缓存
		information = new VulKan::Buffer(wDevice, sizeof(Dungeonmiwustruct), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);
		//创建墙壁数据缓存
		WallBool = new VulKan::Buffer(wDevice, (mNumberX * mNumberY * 16 * 16 * 4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);
		//图片区块索引
		CalculateIndex = new VulKan::Buffer(wDevice, (mNumberX * mNumberY * 2 * 4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);

		int* Index = (int*)CalculateIndex->getupdateBufferByMap();
		for (size_t ix = 0; ix < mNumberX; ++ix)
		{
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				Index[(iy * mNumberX + ix) * 2] = mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mModel->MistPointerY;
				Index[(iy * mNumberX + ix) * 2 + 1] = mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mModel->MistPointerX;
			}
		}
		CalculateIndex->endupdateBufferByMap();

		std::vector<VulKan::CalculateStruct> CalculateBufferS;
		CalculateBufferS.resize(4);
		CalculateBufferS[0] = { &jihsuanTP->getBufferInfo() };
		CalculateBufferS[1] = { &information->getBufferInfo() };
		CalculateBufferS[2] = { &WallBool->getBufferInfo() };
		CalculateBufferS[3] = { &CalculateIndex->getBufferInfo() };

		mCalculate = new VulKan::Calculate(wDevice, &CalculateBufferS, DungeonWarfareMist_spv);
	}

	void Dungeon::UpdataMist(int x, int y, float ang) {
		wymiwustruct.x = y + (mNumberY * 8) - (mMoveTerrain->GetGridSPosY() * 16);
		wymiwustruct.y = x + (mNumberX * 8) - (mMoveTerrain->GetGridSPosX() * 16);
		wymiwustruct.angel = -ang;
		information->updateBufferByMap(&wymiwustruct, sizeof(Dungeonmiwustruct));

		

		VkBufferCopy copyInfo{};
		copyInfo.size = (mNumberX * mNumberY * 16 * 16 * 4);

		mCalculate->begin();
		mCalculate->GetCommandBuffer()->copyBufferToBuffer(WarfareMist->getHOSTImageBuffer(), jihsuanTP->getBuffer(), 1, { copyInfo });//获取原数据
		vkCmdDispatch(mCalculate->GetCommandBuffer()->getCommandBuffer(), (uint32_t)ceil((wymiwustruct.size) / float(64)), 1, 1);//分配计算单元开始计算
		WarfareMist->RewritableDataType(mCalculate->GetCommandBuffer());
		mCalculate->GetCommandBuffer()->copyBufferToImage(//将计算的迷雾结果更新到图片上
			jihsuanTP->getBuffer(),
			WarfareMist->getImage()->getImage(),
			WarfareMist->getImage()->getLayout(),
			WarfareMist->getImage()->getWidth(),
			WarfareMist->getImage()->getHeight()
		);
		WarfareMist->RewriteDataTypeOptimization(mCalculate->GetCommandBuffer());
		mCalculate->end();
		mCalculate->GetCommandBuffer()->submitSync(wDevice->getGraphicQueue(), VK_NULL_HANDLE);
	}

	void Dungeon::UpdateAIPathfindingBlock(int x, int y) {
		if (x > 0) {
			x = x + 1;
			for (size_t ix = mNumberX - x; ix < mNumberX; ++ix)
			{
				for (size_t iy = 0; iy < mNumberY; ++iy)
				{
					TOOL::mThreadPool->enqueue(&Dungeon::BlockPixelWallNumber, this, ix, iy);
				}
			}
		}
		else {
			x -= x - 1;
			for (size_t ix = 0; ix < x; ++ix)
			{
				for (size_t iy = 0; iy < mNumberY; ++iy)
				{
					TOOL::mThreadPool->enqueue(&Dungeon::BlockPixelWallNumber, this, ix, iy);
				}
			}
		}
		if (y > 0) {
			y = y + 1;
			int A = (x < 0 ? -x : 0), B = (x > 0 ? mNumberX - x : 0);
			for (size_t iy = mNumberY - y; iy < mNumberY; ++iy)
			{
				for (size_t ix = A; ix < B; ++ix)
				{
					TOOL::mThreadPool->enqueue(&Dungeon::BlockPixelWallNumber, this, ix, iy);
				}
			}
		}
		else {
			y -= y - 1;
			int A = (x < 0 ? -x : 0), B = (x > 0 ? mNumberX - x : 0);
			for (size_t iy = 0; iy < y; ++iy)
			{
				for (size_t ix = A; ix < B; ++ix)
				{
					TOOL::mThreadPool->enqueue(&Dungeon::BlockPixelWallNumber, this, ix, iy);
				}
			}
		}
	}

	void Dungeon::UpdataMistData(int x, int y) {
		//std::cout << "************************************" << std::endl;
		PathfindingDecoratorDeviationX = (mMoveTerrain->Origin.x - mMoveTerrain->GetGridSPosX()) * 16;
		PathfindingDecoratorDeviationY = (mMoveTerrain->Origin.y - mMoveTerrain->GetGridSPosY()) * 16;
		for (auto& i : MultithreadingGenerate)//等待全部线程任务结束
		{
			i.wait();
		}
		MultithreadingGenerate.clear();//清空
		mBlockData->WholePop();
		UpDataGIFCommandBuffer();
		TOOL::mThreadPool->enqueue(&Dungeon::UpdateAIPathfindingBlock, this, x, y);

		int* Index = (int*)CalculateIndex->getupdateBufferByMap();
		int A;
		TextureAndBuffer* info;
		for (size_t ix = 0; ix < mNumberX; ++ix)
		{
			for (size_t iy = 0; iy < mNumberY; ++iy)
			{
				A = (iy * mNumberX + ix) * 2;
				info = mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mModel;
				Index[A] = info->MistPointerY;
				++A;
				Index[A] = info->MistPointerX;
			}
		}
		CalculateIndex->endupdateBufferByMap();

		TOOL::mTimer->MomentTiming(u8"上传图片耗时");
		//统一上传
		mCalculate->GetCommandBuffer()->begin();
		for (auto i : MultithreadingPixelTexture)//提交所有上传贴图
		{
			i->RewritableDataType(mCalculate->GetCommandBuffer());
			i->UpDataPicture(mCalculate->GetCommandBuffer());
			i->RewriteDataTypeOptimization(mCalculate->GetCommandBuffer());
		}
		mCalculate->GetCommandBuffer()->end();
		mCalculate->GetCommandBuffer()->submitSync(wDevice->getGraphicQueue(), VK_NULL_HANDLE);
		MultithreadingPixelTexture.clear();//清空
		TOOL::mTimer->MomentEnd();
		for (auto& i : MultithreadingGenerate)//等待全部线程任务结束
		{
			i.wait();
		}
		MultithreadingGenerate.clear();//清空
		//关闭HOST指针
		WarfareMist->endHOSTImagePointer();
		WallBool->endupdateBufferByMap();
		Pointerkaiguan = true;
		TOOL::mTimer->MomentEnd();
	}

	void Dungeon::DeleteMist() {
		delete mCalculate;
		delete jihsuanTP;
		delete information;
		delete WallBool;
		delete CalculateIndex;
	}

}