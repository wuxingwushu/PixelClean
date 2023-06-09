#include "Labyrinth.h"




namespace GAME {


	void Labyrinth_SetPixel(int x, int y, void* mclass) {
		Labyrinth* mClass = (Labyrinth*)mclass;
		mClass->SetPixel(x, y, 16);
	}


	Labyrinth::Labyrinth(VulKan::Device* device, int X, int Y)
	{
		mDevice = device;

		numberX = ((X / 2) * 4) + 1;
		numberY = ((Y / 2) * 4) + 1;


		

		ThreadS = 3;
		mThreadCommandPoolS = new VulKan::CommandPool * [ThreadS];
		mThreadCommandBufferS = new VulKan::CommandBuffer * [ThreadS];
		for (int i = 0; i < (ThreadS); i++) {
			mThreadCommandPoolS[i] = new VulKan::CommandPool(device);
			mThreadCommandBufferS[i] = new VulKan::CommandBuffer(device, mThreadCommandPoolS[i], true);
		}
		

		BlockS = new bool* [numberX];
		BlockTypeS = new unsigned int* [numberX];
		for (size_t i = 0; i < numberX; i++)
		{
			BlockS[i] = new bool[numberY];
			BlockTypeS[i] = new unsigned int[numberY];
		}

		bool** lblockS = new bool* [X];
		for (size_t i = 0; i < X; i++)
		{
			lblockS[i] = new bool[Y];
		}
		GenerateMaze(lblockS, X, Y);
		int NX, NY;
		for (size_t x = 0; x < numberX; x++)
		{
			for (size_t y = 0; y < numberY; y++)
			{
				NX = (x / 4) * 2;
				if ((x%4) != 0) {
					NX++;
				}
				NY = (y / 4) * 2;
				if ((y % 4) != 0) {
					NY++;
				}
				BlockS[x][y] = lblockS[NX][NY];
			}
		}

		
		

		mPixelS = new unsigned char[numberX * 16 * numberY * 16 * 4];
		mMistS = new unsigned char[numberX * 16 * numberY * 16 * 4];

		unsigned char Mist[4] = { 0,0,0,230 };
		//std::cout << *((int*)Mist) << std::endl;
		for (size_t i = 0; i < (numberX * 16 * numberY * 16); i++)
		{
			memcpy(&mMistS[i * 4], Mist, 4);
		}

		//Mist[3] = 231;
		//std::cout << *((int*)Mist) << std::endl;

		mFixedSizeTerrain = new SquarePhysics::FixedSizeTerrain(numberX * 16, numberY * 16, 1);
		mFixedSizeTerrain->SetCollisionCallback(Labyrinth_SetPixel, this);
		SquarePhysics::PixelAttribute** LPixelAttribute = mFixedSizeTerrain->GetPixelAttributeData();


		for (size_t x = 0; x < numberX; x++)
		{
			for (size_t y = 0; y < numberY; y++)
			{
				if (BlockS[x][y]) {
					for (size_t i = 0; i < 16; i++)
					{
						memcpy(&mPixelS[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4)], &pixelS[2][(16 * 4 * i)], 16 * 4);
						for (size_t idd = 0; idd < 16; idd++)
						{
							LPixelAttribute[x * 16 + idd][y * 16 + i].Collision = true;
							mMistS[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4) + (idd * 4) + 3] = 231;
						}
					}
				}
				else {
					for (size_t i = 0; i < 16; i++)
					{
						memcpy(&mPixelS[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4)], &pixelS[1][(16 * 4 * i)], 16 * 4);
					}
				}
			}
		}


		



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

		mPositionBuffer = VulKan::Buffer::createVertexBuffer(device, 12 * sizeof(float), mPositions);

		mUVBuffer = VulKan::Buffer::createVertexBuffer(device, 8 * sizeof(float), mUVs);

		mIndexBuffer = VulKan::Buffer::createIndexBuffer(device, 6 * sizeof(float), mIndexDatas);

		mIndexDatasSize = 6;

		InitMist();

		wymiwustruct.size = 1800;
		wymiwustruct.y_size = numberY * 16;
		Mist[3] = 255;
		Mist[0] = 255;
		wymiwustruct.Color = *((int*)Mist);
		//wymiwustruct.Color = -460365120;
	}

	Labyrinth::~Labyrinth()
	{
	}


	void Labyrinth::initUniformManager(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		int frameCount,
		const VkDescriptorSetLayout mDescriptorSetLayout,
		std::vector<VulKan::Buffer*> VPMstdBuffer,
		VulKan::Sampler* sampler
	)
	{

		PixelTextureS = new PixelTexture(device, mThreadCommandPoolS[0], mPixelS, numberX * 16, numberY * 16, 4, sampler);
		
		WarfareMist = new PixelTexture(device, mThreadCommandPoolS[0], mMistS, numberX * 16, numberY * 16, 4, sampler);
		static int asdad = NULL;

		ObjectUniform mUniform;
		
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

		for (int i = 0; i < frameCount; ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(device, objectParam->mSize, nullptr);
			objectParam->mBuffers.push_back(buffer);
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-8, 8, 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			buffer->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
		}

		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam->mPixelTexture = PixelTextureS;

		mUniformParams.push_back(textureParam);

		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParams, frameCount,2);
		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(device, mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);
		mUniformParams.clear();
		mUniformParams.push_back(vpParam);
		mUniformParams.push_back(objectParam);
		textureParam->mPixelTexture = WarfareMist;
		mUniformParams.push_back(textureParam);
		mMistDescriptorSet = new VulKan::DescriptorSet(device, mUniformParams, mDescriptorSetLayout, mDescriptorPool, frameCount);
	}

	void Labyrinth::ThreadUpdateCommandBuffer() {
		std::vector<std::future<void>> pool;
		int UpdateNumber = (numberX * numberY) / (ThreadS / 3);
		int UpdateNumber_yu = (numberX * numberY) % (ThreadS / 3);
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < UpdateNumber_yu; j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&Labyrinth::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + j), (UpdateNumber + 1)));
			}
			for (int j = UpdateNumber_yu; j < (ThreadS / 3); j++) {
				pool.push_back(TOOL::mThreadPool->enqueue(&Labyrinth::ThreadCommandBufferToUpdate, this, i, j, ((UpdateNumber * j) + UpdateNumber_yu), UpdateNumber));
			}
		}
		for (int i = 0; i < (ThreadS); i++) {
			pool[i].wait();
		}
	}

	void Labyrinth::ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count)
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
		commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
		commandbuffer->drawIndex(getIndexCount());//获取绘画物体的顶点个数
		commandbuffer->bindDescriptorSet(mPipeline->getLayout(), mMistDescriptorSet->getDescriptorSet(FrameCount));//获得 模型位置数据， 贴图数据，……
		commandbuffer->drawIndex(getIndexCount());//获取绘画物体的顶点个数
		commandbuffer->end();
	}
	//左下坐标，图片ID
	void Labyrinth::penzhang(unsigned int x, unsigned int y, unsigned int ID) {
		BlockS[x][y] = 0;
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		for (size_t i = 0; i < 16; i++)
		{
			memcpy(&TexturePointer[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4)], &pixelS[ID][(16 * 4 * i)], 16 * 4);
			for (size_t idd = 0; idd < 16; idd++)
			{
				TextureMist[(((x * 16) + i) * numberY * 16 * 4) + (y * 16 * 4) + (idd * 4) + 3] = 230;
			}
		}
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
	}

	//左下坐标，左下坐标，图片ID
	void Labyrinth::SetPixel(unsigned int x, unsigned int y, unsigned int Dx, unsigned int Dy, unsigned int ID) {
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		memcpy(&TexturePointer[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4)], &pixelS[ID][(16 * 4 * Dx) + (Dy * 4)], 4);
		TextureMist[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4) + 3] = 230;
		mMistS[(((x * 16) + Dx) * numberY * 16 * 4) + (((y * 16) + Dy) * 4) + 3] = 230;
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
	}

	void Labyrinth::SetPixel(unsigned int x, unsigned int y, unsigned int ID) {
		unsigned char* TexturePointer = (unsigned char*)PixelTextureS->getHOSTImagePointer();
		unsigned char* TextureMist = (unsigned char*)WarfareMist->getHOSTImagePointer();
		memcpy(&TexturePointer[(x * numberY * 16 * 4) + (y * 4)], &pixelS[ID][(((y % 16) * 16) + (x % 16)) * 4], 4);
		TextureMist[(x * numberY * 16 * 4) + (y * 4) + 3] = 230;
		mMistS[(x * numberY * 16 * 4) + (y * 4) + 3] = 230;
		PixelTextureS->endHOSTImagePointer();
		WarfareMist->endHOSTImagePointer();
		UpDateMapsSwitch = true;
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
			mMistCommandPoolS
		);

		VkBufferCopy copyInfo{};
		copyInfo.size = (numberX * numberY * 16 * 16 * 4);
		mMistCommandBufferS->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		mMistCommandBufferS->copyBufferToBuffer(WarfareMist->getHOSTImageBuffer(), jihsuanTP->getBuffer(), 1, { copyInfo });
		mMistCommandBufferS->bindGraphicPipeline(pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
		mMistCommandBufferS->bindDescriptorSet(pipelineLayout, descriptorSet, VK_PIPELINE_BIND_POINT_COMPUTE);
		vkCmdDispatch(mMistCommandBufferS->getCommandBuffer(), (uint32_t)ceil((wymiwustruct.size) / float(64)), 1, 1);
		mMistCommandBufferS->copyBufferToImage(
			jihsuanTP->getBuffer(),
			WarfareMist->getImage()->getImage(),
			WarfareMist->getImage()->getLayout(),
			WarfareMist->getImage()->getWidth(),
			WarfareMist->getImage()->getHeight()
		);
		mMistCommandBufferS->end();
		mMistCommandBufferS->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

		WarfareMist->getImage()->setImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			region,
			mMistCommandPoolS
		);
	}


	void Labyrinth::InitMist() {
		jihsuanTP = new GAME::VulKan::Buffer(mDevice, (numberX * numberY * 16 * 16 * 4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);

		information = new GAME::VulKan::Buffer(mDevice, sizeof(miwustruct), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE);

		mMistCommandPoolS = new VulKan::CommandPool(mDevice);
		mMistCommandBufferS = new VulKan::CommandBuffer(mDevice, mMistCommandPoolS);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[2] = {};
		descriptorSetLayoutBinding[0].binding = 0; // binding = 0
		descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorSetLayoutBinding[0].descriptorCount = 1;
		descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		descriptorSetLayoutBinding[1].binding = 1; // binding = 0
		descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorSetLayoutBinding[1].descriptorCount = 1;
		descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = 2; // only a single binding in this descriptor set layout. //*******************************************
		descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBinding;                                   //*******************************************

		// Create the descriptor set layout. 
		vkCreateDescriptorSetLayout(mDevice->getDevice(), &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout);

		VkDescriptorPoolSize descriptorPoolSize[2] = {};                                                        //*******************************************
		descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorPoolSize[0].descriptorCount = 1;

		descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;                                         //*******************************************
		descriptorPoolSize[1].descriptorCount = 1;                                                              //*******************************************


		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.maxSets = 1; // we only need to allocate one descriptor set from the pool.
		descriptorPoolCreateInfo.poolSizeCount = 2;                                                             //*******************************************
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

		VkWriteDescriptorSet writeDescriptorSet[2] = {};                                                //*******************************************
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


		// perform the update of the descriptor set.
		vkUpdateDescriptorSets(mDevice->getDevice(), 2, writeDescriptorSet, 0, NULL);                                //*******************************************

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
		//vkFreeDescriptorSets(mDevice, descriptorPool, 1, &descriptorSet);
		vkDestroyDescriptorSetLayout(mDevice->getDevice(), descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(mDevice->getDevice(), descriptorPool, nullptr);
		delete jihsuanTP;
		delete information;
		delete mMistCommandBufferS;
		delete mMistCommandPoolS;
	}
}