#include "GIF.h"

namespace GAME {
	GIF::GIF(VulKan::Device* device, GifPipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* RenderPass, unsigned int FrameS)
	{
		mGifPipeline = pipeline;
		mSwapChain = swapChain;
		mDevice = device;
		mRenderPass = RenderPass;

		mCommandPool = new VulKan::CommandPool*[mSwapChain->getImageCount()];
		mCommandBuffer = new VulKan::CommandBuffer*[mSwapChain->getImageCount()];
		for (size_t i = 0; i < mSwapChain->getImageCount(); ++i)
		{
			mCommandPool[i] = new VulKan::CommandPool(device);
			mCommandBuffer[i] = new VulKan::CommandBuffer(device, mCommandPool[i], true);
		}

		std::vector<float> mPositions = {
			-16.0f, -8.0f, 0.0f,
			16.0f, -8.0f, 0.0f,
			16.0f, 8.0f, 0.0f,
			-16.0f, 8.0f, 0.0f,
		};

		mUniform.chuang = 1.0f / FrameS;
		mUniform.zhen = 0;

		std::vector<float> mUVs = {
			0.0f,1.0f,
			(1.0f / FrameS),1.0f,
			(1.0f / FrameS),0.0f,
			0.0f,0.0f,
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
	}

	GIF::~GIF()
	{
	}

	void GIF::initUniformManager(const char* path, std::vector<VulKan::Buffer*> VPMstdBuffer, VulKan::Sampler* sampler) {

		mtexture = new Texture(mDevice, mCommandPool[0], path, sampler);

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
		objectParam->mSize = sizeof(ObjectUniformGIF);
		objectParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;

		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			VulKan::Buffer* buffer = VulKan::Buffer::createUniformBuffer(mDevice, objectParam->mSize, nullptr);
			objectParam->mBuffers.push_back(buffer);
		}

		mUniformParams.push_back(objectParam);

		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 2;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam->mTexture = mtexture;

		mUniformParams.push_back(textureParam);

		//各种类型申请多少个
		mDescriptorPool = new VulKan::DescriptorPool(mDevice);
		mDescriptorPool->build(mUniformParams, mSwapChain->getImageCount());

		//将申请的各种类型按照Layout绑定起来
		mDescriptorSet = new VulKan::DescriptorSet(mDevice, mUniformParams, mGifPipeline->GetPipeline()->DescriptorSetLayout, mDescriptorPool, mSwapChain->getImageCount());


		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(8, 16, 0.0f));//位移矩阵
		SetGamePlayerMatrix(mUniform.mModelMatrix, mSwapChain->getImageCount(), true);
	}

	void GIF::SetGamePlayerMatrix(glm::mat4 Matrix, const int& frameCount, bool mode)
	{
		mUniform.mModelMatrix = Matrix;
		if (mode) {
			for (int i = 0; i < frameCount; ++i) {
				mUniformParams[1]->mBuffers[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniformGIF));
			}
		}
		else {
			mUniformParams[1]->mBuffers[frameCount]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniformGIF));
		}
	}

	void GIF::SetFrame(unsigned int frame, const int& frameCount) {
		mUniform.zhen = frame;
		for (int i = 0; i < frameCount; ++i) {
			mUniformParams[1]->mBuffers[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniformGIF));
		}
	}

	void GIF::UpDataCommandBuffer() {
		for (size_t i = 0; i < 3; ++i)
		{
			VkCommandBufferInheritanceInfo InheritanceInfo{};
			InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			InheritanceInfo.renderPass = mRenderPass->getRenderPass();
			InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);



			mCommandBuffer[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, InheritanceInfo);//开始录制二级指令
			mCommandBuffer[i]->bindGraphicPipeline(mGifPipeline->GetPipeline()->getPipeline());//获得渲染管线

			mCommandBuffer[i]->bindDescriptorSet(mGifPipeline->GetPipeline()->getLayout(), getDescriptorSet(i));//获得 模型位置数据， 贴图数据，……
			mCommandBuffer[i]->bindVertexBuffer(getVertexBuffers());//获取顶点数据，UV值
			mCommandBuffer[i]->bindIndexBuffer(getIndexBuffer()->getBuffer());//获得顶点索引
			mCommandBuffer[i]->drawIndex(getIndexCount());//获取绘画物体的顶点个数

			mCommandBuffer[i]->end();
		}
	}

	GifPipeline::GifPipeline(unsigned int mHeight, unsigned int mWidth, VulKan::Device* Device, VulKan::RenderPass* RenderPass)
	{
		mRenderPass = RenderPass;
		mDevice = Device;

		mPipeline = new VulKan::Pipeline(mDevice, mRenderPass);

		//设置视口
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float)mHeight;
		viewport.width = (float)mWidth;
		viewport.height = -(float)mHeight;
		viewport.minDepth = 0.0f;//最近显示为 0 的物体
		viewport.maxDepth = 1.0f;//最远显示为 1 的物体（最远为 1 ）

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };//偏移
		scissor.extent = { mWidth, mHeight };//剪裁大小

		mPipeline->setViewports({ viewport });
		mPipeline->setScissors({ scissor });


		//设置shader
		std::vector<VulKan::Shader*> shaderGroup{};

		VulKan::Shader* shaderVertex = new VulKan::Shader(mDevice, GifVert, VK_SHADER_STAGE_VERTEX_BIT, "main");
		shaderGroup.push_back(shaderVertex);

		VulKan::Shader* shaderFragment = new VulKan::Shader(mDevice, GifFrag, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
		shaderGroup.push_back(shaderFragment);

		mPipeline->setShaderGroup(shaderGroup);

		//顶点的排布模式
		std::vector<VkVertexInputBindingDescription> vertexBindingDes{};
		vertexBindingDes.resize(2);
		vertexBindingDes[0].binding = 0;
		vertexBindingDes[0].stride = sizeof(float) * 3;
		vertexBindingDes[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		vertexBindingDes[1].binding = 1;
		vertexBindingDes[1].stride = sizeof(float) * 2;
		vertexBindingDes[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributeDes{};
		attributeDes.resize(2);
		attributeDes[0].binding = 0;
		attributeDes[0].location = 0;
		attributeDes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDes[0].offset = 0;
		attributeDes[1].binding = 1;
		attributeDes[1].location = 2;
		attributeDes[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDes[1].offset = 0;


		//顶点的排布模式
		mPipeline->mVertexInputState.vertexBindingDescriptionCount = vertexBindingDes.size();
		mPipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
		mPipeline->mVertexInputState.vertexAttributeDescriptionCount = attributeDes.size();
		mPipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

		//图元装配
		mPipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mPipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//说明那些 MVP 变换完的点，组成什么（这里是组成离散的三角形）
		mPipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		//光栅化设置
		mPipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mPipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;//其他模式需要开启gpu特性 // VK_POLYGON_MODE_LINE 线框 // VK_POLYGON_MODE_FILL 正常画面
		mPipeline->mRasterState.lineWidth = 1.0f;//大于1需要开启gpu特性
		mPipeline->mRasterState.cullMode = VK_CULL_MODE_BACK_BIT;//开启背面剔除
		mPipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//设置逆时针为正面

		mPipeline->mRasterState.depthBiasEnable = VK_FALSE; //是否开启深度在处理（比如深度后一点 ，前一点）
		mPipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		mPipeline->mRasterState.depthBiasClamp = 0.0f;
		mPipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		//多重采样
		mPipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		mPipeline->mSampleState.rasterizationSamples = mDevice->getMaxUsableSampleCount();//采样的Bit数
		mPipeline->mSampleState.minSampleShading = 1.0f;
		mPipeline->mSampleState.pSampleMask = nullptr;
		mPipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		mPipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		//深度与模板测试
		mPipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		mPipeline->mDepthStencilState.depthWriteEnable = VK_TRUE;
		mPipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		//颜色混合

		//这个是颜色混合掩码，得到的混合结果，按照通道与掩码进行AND操作，输出
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |//开启的颜色通道
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		blendAttachment.blendEnable = VK_TRUE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		mPipeline->pushBlendAttachment(blendAttachment);

		//1 blend有两种计算方式，第一种如上所述，进行alpha为基础的计算，第二种进行位运算
		//2 如果开启了logicOp，那么上方设置的alpha为基础的运算，失灵
		//3 ColorWrite掩码，仍然有效，即便开启了logicOP
		//4 因为，我们可能会有多个FrameBuffer输出，所以可能需要多个BlendAttachment
		mPipeline->mBlendState.logicOpEnable = VK_FALSE;
		mPipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;

		//配合blendAttachment的factor与operation
		mPipeline->mBlendState.blendConstants[0] = 0.0f;
		mPipeline->mBlendState.blendConstants[1] = 0.0f;
		mPipeline->mBlendState.blendConstants[2] = 0.0f;
		mPipeline->mBlendState.blendConstants[3] = 0.0f;




		//uniform的传递 就是一些临时信息，比如当前摄像机的方向，（在下一帧使用的信息）
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
		layoutBindings.resize(3);
		layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//绑定类型
		layoutBindings[0].binding = 0;//那个Binding通道
		layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBindings[0].descriptorCount = 1;//多少个数据

		layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//绑定类型
		layoutBindings[1].binding = 1;//那个Binding通道
		layoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBindings[1].descriptorCount = 1;//多少个数据

		layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;//绑定类型
		layoutBindings[2].binding = 2;//那个Binding通道
		layoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBindings[2].descriptorCount = 1;//多少个数据

		//uniform的传递
		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		createInfo.pBindings = layoutBindings.data();

		//VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;

		if (vkCreateDescriptorSetLayout(mDevice->getDevice(), &createInfo, nullptr, &mPipeline->DescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayout;
		mDescriptorSetLayout.push_back(mPipeline->DescriptorSetLayout);

		mPipeline->mLayoutState.setLayoutCount = mDescriptorSetLayout.size();
		mPipeline->mLayoutState.pSetLayouts = mDescriptorSetLayout.data();
		//mPipeline->mLayoutState.pSetLayouts = &mPipeline->DescriptorSetLayout;
		mPipeline->mLayoutState.pushConstantRangeCount = 0;
		mPipeline->mLayoutState.pPushConstantRanges = nullptr;



		mPipeline->build();
	}

	GifPipeline::~GifPipeline()
	{
		delete mPipeline;
	}
}
