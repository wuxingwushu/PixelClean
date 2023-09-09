#include "Dungeon.h"
#include "../BlockS/PixelS.h"
#include "../VulkanTool/PixelTexture.h"


namespace GAME {

	Dungeon::Dungeon(VulKan::Device* device, unsigned int X, unsigned int Y)
		:wDevice(device),
		mNumberX(X),
		mNumberY(Y)
	{
		mMoveTerrain = new SquarePhysics::MoveTerrain<TextureAndBuffer>(mNumberX, mNumberY, 16, 1);
		mMoveTerrain->SetCallback(
			[](SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* mT, int x, int y, void* Data) {
				GAME::Dungeon* LDungeon = (GAME::Dungeon*)Data;
				ObjectUniform LUniform;
				LUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x * 16, y * 16, 0.0f));//位移矩阵
				for (size_t i = 0; i < LDungeon->wFrameCount; i++)
				{
					mT->mModel->mBufferS[i]->updateBufferByMap((void*)&LUniform, sizeof(ObjectUniform));
				}
				mT->mModel->mPixelTexture->ModifyImage(16*16*4, (void*)pixelS[11]);
			},
			this,
			[](SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* mT, void* Data) {

			},
			this
		);

		const float Positions[] = {
			-8.0f, -8.0f, 0.0f,
			8.001f, -8.0f, 0.0f,
			8.001f, 8.001f, 0.0f,
			-8.0f, 8.001f, 0.0f,
		};

		const float UVs[] = {
			1.0f,0.0f,
			0.0f,0.0f,
			0.0f,1.0f,
			1.0f,1.0f,
		};

		const unsigned int IndexDatas[] = {
			0,1,2,
			2,3,0,
		};

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(wDevice, 12 * sizeof(float), (void*)Positions);
		mUVBuffer = VulKan::Buffer::createVertexBuffer(wDevice, 8 * sizeof(float), (void*)UVs);
		mIndexBuffer = VulKan::Buffer::createIndexBuffer(wDevice, 6 * sizeof(unsigned int), (void*)IndexDatas);
		mIndexDatasSize = 6;

		mTextureAndBuffer = new TextureAndBuffer*[mNumberX];
		for (size_t ix = 0; ix < mNumberX; ix++)
		{
			mTextureAndBuffer[ix] = new TextureAndBuffer[mNumberY];
			for (size_t iy = 0; iy < mNumberY; iy++)
			{
				mMoveTerrain->GetRigidBodyAndModel(ix, iy)->mModel = &mTextureAndBuffer[ix][iy];
			}
		}
	}

	Dungeon::~Dungeon()
	{
	}

	//初始化描述符
	void Dungeon::initUniformManager(
		int frameCount, //GPU画布的数量
		const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
		std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
		VulKan::Sampler* sampler//图片采样器
	) {
		wFrameCount = frameCount;
		mCommandPool = new VulKan::CommandPool(wDevice);
		mCommandBuffer = new VulKan::CommandBuffer*[wFrameCount];
		for (size_t i = 0; i < wFrameCount; i++)
		{
			mCommandBuffer[i] = new VulKan::CommandBuffer(wDevice, mCommandPool, true);
		}

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
		objectParam->mSize = sizeof(ObjectUniform);
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

		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(wDevice);
		mDescriptorPool->build(mUniformParams, wFrameCount, mNumberX*mNumberY);
		ObjectUniform mUniform;

		mDescriptorSet = new VulKan::DescriptorSet**[mNumberX];
		for (size_t ix = 0; ix < mNumberX; ix++)
		{
			mDescriptorSet[ix] = new VulKan::DescriptorSet*[mNumberX];
			for (size_t iy = 0; iy < mNumberY; iy++)
			{
				mTextureAndBuffer[ix][iy].mBufferS = new VulKan::Buffer*[wFrameCount];
				mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(ix * 16, iy * 16, 0.0f));//位移矩阵
				for (size_t i = 0; i < wFrameCount; i++)
				{
					mTextureAndBuffer[ix][iy].mBufferS[i] = VulKan::Buffer::createUniformBuffer(wDevice, sizeof(ObjectUniform));
					mTextureAndBuffer[ix][iy].mBufferS[i]->updateBufferByMap((void*)&mUniform, sizeof(ObjectUniform));
					objectParam->mBuffers[i] = mTextureAndBuffer[ix][iy].mBufferS[i];
				}
				mTextureAndBuffer[ix][iy].mPixelTexture = new GAME::PixelTexture(wDevice, mCommandPool, pixelS[8], 16, 16, 4, sampler);
				textureParam->mPixelTexture = mTextureAndBuffer[ix][iy].mPixelTexture;
				mDescriptorSet[ix][iy] = new VulKan::DescriptorSet(wDevice, mUniformParams, mDescriptorSetLayout, mDescriptorPool, wFrameCount);
			}
		}
	}

	void Dungeon::initCommandBuffer() {
		for (size_t i = 0; i < wFrameCount; i++)
		{
			VkCommandBufferInheritanceInfo InheritanceInfo{};
			InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			InheritanceInfo.renderPass = mRenderPass->getRenderPass();
			InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mCommandBuffer[i]->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
			mCommandBuffer[i]->bindIndexBuffer(mIndexBuffer->getBuffer());//获得顶点索引
			for (size_t ix = 0; ix < mNumberX; ix++)
			{
				for (size_t iy = 0; iy < mNumberY; iy++)
				{
					mCommandBuffer[i]->bindDescriptorSet(mPipeline->getLayout(), mDescriptorSet[ix][iy]->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
					mCommandBuffer[i]->drawIndex(mIndexDatasSize);//获取绘画物体的顶点个数
				}
			}
			mCommandBuffer[i]->end();
		}
	}

}