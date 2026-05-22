#include "Calculate.h"
#include "../GeneralCalculationGPU/GPU.h"
#include "../DebugLog.h"

namespace VulKan {

	Calculate::Calculate(Device* Device, std::vector<CalculateStruct>* CalculateStructS, const char* Comp, uint32_t localSizeX)
	{
		LOGD("[Calculate] Constructor");
		wDevice = Device;

		mCommandPool = new CommandPool(wDevice);
		mCommandBuffer = new CommandBuffer(wDevice, mCommandPool);
		

		const int BufferSize = CalculateStructS->size();
		std::vector<VkDescriptorSetLayoutBinding> DescriptorSetLayoutBindingS;
		DescriptorSetLayoutBindingS.resize(BufferSize);
		for (size_t i = 0; i < BufferSize; ++i)
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
		for (size_t i = 0; i < BufferSize; ++i)
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
		for (size_t i = 0; i < BufferSize; ++i)
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
		if (code == nullptr || filelength == 0) {
			LOGE("[Calculate] Failed to read compute shader file: %s", Comp);
			throw std::runtime_error("Error: failed to read compute shader file");
		}

		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pCode = code;
		createInfo.codeSize = filelength;

		vkCreateShaderModule(wDevice->getDevice(), &createInfo, NULL, &mShaderModule);
		delete[] code;



		VkSpecializationMapEntry specializationMapEntry = {};
		specializationMapEntry.constantID = 0;
		specializationMapEntry.offset     = 0;
		specializationMapEntry.size       = sizeof(uint32_t);

		VkSpecializationInfo specializationInfo = {};
		specializationInfo.mapEntryCount = 1;
		specializationInfo.pMapEntries   = &specializationMapEntry;
		specializationInfo.dataSize      = sizeof(uint32_t);
		specializationInfo.pData         = &localSizeX;

		VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageCreateInfo.module = mShaderModule;
		shaderStageCreateInfo.pName = "main";
		shaderStageCreateInfo.pSpecializationInfo = &specializationInfo;

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

	/*
	 * ===================== 共享模式实现 =====================
	 *
	 * 与 begin() 的核心区别：
	 * - begin() 会调用 mCommandBuffer->begin() 开启新的录制，然后绑定管线/描述符
	 * - recordDispatch() 不管理 CommandBuffer 生命周期，仅执行绑定 + dispatch + 恢复状态
	 *
	 * 为什么需要恢复管线/描述符状态？
	 * - Vulkan 中 vkCmdBindPipeline / vkCmdBindDescriptorSets 会修改 CommandBuffer 的绑定状态
	 * - 如果两个不同 Calculate 向同一个 CommandBuffer 写入 dispatch 指令，第二个
	 *   Calculate 的绑定会覆盖第一个的。由于我们在 recordDispatch 内已经完成了绑定 +
	 *   dispatch，当前 CommandBuffer 的状态停留在"最后一个绑定的管线"。这对后续指令
	 *   无影响（后续指令会重新绑定自己的管线），但对调用者来说是确定性的。
	 */
	void Calculate::recordDispatch(
		VkCommandBuffer cmdBuffer,
		uint32_t groupCountX,
		uint32_t groupCountY,
		uint32_t groupCountZ
	) {
		// 绑定此 Calculate 的计算管线到外部 CommandBuffer
		// 注意：这里使用 VK_PIPELINE_BIND_POINT_COMPUTE 而非 GRAPHICS
		vkCmdBindPipeline(
			cmdBuffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			mPipeline
		);

		// 绑定此 Calculate 的描述符集（包含所有 SSBO 的引用）
		// firstSet = 0 表示绑定到 descriptor set #0
		// 30 个 dynamic offset 全部初始化为 0（本类不使用 dynamic uniform/storage buffer）
		vkCmdBindDescriptorSets(
			cmdBuffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			mPipelineLayout,
			0,                         // firstSet
			1,                         // descriptorSetCount
			&mDescriptorSet,
			0,                         // dynamicOffsetCount
			nullptr                    // pDynamicOffsets
		);

		// 执行 Compute Shader dispatch
		vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
	}
}