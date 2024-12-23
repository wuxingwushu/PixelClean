#include "FixedMaze.h"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"
#include "../../SoundEffect/SoundEffect.h"
#include "../../BlockS/PixelS.h"
#include "../../Tool/GenerateMaze.h"


namespace GAME {


	void Labyrinth_SetPixel_2(int x, int y, bool Bool, SquarePhysics::ObjectDecorator* Object, void* mclass) {
		FixedMaze* mClass = (FixedMaze*)mclass;
		//破坏的像素
		mClass->mPixelQueue->add({ x, y, Bool });//储存起来，统一上传
		//同步
		if (Global::MultiplePeopleMode) {
			if (Global::ServerOrClient) {
				RoleSynchronizationData* LServerPos = server::GetServer()->GetServerData()->GetKeyData(0);
				BufferEventSingleData* LBufferEventSingleData;
				for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); ++i)
				{
					LBufferEventSingleData = LServerPos[i].mBufferEventSingleData;
					LBufferEventSingleData->mLabyrinthPixel->add({ x,y,false });
				}
			}
			else {
				client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mLabyrinthPixel->add({ x,y,false });
			}
		}
	}

	FixedMaze::FixedMaze(SquarePhysics::SquarePhysics* squarePhysics):
		wSquarePhysics(squarePhysics)
	{

	}

	void FixedMaze::AgainGenerateLabyrinth(int X, int Y) {
		this->~FixedMaze();
		InitLabyrinth(mDevice, X, Y);
		initUniformManager(
			mDevice,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mVPMstdBuffer,
			mSampler
		);
		RecordingCommandBuffer(mRenderPass, mSwapChain, mPipeline);
	}

	void FixedMaze::InitLabyrinth(VulKan::Device* device, int X, int Y)
	{
		mDevice = device;

		GenerateMaze LGenerateMaze;
		GenerateMaze::Pos Mpos = LGenerateMaze.GetMazeSize(X, Y, 4, 60);

		numberX = Mpos.X;
		numberY = Mpos.Y;

		BlockPixelS = new int [numberX * numberY] {false};
		
		LGenerateMaze.SetGenerateMazeCallback(
			[](int x, int y, bool B, void* Lclass) {
				FixedMaze* LLabyrinth = (FixedMaze*)Lclass;
				LLabyrinth->BlockPixelS[x * LLabyrinth->numberY + y] = B;
			},
			this
		);
		LGenerateMaze.RunGenerateMaze();
		
		mPixelQueue = new Queue<PixelState>(1000);

		mGridNavigation = new GridNavigation<short>(numberX, numberY, 10);
		mGridNavigation->SetRoadCallback([](int x, int y, void* P)->bool{
			FixedMaze* F = (FixedMaze*)P;
			if (x < 0) return true;
			if (y < 0) return true;
			if (x >= F->numberX) return true;
			if (y >= F->numberY) return true;
			return F->BlockPixelS[(x * F->numberY) + y] == 1;
		}, this);
		mGridNavigation->ScanGridNavigation();


		LabyrinthBuffer();
	}

	void FixedMaze::LabyrinthBuffer() {
		mFixedSizeTerrain = new SquarePhysics::FixedSizeTerrain(numberX, numberY, 1);
		mFixedSizeTerrain->SetCollisionCallback(Labyrinth_SetPixel_2, this);
		wSquarePhysics->SetFixedSizeTerrain(mFixedSizeTerrain);//添加地图碰撞

		int Ax = -(numberX / 2);
		int Ay = -(numberY / 2);
		int Bx = numberX + Ax;
		int By = numberY + Ay;
		mOriginX = (unsigned int)(-Ax);
		mOriginY = (unsigned int)(-Ay);
		mFixedSizeTerrain->SetOrigin(mOriginX, mOriginY);




		float mPositions[12] = {
			(float)Ax, (float)Ay, 0.0f,
			(float)Bx, (float)Ay, 0.0f,
			(float)Bx, (float)By, 0.0f,
			(float)Ax, (float)By, 0.0f,
		};

		float mUVs[8] = {
			1.0f,0.0f,
			0.0f,0.0f,
			0.0f,1.0f,
			1.0f,1.0f,
		};

		unsigned int mIndexDatas[6] = {
			0,1,2,
			2,3,0,
		};

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(mDevice, 12 * sizeof(float), mPositions);

		mUVBuffer = VulKan::Buffer::createVertexBuffer(mDevice, 8 * sizeof(float), mUVs);

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(mDevice, 6 * sizeof(float), mIndexDatas);

		mIndexDatasSize = 6;
	}

	void MixColors_2(unsigned char* color, const unsigned char* Hcolor) {
		color[0] = (unsigned char)(Hcolor[0] * 0.3f);
		color[1] = (unsigned char)(Hcolor[1] * 0.3f);
		color[2] = (unsigned char)(Hcolor[2] * 0.3f);
		color[3] = 255;
	}

	FixedMaze::~FixedMaze()
	{
		delete[] BlockPixelS;
		delete mGridNavigation;

		//销毁地面碰撞系统
		delete mFixedSizeTerrain;

		//销毁 网格 UV 点索引
		delete mPositionBuffer;
		delete mUVBuffer;
		delete mIndexBuffer;

		//销毁贴图
		delete PixelTextureS;

		//销毁描述符等
		delete mDescriptorSet;
		delete mDescriptorPool;

		for (size_t i = 0; i < mUniformParams->size(); ++i)
		{
			if (i == 1) {
				for (size_t ib = 0; ib < (*mUniformParams)[i]->mBuffers.size(); ++ib)
				{
					delete (*mUniformParams)[i]->mBuffers[ib];
				}
			}
			delete (*mUniformParams)[i];
		}
		mUniformParams->clear();
		delete mUniformParams;

		//销毁指令缓存 指令池
		for (int i = 0; i < mFrameCount; ++i) {
			delete mThreadCommandBufferS[i];
			delete mThreadCommandPoolS[i];
		}
		delete[] mThreadCommandBufferS;
		delete[] mThreadCommandPoolS;
	}


	void FixedMaze::initUniformManager(
		VulKan::Device* device,
		int frameCount,
		const VkDescriptorSetLayout DescriptorSetLayout,
		std::vector<VulKan::Buffer*>* VPMstdBuffer,
		VulKan::Sampler* sampler
	)
	{
		mSampler = sampler;
		mFrameCount = frameCount;
		mVPMstdBuffer = VPMstdBuffer;
		mDescriptorSetLayout = DescriptorSetLayout;

		mThreadCommandPoolS = new VulKan::CommandPool * [mFrameCount];
		mThreadCommandBufferS = new VulKan::CommandBuffer * [mFrameCount];
		for (int i = 0; i < (mFrameCount); ++i) {
			mThreadCommandPoolS[i] = new VulKan::CommandPool(device);
			mThreadCommandBufferS[i] = new VulKan::CommandBuffer(device, mThreadCommandPoolS[i], true);
		}

		unsigned char* mPixelS = new unsigned char[numberX * numberY * 4];

		//SquarePhysics::PixelAttribute** LPixelAttribute = mFixedSizeTerrain->GetPixelAttribute();
		unsigned char CLR1[4] = { 255,255,255,255 };
		unsigned char CLR2[4] = { 50,50,50,255 };
		for (size_t x = 0; x < numberX; ++x)
		{
			for (size_t y = 0; y < numberY; ++y)
			{
				if (BlockPixelS[x * numberY + y] == 1) {
					memcpy(&mPixelS[(x * numberY + y) * 4], CLR1, 4);
					mFixedSizeTerrain->at({ x, y })->Collision = true;
				}
				else {
					memcpy(&mPixelS[(x * numberY + y) * 4], CLR2, 4);
					mFixedSizeTerrain->at({ x, y })->Collision = false;
				}
			}
		}

		PixelTextureS = new VulKan::PixelTexture(device, mThreadCommandPoolS[0], mPixelS, numberX, numberY, 4, sampler);

		delete[] mPixelS;

		ObjectUniform mUniform;
		mUniformParams = new std::vector<VulKan::UniformParameter*>;
		
		VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
		vpParam->mBinding = 0;
		vpParam->mCount = 1;
		vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam->mSize = sizeof(VPMatrices);
		vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam->mBuffers = *mVPMstdBuffer;

		mUniformParams->push_back(vpParam);

		VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
		objectParam->mBinding = 1;
		objectParam->mCount = 1;
		objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam->mSize = sizeof(ObjectUniform);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;

		for (int i = 0; i < frameCount; ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
			objectParam->mBuffers.push_back(buffer);
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(-90.0f), glm::vec3(0.0, 0.0, 1.0));
			buffer->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
		}

		mUniformParams->push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam->mPixelTexture = PixelTextureS;

		mUniformParams->push_back(textureParam);

		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(*mUniformParams, frameCount, 2);
		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(device, *mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);
	}

	void FixedMaze::ThreadUpdateCommandBuffer() {
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer

		std::vector<std::future<void>> pool;
		int UpdateNumber = (numberX * numberY) / (mFrameCount / 3);
		int UpdateNumber_yu = (numberX * numberY) % (mFrameCount / 3);
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < UpdateNumber_yu; ++j) {
				pool.push_back(TOOL::mThreadPool->enqueue(&FixedMaze::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1)));
			}
			for (int j = UpdateNumber_yu; j < (mFrameCount / 3); ++j) {
				pool.push_back(TOOL::mThreadPool->enqueue(&FixedMaze::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber));
			}
		}
		for (int i = 0; i < (mFrameCount); ++i) {
			pool[i].wait();
		}
	}

	void FixedMaze::ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count)
	{
		unsigned int mFrameBufferCount = ((mFrameCount / 3) * FrameCount) + BufferCount;
		VulKan::CommandBuffer* commandbuffer = mThreadCommandBufferS[mFrameBufferCount];

		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass->getRenderPass();
		InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(FrameCount);



		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
		commandbuffer->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线
		commandbuffer->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
		commandbuffer->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
		commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
		commandbuffer->drawIndex(getIndexCount());//获取绘画物体的顶点个数
		commandbuffer->end();
	}

	bool FixedMaze::RangeLegitimate(int x, int y) {
		bool Bool = true;
		for (int ix = -1; ix <= 1; ++ix)
		{
			for (int iy = -1; iy <= 1; ++iy)
			{
				if (!(GetPixelWallNumber(x + ix, y + iy) <= 0)) {
					Bool = false;
				}
			}
		}
		return Bool;
	}

	glm::ivec2 FixedMaze::GetLegitimateGeneratePos() {
		glm::ivec2 rxy;
		do{
			rxy.x = rand() % numberX - mOriginX;
			rxy.y = rand() % numberY - mOriginY;
		} while (!RangeLegitimate(rxy.x, rxy.y));
		return { rxy.x, rxy.y };
	}

	void FixedMaze::UpDateMaps() {
		if (mPixelQueue->GetNumber() != 0) {
			//音效
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Impact", MP3, false, Global::SoundEffectsVolume, 0.0f);

			PixelState* LPixel;
			GetPixelSPointer();
			while (mPixelQueue->GetNumber() != 0)
			{
				LPixel = mPixelQueue->pop();
				if (LPixel != nullptr) {
					if ((LPixel->X >= 0) && (LPixel->Y >= 0) && (LPixel->X < (numberX * 16)) && (LPixel->Y < (numberY * 16))) {
						SetPixelS(LPixel->X, LPixel->Y, LPixel->State);
					}
				}
			}
			EndPixelSPointer();
			PixelTextureS->UpDataImage();
		}
	}

	void FixedMaze::GetPixelSPointer() {
		TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
	}
	void FixedMaze::SetPixelS(unsigned int x, unsigned int y, bool Switch) {
		if ((BlockPixelS[(x * numberY) + y] == 1) == Switch) {
			return;
		}
		if (Switch) {
			BlockPixelS[(x * numberY) + y] = 1;
			memcpy(&TexturePointer[(x * numberY * 4) + (y * 4)], &pixelS[2][(((x % 16)) + (y % 16)) * 4], 4);
			mGridNavigation->add(x,y);
		}
		else {
			BlockPixelS[(x * numberY) + y] = 0;
			memcpy(&TexturePointer[(x * numberY * 4) + (y * 4)], &pixelS[0][(((x % 16)) + (y % 16)) * 4], 4);
			mGridNavigation->pop(x, y);
		}
		
	}
	void FixedMaze::EndPixelSPointer() {
		PixelTextureS->endHOSTImagePointer();
	}
}