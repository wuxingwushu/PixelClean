#include "Calculate.h"
#include "../GeneralCalculationGPU/GPU.h"

namespace VulKan {

	Calculate::Calculate(Device* Device, std::vector<CalculateStruct>* CalculateStructS, const char* Comp)
	{
		wDevice = Device;

		mCommandPool = new CommandPool(wDevice);
		mCommandBuffer = new CommandBuffer(wDevice, mCommandPool);
		

		const int BufferSize = CalculateStructS->size();
		std::vector<VkDescriptorSetLayoutBinding> DescriptorSetLayoutBindingS;
		DescriptorSetLayoutBindingS.resize(BufferSize);
		for (size_t i = 0; i < BufferSize; i++)
		{
			DescriptorSetLayoutBindingS[i].binding = i;
			DescriptorSetLayoutBindingS[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			DescriptorSetLayoutBindingS[i].descriptorCount = 1;
			DescriptorSetLayoutBindingS[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = BufferSize; // only a single binding in this descriptor set layout.
		descriptorSetLayoutCreateInfo.pBindings = DescriptorSetLayoutBindingS.data();
	
		vkCreateDescriptorSetLayout(wDevice->getDevice(), &descriptorSetLayoutCreateInfo, NULL, &mDescriptorSetLayout);
	


		std::vector<VkDescriptorPoolSize> descriptorPoolSize;
		descriptorPoolSize.resize(BufferSize);
		for (size_t i = 0; i < BufferSize; i++)
		{
			descriptorPoolSize[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorPoolSize[i].descriptorCount = 1;
		}

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; //这个标志的作用就是指示VkDescriptorPool可以释放包含VkDescriptorSet的内存。
		descriptorPoolCreateInfo.maxSets = 1; // we only need to allocate one descriptor set from the pool.
		descriptorPoolCreateInfo.poolSizeCount = BufferSize;
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize.data();
	
		vkCreateDescriptorPool(wDevice->getDevice(), &descriptorPoolCreateInfo, NULL, &mDescriptorPool);



		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = mDescriptorPool; // pool to allocate from.
		descriptorSetAllocateInfo.descriptorSetCount = 1; // allocate a single descriptor set.
		descriptorSetAllocateInfo.pSetLayouts = &mDescriptorSetLayout;

		vkAllocateDescriptorSets(wDevice->getDevice(), &descriptorSetAllocateInfo, &mDescriptorSet);
	

	
		std::vector<VkWriteDescriptorSet> DescriptorSet;
		DescriptorSet.resize(BufferSize);
		for (size_t i = 0; i < BufferSize; i++)
		{
			DescriptorSet[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			DescriptorSet[i].dstSet = mDescriptorSet; // write to this descriptor set.
			DescriptorSet[i].dstBinding = i; // write to the first, and only binding.
			DescriptorSet[i].descriptorCount = 1; // update a single descriptor.
			DescriptorSet[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // storage buffer.
			DescriptorSet[i].pBufferInfo = (*CalculateStructS)[i].mBufferInfo;
		}

		vkUpdateDescriptorSets(wDevice->getDevice(), BufferSize, DescriptorSet.data(), 0, NULL);                                //*******************************************



		uint32_t filelength;
		uint32_t* code = readFile(filelength, Comp);
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pCode = code;
		createInfo.codeSize = filelength;

		vkCreateShaderModule(wDevice->getDevice(), &createInfo, NULL, &mShaderModule);
		delete[] code;



		VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageCreateInfo.module = mShaderModule;
		shaderStageCreateInfo.pName = "main";

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;
		vkCreatePipelineLayout(wDevice->getDevice(), &pipelineLayoutCreateInfo, NULL, &mPipelineLayout);

		VkComputePipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stage = shaderStageCreateInfo;
		pipelineCreateInfo.layout = mPipelineLayout;

		vkCreateComputePipelines(wDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &mPipeline);
	}

	Calculate::~Calculate()
	{
		delete mCommandBuffer;
		delete mCommandPool;

		vkDestroyPipelineLayout(wDevice->getDevice(), mPipelineLayout, nullptr);
		vkDestroyPipeline(wDevice->getDevice(), mPipeline, nullptr);
		vkDestroyShaderModule(wDevice->getDevice(), mShaderModule, nullptr);
		vkFreeDescriptorSets(wDevice->getDevice(), mDescriptorPool, 1, &mDescriptorSet);
		vkDestroyDescriptorSetLayout(wDevice->getDevice(), mDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(wDevice->getDevice(), mDescriptorPool, nullptr);
	}


	void Calculate::begin() {
		mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		mCommandBuffer->bindGraphicPipeline(mPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);//设置计算管线
		mCommandBuffer->bindDescriptorSet(mPipelineLayout, mDescriptorSet, VK_PIPELINE_BIND_POINT_COMPUTE);//获取描述符
	}

	void Calculate::end() {
		mCommandBuffer->end();
	}
}