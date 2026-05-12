#include "ParticleSystem.h"
#include "../DebugLog.h"
#include "../BlockS/PixelS.h"
#include "../GlobalStructural.h"

namespace GAME {
	/**
	 * @brief 粒子系统
	 */
	ParticleSystem::ParticleSystem(VulKan::Device* device, int Number)
	{
		mNumber = Number;
		mParticle = new PileUp<Particle>(mNumber);

		

		mUniformParams = new std::vector<VulKan::UniformParameter*>[mNumber];
		mDescriptorSet = new VulKan::DescriptorSet*[mNumber];
		PixelTextureS = new VulKan::PixelTexture*[mNumber];


		float mPositions[12] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f,
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

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(device, 12 * sizeof(float), mPositions);

		mUVBuffer = VulKan::Buffer::createVertexBuffer(device, 8 * sizeof(float), mUVs);

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(device, 6 * sizeof(unsigned int), mIndexDatas);

		mIndexDatasSize = 6;
	}

	ParticleSystem::~ParticleSystem()
	{
		delete mParticle;

		for (size_t i = 0; i < mNumber; ++i)
		{
			for (size_t iu = 0; iu < mUniformParams[i].size(); ++iu)
			{
				if (iu == 1)
				{
					for (size_t ib = 0; ib < ThreadS; ib++)
					{
						delete mUniformParams[i][iu]->mBuffers[ib];
					}
				}
				delete mUniformParams[i][iu];
			}
			delete mDescriptorSet[i];
			delete PixelTextureS[i];
		}

		delete mDescriptorPool;

		delete mUniformParams;
		delete mDescriptorSet;
		delete PixelTextureS;

		delete mPositionBuffer;
		delete mUVBuffer;
		delete mIndexBuffer;

		for (size_t i = 0; i < ThreadS; ++i)
		{
			delete mThreadCommandBufferS[i];
			delete mThreadCommandPoolS[i];
		}
		delete mThreadCommandBufferS;
		delete mThreadCommandPoolS;
	}

	void ParticleSystem::initUniformManager(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		int frameCount,
		const VkDescriptorSetLayout mDescriptorSetLayout,
		std::vector<VulKan::Buffer*> VPMstdBuffer,
		VulKan::Sampler* sampler
	)
	{
		LOGI("ParticleSystem::initUniformManager() called, mNumber=%d, frameCount=%d", mNumber, frameCount);
		ThreadS = frameCount;
		mThreadCommandPoolS = new VulKan::CommandPool * [ThreadS];
		mThreadCommandBufferS = new VulKan::CommandBuffer * [ThreadS];
		for (int i = 0; i < (ThreadS); ++i) {
			mThreadCommandPoolS[i] = new VulKan::CommandPool(device);
			mThreadCommandBufferS[i] = new VulKan::CommandBuffer(device, mThreadCommandPoolS[i], true);
		}

		LOGI("ParticleSystem: creating %d PixelTextures + UniformParams...", mNumber);
		ObjectUniform mUniform;
		for (int x = 0; x < mNumber; ++x)
		{
			PixelTextureS[x] = new VulKan::PixelTexture(device, mThreadCommandPoolS[0], pixelS[1], 1, 1, 4, sampler);

			VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
			vpParam->mBinding = 0;
			vpParam->mCount = 1;
			vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			vpParam->mSize = sizeof(VPMatrices);
			vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
			vpParam->mBuffers = VPMstdBuffer;

			mUniformParams[x].push_back(vpParam);

			VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
			objectParam->mBinding = 1;
			objectParam->mCount = 1;
			objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			objectParam->mSize = sizeof(ObjectUniform);
			objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
			
			for (int i = 0; i < frameCount; ++i) {
				VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
				objectParam->mBuffers.push_back(buffer);
				mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10000.0f));
				buffer->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
			}

			mUniformParams[x].push_back(objectParam);

			VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
			textureParam->mBinding = 2;
			textureParam->mCount = 1;
			textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
			textureParam->mPixelTexture = PixelTextureS[x];

			mUniformParams[x].push_back(textureParam);

			mParticle->add(Particle{ PixelTextureS[x], &objectParam->mBuffers });

			if (x % 100 == 0) {
				LOGI("ParticleSystem: PixelTexture+UniformParams progress: %d/%d", x, mNumber);
			}
		}
		LOGI("ParticleSystem: all PixelTextures + UniformParams created");

		LOGI("ParticleSystem: creating DescriptorPool...");
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams[0], frameCount, (mNumber));
		LOGI("ParticleSystem: DescriptorPool created, now creating %d DescriptorSets...", mNumber);
		for (size_t x = 0; x < mNumber; ++x)
		{
			mDescriptorSet[x] = new VulKan::DescriptorSet(device, mUniformParams[x], mDescriptorSetLayout, mDescriptorPool, frameCount);
			if (x % 100 == 0) {
				LOGI("ParticleSystem: DescriptorSet progress: %zu/%d", x, mNumber);
			}
		}
		LOGI("ParticleSystem: all DescriptorSets created");
	}

	void ParticleSystem::ThreadUpdateCommandBuffer() {
#if defined(__ANDROID__)
		LOGI("ParticleSystem::ThreadUpdateCommandBuffer() - Android synchronous path, ThreadS=%d, mNumber=%d", ThreadS, mNumber);
		for (unsigned int frame = 0; frame < ThreadS; ++frame) {
			LOGI("ParticleSystem: recording command buffer for frame %u/%u", frame, ThreadS);
			unsigned int mFrameBufferCount = frame;
			VulKan::CommandBuffer* commandbuffer = mThreadCommandBufferS[mFrameBufferCount];

			VkCommandBufferInheritanceInfo InheritanceInfo{};
			InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			InheritanceInfo.renderPass = mRenderPass->getRenderPass();
			InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(frame);

			commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);
			commandbuffer->bindGraphicPipeline(mPipeline->getPipeline());
			commandbuffer->bindVertexBuffer(getVertexBuffers());
			commandbuffer->bindIndexBuffer(getIndexBuffer()->getBuffer());
			for (int i = 0; i < mNumber; ++i) {
				commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mDescriptorSet[i]->getDescriptorSet(frame));
				commandbuffer->drawIndex(getIndexCount());
			}
			commandbuffer->end();
			LOGI("ParticleSystem: frame %u command buffer recorded (%d draw calls)", frame, mNumber);
		}
		LOGI("ParticleSystem::ThreadUpdateCommandBuffer() - Android synchronous path DONE");
#else
		std::vector<std::future<void>> pool;
		int UpdateNumber = (mNumber) / (ThreadS / 3);
		int UpdateNumber_yu = (mNumber) % (ThreadS / 3);
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < UpdateNumber_yu; ++j) {
				pool.push_back(TOOL::mThreadPool->enqueue(&ParticleSystem::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1)));
			}
			for (int j = UpdateNumber_yu; j < (ThreadS / 3); ++j) {
				pool.push_back(TOOL::mThreadPool->enqueue(&ParticleSystem::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber));
			}
		}
		for (size_t i = 0; i < pool.size(); ++i) {
			pool[i].wait();
		}
#endif
	}

	void ParticleSystem::ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count)
	{
		unsigned int mFrameBufferCount = ((ThreadS / 3) * FrameCount) + BufferCount;
		VulKan::CommandBuffer* commandbuffer = mThreadCommandBufferS[mFrameBufferCount];

		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass->getRenderPass();
		InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(FrameCount);

		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
		commandbuffer->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线
		commandbuffer->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
		commandbuffer->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
		for (int i = AddresShead; i < AddresShead + Count; ++i) {
			commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mDescriptorSet[i]->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
			commandbuffer->drawIndex(getIndexCount());//获取绘画物体的顶点个数
		}
		commandbuffer->end();
	}
}