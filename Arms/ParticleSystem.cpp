#include "ParticleSystem.h"
namespace GAME {
	ParticleSystem::ParticleSystem(VulKan::Device* device, int Number)
	{
		mNumber = Number;
		mParticle = new PileUp<Particle>(mNumber);

		ThreadS = (TOOL::mThreadCount / 3) * 3;
		mThreadCommandPoolS = new VulKan::CommandPool * [ThreadS];
		mThreadCommandBufferS = new VulKan::CommandBuffer * [ThreadS];
		for (int i = 0; i < (ThreadS); i++) {
			mThreadCommandPoolS[i] = new VulKan::CommandPool(device);
			mThreadCommandBufferS[i] = new VulKan::CommandBuffer(device, mThreadCommandPoolS[i], true);
		}

		mUniformParams = new std::vector<VulKan::UniformParameter*>[mNumber];
		mDescriptorSet = new VulKan::DescriptorSet*[mNumber];
		PixelTextureS = new PixelTexture*[mNumber];


		float mPositions[12] = {
			-1.0f, -1.0f, 0.0f,
			 1.0f, -1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f,
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

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(device, 6 * sizeof(float), mIndexDatas);

		mIndexDatasSize = 6;
	}

	ParticleSystem::~ParticleSystem()
	{
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
		ObjectUniform mUniform;
		for (int x = 0; x < mNumber; x++)
		{
			PixelTextureS[x] = new PixelTexture(device, mThreadCommandPoolS[0], pixelS[1], 1, 1, 4, sampler);

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
				mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f));
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
		}


		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams[0], frameCount, (mNumber));
		for (size_t x = 0; x < mNumber; x++)
		{
			//将申请的各种类型按照Layout绑定起来
			mDescriptorSet[x] = new VulKan::DescriptorSet(device, mUniformParams[x], mDescriptorSetLayout, mDescriptorPool, frameCount);
		}
	}

	void ParticleSystem::ThreadUpdateCommandBuffer() {
		std::vector<std::future<void>> pool;
		int UpdateNumber = (mNumber) / (ThreadS / 3);
		int UpdateNumber_yu = (mNumber) % (ThreadS / 3);
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < UpdateNumber_yu; j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&ParticleSystem::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1)));
			}
			for (int j = UpdateNumber_yu; j < (ThreadS / 3); j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&ParticleSystem::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber));
			}
		}
		for (int i = 0; i < (ThreadS); i++) {
			pool[i].wait();
		}
	}

	void ParticleSystem::ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count)
	{
		unsigned int mFrameBufferCount = ((ThreadS / 3) * FrameCount) + BufferCount;
		VulKan::CommandBuffer* commandbuffer = mThreadCommandBufferS[mFrameBufferCount];

		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass;
		InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(FrameCount);

		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
		commandbuffer->bindGraphicPipeline(mPipeline->getPipeline());//获得渲染管线
		commandbuffer->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
		commandbuffer->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
		for (int i = AddresShead; i < AddresShead + Count; i++) {
			commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mDescriptorSet[i]->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
			commandbuffer->drawIndex(getIndexCount());//获取绘画物体的顶点个数
		}
		commandbuffer->end();
	}
}