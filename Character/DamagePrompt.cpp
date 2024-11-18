#include "DamagePrompt.h"


namespace GAME {



	DamagePrompt::DamagePrompt(VulKan::Device* device, 
		const VkDescriptorSetLayout mDescriptorSetLayout, std::vector<VulKan::Buffer*> camera, 
		VulKan::Texture* texture, unsigned int frameCount, unsigned int size):
		mSize(size)
	{
		mDamagePromptAngle = new Queue<float>(mSize);
		//mDamagePromptAngle->SetPopCallback(2000, [](float* data, void* ptr) {}, this);

		mFreeLocation = new PileUp<VulKan::Buffer*>(mSize);
		mPlaceOfUse = new Queue<VulKan::Buffer*>(mSize);

		std::vector<float> mPositions = {
			-11.0f, -11.0f, 0.0f,
			 11.0f, -11.0f, 0.0f,
			 11.0f,  11.0f, 0.0f,
			-11.0f,  11.0f, 0.0f,
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

		mCommandPool = new VulKan::CommandPool(device);
		mCommandBuffer = new VulKan::CommandBuffer*[frameCount];
		for (size_t i = 0; i < frameCount; i++)
		{
			mCommandBuffer[i] = new VulKan::CommandBuffer(device, mCommandPool, true);
		}


		std::vector<VulKan::UniformParameter*> mUniformParams{};

		VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
		vpParam->mBinding = 0;
		vpParam->mCount = 1;
		vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpParam->mSize = sizeof(VPMatrices);
		vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		vpParam->mBuffers = camera;
		mUniformParams.push_back(vpParam);

		VulKan::UniformParameter* objectParam = new VulKan::UniformParameter();
		objectParam->mBinding = 1;
		objectParam->mCount = 1;
		objectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		objectParam->mSize = sizeof(ObjectUniform);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
		objectParam->mBuffers.resize(frameCount);
		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam->mTexture = texture;
		mUniformParams.push_back(textureParam);

		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams, frameCount, mSize);

		ObjectUniform mUniform{};
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -100000.0f));
		mUniform.StrikeColour = { 0.0f, 0.0f, 0.0f };
		mDescriptorSet = new VulKan::DescriptorSet*[mSize];
		for (size_t i = 0; i < mSize; i++)
		{
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
			ObjectUniform* wObjectUniform = (ObjectUniform*)buffer->getupdateBufferByMap();
			*wObjectUniform = mUniform;
			buffer->endupdateBufferByMap();
			mFreeLocation->add(buffer);
			for (size_t fi = 0; fi < frameCount; fi++)
			{
				objectParam->mBuffers[fi] = buffer;
			}

			mDescriptorSet[i] = new VulKan::DescriptorSet(device, mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);
		}

		for (auto i: mUniformParams)
		{
			delete i;
		}
	}

	DamagePrompt::~DamagePrompt()
	{
		delete mDamagePromptAngle;
		delete mFreeLocation;
		delete mPlaceOfUse;

		delete mPositionBuffer;
		delete mUVBuffer;
		delete mIndexBuffer;

		for (size_t i = 0; i < mSize; i++)
		{
			delete mDescriptorSet[i];
		}
		delete mDescriptorSet;
		delete mDescriptorPool;

		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			delete mCommandBuffer[i];
		}
		delete mCommandBuffer;
		delete mCommandPool;
	}

	void DamagePrompt::initCommandBuffer()
	{
		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = wRenderPass->getRenderPass();

		for (size_t i = 0; i < wSwapChain->getImageCount(); ++i)
		{
			InheritanceInfo.framebuffer = wSwapChain->getFrameBuffer(i);

			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mCommandBuffer[i]->bindGraphicPipeline(wPipeline->getPipeline());//获得渲染管线
			mCommandBuffer[i]->bindVertexBuffer({ mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() });//获取顶点数据，UV值
			mCommandBuffer[i]->bindIndexBuffer(mIndexBuffer->getBuffer());//获得顶点索引
			for (size_t id = 0; id < mSize; id++)
			{
				mCommandBuffer[i]->bindDescriptorSet(wPipeline->getLayout(), mDescriptorSet[id]->getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
				mCommandBuffer[i]->drawIndex(mIndexDatasSize);//获取绘画物体的顶点个数
			}
			mCommandBuffer[i]->end();
		}
	}

	void DamagePrompt::AddDamagePrompt(double Angle)
	{
		mDamagePromptAngle->add(Angle);
		VulKan::Buffer* buffer = *mFreeLocation->pop();
		ObjectUniform* wObjectUniform = (ObjectUniform*)buffer->getupdateBufferByMap();
		wObjectUniform->StrikeState = 1.0f;
		buffer->endupdateBufferByMap();
		mPlaceOfUse->add(buffer);
	}

	void DamagePrompt::UpDataDamagePrompt(double x, double y, double time)
	{
		ObjectUniform mUniform{};
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
		mDamagePromptAngle->InitData();
		mPlaceOfUse->InitData();
		VulKan::Buffer* buffer = nullptr;
		ObjectUniform* wObjectUniform = nullptr;
		for (size_t i = 0; i < mDamagePromptAngle->GetNumber(); i++)
		{
			buffer = *mPlaceOfUse->popData();
			wObjectUniform = (ObjectUniform*)buffer->getupdateBufferByMap();
			if (wObjectUniform->StrikeState > 0) {
				wObjectUniform->mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(*mDamagePromptAngle->popData() * 180.0 / 3.14), glm::vec3(0.0, 0.0, 1.0));
				wObjectUniform->StrikeState -= time;
				if (wObjectUniform->StrikeState < 0)wObjectUniform->StrikeState = 0.0f;
			}
			else {
				mFreeLocation->add(*mPlaceOfUse->pop());
				mDamagePromptAngle->_pop();
				wObjectUniform->mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -100000.0f));
			}
			buffer->endupdateBufferByMap();
		}
	}

}
