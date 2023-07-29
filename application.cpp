#include "application.h"

int zuojian;

namespace GAME {

	//ImGui 错误信息回调函数
	static void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	Application::Application() {
		ReadConfigure();
	}

	//总初始化
	void Application::run(VulKan::Window* w) {
		mWindow = w;
		TOOL::mTimer->MomentTiming(u8"总初始化");
		TOOL::mTimer->MomentTiming(u8"初始化窗口");
		initWindow();//初始化窗口
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentTiming(u8"初始化Vulkan");
		initVulkan();//初始化Vulkan
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentTiming(u8"初始化ImGui");
		initImGui();//初始化ImGui
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentTiming(u8"初始化游戏");
		initGame();//初始化游戏
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentEnd();
		mainLoop();//开启主循环main
		StorageConfigure();//储存配置
		cleanUp();//回收资源
	}

	//相机的鼠标事件
	void Application::onMouseMove(double xpos, double ypos) {
		mCamera.onMouseMove(xpos, ypos);
	}

	//相机的键盘事件
	void Application::onKeyDown(CAMERA_MOVE moveDirection) {
		int lidaxiao = 100;
		switch (moveDirection)
		{
		case CAMERA_MOVE::MOVE_LEFT:
			PlayerSpeed.x -= lidaxiao;
			break;
		case CAMERA_MOVE::MOVE_RIGHT:
			PlayerSpeed.x += lidaxiao;
			break;
		case CAMERA_MOVE::MOVE_FRONT:
			PlayerSpeed.y += lidaxiao;
			break;
		case CAMERA_MOVE::MOVE_BACK:
			PlayerSpeed.y -= lidaxiao;
			break;
		default:
			break;
		}
		mGamePlayer->mObjectCollision->SetForce(PlayerSpeed);//设置玩家受力
	}

	//读取配置信息
	void Application::ReadConfigure() {
		mWidth = iniData.Get<int>("Window", "Width");
		mHeight = iniData.Get<int>("Window", "Height");
	}

	//储存配置信息
	void Application::StorageConfigure() {
		iniData.UpdateEntry("Window", "Width", mWidth);//修改
		iniData.UpdateEntry("Window", "Height", mHeight);//修改
		inih::INIWriter::write_Gai(IniPath, iniData);//保存

		//cleanUp();//回收资源
	}

	//窗口的初始化
	void Application::initWindow() {
		//mWindow = new VulKan::Window(mWidth, mHeight, false, false);
		//mWindow->setApp(shared_from_this());//把 application 本体指针传给 Window ，便于调用 Camera
		//设置摄像机   位置，朝向（后面两个 vec3 来决定）
		mCamera.lookAt(glm::vec3(20.0f, 20.0f, 400.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//设置Perpective
		mCamera.setPerpective(45.0f, (float)mWidth / (float)mHeight, 0.1f, 1000.0f);
		//设置摄像机移动速度
		mCamera.setSpeed(0.5f);
		mCamera.setSpeedX(0.5f);
		mCamera.setSpeedY(0.5f);
	}

	//初始化Vulkan
	//1 rendePass 加入pipeline 2 生成FrameBuffer
	void Application::initVulkan() {
		mInstance = new VulKan::Instance(false);//实列化需要的VulKan功能APi
		mWindowSurface = new VulKan::WindowSurface(mInstance, mWindow);//获取你在什么平台运行调用不同的API（比如：Window，Android）
		mDevice = new VulKan::Device(mInstance, mWindowSurface);//获取电脑的硬件设备，vma内存分配器 也在这里被创建了
		mCommandPool = new VulKan::CommandPool(mDevice);//创建指令池，给CommandBuffer用的
		mSwapChain = new VulKan::SwapChain(mDevice, mWindow, mWindowSurface, mCommandPool);//设置VulKan的工作细节
		mWidth = mSwapChain->getExtent().width;
		mHeight = mSwapChain->getExtent().height;
		mRenderPass = new VulKan::RenderPass(mDevice);//创建GPU画布描述
		mImGuuiRenderPass = new VulKan::RenderPass(mDevice);
		mSampler = new VulKan::Sampler(mDevice);//采样器
		createRenderPass();//设置GPU画布描述
		mSwapChain->createFrameBuffers(mRenderPass);//把GPU画布描述传给交换链生成GPU画布

		//创建渲染管线
		mPipeline = new VulKan::Pipeline(mDevice, mRenderPass);

		createPipeline();//设置渲染管线

		//主指令缓存录制
		mCommandBuffers.resize(mSwapChain->getImageCount());//用来给每个GPU画布都绑上单独的 主CommandBuffer
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			mCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mCommandPool);//创建主指令缓存
		}

		createSyncObjects();//创建信号量（用于渲染同步）
	}
	
	void GAME::Application::initImGui()
	{
		//准备填写需要的信息
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = mInstance->getInstance();
		init_info.PhysicalDevice = mDevice->getPhysicalDevice();
		init_info.Device = mDevice->getDevice();
		init_info.QueueFamily = mDevice->getGraphicQueueFamily().value();
		init_info.Queue = mDevice->getGraphicQueue();
		init_info.PipelineCache = nullptr;
		//init_info.DescriptorPool = g_DescriptorPool;
		init_info.Subpass = 0;
		//init_info.MinImageCount = g_MinImageCount;
		init_info.ImageCount = mSwapChain->getImageCount();
		init_info.MSAASamples = mDevice->getMaxUsableSampleCount();
		init_info.Allocator = nullptr;//内存分配器
		init_info.CheckVkResultFn = check_vk_result;//错误处理
		

		InterFace = new ImGuiInterFace(mDevice, mWindow, init_info, mRenderPass, mCommandBuffers[0], mSwapChain->getImageCount());

		mImGuuiCommandPool = new VulKan::CommandPool(mDevice);
		mImGuuiCommandBuffers = new VulKan::CommandBuffer(mDevice, mImGuuiCommandPool);
	}

	

	//初始化游戏
	void Application::initGame() {
		//生成Camera Buffer
		mCameraVPMatricesBuffer.resize(mSwapChain->getImageCount());//每个GPU画布都要分配单独的 VkBuffer
		for (int i = 0; i < mCameraVPMatricesBuffer.size(); i++) {
			mCameraVPMatricesBuffer[i] = VulKan::Buffer::createUniformBuffer(mDevice, sizeof(VPMatrices), nullptr);
		}

		//生成迷宫
		mLabyrinth = new Labyrinth(mDevice, 21, 21);
		mLabyrinth->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mLabyrinth->RecordingCommandBuffer(mRenderPass->getRenderPass(), mSwapChain, mPipeline);

		//创建粒子系统
		mParticleSystem = new ParticleSystem(mDevice, 1000);
		mParticleSystem->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mParticleSystem->RecordingCommandBuffer(mRenderPass->getRenderPass(), mSwapChain, mPipeline);

		mParticlesSpecialEffect = new ParticlesSpecialEffect(mParticleSystem, 1000);


		//创建物理系统
		mSquarePhysics = new SquarePhysics::SquarePhysics(400, 400);

		//创建武器系统
		mArms = new Arms(mParticleSystem, 1000);
		mArms->SetSpecialEffect(mParticlesSpecialEffect);
		mArms->SetSquarePhysics(mSquarePhysics);//武器系统导入物理系统

		//创建多人玩家
		mCrowd = new Crowd(100, mDevice, mPipeline, mSwapChain, mRenderPass, mSampler, mCameraVPMatricesBuffer);
		mCrowd->SteSquarePhysics(mSquarePhysics);

		//创建玩家
		mGamePlayer = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, mCamera.getCameraPos().x, mCamera.getCameraPos().y);
		mGamePlayer->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			10,
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mGamePlayer->InitCommandBuffer();

		//武器系统添加
		mSquarePhysics->SetFixedSizeTerrain(mLabyrinth->mFixedSizeTerrain);//添加地图碰撞
		mSquarePhysics->AddObjectCollision(mGamePlayer->mObjectCollision);//添加玩家碰撞

		//测试
		mGifPipeline = new GifPipeline(mHeight, mWidth, mDevice, mRenderPass);
		mGIF = new GIF(mDevice, mGifPipeline, mSwapChain, mRenderPass, 44);
		mGIF->initUniformManager("002.png", mCameraVPMatricesBuffer, mSampler);
		mGIF->UpDataCommandBuffer();
	}


	void Application::createPipeline() {
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

		VulKan::Shader* shaderVertex = new VulKan::Shader(mDevice, vs_spv, VK_SHADER_STAGE_VERTEX_BIT, "main");
		shaderGroup.push_back(shaderVertex);

		VulKan::Shader* shaderFragment = new VulKan::Shader(mDevice, fs_spv, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
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


		//uniform的传递 就是一些临时信息，比如当前摄像机的方向，（在下一帧使用的信息）
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings2{};
		layoutBindings2.resize(2);

		layoutBindings2[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//绑定类型
		layoutBindings2[0].binding = 1;//那个Binding通道
		layoutBindings2[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBindings2[0].descriptorCount = 1;//多少个数据

		layoutBindings2[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;//绑定类型
		layoutBindings2[1].binding = 2;//那个Binding通道
		layoutBindings2[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBindings2[1].descriptorCount = 1;//多少个数据

		//uniform的传递
		VkDescriptorSetLayoutCreateInfo createInfo2{};
		createInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo2.bindingCount = static_cast<uint32_t>(layoutBindings2.size());
		createInfo2.pBindings = layoutBindings2.data();

		//VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;

		if (vkCreateDescriptorSetLayout(mDevice->getDevice(), &createInfo2, nullptr, &mPipeline->DescriptorSetLayout2) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayout;
		mDescriptorSetLayout.push_back(mPipeline->DescriptorSetLayout);
		mDescriptorSetLayout.push_back(mPipeline->DescriptorSetLayout2);

		mPipeline->mLayoutState.setLayoutCount = mDescriptorSetLayout.size();
		mPipeline->mLayoutState.pSetLayouts = mDescriptorSetLayout.data();
		//mPipeline->mLayoutState.pSetLayouts = &mPipeline->DescriptorSetLayout;
		mPipeline->mLayoutState.pushConstantRangeCount = 0;
		mPipeline->mLayoutState.pPushConstantRanges = nullptr;



		mPipeline->build();
	}

	void Application::createRenderPass() {
		//0：最终输出图片 1：Resolve图片（MutiSample） 2：Depth图片

		//0号位：是SwapChain原来那张图片，是Resolve的目标点，即需要设置到SubPass的Resolve当中
		VkAttachmentDescription finalAttachmentDes{};
		finalAttachmentDes.format = mSwapChain->getFormat();//获取图片格式
		finalAttachmentDes.samples = VK_SAMPLE_COUNT_1_BIT;//采样
		finalAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//原来上一张的图片，应该怎么处理（这里是不管直接清空）
		finalAttachmentDes.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//输出图片怎么处理（这里是保存）
		finalAttachmentDes.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//我们没有用到他，所以不关心
		finalAttachmentDes.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//我们没有用到他，所以不关心
		finalAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//
		finalAttachmentDes.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//保存的格式，是一直适合显示的格式

		mRenderPass->addAttachment(finalAttachmentDes);

		//1号位：被Resolve的图片，即多重采样的源头图片，也即颜色输出的目标图片
		VkAttachmentDescription MutiAttachmentDes{};
		MutiAttachmentDes.format = mSwapChain->getFormat();
		MutiAttachmentDes.samples = mDevice->getMaxUsableSampleCount();
		MutiAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		MutiAttachmentDes.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		MutiAttachmentDes.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		MutiAttachmentDes.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		MutiAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		MutiAttachmentDes.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		mRenderPass->addAttachment(MutiAttachmentDes);


		//2号位：深度缓存attachment
		VkAttachmentDescription depthAttachmentDes{};
		depthAttachmentDes.format = VulKan::Image::findDepthFormat(mDevice);
		depthAttachmentDes.samples = mDevice->getMaxUsableSampleCount();
		depthAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDes.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDes.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachmentDes.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachmentDes.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		mRenderPass->addAttachment(depthAttachmentDes);

		//对于画布的索引设置以及格式要求
		VkAttachmentReference finalAttachmentRef{};
		finalAttachmentRef.attachment = 0;//画布的索引
		finalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//我们希望的格式

		VkAttachmentReference mutiAttachmentRef{};
		mutiAttachmentRef.attachment = 1;
		mutiAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 2;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//创建子流程
		VulKan::SubPass subPass{};
		subPass.addColorAttachmentReference(mutiAttachmentRef);
		subPass.setDepthStencilAttachmentReference(depthAttachmentRef);
		subPass.setResolveAttachmentReference(finalAttachmentRef);

		subPass.buildSubPassDescription();

		mRenderPass->addSubPass(subPass);

		//子流程之间的依赖关系
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//vulkan 给的开始的虚拟流程的
		dependency.dstSubpass = 0;//代表的是 mRenderPass 数组的第 0 个 SubPass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//我上一个流程，进行到那个一步可以进行下一个子流程
		dependency.srcAccessMask = 0;//上一个流程到那一步算结束（0 代表我不关心，下一个流程直接开始）
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//下一个流程要在哪里等待上一个流程结束
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;//我到了 图片读写操作是要等待上一个子流程结束

		mRenderPass->addDependency(dependency);

		mRenderPass->buildRenderPass();

		finalAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		mImGuuiRenderPass->addAttachment(finalAttachmentDes);
		MutiAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		mImGuuiRenderPass->addAttachment(MutiAttachmentDes);
		mImGuuiRenderPass->addAttachment(depthAttachmentDes);
		mImGuuiRenderPass->addSubPass(subPass);
		mImGuuiRenderPass->addDependency(dependency);
		mImGuuiRenderPass->buildRenderPass();
	}
	
	void Application::createCommandBuffers(unsigned int i)
	{
		ThreadCommandBufferS.clear();//清空显示队列

		VkCommandBufferInheritanceInfo InheritanceInfo{};
		InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		InheritanceInfo.renderPass = mRenderPass->getRenderPass();
		InheritanceInfo.framebuffer = mSwapChain->getFrameBuffer(i);





		VkRenderPassBeginInfo renderBeginInfo{};
		renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginInfo.renderPass = mRenderPass->getRenderPass();//获得画布信息
		renderBeginInfo.framebuffer = mSwapChain->getFrameBuffer(i);//设置是那个GPU画布
		renderBeginInfo.renderArea.offset = { 0, 0 };//画布从哪里开始画
		renderBeginInfo.renderArea.extent = mSwapChain->getExtent();//画到哪里结束

		//0：final   1：muti   2：depth
		std::vector< VkClearValue> clearColors{};//在使用画布前，用什么颜色清理他，
		VkClearValue finalClearColor{};
		finalClearColor.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		clearColors.push_back(finalClearColor);

		VkClearValue mutiClearColor{};
		mutiClearColor.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		clearColors.push_back(mutiClearColor);

		VkClearValue depthClearColor{};
		depthClearColor.depthStencil = { 1.0f, 0 };
		clearColors.push_back(depthClearColor);

		renderBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());//有多少个
		renderBeginInfo.pClearValues = clearColors.data();//用来清理的颜色数据

		mCommandBuffers[i]->begin();//开始录制主指令

		mCommandBuffers[i]->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);//关于画布信息  !!!这个只有主指令才有

		if (!InterFace->GetInterFaceBool()) {
			GameCommandBuffers(i, InheritanceInfo);
		}
		else
		{
			ThreadCommandBufferS.push_back(InterFace->GetCommandBuffer(i, InheritanceInfo));
		}
		
		//把全部二级指令绑定到主指令缓存
		vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), ThreadCommandBufferS.size(), ThreadCommandBufferS.data());

		mCommandBuffers[i]->endRenderPass();//结束RenderPass

		mCommandBuffers[i]->end();//结束

		if (!InterFace->GetInterFaceBool())
		{
			renderBeginInfo.renderPass = mImGuuiRenderPass->getRenderPass();
			mImGuuiCommandBuffers->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			mImGuuiCommandBuffers->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mImGuuiCommandBuffers->getCommandBuffer());
			mImGuuiCommandBuffers->endRenderPass();//结束RenderPass
			mImGuuiCommandBuffers->end();//结束
		}
	}

	void Application::createSyncObjects() {
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			VulKan::Semaphore* imageSemaphore = new VulKan::Semaphore(mDevice);
			mImageAvailableSemaphores.push_back(imageSemaphore);

			VulKan::Semaphore* renderSemaphore = new VulKan::Semaphore(mDevice);
			mRenderFinishedSemaphores.push_back(renderSemaphore);

			VulKan::Fence* fence = new VulKan::Fence(mDevice);
			mFences.push_back(fence);
		}
	}

	void Application::recreateSwapChain() {
		int width = 0, height = 0;
		glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwWaitEvents();
			glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		}
		vkDeviceWaitIdle(mDevice->getDevice());
		cleanupSwapChain();
		mSwapChain = new VulKan::SwapChain(mDevice, mWindow, mWindowSurface, mCommandPool);
		mWidth = mSwapChain->getExtent().width;
		mHeight = mSwapChain->getExtent().height;
		//mRenderPass = new VulKan::RenderPass(mDevice);
		//createRenderPass();
		mSwapChain->createFrameBuffers(mRenderPass);
		mPipeline = new VulKan::Pipeline(mDevice, mRenderPass);
		createPipeline();
		createSyncObjects();
	}

	void Application::cleanupSwapChain() {
		//mRenderPass->~RenderPass();
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			delete mImageAvailableSemaphores[i];
			delete mRenderFinishedSemaphores[i];
			delete mFences[i];
		}
		delete mSwapChain;
		delete mPipeline;
		mImageAvailableSemaphores.clear();
		mRenderFinishedSemaphores.clear();
		mFences.clear();
	}

	//主循环main
	void Application::mainLoop() {
		SoundEffect::SoundEffect* S = new SoundEffect::SoundEffect();
		S->PlayFile(夜に駆ける);
		
		
		while (!mWindow->shouldClose()) {//窗口被关闭结束循环
			PlayerSpeed = { 0,0 };
			mWindow->pollEvents();
			
			if (InterFace->GetInterFaceBool()) {
				mWindow->ImGuiKeyBoardEvent();//监听键盘
				InterFace->InterFace();
				//S->Pause();
				if (InterFace->GetMultiplePeople())
				{
					if (InterFace->GetServerToClient()) {
						server::GetServer()->SetArms(mArms);
						server::GetServer()->SetCrowd(mCrowd);
						server::GetServer()->SetGamePlayer(mGamePlayer);
						server::GetServer()->GetServerData()->Get(0)->mBufferEventSingleData->mBrokenData = mGamePlayer->GetBrokenData();//玩家受损状态指针
					}
					else {
						client::GetClient()->SetArms(mArms);
						client::GetClient()->SetCrowd(mCrowd);
						client::GetClient()->SetGamePlayer(mGamePlayer);
					}
				}
			}
			else {
				S->Play();

				TOOL::mTimer->StartTiming(u8"游戏逻辑", true);

				mWindow->processEvent();//监听键盘
				GameLoop();
				TOOL::mTimer->StartEnd();

				

				TOOL::FPS();//刷新帧数

				TOOL::mTimer->RefreshTiming();
			}

			TOOL::mTimer->StartTiming(u8"画面渲染");
			Render();//根据录制的主指令缓存显示画面
			TOOL::mTimer->StartEnd();
			if (InterFace->GetMultiplePeople())
			{
				if (InterFace->GetServerToClient()) {
					PlayerPos* pos = server::GetServer()->GetServerData()->Get(0);
					if (pos != nullptr) {
						pos->X = m_position.x;
						pos->Y = m_position.y;
						pos->ang = m_angle * 180 / M_PI;
					}

					event_base_loop(server::GetServer()->GetEvent_Base(), EVLOOP_NONBLOCK);
				}
				else {

					event_base_loop(client::GetClient()->GetEvent_Base(), EVLOOP_ONCE);
				}
			}
		}
		S->~SoundEffect();
		//等待设备所以命令执行完毕才可以开始销毁，
		vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕
	}

	void Application::GameLoop() {

		/*++++++++++++++++++*/
		static clock_t GIFtime = 0;
		static unsigned int GIFzheng = 0;
		if ((clock() - GIFtime) > 66) {
			GIFtime = clock();
			GIFzheng++;
			if (GIFzheng >= 44) {
				GIFzheng = 0;
			}
			mGIF->SetFrame(GIFzheng, 3);
		}
		/*++++++++++++++++++*/
		
		m_angle = std::atan2(960 - CursorPosX, 540 - CursorPosY);//获取角度
		mGamePlayer->mObjectCollision->SetAngle(m_angle);//设置玩家物理角度
		mSquarePhysics->PhysicsSimulation(TOOL::FPStime);//物理事件
		mGamePlayer->mObjectCollision->SetForce({0,0});//设置玩家力
		mCamera.setCameraPos(mGamePlayer->mObjectCollision->GetPos());

		mParticlesSpecialEffect->SpecialEffectsEvent(mCurrentFrame, TOOL::FPStime);

		mVPMatrices.mViewMatrix = mCamera.getViewMatrix();//获取ViewMatrix数据
		mVPMatrices.mProjectionMatrix = mCamera.getProjectMatrix();//获取ProjectionMatrix数据
		mCameraVPMatricesBuffer[mCurrentFrame]->updateBufferByMap((void*)(&mVPMatrices), sizeof(VPMatrices));//更新Camera变换矩阵

		
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		mGamePlayer->setGamePlayerMatrix(
			glm::rotate(
				glm::translate(glm::mat4(1.0f), glm::vec3(mCamera.getCameraPos().x, mCamera.getCameraPos().y, 0.0f)),
				glm::radians(float(m_angle * 180.0f / M_PI)),
				glm::vec3(0.0f, 0.0f, 1.0f)
			),
			mCurrentFrame
		);

		static double ArmsContinuityFire = 0;
		ArmsContinuityFire += TOOL::FPStime;
		int Lzuojian = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
		if ((Lzuojian == GLFW_PRESS) && ((zuojian != Lzuojian) || (ArmsContinuityFire > 0.1f)))
		{
			ArmsContinuityFire = 0;
			unsigned char color[4] = { 0,255,0,125 };
			glm::dvec2 Armsdain =  SquarePhysics::vec2angle(glm::dvec2{ 9.0f, 0.0f }, m_angle + 1.57f);
			mArms->ShootBullets(mCamera.getCameraPos().x + Armsdain.x, mCamera.getCameraPos().y + Armsdain.y, color, m_angle + 1.57f, 500);
			if (InterFace->GetMultiplePeople()) {//是否为多人模式
				if (InterFace->GetServerToClient()) {//服务器还是客户端
					PlayerPos* LServerPos = server::GetServer()->GetServerData()->GetKeyData(0);
					BufferEventSingleData* LBufferEventSingleData;
					for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); i++)
					{
						LBufferEventSingleData = LServerPos[i].mBufferEventSingleData;
						LBufferEventSingleData->mSubmitBullet->add({ float(mCamera.getCameraPos().x + Armsdain.x), float(mCamera.getCameraPos().y + Armsdain.y), m_angle + 1.57f });
					}
				}
				else {
					client::GetClient()->GetBufferEventSingleData()->mSubmitBullet->add({ float(mCamera.getCameraPos().x + Armsdain.x), float(mCamera.getCameraPos().y + Armsdain.y), m_angle + 1.57f });
				}
			}
		}
		zuojian = Lzuojian;

		mArms->BulletsEvent();
		
		
		mLabyrinth->UpDateMaps();

		//战争迷雾
		TOOL::mTimer->StartTiming(u8"战争迷雾耗时", true);
		mLabyrinth->UpdataMist(int(mCamera.getCameraPos().x), int(mCamera.getCameraPos().y), m_angle + 0.7853981633975f);
		TOOL::mTimer->StartEnd();

		//ImGui显示录制
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin(u8"监视器");
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(400, 600));
		ImGui::Text(u8"相机位置：%10.1f  |  %10.1f  |  %10.1f", mCamera.getCameraPos().x, mCamera.getCameraPos().y, mCamera.getCameraPos().z);
		ImGui::Text(u8"鼠标角度：%10.3f", m_angle * 180 / M_PI);
		ImGui::Text(u8"玩家速度：%10.3f  |  %10.3f", mGamePlayer->mObjectCollision->GetSpeed().x, mGamePlayer->mObjectCollision->GetSpeed().y);
		ImGui::Text(u8"剩余粒子：%d", mParticleSystem->mParticle->GetNumber());
		if (InterFace->GetMultiplePeople()) {
			if (InterFace->GetServerToClient()) {
				ImGui::Text(u8"S M：%d  |  %d", server::GetServer()->GetServerData()->GetNumber(), mCrowd->GetNumber());
			}
			else {
				ImGui::Text(u8"C M：%d  |  %d", client::GetClient()->GetClientData()->GetNumber(), mCrowd->GetNumber());
			}
		}
		InterFace->ImGuiShowFPS();
		InterFace->ImGuiShowTiming();
		ImGui::End();

		//ImGui::ShowDemoWindow();

		ImGui::Render();
	}

	void Application::GameCommandBuffers(unsigned int Format_i, VkCommandBufferInheritanceInfo info) {
		

		mLabyrinth->GetCommandBuffer(&ThreadCommandBufferS, Format_i);

		ThreadCommandBufferS.push_back(mGIF->getCommandBuffer(Format_i));

		mParticleSystem->GetCommandBuffer(&ThreadCommandBufferS, Format_i);
		//加到显示数组中
		mCrowd->TimeoutDetection();
		mCrowd->GetCommandBufferS(&ThreadCommandBufferS, Format_i);

		mLabyrinth->GetMistCommandBuffer(&ThreadCommandBufferS, Format_i);

		ThreadCommandBufferS.push_back(mGamePlayer->getCommandBuffer(Format_i));
	}

	void Application::Render() {
		//等待当前要提交的CommandBuffer执行完毕
		mFences[mCurrentFrame]->block();
		

		//获取交换链当中的下一帧
		uint32_t imageIndex{ 0 };
		VkResult result = vkAcquireNextImageKHR(
			mDevice->getDevice(),
			mSwapChain->getSwapChain(),
			UINT64_MAX,
			mImageAvailableSemaphores[mCurrentFrame]->getSemaphore(),
			VK_NULL_HANDLE,
			&imageIndex);

	
		//窗体发生了尺寸变化 （窗口大小改变了话，指令要全部重新录制）
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
		}//VK_SUBOPTIMAL_KHR得到了一张认为可用的图像，但是表面格式不一定匹配
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Error: failed to acquire next image");
		}

		//重新录制指令
		TOOL::mTimer->StartTiming(u8"录制指令");
		createCommandBuffers(imageIndex);
		TOOL::mTimer->StartEnd();

		//构建提交信息
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		

		//同步信息，渲染对于显示图像的依赖，显示完毕后，才能输出颜色
		VkSemaphore waitSemaphores[] = { mImageAvailableSemaphores[mCurrentFrame]->getSemaphore() };//获取储存信号量类
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };//管线颜色输出阶段，等待信号量

		submitInfo.waitSemaphoreCount = 1;//等待多少个信号量
		submitInfo.pWaitSemaphores = waitSemaphores;//储存到那个信号量
		submitInfo.pWaitDstStageMask = waitStages;//那个阶段等待信号量


		VkCommandBuffer commandBuffers[1];
		//指定提交哪些命令
		commandBuffers[0] = { mCommandBuffers[imageIndex]->getCommandBuffer() };
		submitInfo.commandBufferCount = (uint32_t)1;
		submitInfo.pCommandBuffers = commandBuffers;

		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame]->getSemaphore()};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		mFences[mCurrentFrame]->resetFence();
		TOOL::mTimer->StartTiming(u8"等待渲染完成");
		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to submit renderCommand");
		}
		if (!InterFace->GetInterFaceBool())
		{
			mImGuuiCommandBuffers->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);
		}
		TOOL::mTimer->StartEnd();

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = {mSwapChain->getSwapChain()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		//如果开了帧数限制，GPU会在这里等待，让当前循环符合GPU设置的帧数
		TOOL::mTimer->StartTiming(u8"垂直同步");
		result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);//开始渲染
		TOOL::mTimer->StartEnd();

		//由于驱动程序不一定精准，所以我们还需要用自己的标志位判断
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mWindow->mWindowResized) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to present");
		}

		mCurrentFrame = (mCurrentFrame + 1) % mSwapChain->getImageCount();
	}

	//回收资源
	void Application::cleanUp() {
		vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕

		delete mGamePlayer;

		delete mCrowd;


		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			delete mCameraVPMatricesBuffer[i];
			delete mCommandBuffers[i];//必须先销毁 CommandBuffer ，才可以销毁绑定的 CommandPool。
			delete mImageAvailableSemaphores[i];
			delete mRenderFinishedSemaphores[i];
			delete mFences[i];
		}


		delete InterFace;

		delete mSampler;
		delete mPipeline;
		delete mRenderPass;
		delete mSwapChain;
		delete mCommandPool;
		delete mDevice;
		delete mWindowSurface;
		delete mInstance;
	}
}