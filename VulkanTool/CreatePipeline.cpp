#include "CreatePipeline.h"
#include "../GlobalVariable.h"

namespace VulKan {

	Pipeline* MainPipeline(Pipeline* Pipeline, Device* Device) {
		//设置视口
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float)Global::mHeight;
		viewport.width = (float)Global::mWidth;
		viewport.height = -(float)Global::mHeight;
		viewport.minDepth = 0.0f;//最近显示为 0 的物体
		viewport.maxDepth = 1.0f;//最远显示为 1 的物体（最远为 1 ）

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };//偏移
		scissor.extent = { Global::mWidth, Global::mHeight };//剪裁大小

		Pipeline->setViewports({ viewport });
		Pipeline->setScissors({ scissor });


		//设置shader
		std::vector<VulKan::Shader*> shaderGroup{};

		VulKan::Shader* shaderVertex = new VulKan::Shader(Device, vs_spv, VK_SHADER_STAGE_VERTEX_BIT, "main");
		shaderGroup.push_back(shaderVertex);

		VulKan::Shader* shaderFragment = new VulKan::Shader(Device, fs_spv, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
		shaderGroup.push_back(shaderFragment);

		Pipeline->setShaderGroup(shaderGroup);

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
		Pipeline->mVertexInputState.vertexBindingDescriptionCount = vertexBindingDes.size();
		Pipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
		Pipeline->mVertexInputState.vertexAttributeDescriptionCount = attributeDes.size();
		Pipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

		//图元装配
		Pipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		Pipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//说明那些 MVP 变换完的点，组成什么（这里是组成离散的三角形）
		Pipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		//光栅化设置
		Pipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		Pipeline->mRasterState.polygonMode = Global::DrawLinesMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;//其他模式需要开启gpu特性 // VK_POLYGON_MODE_LINE 线框 // VK_POLYGON_MODE_FILL 正常画面
		Pipeline->mRasterState.lineWidth = 1.0f;//大于1需要开启gpu特性
		Pipeline->mRasterState.cullMode = VK_CULL_MODE_BACK_BIT;//开启背面剔除
		Pipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//设置逆时针为正面

		Pipeline->mRasterState.depthBiasEnable = VK_FALSE; //是否开启深度在处理（比如深度后一点 ，前一点）
		Pipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		Pipeline->mRasterState.depthBiasClamp = 0.0f;
		Pipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		//多重采样
		Pipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		Pipeline->mSampleState.rasterizationSamples = Device->getMaxUsableSampleCount();//采样的Bit数
		Pipeline->mSampleState.minSampleShading = 1.0f;
		Pipeline->mSampleState.pSampleMask = nullptr;
		Pipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		Pipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		//深度与模板测试
		Pipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		Pipeline->mDepthStencilState.depthWriteEnable = VK_TRUE;
		Pipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

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

		Pipeline->pushBlendAttachment(blendAttachment);

		//1 blend有两种计算方式，第一种如上所述，进行alpha为基础的计算，第二种进行位运算
		//2 如果开启了logicOp，那么上方设置的alpha为基础的运算，失灵
		//3 ColorWrite掩码，仍然有效，即便开启了logicOP
		//4 因为，我们可能会有多个FrameBuffer输出，所以可能需要多个BlendAttachment
		Pipeline->mBlendState.logicOpEnable = VK_FALSE;
		Pipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;

		//配合blendAttachment的factor与operation
		Pipeline->mBlendState.blendConstants[0] = 0.0f;
		Pipeline->mBlendState.blendConstants[1] = 0.0f;
		Pipeline->mBlendState.blendConstants[2] = 0.0f;
		Pipeline->mBlendState.blendConstants[3] = 0.0f;




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

		if (vkCreateDescriptorSetLayout(Device->getDevice(), &createInfo, nullptr, &Pipeline->DescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayout;
		mDescriptorSetLayout.push_back(Pipeline->DescriptorSetLayout);

		Pipeline->mLayoutState.setLayoutCount = mDescriptorSetLayout.size();
		Pipeline->mLayoutState.pSetLayouts = mDescriptorSetLayout.data();
		//mPipeline->mLayoutState.pSetLayouts = &mPipeline->DescriptorSetLayout;
		Pipeline->mLayoutState.pushConstantRangeCount = 0;
		Pipeline->mLayoutState.pPushConstantRanges = nullptr;



		Pipeline->build();

		return Pipeline;
	}


	Pipeline* LinePipeline(Pipeline* Pipeline, Device* Device) {
		//设置视口
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float)Global::mHeight;
		viewport.width = (float)Global::mWidth;
		viewport.height = -(float)Global::mHeight;
		viewport.minDepth = 0.0f;//最近显示为 0 的物体
		viewport.maxDepth = 1.0f;//最远显示为 1 的物体（最远为 1 ）

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };//偏移
		scissor.extent = { Global::mWidth, Global::mHeight };//剪裁大小

		Pipeline->setViewports({ viewport });
		Pipeline->setScissors({ scissor });


		//设置shader
		std::vector<VulKan::Shader*> shaderGroup{};

		VulKan::Shader* shaderVertex = new VulKan::Shader(Device, LineV_spv, VK_SHADER_STAGE_VERTEX_BIT, "main");
		shaderGroup.push_back(shaderVertex);

		VulKan::Shader* shaderFragment = new VulKan::Shader(Device, LineF_spv, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
		shaderGroup.push_back(shaderFragment);

		Pipeline->setShaderGroup(shaderGroup);

		//顶点的排布模式
		std::vector<VkVertexInputBindingDescription> vertexBindingDes{};
		vertexBindingDes.resize(1);
		vertexBindingDes[0].binding = 0;
		vertexBindingDes[0].stride = sizeof(float) * (3 + 4);//线段， 颜色
		vertexBindingDes[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributeDes{};
		attributeDes.resize(2);
		attributeDes[0].binding = 0;
		attributeDes[0].location = 0;//线段头
		attributeDes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDes[0].offset = 0;

		attributeDes[1].binding = 0;
		attributeDes[1].location = 1;//颜色
		attributeDes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDes[1].offset = sizeof(float) * 3;


		//顶点的排布模式
		Pipeline->mVertexInputState.vertexBindingDescriptionCount = vertexBindingDes.size();
		Pipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
		Pipeline->mVertexInputState.vertexAttributeDescriptionCount = attributeDes.size();
		Pipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

		//图元装配
		Pipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		Pipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;//说明那些 MVP 变换完的点，组成什么（这里是组成离散的线段）
		Pipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		//光栅化设置
		Pipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		Pipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;//其他模式需要开启gpu特性 // VK_POLYGON_MODE_LINE 线框 // VK_POLYGON_MODE_FILL 正常画面
		Pipeline->mRasterState.lineWidth = 1.0f;//大于1需要开启gpu特性
		Pipeline->mRasterState.cullMode = VK_CULL_MODE_NONE;//不剔除
		Pipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//设置逆时针为正面

		Pipeline->mRasterState.depthBiasEnable = VK_FALSE; //是否开启深度在处理（比如深度后一点 ，前一点）
		Pipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		Pipeline->mRasterState.depthBiasClamp = 0.0f;
		Pipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		//多重采样
		Pipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		Pipeline->mSampleState.rasterizationSamples = Device->getMaxUsableSampleCount();//采样的Bit数
		Pipeline->mSampleState.minSampleShading = 1.0f;
		Pipeline->mSampleState.pSampleMask = nullptr;
		Pipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		Pipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		//深度与模板测试
		Pipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		Pipeline->mDepthStencilState.depthWriteEnable = VK_TRUE;
		Pipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		//颜色混合

		//这个是颜色混合掩码，得到的混合结果，按照通道与掩码进行AND操作，输出
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |//开启的颜色通道
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		blendAttachment.blendEnable = VK_FALSE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		Pipeline->pushBlendAttachment(blendAttachment);

		//1 blend有两种计算方式，第一种如上所述，进行alpha为基础的计算，第二种进行位运算
		//2 如果开启了logicOp，那么上方设置的alpha为基础的运算，失灵
		//3 ColorWrite掩码，仍然有效，即便开启了logicOP
		//4 因为，我们可能会有多个FrameBuffer输出，所以可能需要多个BlendAttachment
		Pipeline->mBlendState.logicOpEnable = VK_FALSE;
		Pipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;

		//配合blendAttachment的factor与operation
		Pipeline->mBlendState.blendConstants[0] = 0.0f;
		Pipeline->mBlendState.blendConstants[1] = 0.0f;
		Pipeline->mBlendState.blendConstants[2] = 0.0f;
		Pipeline->mBlendState.blendConstants[3] = 0.0f;




		//uniform的传递 就是一些临时信息，比如当前摄像机的方向，（在下一帧使用的信息）
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
		layoutBindings.resize(1);
		layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//绑定类型
		layoutBindings[0].binding = 0;//那个Binding通道
		layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBindings[0].descriptorCount = 1;//多少个数据

		//uniform的传递
		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		createInfo.pBindings = layoutBindings.data();

		//VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;

		if (vkCreateDescriptorSetLayout(Device->getDevice(), &createInfo, nullptr, &Pipeline->DescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayout;
		mDescriptorSetLayout.push_back(Pipeline->DescriptorSetLayout);

		Pipeline->mLayoutState.setLayoutCount = mDescriptorSetLayout.size();
		Pipeline->mLayoutState.pSetLayouts = mDescriptorSetLayout.data();
		Pipeline->mLayoutState.pushConstantRangeCount = 0;
		Pipeline->mLayoutState.pPushConstantRanges = nullptr;



		Pipeline->build();

		return Pipeline;
	}

	Pipeline* SpotPipeline(Pipeline* Pipeline, Device* Device) {
		//设置视口
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float)Global::mHeight;
		viewport.width = (float)Global::mWidth;
		viewport.height = -(float)Global::mHeight;
		viewport.minDepth = 0.0f;//最近显示为 0 的物体
		viewport.maxDepth = 1.0f;//最远显示为 1 的物体（最远为 1 ）

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };//偏移
		scissor.extent = { Global::mWidth, Global::mHeight };//剪裁大小

		Pipeline->setViewports({ viewport });
		Pipeline->setScissors({ scissor });


		//设置shader
		std::vector<VulKan::Shader*> shaderGroup{};

		VulKan::Shader* shaderVertex = new VulKan::Shader(Device, SpotV_spv, VK_SHADER_STAGE_VERTEX_BIT, "main");
		shaderGroup.push_back(shaderVertex);

		VulKan::Shader* shaderGeometry = new VulKan::Shader(Device, SpotNormal_spv, VK_SHADER_STAGE_GEOMETRY_BIT, "main");
		shaderGroup.push_back(shaderGeometry);

		VulKan::Shader* shaderFragment = new VulKan::Shader(Device, SpotF_spv, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
		shaderGroup.push_back(shaderFragment);

		Pipeline->setShaderGroup(shaderGroup);

		//顶点的排布模式
		std::vector<VkVertexInputBindingDescription> vertexBindingDes{};
		vertexBindingDes.resize(1);
		vertexBindingDes[0].binding = 0;
		vertexBindingDes[0].stride = sizeof(float) * (3 + 4);//线段， 颜色
		vertexBindingDes[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributeDes{};
		attributeDes.resize(2);
		attributeDes[0].binding = 0;
		attributeDes[0].location = 0;//线段头
		attributeDes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDes[0].offset = 0;

		attributeDes[1].binding = 0;
		attributeDes[1].location = 1;//颜色
		attributeDes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDes[1].offset = sizeof(float) * 3;


		//顶点的排布模式
		Pipeline->mVertexInputState.vertexBindingDescriptionCount = vertexBindingDes.size();
		Pipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
		Pipeline->mVertexInputState.vertexAttributeDescriptionCount = attributeDes.size();
		Pipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

		//图元装配
		Pipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		Pipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;//说明那些 MVP 变换完的点，组成什么（这里是组成离散的点）
		Pipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		//光栅化设置
		Pipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		Pipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;//其他模式需要开启gpu特性 // VK_POLYGON_MODE_LINE 线框 // VK_POLYGON_MODE_FILL 正常画面
		Pipeline->mRasterState.lineWidth = 1.0f;//大于1需要开启gpu特性
		Pipeline->mRasterState.cullMode = VK_CULL_MODE_NONE;//不剔除
		Pipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//设置逆时针为正面

		Pipeline->mRasterState.depthBiasEnable = VK_FALSE; //是否开启深度在处理（比如深度后一点 ，前一点）
		Pipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		Pipeline->mRasterState.depthBiasClamp = 0.0f;
		Pipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		//多重采样
		Pipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		Pipeline->mSampleState.rasterizationSamples = Device->getMaxUsableSampleCount();//采样的Bit数
		Pipeline->mSampleState.minSampleShading = 1.0f;
		Pipeline->mSampleState.pSampleMask = nullptr;
		Pipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		Pipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		//深度与模板测试
		Pipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		Pipeline->mDepthStencilState.depthWriteEnable = VK_TRUE;
		Pipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		//颜色混合

		//这个是颜色混合掩码，得到的混合结果，按照通道与掩码进行AND操作，输出
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |//开启的颜色通道
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		blendAttachment.blendEnable = VK_FALSE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		Pipeline->pushBlendAttachment(blendAttachment);

		//1 blend有两种计算方式，第一种如上所述，进行alpha为基础的计算，第二种进行位运算
		//2 如果开启了logicOp，那么上方设置的alpha为基础的运算，失灵
		//3 ColorWrite掩码，仍然有效，即便开启了logicOP
		//4 因为，我们可能会有多个FrameBuffer输出，所以可能需要多个BlendAttachment
		Pipeline->mBlendState.logicOpEnable = VK_FALSE;
		Pipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;

		//配合blendAttachment的factor与operation
		Pipeline->mBlendState.blendConstants[0] = 0.0f;
		Pipeline->mBlendState.blendConstants[1] = 0.0f;
		Pipeline->mBlendState.blendConstants[2] = 0.0f;
		Pipeline->mBlendState.blendConstants[3] = 0.0f;




		//uniform的传递 就是一些临时信息，比如当前摄像机的方向，（在下一帧使用的信息）
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
		layoutBindings.resize(1);
		layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//绑定类型
		layoutBindings[0].binding = 0;//那个Binding通道
		layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBindings[0].descriptorCount = 1;//多少个数据

		//uniform的传递
		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		createInfo.pBindings = layoutBindings.data();

		//VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;

		if (vkCreateDescriptorSetLayout(Device->getDevice(), &createInfo, nullptr, &Pipeline->DescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayout;
		mDescriptorSetLayout.push_back(Pipeline->DescriptorSetLayout);

		Pipeline->mLayoutState.setLayoutCount = mDescriptorSetLayout.size();
		Pipeline->mLayoutState.pSetLayouts = mDescriptorSetLayout.data();
		Pipeline->mLayoutState.pushConstantRangeCount = 0;
		Pipeline->mLayoutState.pPushConstantRanges = nullptr;



		Pipeline->build();

		return Pipeline;
	}
}