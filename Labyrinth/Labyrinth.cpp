#include "Labyrinth.h"
#include "../NetworkTCP/Server.h"
#include "../NetworkTCP/Client.h"
#include "../SoundEffect/SoundEffect.h"
#include "../BlockS/PixelS.h"

namespace GAME {


	void Labyrinth_SetPixel(int x, int y, void* mclass) {
		Labyrinth* mClass = (Labyrinth*)mclass;
		//音效
		SoundEffect::SoundEffect::GetSoundEffect()->Play("Impact", MP3, 0, 1.0f, 0.0f);
		//破坏像素
		mClass->SetPixel(x, y);
		//同步
		if (Global::MultiplePeopleMode) {
			if (Global::ServerOrClient) {
				PlayerPos* LServerPos = server::GetServer()->GetServerData()->GetKeyData(0);
				BufferEventSingleData* LBufferEventSingleData;
				for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); i++)
				{
					LBufferEventSingleData = LServerPos[i].mBufferEventSingleData;
					LBufferEventSingleData->mLabyrinthPixel->add({ x,y,false });
				}
			}
			else {
				client::GetClient()->GetBufferEventSingleData()->mLabyrinthPixel->add({ x,y,false });
			}
		}
	}

	Labyrinth::Labyrinth() {

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


	void Labyrinth::InitLabyrinth(VulKan::Device* device, int X, int Y, bool** LlblockS)
	{
		mDevice = device;

		numberX = ((X / 2) * 4) + 1;
		numberY = ((Y / 2) * 4) + 1;

		PerlinNoise* P = new PerlinNoise();


		BlockS = new bool* [numberX];
		BlockPixelS = new int [numberX * 16 * numberY * 16]{false};
		BlockTypeS = new unsigned int* [numberX];
		for (size_t i = 0; i < numberX; i++)
		{
			BlockS[i] = new bool[numberY];
			BlockTypeS[i] = new unsigned int[numberX];
		}

		bool** lblockS = nullptr;
		if (LlblockS == nullptr) {
			lblockS = new bool* [X];
			for (size_t i = 0; i < X; i++)
			{
				lblockS[i] = new bool[Y];
			}
			GenerateMaze(lblockS, X, Y);
		}
		else {
			lblockS = LlblockS;
		}
		int NX, NY;
		for (size_t x = 0; x < numberX; x++)
		{
			for (size_t y = 0; y < numberY; y++)
			{
				NX = (x / 4) * 2;
				if ((x % 4) != 0) {
					NX++;
				}
				NY = (y / 4) * 2;
				if ((y % 4) != 0) {
					NY++;
				}
				BlockS[x][y] = lblockS[NX][NY];
				for (size_t ix = 0; ix < 16; ix++)
				{
					for (size_t iy = 0; iy < 16; iy++)
					{
						BlockPixelS[((x * 16 + ix) * (numberY * 16)) + (y * 16) + iy] = BlockS[x][y];
					}
				}
				BlockTypeS[x][y] = P->noise(x * 0.01f, y * 0.01f, 0.5f) * TextureNumber;
			}
		}

		delete P;
		

		for (size_t i = 0; i < X; i++)
		{
			delete[] lblockS[i];
		}
		delete[] lblockS;

		LabyrinthBuffer();
	}

	void Labyrinth::LabyrinthBuffer() {
		mFixedSizeTerrain = new SquarePhysics::FixedSizeTerrain(numberX * 16, numberY * 16, 1);
		mFixedSizeTerrain->SetCollisionCallback(Labyrinth_SetPixel, this);


		int Ax = -((numberX / 2) * 16);
		int Ay = -((numberY / 2) * 16);
		int Bx = (numberX * 16) + Ax;
		int By = (numberY * 16) + Ay;

		mFixedSizeTerrain->SetOrigin(unsigned int(-Ax + 8), unsigned int(-Ay + 8));




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

		SquarePhysics::PixelAttribute** LPixelAttribute = mFixedSizeTerrain->GetPixelAttributeData();

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
	//左下坐标，图片ID
	/*void Labyrinth::penzhang(unsigned int x, unsigned int y) {
		BlockS[x][y] = 0;
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		for (size_t i = 0; i < 16; i++)
		{
			memcpy(&TexturePointer[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4)], &pixelS[BlockTypeS[x][y]][(16 * 4 * i)], 16 * 4);
			for (size_t idd = 0; idd < 16; idd++)
			{
				TextureMist[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4) + (idd * 4) + 3] = 230;
			}
		}
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
	}*/

	//左下坐标，左下坐标，图片ID
	void Labyrinth::SetPixel(unsigned int x, unsigned int y, unsigned int Dx, unsigned int Dy) {
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		int* Mistwall = (int*)WallBool->getupdateBufferByMap();
		memcpy(&TexturePointer[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4)], &pixelS[BlockTypeS[x][y]][(16 * 4 * Dx) + (Dy * 4)], 4);
		//TextureMist[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4) + 3] = 230;
		MixColors(&TextureMist[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4)], &pixelS[BlockTypeS[x][y]][(16 * 4 * Dx) + (Dy * 4)]);
		Mistwall[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4)] = 0;
		WallBool->endupdateBufferByMap();
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
	}

	void Labyrinth::SetPixel(unsigned int x, unsigned int y) {
		BlockPixelS[(x * numberY * 16) + y] = 0;
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		int* Mistwall = (int*)WallBool->getupdateBufferByMap();
		memcpy(&TexturePointer[(x * numberY * 16 * 4) + (y * 4)], &pixelS[BlockTypeS[x / 16][y / 16]][(((x % 16) * 16) + (y % 16)) * 4], 4);
		//TextureMist[(x * numberY * 16 * 4) + (y * 4) + 3] = 230;
		MixColors(&TextureMist[(x * numberY * 16 * 4) + (y * 4)], &pixelS[BlockTypeS[x / 16][y / 16]][(((x % 16) * 16) + (y % 16)) * 4]);
		Mistwall[(x * numberY * 16) + y] = 0;
		WallBool->endupdateBufferByMap();
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
	}

	void Labyrinth::AddPixel(unsigned int x, unsigned int y) {
		BlockPixelS[(x * numberY * 16) + y] = 1;
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		int* Mistwall = (int*)WallBool->getupdateBufferByMap();
		memcpy(&TexturePointer[(x * numberY * 16 * 4) + (y * 4)], &pixelS[2][(((x % 16) * 16) + (y % 16)) * 4], 4);
		//TextureMist[(x * numberY * 16 * 4) + (y * 4) + 3] = 230;
		MixColors(&TextureMist[(x * numberY * 16 * 4) + (y * 4)], &pixelS[2][(((x % 16) * 16) + (y % 16)) * 4]);
		Mistwall[(x * numberY * 16) + y] = 1;
		WallBool->endupdateBufferByMap();
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
	}

	bool Labyrinth::GetPixel(unsigned int x, unsigned int y) {
		return BlockPixelS[(x * numberY * 16) + y] == 0 ? 0 : 1;
	}

	void Labyrinth::UpDateMaps() {
		if (UpDateMapsSwitch) {
			UpDateMapsSwitch = false;
			PixelTextureS->UpDataImage();
		}
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
			mMistCalculateCommandPoolS
		);

		VkBufferCopy copyInfo{};
		copyInfo.size = (numberX * numberY * 16 * 16 * 4);
		mMistCalculate->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		mMistCalculate->copyBufferToBuffer(WarfareMist->getHOSTImageBuffer(), jihsuanTP->getBuffer(), 1, { copyInfo });//获取原数据
		mMistCalculate->bindGraphicPipeline(pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);//设置计算管线
		mMistCalculate->bindDescriptorSet(pipelineLayout, descriptorSet, VK_PIPELINE_BIND_POINT_COMPUTE);//获取描述符
		vkCmdDispatch(mMistCalculate->getCommandBuffer(), (uint32_t)ceil((wymiwustruct.size) / float(64)), 1, 1);//分配计算单元开始计算
		mMistCalculate->copyBufferToImage(//将计算的迷雾结果更新到图片上
			jihsuanTP->getBuffer(),
			WarfareMist->getImage()->getImage(),
			WarfareMist->getImage()->getLayout(),
			WarfareMist->getImage()->getWidth(),
			WarfareMist->getImage()->getHeight()
		);
		mMistCalculate->end();
		mMistCalculate->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

		WarfareMist->getImage()->setImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			region,
			mMistCalculateCommandPoolS
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

		mMistCalculateCommandPoolS = new VulKan::CommandPool(mDevice);
		mMistCalculate = new VulKan::CommandBuffer(mDevice, mMistCalculateCommandPoolS);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[3] = {};
		descriptorSetLayoutBinding[0].binding = 0; // binding = 0  迷雾计算结果
		descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorSetLayoutBinding[0].descriptorCount = 1;
		descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		descriptorSetLayoutBinding[1].binding = 1; // binding = 0  描述符
		descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorSetLayoutBinding[1].descriptorCount = 1;
		descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		descriptorSetLayoutBinding[2].binding = 2; // binding = 0  墙壁
		descriptorSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorSetLayoutBinding[2].descriptorCount = 1;
		descriptorSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = 3; // only a single binding in this descriptor set layout. //*******************************************
		descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBinding;                                   //*******************************************

		// Create the descriptor set layout. 
		vkCreateDescriptorSetLayout(mDevice->getDevice(), &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout);

		VkDescriptorPoolSize descriptorPoolSize[3] = {};                                                        //*******************************************
		descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorPoolSize[0].descriptorCount = 1;

		descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;                                         //*******************************************
		descriptorPoolSize[1].descriptorCount = 1;                                                              //*******************************************

		descriptorPoolSize[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;                                         //*******************************************
		descriptorPoolSize[2].descriptorCount = 1;                                                              //*******************************************


		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; //这个标志的作用就是指示VkDescriptorPool可以释放包含VkDescriptorSet的内存。
		descriptorPoolCreateInfo.maxSets = 1; // we only need to allocate one descriptor set from the pool.
		descriptorPoolCreateInfo.poolSizeCount = 3;                                                             //*******************************************
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize;

		// create descriptor pool.
		vkCreateDescriptorPool(mDevice->getDevice(), &descriptorPoolCreateInfo, NULL, &descriptorPool);

		/*
		With the pool allocated, we can now allocate the descriptor set.
		*/
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool; // pool to allocate from.
		descriptorSetAllocateInfo.descriptorSetCount = 1; // allocate a single descriptor set.
		descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

		// allocate descriptor set.
		vkAllocateDescriptorSets(mDevice->getDevice(), &descriptorSetAllocateInfo, &descriptorSet);

		/*
		Next, we need to connect our actual storage buffer with the descrptor.
		We use vkUpdateDescriptorSets() to update the descriptor set.
		*/

		// Specify the buffer to bind to the descriptor.

		VkWriteDescriptorSet writeDescriptorSet[3] = {};                                                //*******************************************
		writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet[0].dstSet = descriptorSet; // write to this descriptor set.
		writeDescriptorSet[0].dstBinding = 0; // write to the first, and only binding.
		writeDescriptorSet[0].descriptorCount = 1; // update a single descriptor.
		writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
		writeDescriptorSet[0].pBufferInfo = &jihsuanTP->getBufferInfo();

		writeDescriptorSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet[1].dstSet = descriptorSet; // write to this descriptor set.
		writeDescriptorSet[1].dstBinding = 1; // write to the first, and only binding.
		writeDescriptorSet[1].descriptorCount = 1; // update a single descriptor.
		writeDescriptorSet[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
		writeDescriptorSet[1].pBufferInfo = &information->getBufferInfo();

		writeDescriptorSet[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet[2].dstSet = descriptorSet; // write to this descriptor set.
		writeDescriptorSet[2].dstBinding = 2; // write to the first, and only binding.
		writeDescriptorSet[2].descriptorCount = 1; // update a single descriptor.
		writeDescriptorSet[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
		writeDescriptorSet[2].pBufferInfo = &WallBool->getBufferInfo();


		// perform the update of the descriptor set.
		vkUpdateDescriptorSets(mDevice->getDevice(), 3, writeDescriptorSet, 0, NULL);                                //*******************************************

		uint32_t filelength;
		// the code in comp.spv was created by running the command:
		// glslangValidator.exe -V shader.comp
		uint32_t* code = readFile(filelength, WarfareMist_spv);
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pCode = code;
		createInfo.codeSize = filelength;

		vkCreateShaderModule(mDevice->getDevice(), &createInfo, NULL, &computeShaderModule);
		delete[] code;


		VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageCreateInfo.module = computeShaderModule;
		shaderStageCreateInfo.pName = "main";

		/*
		The pipeline layout allows the pipeline to access descriptor sets.
		So we just specify the descriptor set layout we created earlier.
		*/
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		vkCreatePipelineLayout(mDevice->getDevice(), &pipelineLayoutCreateInfo, NULL, &pipelineLayout);

		VkComputePipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stage = shaderStageCreateInfo;
		pipelineCreateInfo.layout = pipelineLayout;

		/*
		Now, we finally create the compute pipeline.
		*/
		vkCreateComputePipelines(mDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &pipeline);
	}


	void Labyrinth::DeleteMist() {
		vkDestroyPipelineLayout(mDevice->getDevice(), pipelineLayout, nullptr);
		vkDestroyPipeline(mDevice->getDevice(), pipeline, nullptr);
		vkDestroyShaderModule(mDevice->getDevice(), computeShaderModule, nullptr);
		vkFreeDescriptorSets(mDevice->getDevice(), descriptorPool, 1, &descriptorSet);
		vkDestroyDescriptorSetLayout(mDevice->getDevice(), descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(mDevice->getDevice(), descriptorPool, nullptr);
		delete jihsuanTP;
		delete information;
		delete WallBool;
		delete mMistCalculate;
		delete mMistCalculateCommandPoolS;
	}
}