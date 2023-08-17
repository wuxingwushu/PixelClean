#include "GamePlayer.h"
#include "../BlockS/PixelS.h"

namespace GAME {

	void GamePlayerDestroyPixel(int x, int y, bool Bool, void* mclass) {
		GamePlayer* Class = (GamePlayer*)mclass;
		Class->mPixelQueue->add({x,y, Bool });
	}

	GamePlayer::GamePlayer(VulKan::Device* device, VulKan::Pipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* renderPass, 
		SquarePhysics::SquarePhysics* SquarePhysics, float X, float Y)
	{
		mPipeline = pipeline;
		mSwapChain = swapChain;
		mRenderPass = renderPass;
		mSquarePhysics = SquarePhysics;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(X, Y, 0.0f));//位移矩阵

		mPixelQueue = new Queue<PixelState>(100);

		mObjectCollision = new SquarePhysics::ObjectCollision(16, 16, 1);
		mBrokenData = new bool[16 * 16];
		for (size_t x = 0; x < 16; x++)
		{
			for (size_t y = 0; y < 16; y++)
			{
				mObjectCollision->GetPixelAttribute()[x][y].Collision = true;
				mBrokenData[x * 16 + y] = true;
			}
		}
		mObjectCollision->OutlineCalculate();//计算外骨架
		mObjectCollision->SetOrigin(8, 8);//设置原点
		mObjectCollision->SetPos({ X, Y });//设置位置
		mObjectCollision->SetFrictionCoefficient(10.0f);//设置摩擦系数
		mObjectCollision->SetCollisionCallback(GamePlayerDestroyPixel, this);//设置回调函数
		mSquarePhysics->AddObjectCollision(mObjectCollision);//添加玩家碰撞

		std::vector<float> mPositions = {
			-8.0f, -8.0f, 0.0f,
			8.0f, -8.0f, 0.0f,
			8.0f, 8.0f, 0.0f,
			-8.0f, 8.0f, 0.0f,
		};

		std::vector<float> mUVs = {
			0.01f,0.01f,
			0.01f,0.99f,
			0.99f,0.99f,
			0.99f,0.01f,
			
		};

		std::vector<unsigned int> mIndexDatas = {
			0,1,2,
			2,3,0,
		};

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(device, mPositions.size() * sizeof(float), mPositions.data());

		mUVBuffer = VulKan::Buffer::createVertexBuffer(device, mUVs.size() * sizeof(float), mUVs.data());

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(device, mIndexDatas.size() * sizeof(float), mIndexDatas.data());

		mIndexDatasSize = mIndexDatas.size();
		mPositions.clear();
		mUVs.clear();
		mIndexDatas.clear();


		mBufferCopyCommandPool = new VulKan::CommandPool(device);
		mBufferCopyCommandBuffer = new VulKan::CommandBuffer*[mSwapChain->getImageCount()];
		for (size_t i = 0; i < mSwapChain->getImageCount(); i++)
		{
			mBufferCopyCommandBuffer[i] = new VulKan::CommandBuffer(device, mBufferCopyCommandPool, true);
		}
	}

	GamePlayer::~GamePlayer()
	{
		mSquarePhysics->RemoveObjectCollision(mObjectCollision);//移除玩家碰撞

		if (mPositionBuffer != nullptr) {
			delete mPositionBuffer;
		}
		if (mUVBuffer != nullptr) {
			delete mUVBuffer;
		}
		if (mIndexBuffer != nullptr) {
			delete mIndexBuffer;
		}

		for (VulKan::UniformParameter* UPData : mUniformParams) {
			if (UPData->mBinding == 0) { delete UPData; continue; }
			for (VulKan::Buffer* Data : UPData->mBuffers) {
				if (Data != nullptr) {
					delete Data;
				}
			}
			delete UPData;
		}

		if (mDescriptorSet != nullptr) {
			delete mDescriptorSet;
		}

		if (mDescriptorPool != nullptr) {
			delete mDescriptorPool;
		}
		for (size_t i = 0; i < mSwapChain->getImageCount(); i++)
		{
			if (mBufferCopyCommandBuffer[i] != nullptr) {
				delete mBufferCopyCommandBuffer[i];
			}
		}
		if (mBufferCopyCommandPool != nullptr) {
			delete mBufferCopyCommandPool;
		}
		
		delete mObjectCollision;
	}

	void GamePlayer::setGamePlayerMatrix(glm::mat4 Matrix, const int& frameCount, bool mode)
	{
		mUniform.mModelMatrix = Matrix;
		if (mode) {
			for (int i = 0; i < frameCount; i++) {
				mUniformParams[1]->mBuffers[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
			}
		}
		else {
			mUniformParams[1]->mBuffers[frameCount]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
		}
	}

	void GamePlayer::UpDataBroken(bool* Broken) {
		unsigned char* TexturePointer = (unsigned char*)mPixelTexture->getHOSTImagePointer();
		for (size_t x = 0; x < 16; x++)
		{
			for (size_t y = 0; y < 16; y++)
			{
				if (Broken[x * 16 + y]) {
					memset(&TexturePointer[(x * 16 + y) * 4 + 3], 255, 1);
				}
				else {
					memset(&TexturePointer[(x * 16 + y) * 4 + 3], 0, 1);
				}
			}
		}
		mPixelTexture->endHOSTImagePointer();
		mPixelTexture->UpDataImage();
	}

	void GamePlayer::initUniformManager(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		int frameCount, 
		int textureID,
		const VkDescriptorSetLayout mDescriptorSetLayout,
		std::vector<VulKan::Buffer*> VPMstdBuffer,
		VulKan::Sampler* sampler
	)
	{
		mPixelTexture = new PixelTexture(device, commandPool, pixelS[10],16,16,4, sampler);

		VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
		vpParam->mBinding = 0;
		vpParam->mCount = 1;
		vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam->mSize = sizeof(VPMatrices);
		vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam->mBuffers = VPMstdBuffer;

		/*for (int i = 0; i < frameCount; ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, vpParam->mSize, nullptr);
			vpParam->mBuffers.push_back(buffer);
		}*/

		mUniformParams.push_back(vpParam);

		VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
		objectParam->mBinding = 1;
		objectParam->mCount = 1;
		objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam->mSize = sizeof(ObjectUniform);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;

		for (int i = 0; i < frameCount; ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
			objectParam->mBuffers.push_back(buffer);
		}

		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		//textureParam->mTexture = new Texture(device, commandPool, texturepath);
		textureParam->mPixelTexture = mPixelTexture;

		mUniformParams.push_back(textureParam);


		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams, frameCount);

		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(device, mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);

		setGamePlayerMatrix(mUniform.mModelMatrix, frameCount, true);
	}

	void GamePlayer::InitCommandBuffer() {
		for (size_t i = 0; i < 3; i++)
		{
			VkCommandBufferInheritanceInfo InheritanceInfo{};
			InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			InheritanceInfo.renderPass = mRenderPass->getRenderPass();
			InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);



			mBufferCopyCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mBufferCopyCommandBuffer[i]->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线

			mBufferCopyCommandBuffer[i]->bindDescriptorSet(mPipeline->getLayout(), getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mBufferCopyCommandBuffer[i]->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
			mBufferCopyCommandBuffer[i]->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
			mBufferCopyCommandBuffer[i]->drawIndex(getIndexCount());//获取绘画物体的顶点个数

			mBufferCopyCommandBuffer[i]->end();
		}
	}

	void GamePlayer::GetPixelSPointer() {
		TexturePointer = (unsigned char*)mPixelTexture->getHOSTImagePointer();
	}
	void GamePlayer::SetPixelS(unsigned int x, unsigned int y, bool Switch) {
		memset(&TexturePointer[(x * 16 + y) * 4 + 3], 0, 1);
		mBrokenData[x * 16 + y] = false;
	}
	void GamePlayer::EndPixelSPointer() {
		mPixelTexture->endHOSTImagePointer();
		mPixelTexture->UpDataImage();
	}
	void GamePlayer::UpData() {
		if (mPixelQueue->GetNumber() != 0) {
			PixelState* LPixel;
			GetPixelSPointer();
			while (mPixelQueue->GetNumber() != 0)
			{
				LPixel = mPixelQueue->pop();
				if (LPixel != nullptr) {
					if ((LPixel->X >= 0) && (LPixel->Y >= 0) && (LPixel->X < 16) && (LPixel->Y < 16)) {
						SetPixelS(LPixel->X, LPixel->Y, LPixel->State);
					}
				}
			}
			EndPixelSPointer();
		}
	}
}
