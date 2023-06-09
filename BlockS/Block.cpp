#include "Block.h"

namespace GAME {

	Block::Block(VulKan::Device* device, int X, int Y, int MemoryPooli, bool collision) {
		mX = X;
		mY = Y;
		MemoryPool_i = MemoryPooli;//是那个内存池申请的
		Collision = collision;//是否开启碰撞
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(X, Y, 0.0f));//位移矩阵
		//trans = glm::rotate(trans, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));//旋转矩阵
		//trans = glm::scale(trans, glm::vec3(1.30, 1.30, 1.30));//缩放矩阵

		/*mPositions = {
			0.0f, 0.0f, 0.0f,
			16.0f, 0.0f, 0.0f,
			16.0f, 16.0f, 0.0f,
			0.0f, 16.0f, 0.0f,
		};*/

		mUVs = {
			1.0f,0.0f,
			0.0f,0.0f,
			0.0f,1.0f,
			1.0f,1.0f,
		};

		mIndexDatas = {
			0,1,2,
			2,3,0,
		};

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(device, 12 * sizeof(float), mPositions, true);

		mUVBuffer = VulKan::Buffer::createVertexBuffer(device, mUVs.size() * sizeof(float), mUVs.data(), true);

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(device, mIndexDatas.size() * sizeof(float), mIndexDatas.data(), true);

		mIndexDatasSize = mIndexDatas.size();
		//mPositions.clear();
		mUVs.clear();
		mIndexDatas.clear();
	}

	Block::~Block() {
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
	}

	void Block::setBlockMatrix(glm::mat4 Matrix, const int& frameCount, bool mode) {
		if (mode) {
			mUniform.mModelMatrix = Matrix;
			for (int i = 0; i < frameCount; i++) {
				//update object Matrix
				mUniformParams[1]->mBuffers[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
			}
		}
		else {
			mUniform.mModelMatrix = Matrix;
			//update object Matrix
			mUniformParams[1]->mBuffers[frameCount]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
		}
	}

	void Block::initUniformManager(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		int frameCount, PixelTexture* texturepath,
		const VkDescriptorSetLayout mDescriptorSetLayout,
		std::vector<VulKan::Buffer*> VPMstdBuffer,
		VulKan::Sampler* sampler
	)
	{
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
		textureParam->mPixelTexture = texturepath;

		mUniformParams.push_back(textureParam);


		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams, frameCount);

		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(device, mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);

		setBlockMatrix(mUniform.mModelMatrix, frameCount, true);
	}

	void Block::getBlockCommandBuffer(VulKan::CommandBuffer* BlockCommandBuffer) {
		mPositionBuffer->getUpDateBufferCommandBuffer(BlockCommandBuffer);
		mUVBuffer->getUpDateBufferCommandBuffer(BlockCommandBuffer);
		mIndexBuffer->getUpDateBufferCommandBuffer(BlockCommandBuffer);
		//mUniformParams[2]->mPixelTexture->ThreadPixelTextureCommandBuffer(BlockCommandBuffer);
	}

}