#include "descriptorSet.h"

namespace VulKan {

	DescriptorSet::DescriptorSet(
		Device* device,
		const std::vector<UniformParameter*> params,
		const VkDescriptorSetLayout layout,
		const DescriptorPool* pool,
		int frameCount,
		std::mutex* wMutex
	) :
		mDevice(device),
		mDescriptorPool(pool),
		wFrameCount(frameCount)
	{
		std::vector<VkDescriptorSetLayout> layouts(frameCount, layout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool->getPool();
		allocInfo.descriptorSetCount = frameCount;
		allocInfo.pSetLayouts = layouts.data();
		
		mDescriptorSets.resize(frameCount);
		if (wMutex != nullptr) {
			wMutex->lock();
			if (vkAllocateDescriptorSets(mDevice->getDevice(), &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("Error: failed to allocate descriptor sets");
			}
			wMutex->unlock();
		}
		else {
			if (vkAllocateDescriptorSets(mDevice->getDevice(), &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("Error: failed to allocate descriptor sets");
			}
		}
		
		mDescriptorSize = params.size();

		for (int i = 0; i < frameCount; ++i) {
			std::vector<VkWriteDescriptorSet> LSdescriptorSetWrites;
			for (const auto& param : params) {
				VkWriteDescriptorSet descriptorSetWrite{};
				descriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorSetWrite.dstSet = mDescriptorSets[i];//
				descriptorSetWrite.dstArrayElement = 0;
				descriptorSetWrite.descriptorType = param->mDescriptorType;//数据类型
				descriptorSetWrite.descriptorCount = param->mCount;//多少个这样的数据
				descriptorSetWrite.dstBinding = param->mBinding;//绑定到那个Binding
				
				if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
					descriptorSetWrite.pBufferInfo = &param->mBuffers[i]->getBufferInfo();
				}
				else if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
					if (param->mTexture == nullptr) {
						descriptorSetWrite.pImageInfo = &param->mPixelTexture->getImageInfo();
					}
					else {
						descriptorSetWrite.pImageInfo = &param->mTexture->getImageInfo();
					}
					
				}

				LSdescriptorSetWrites.push_back(descriptorSetWrite);
			}
			descriptorSetWrites.push_back(LSdescriptorSetWrites);

			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(LSdescriptorSetWrites.size()), LSdescriptorSetWrites.data(), 0, nullptr);
		}
	}

	void DescriptorSet::UpDataImagePicture(unsigned int Index, VkDescriptorImageInfo* ImageInfo) {
		for (auto DescriptorSetWrites : descriptorSetWrites) {
			if (DescriptorSetWrites[Index].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
				DescriptorSetWrites[Index].pImageInfo = ImageInfo;
			}
			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(DescriptorSetWrites.size()), DescriptorSetWrites.data(), 0, nullptr);
		}
	}

	void DescriptorSet::UpDataBufferPicture(unsigned int Index, VkDescriptorBufferInfo* BufferInfo) {
		for (auto DescriptorSetWrites : descriptorSetWrites) {
			if (DescriptorSetWrites[Index].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				DescriptorSetWrites[Index].pBufferInfo = BufferInfo;
			}
			vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(DescriptorSetWrites.size()), DescriptorSetWrites.data(), 0, nullptr);
		}
	}

	DescriptorSet::~DescriptorSet() {
		vkFreeDescriptorSets(mDevice->getDevice(), mDescriptorPool->getPool(), mDescriptorSets.size(), mDescriptorSets.data());
	}
}