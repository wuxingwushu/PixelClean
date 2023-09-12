#include "Labyrinth.h"
#include "../NetworkTCP/Server.h"
#include "../NetworkTCP/Client.h"
#include "../SoundEffect/SoundEffect.h"
#include "../BlockS/PixelS.h"
#include "../Tool/GenerateMaze.h"


namespace GAME {


	void Labyrinth_SetPixel(int x, int y, bool Bool, void* mclass) {
		Labyrinth* mClass = (Labyrinth*)mclass;
		//破坏的像素
		mClass->mPixelQueue->add({ x, y, Bool });//储存起来，统一上传
		//同步
		if (Global::MultiplePeopleMode) {
			if (Global::ServerOrClient) {
				RoleSynchronizationData* LServerPos = server::GetServer()->GetServerData()->GetKeyData(0);
				BufferEventSingleData* LBufferEventSingleData;
				for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); i++)
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

	Labyrinth::Labyrinth(SquarePhysics::SquarePhysics* squarePhysics):
		wSquarePhysics(squarePhysics)
	{

	}

	void Labyrinth::AgainGenerateLabyrinth(int X, int Y) {
		this->~Labyrinth();
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

	void Labyrinth::LoadLabyrinth(int X, int Y, int* PixelData, unsigned int* BlockTypeData) {
		std::cout << "加载地图" << std::endl;
		this->~Labyrinth();
		numberX = X;
		numberY = Y;

		BlockPixelS = new int [numberX * 16 * numberY * 16] {false};
		BlockTypeS = new unsigned int* [numberX];
		for (size_t i = 0; i < numberX; i++)
		{
			BlockTypeS[i] = new unsigned int[numberY];
		}

		memcpy(BlockPixelS, PixelData, (numberX * numberY * 16 * 16 * sizeof(int)));
		char* PBlockTypeData = (char*)BlockTypeData;
		for (size_t i = 0; i < numberX; i++)
		{
			memcpy(BlockTypeS[i], PBlockTypeData, (numberY * sizeof(unsigned int)));
			PBlockTypeData = PBlockTypeData + (numberY * sizeof(unsigned int));
		}

		PixelWallNumber = new short* [numberX * 16];

		for (size_t ix = 0; ix < (numberX * 16); ix++)
		{
			PixelWallNumber[ix] = new short[numberY * 16];
			for (size_t iy = 0; iy < (numberY * 16); iy++)
			{
				PixelWallNumber[ix][iy] = 0;
			}
		}

		LabyrinthBuffer();
		initUniformManager(
			mDevice,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mVPMstdBuffer,
			mSampler
		);
		RecordingCommandBuffer(mRenderPass, mSwapChain, mPipeline);
	}


	void Labyrinth::InitLabyrinth(VulKan::Device* device, int X, int Y)
	{
		mDevice = device;

		/*bool** lblockS = nullptr;
		lblockS = new bool* [X];
		for (size_t i = 0; i < X; i++)
		{
			lblockS[i] = new bool[Y];
		}
		GenerateMaze LGenerateMaze(lblockS, X, Y);*/

		//numberX = ((X / 2) * 4) + 1;
		//numberY = ((Y / 2) * 4) + 1;

		numberX = ((X / 2) * 4) + 1;
		numberY = ((Y / 2) * 4) + 1;

		BlockS = new bool* [numberX];
		BlockPixelS = new int [numberX * 16 * numberY * 16] {false};
		BlockTypeS = new unsigned int* [numberX];
		for (size_t i = 0; i < numberX; i++)
		{
			BlockS[i] = new bool[numberY];
			BlockTypeS[i] = new unsigned int[numberY];
		}

		GenerateMaze LGenerateMaze;
		LGenerateMaze.SetGenerateMazeCallback(
			[](int x, int y, bool B, void* Lclass) {
				Labyrinth* LLabyrinth = (Labyrinth*)Lclass;
				LLabyrinth->BlockS[x][y] = B;
				for (size_t ix = 0; ix < 16; ix++)
				{
					for (size_t iy = 0; iy < 16; iy++)
					{
						LLabyrinth->BlockPixelS[((x * 16 + ix) * (LLabyrinth->numberY * 16)) + (y * 16) + iy] = LLabyrinth->BlockS[x][y];
					}
				}
				PerlinNoise PNoise;
				LLabyrinth->BlockTypeS[x][y] = PNoise.noise(x * 0.01f, y * 0.01f, 0.5f) * TextureNumber;
			},
			this
		);
		LGenerateMaze.GetGenerateMaze(nullptr, X, Y, 4);
		

		mPixelQueue = new Queue<PixelState>(1000);


		

		

		


		PixelWallNumber = new short* [numberX * 16];
		
		for (size_t ix = 0; ix < (numberX * 16); ix++)
		{
			PixelWallNumber[ix] = new short[numberY * 16];
			for (size_t iy = 0; iy < (numberY * 16); iy++)
			{
				PixelWallNumber[ix][iy] = 0;
			}
		}
		for (size_t ix = 0; ix < (numberX * 16); ix++)
		{
			for (size_t iy = 0; iy < (numberY * 16); iy++)
			{
				if (BlockPixelS[(ix * numberY * 16) + iy] == 1) {
					PixelWallNumberAdd(ix, iy);
				}
			}
		}


		LabyrinthBuffer();
	}

	void Labyrinth::LabyrinthBuffer() {
		mFixedSizeTerrain = new SquarePhysics::FixedSizeTerrain(numberX * 16, numberY * 16, 1);
		mFixedSizeTerrain->SetCollisionCallback(Labyrinth_SetPixel, this);
		wSquarePhysics->SetFixedSizeTerrain(mFixedSizeTerrain);//添加地图碰撞

		int Ax = -((numberX / 2) * 16);
		int Ay = -((numberY / 2) * 16);
		int Bx = (numberX * 16) + Ax;
		int By = (numberY * 16) + Ay;
		mOriginX = unsigned int(-Ax + 8);
		mOriginY = unsigned int(-Ay + 8);
		mFixedSizeTerrain->SetOrigin(mOriginX, mOriginY);




		float mPositions[12] = {
			Ax, Ay, 0.0f,
			Bx, Ay, 0.0f,
			Bx, By, 0.0f,
			Ax, By, 0.0f,
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

		InitMist();


		unsigned char Mist[4] = { 0,0,0,230 };
		//std::cout << *((int*)Mist) << std::endl;
		wymiwustruct.size = 1800;
		wymiwustruct.y_size = numberY * 16;
		Mist[3] = 255;
		Mist[0] = 255;
		wymiwustruct.Color = *((int*)Mist);
		//wymiwustruct.Color = -460365120;
	}

	void MixColors(unsigned char* color, const unsigned char* Hcolor) {
		color[0] = unsigned char(Hcolor[0] * 0.3f);
		color[1] = unsigned char(Hcolor[1] * 0.3f);
		color[2] = unsigned char(Hcolor[2] * 0.3f);
		color[3] = 255;
	}

	Labyrinth::~Labyrinth()
	{
		//销毁是否是墙壁
		for (size_t i = 0; i < numberX; i++)
		{
			delete[] BlockS[i];
			delete[] BlockTypeS[i];
		}	
		delete[] BlockS;
		delete[] BlockPixelS;
		delete[] BlockTypeS;

		delete[] PixelWallNumber;

		//销毁地面碰撞系统
		delete mFixedSizeTerrain;

		//销毁 网格 UV 点索引
		delete mPositionBuffer;
		delete mUVBuffer;
		delete mIndexBuffer;

		//销毁贴图
		delete PixelTextureS;
		delete WarfareMist;

		//销毁描述符等
		delete mDescriptorSet;
		delete mMistDescriptorSet;
		delete mDescriptorPool;

		for (size_t i = 0; i < mUniformParams->size(); i++)
		{
			if (i == 1) {
				for (size_t ib = 0; ib < (*mUniformParams)[i]->mBuffers.size(); ib++)
				{
					delete (*mUniformParams)[i]->mBuffers[ib];
				}
			}
			delete (*mUniformParams)[i];
		}
		mUniformParams->clear();
		delete mUniformParams;

		//销毁迷雾
		DeleteMist();

		//销毁指令缓存 指令池
		for (int i = 0; i < mFrameCount; i++) {
			delete mThreadCommandBufferS[i];
			delete mMistCommandBufferS[i];
			delete mThreadCommandPoolS[i];
		}
		delete[] mThreadCommandBufferS;
		delete[] mMistCommandBufferS;
		delete[] mThreadCommandPoolS;
	}


	void Labyrinth::initUniformManager(
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
		mMistCommandBufferS = new VulKan::CommandBuffer * [mFrameCount];
		for (int i = 0; i < (mFrameCount); i++) {
			mThreadCommandPoolS[i] = new VulKan::CommandPool(device);
			mThreadCommandBufferS[i] = new VulKan::CommandBuffer(device, mThreadCommandPoolS[i], true);
			mMistCommandBufferS[i] = new VulKan::CommandBuffer(device, mThreadCommandPoolS[i], true);
		}

		unsigned char* mPixelS = new unsigned char[numberX * 16 * numberY * 16 * 4];
		unsigned char* mMistS = new unsigned char[numberX * 16 * numberY * 16 * 4];

		SquarePhysics::PixelAttribute** LPixelAttribute = mFixedSizeTerrain->GetPixelAttribute();

		for (size_t x = 0; x < numberX; x++)
		{
			for (size_t y = 0; y < numberY; y++)
			{
				for (size_t pX = 0; pX < 16; pX++)
				{
					for (size_t pY = 0; pY < 16; pY++)
					{
						if (BlockPixelS[(((x * 16) + pX) * (numberY * 16)) + ((y * 16) + pY)] == 1) {
							memcpy(&mPixelS[(((x * 16) + pX) * numberY * 16 * 4) + (((y * 16) + pY) * 4)], &pixelS[2][((pX * 16 * 4) + (pY * 4))], 4);
							MixColors(&mMistS[(((x * 16) + pX) * numberY * 16 * 4) + (((y * 16) + pY) * 4)], &pixelS[2][((pX * 16 * 4) + (pY * 4))]);
							//memcpy(&mMistS[(((x * 16) + pX) * numberY * 16 * 4) + (((y * 16) + pY) * 4)], &pixelS[2][((pX * 16 * 4) + (pY * 4))], 4);
							LPixelAttribute[x * 16 + pX][y * 16 + pY].Collision = true;
						}else{
							/*if (BlockTypeS[x][y] >= TextureNumber) {
								std::cout << "Error: BlockTypeS > " << TextureNumber << "。  BlockTypeS = " << BlockTypeS[x][y] << std::endl;
							}*/
							memcpy(&mPixelS[(((x * 16) + pX) * numberY * 16 * 4) + (((y * 16) + pY) * 4)], &pixelS[BlockTypeS[x][y]][((pX * 16 * 4) + (pY * 4))], 4);
							MixColors(&mMistS[(((x * 16) + pX) * numberY * 16 * 4) + (((y * 16) + pY) * 4)], &pixelS[BlockTypeS[x][y]][((pX * 16 * 4) + (pY * 4))]);
							//memcpy(&mMistS[(((x * 16) + pX) * numberY * 16 * 4) + (((y * 16) + pY) * 4)], &pixelS[BlockTypeS[x][y]][((pX * 16 * 4) + (pY * 4))], 4);
							LPixelAttribute[x * 16 + pX][y * 16 + pY].Collision = false;
						}
					}
				}
			}
		}

		PixelTextureS = new PixelTexture(device, mThreadCommandPoolS[0], mPixelS, numberX * 16, numberY * 16, 4, sampler);
		
		WarfareMist = new PixelTexture(device, mThreadCommandPoolS[0], mMistS, numberX * 16, numberY * 16, 4, sampler);

		delete[] mPixelS;
		delete[] mMistS;

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
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-8, 8, 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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
		(*mUniformParams)[2]->mPixelTexture = WarfareMist;
		mMistDescriptorSet = new VulKan::DescriptorSet(device, *mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);
	}

	void Labyrinth::ThreadUpdateCommandBuffer() {
		Global::MainCommandBufferUpdateRequest();//请求更新 MainCommandBuffer

		std::vector<std::future<void>> pool;
		int UpdateNumber = (numberX * numberY) / (mFrameCount / 3);
		int UpdateNumber_yu = (numberX * numberY) % (mFrameCount / 3);
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < UpdateNumber_yu; j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&Labyrinth::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1)));
			}
			for (int j = UpdateNumber_yu; j < (mFrameCount / 3); j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&Labyrinth::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber));
			}
		}
		for (int i = 0; i < (mFrameCount); i++) {
			pool[i].wait();
		}
	}

	void Labyrinth::ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count)
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

		commandbuffer = mMistCommandBufferS[mFrameBufferCount];

		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
		commandbuffer->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线
		commandbuffer->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
		commandbuffer->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
		commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mMistDescriptorSet->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
		commandbuffer->drawIndex(getIndexCount());//获取绘画物体的顶点个数
		commandbuffer->end();
	}

	bool Labyrinth::RangeLegitimate(int x, int y) {
		bool Bool = true;
		for (int ix = -1; ix <= 1; ix++)
		{
			for (int iy = -1; iy <= 1; iy++)
			{
				if (!(GetPixelWallNumber(x + ix, y + iy) <= 0)) {
					Bool = false;
				}
			}
		}
		return Bool;
	}

	glm::ivec2 Labyrinth::GetLegitimateGeneratePos() {
		glm::ivec2 rxy;
		do{
			rxy.x = rand() % (numberX * 16) - mOriginX;
			rxy.y = rand() % (numberY * 16) - mOriginY;
		} while (!RangeLegitimate(rxy.x, rxy.y));
		return { rxy.x, rxy.y };
	}

	bool Labyrinth::GetPixel(int x, int y) {
		if (x >= (numberX * 16) || x < 0) { return false; }
		if (y >= (numberY * 16) || y < 0) { return false; }
		return BlockPixelS[(x * numberY * 16) + y] == 0 ? 0 : 1;
	}

	bool Labyrinth::GetPixelLegitimate(int x, int y) {
		if (x >= (numberX * 16) || x < 0) { return false; }
		if (y >= (numberY * 16) || y < 0) { return false; }
		return true;
	}

	short Labyrinth::GetPixelWallNumber(int x, int y) {
		x += mOriginX;
		y += mOriginY;
		if (GetPixelLegitimate(x,y)) {
			return PixelWallNumber[x][y];
		}
		else {
			return 255;
		}
	}

	void Labyrinth::PixelWallNumberCalculate(int x, int y) {
		for (int ix = -9; ix < 10; ix++)
		{
			for (int iy = -9; iy < 10; iy++)
			{
				if (GetPixel(x+ix,y+iy)) {
					PixelWallNumber[x][y]++;
				}
			}
		}
	}

	void Labyrinth::PixelWallNumberAdd(int x, int y) {
		for (int ix = -9; ix < 10; ix++)
		{
			for (int iy = -9; iy < 10; iy++)
			{
				if (GetPixelLegitimate(x + ix, y + iy)) {
					PixelWallNumber[x + ix][y + iy]++;
				}
			}
		}
	}

	void Labyrinth::PixelWallNumberReduce(int x, int y) {
		for (int ix = -9; ix < 10; ix++)
		{
			for (int iy = -9; iy < 10; iy++)
			{
				if (GetPixelLegitimate(x + ix, y + iy)) {
					PixelWallNumber[x + ix][y + iy]--;
				}
			}
		}
	}

	void Labyrinth::UpDateMaps() {
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

	void Labyrinth::GetPixelSPointer() {
		TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		Mistwall = (int*)WallBool->getupdateBufferByMap();
	}
	void Labyrinth::SetPixelS(unsigned int x, unsigned int y, bool Switch) {
		if ((BlockPixelS[(x * numberY * 16) + y] == 1) == Switch) {
			return;
		}
		if (Switch) {
			BlockPixelS[(x * numberY * 16) + y] = 1;
			memcpy(&TexturePointer[(x * numberY * 16 * 4) + (y * 4)], &pixelS[2][(((x % 16) * 16) + (y % 16)) * 4], 4);
			MixColors(&TextureMist[(x * numberY * 16 * 4) + (y * 4)], &pixelS[2][(((x % 16) * 16) + (y % 16)) * 4]);
			Mistwall[(x * numberY * 16) + y] = 1;
			PixelWallNumberAdd(x,y);
		}
		else {
			BlockPixelS[(x * numberY * 16) + y] = 0;
			memcpy(&TexturePointer[(x * numberY * 16 * 4) + (y * 4)], &pixelS[BlockTypeS[x / 16][y / 16]][(((x % 16) * 16) + (y % 16)) * 4], 4);
			MixColors(&TextureMist[(x * numberY * 16 * 4) + (y * 4)], &pixelS[BlockTypeS[x / 16][y / 16]][(((x % 16) * 16) + (y % 16)) * 4]);
			Mistwall[(x * numberY * 16) + y] = 0;
			PixelWallNumberReduce(x, y);
		}
		
	}
	void Labyrinth::EndPixelSPointer() {
		WallBool->endupdateBufferByMap();
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
	}
	


	void Labyrinth::UpdataMist(int wjx, int wjy, float ang) {
		wymiwustruct.x = wjx + (numberX * 8);
		wymiwustruct.y = wjy + (numberY * 8);
		wymiwustruct.angel = ang;
		information->updateBufferByMap(&wymiwustruct, sizeof(miwustruct));

		VkImageSubresourceRange region{};
		region.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		region.baseArrayLayer = 0;
		region.layerCount = 1;

		region.baseMipLevel = 0;
		region.levelCount = 1;

		WarfareMist->getImage()->setImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			region,
			mThreadCommandPoolS[0]
		);

		VkBufferCopy copyInfo{};
		copyInfo.size = (numberX * numberY * 16 * 16 * 4);

		mCalculate->begin();
		mCalculate->GetCommandBuffer()->copyBufferToBuffer(WarfareMist->getHOSTImageBuffer(), jihsuanTP->getBuffer(), 1, { copyInfo });//获取原数据
		vkCmdDispatch(mCalculate->GetCommandBuffer()->getCommandBuffer(), (uint32_t)ceil((wymiwustruct.size) / float(64)), 1, 1);//分配计算单元开始计算
		mCalculate->GetCommandBuffer()->copyBufferToImage(//将计算的迷雾结果更新到图片上
			jihsuanTP->getBuffer(),
			WarfareMist->getImage()->getImage(),
			WarfareMist->getImage()->getLayout(),
			WarfareMist->getImage()->getWidth(),
			WarfareMist->getImage()->getHeight()
		);
		mCalculate->end();
		mCalculate->GetCommandBuffer()->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

		WarfareMist->getImage()->setImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			region,
			mThreadCommandPoolS[0]
		);
	}


	void Labyrinth::InitMist() {
		//创建迷雾计算结果储存的缓存
		jihsuanTP = new VulKan::Buffer(mDevice, (numberX * numberY * 16 * 16 * 4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);
		//创建迷雾原数据缓存
		information = new VulKan::Buffer(mDevice, sizeof(miwustruct), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);
		//创建墙壁数据缓存
		WallBool = new VulKan::Buffer(mDevice, (numberX * numberY * 16 * 16 * 4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);

		WallBool->updateBufferByMap(BlockPixelS, (numberX * numberY * 16 * 16 * 4));

		CalculateBufferS.clear();
		CalculateBufferS.resize(3);
		CalculateBufferS[0] = { &jihsuanTP->getBufferInfo() };
		CalculateBufferS[1] = { &information->getBufferInfo() };
		CalculateBufferS[2] = { &WallBool->getBufferInfo() };


		mCalculate = new VulKan::Calculate(mDevice, &CalculateBufferS, WarfareMist_spv);
	}

	
	void Labyrinth::DeleteMist() {
		delete mCalculate;
		delete jihsuanTP;
		delete information;
		delete WallBool;
	}
}