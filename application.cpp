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
		static int initW = NULL;
		static int initV = NULL;
		static int initI = NULL;
		static int initG = NULL;
		static int initZ = NULL;
		mWindow = w;
		TOOL::MomentTiming(u8"总初始化", &initZ);
		TOOL::MomentTiming(u8"初始化窗口", &initW);
		initWindow();//初始化窗口
		TOOL::MomentEnd();
		TOOL::MomentTiming(u8"初始化Vulkan", &initV);
		initVulkan();//初始化Vulkan
		TOOL::MomentEnd();
		TOOL::MomentTiming(u8"初始化ImGui", &initI);
		initImGui();//初始化ImGui
		TOOL::MomentEnd();
		TOOL::MomentTiming(u8"初始化游戏", &initG);
		initGame();//初始化游戏
		TOOL::MomentEnd();
		TOOL::MomentEnd();
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
		//mGamePlayer->mObjectCollision->SetForce(LI);

		mGamePlayer->mObjectCollision->SetSpeed(m_angle + 1.57f, PlayerSpeed);

		//mGamePlayer->mObjectCollision->SetSpeed(LI)
		//mGamePlayer->mObjectCollision->SetAngle(m_angle);
		//mGamePlayer->mObjectCollision->SetForce(LI);
		//PlayerMoveBool = true;
		//mCamera.setSpeedX(PlayerSpeedX * TOOL::FPStime);//获取速度，不受帧数影响
		//mCamera.setSpeedY(PlayerSpeedY * TOOL::FPStime);//获取速度，不受帧数影响
		//mCamera.move(moveDirection);
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
		mCamera.lookAt(glm::vec3(0.0f, 20.0f, 500.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
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
		mInstance = new VulKan::Instance(true);//实列化需要的VulKan功能APi
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

	void DeletePlayer(GamePlayer* x) {
		std::cout << "Delete Player, 释放 玩家" << std::endl;
		delete x;
	}

	bool TimeoutPlayer(GamePlayer* x) {
		std::cout << "Timeout Player, 超时 玩家" << std::endl;
		return 1;
	}

	//初始化游戏
	void Application::initGame() {
		//生成Camera Buffer
		mCameraVPMatricesBuffer.resize(mSwapChain->getImageCount());//每个GPU画布都要分配单独的 VkBuffer
		for (int i = 0; i < mCameraVPMatricesBuffer.size(); i++) {
			mCameraVPMatricesBuffer[i] = VulKan::Buffer::createUniformBuffer(mDevice, sizeof(VPMatrices), nullptr);
		}

		/*mBlockS = new BlockS(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mCamera.getCameraPos(),
			mWidth,
			mHeight,
			mSampler,
			mRenderPass->getRenderPass(),
			mPipeline,
			mSwapChain
		);*/
		mLabyrinth = new Labyrinth(mDevice, 41, 41);
		mLabyrinth->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mLabyrinth->RecordingCommandBuffer(mRenderPass->getRenderPass(), mSwapChain, mPipeline);


		mParticleSystem = new ParticleSystem(mDevice, 400);
		mParticleSystem->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mParticleSystem->RecordingCommandBuffer(mRenderPass->getRenderPass(), mSwapChain, mPipeline);

		mArms = new Arms(mParticleSystem, 400);

		mPixelTextureS = new PixelTextureS(mDevice,mCommandPool, mSampler);

		MapPlayerS = new ContinuousMap<evutil_socket_t, GamePlayer*>(100);
		MapPlayerS->SetDeleteCallback(DeletePlayer);
		MapPlayerS->SetTimeoutCallback(TimeoutPlayer);

		mGamePlayer = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, mCamera.getCameraPos().x, mCamera.getCameraPos().y);
		mGamePlayer->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPixelTextureS->getPixelTexture(2),
			mPipeline->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mGamePlayer->InitCommandBuffer();

		mSquarePhysics = new SquarePhysics::SquarePhysics(400, 400);
		mSquarePhysics->SetFixedSizeTerrain(mLabyrinth->mFixedSizeTerrain);
		mArms->SetSquarePhysics(mSquarePhysics);
		mSquarePhysics->AddObjectCollision(mGamePlayer->mObjectCollision);
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

		//if (!InterFace->GetInterFaceBool()) {
			GameCommandBuffers(i, InheritanceInfo);
		//}
		//ThreadCommandBufferS.push_back(InterFace->GetCommandBuffer(i, InheritanceInfo));
		
		//把全部二级指令绑定到主指令缓存
		vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), ThreadCommandBufferS.size(), ThreadCommandBufferS.data());

		mCommandBuffers[i]->endRenderPass();//结束RenderPass

		mCommandBuffers[i]->end();//结束


		renderBeginInfo.renderPass = mImGuuiRenderPass->getRenderPass();
		mImGuuiCommandBuffers->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		mImGuuiCommandBuffers->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mImGuuiCommandBuffers->getCommandBuffer());
		mImGuuiCommandBuffers->endRenderPass();//结束RenderPass
		mImGuuiCommandBuffers->end();//结束

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
		delete mSwapChain;
		delete mPipeline;
		//mRenderPass->~RenderPass();
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			delete mImageAvailableSemaphores[i];
			delete mRenderFinishedSemaphores[i];
			delete mFences[i];
		}
		mImageAvailableSemaphores.clear();
		mRenderFinishedSemaphores.clear();
		mFences.clear();
	}

	//主循环main
	void Application::mainLoop() {
		SoundEffect::SoundEffect* S = new SoundEffect::SoundEffect();
		S->PlayFile(夜に駆ける);
		

		while (!mWindow->shouldClose()) {//窗口被关闭结束循环
			mWindow->pollEvents();
			mWindow->ImGuiKeyBoardEvent();

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin(u8"启动模式");
			if (ImGui::Button(u8"服务器"))
			{
				ServerToClient = true;
				ImGui::End();

				ImGui::Render();
				break;
			}
			if (ImGui::Button(u8"客户端"))
			{
				ServerToClient = false;
				ImGui::End();

				ImGui::Render();
				break;
			}
			ImGui::End();

			ImGui::Render();

			RenderInterFace();
		}

		if (ServerToClient) {
			server_init(25565);
		}
		else {
			client_init("127.0.0.1", 25565);
		}

		while (!mWindow->shouldClose()) {//窗口被关闭结束循环
			PlayerSpeed = { 0,0 };
			mWindow->pollEvents();
			
			if (InterFace->GetInterFaceBool()) {
				mWindow->ImGuiKeyBoardEvent();//监听键盘
				InterFace->InterFace();
				RenderInterFace();//根据录制的主指令缓存显示画面

				//S->Pause();
			}
			else {
				S->Play();

				TOOL::StartTiming(u8"游戏逻辑", true);
				PlayerKeyBoolY = true;
				PlayerKeyBoolX = true;
				mWindow->processEvent();//监听键盘
				GameLoop();
				TOOL::StartEnd();

				TOOL::StartTiming(u8"画面渲染");
				Render();//根据录制的主指令缓存显示画面
				TOOL::StartEnd();

				TOOL::FPS();//刷新帧数

				TOOL::RefreshTiming();
			}

			if (ServerToClient) {
				ServerPos* pos = server.Get(0);
				if (pos != nullptr) {
					pos->X = m_position.x;
					pos->Y = m_position.y;
					pos->ang = m_angle * 180 / M_PI;
				}

				
				if (server.GetKeyNumber() != MapPlayerS->GetNumber())
				{
					ServerPos* sposs = server.GetKeyData(0);
					for (size_t i = 0; i < server.GetKeyNumber(); i++)
					{
						GamePlayer** Players = MapPlayerS->Get(sposs[i].Key);
						if (Players == nullptr)
						{
							std::cout << "Players - fd: " << sposs[i].Key << " X: " << sposs[i].X << " Y: " << sposs[i].Y << std::endl;
							Players = MapPlayerS->New(sposs[i].Key);
							(*Players) = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, sposs[i].X, sposs[i].Y);
							(*Players)->initUniformManager(
								mDevice,
								mCommandPool,
								mSwapChain->getImageCount(),
								mPixelTextureS->getPixelTexture(sposs[i].Key % TextureNumber),
								mPipeline->DescriptorSetLayout,
								mCameraVPMatricesBuffer,
								mSampler
							);
							(*Players)->InitCommandBuffer();
						}
					}
				}
				else {
					ServerPos* sposs = server.GetKeyData(0);
					for (size_t i = 0; i < server.GetKeyNumber(); i++)
					{
						GamePlayer** Players = MapPlayerS->Get(sposs[i].Key);
						if (Players != nullptr) {
							(*Players)->setGamePlayerMatrix(
								glm::rotate(
									glm::translate(glm::mat4(1.0f), glm::vec3(sposs[i].X, sposs[i].Y, 0.0f)),
									glm::radians(float(sposs[i].ang)),
									glm::vec3(0.0f, 0.0f, 1.0f)
								),
								mCurrentFrame
							);
						}
					}
				}

				event_base_loop(server_base, EVLOOP_NONBLOCK);
			}
			else {

				if (client.GetNumber() > (MapPlayerS->GetNumber()))
				{
					ClientPos* sposs = client.GetData();
					for (size_t i = 0; i < client.GetNumber(); i++)
					{
						GamePlayer** Players = MapPlayerS->Get(sposs[i].Key);
						if (Players == nullptr)
						{
							Players = MapPlayerS->New(sposs[i].Key);
							(*Players) = new GamePlayer(mDevice, mPipeline, mSwapChain, mRenderPass, sposs[i].X, sposs[i].Y);
							(*Players)->initUniformManager(
								mDevice,
								mCommandPool,
								mSwapChain->getImageCount(),
								mPixelTextureS->getPixelTexture(sposs[i].Key % TextureNumber),
								mPipeline->DescriptorSetLayout,
								mCameraVPMatricesBuffer,
								mSampler
							);
							(*Players)->InitCommandBuffer();
						}
					}
				}
				else {
					ClientPos* sposs = client.GetData();
					for (size_t i = 0; i < client.GetNumber(); i++)
					{
						GamePlayer** Players = MapPlayerS->Get(sposs[i].Key);
						(*Players)->setGamePlayerMatrix(
							glm::rotate(
								glm::translate(glm::mat4(1.0f), glm::vec3(sposs[i].X, sposs[i].Y, 0.0f)),
								glm::radians(float(sposs[i].ang)),
								glm::vec3(0.0f, 0.0f, 1.0f)
							),
							mCurrentFrame
						);
					}
				}

				event_base_loop(client_base, EVLOOP_ONCE);
			}
		}
		S->~SoundEffect();
		//等待设备所以命令执行完毕才可以开始销毁，
		vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕
	}

	void Application::GameLoop() {
		//if ((PlayerSpeedX != 0.0f) && PlayerKeyBoolX) {
		//	if (PlayerSpeedX > 0) {
		//		PlayerMoveBoolX = true;
		//		PlayerSpeedX -= TOOL::FPStime * 300.0f;
		//	}
		//	else {
		//		PlayerMoveBoolX = false;
		//		PlayerSpeedX += TOOL::FPStime * 300.0f;
		//	}

		//	if (PlayerMoveBoolX) {
		//		if (PlayerSpeedX < 0) {
		//			PlayerSpeedX = 0;
		//		}
		//	}
		//	else if (PlayerSpeedX > 0) {
		//		PlayerSpeedX = 0;
		//	}
		//}

		//if ((PlayerSpeedY != 0.0f) && PlayerKeyBoolY) {
		//	if (PlayerSpeedY > 0) {
		//		PlayerMoveBoolY = true;
		//		PlayerSpeedY -= TOOL::FPStime * 300.0f;
		//	}
		//	else {
		//		PlayerMoveBoolY = false;
		//		PlayerSpeedY += TOOL::FPStime * 300.0f;
		//	}

		//	if (PlayerMoveBoolY) {
		//		if (PlayerSpeedY < 0) {
		//			PlayerSpeedY = 0;
		//		}
		//	}
		//	else if (PlayerSpeedY > 0) {
		//		PlayerSpeedY = 0;
		//	}
		//}

		
		//if (1) {//坐标轴移动
		//	mCamera.setSpeedX(PlayerSpeedX * TOOL::FPStime);
		//	mCamera.setSpeedY(PlayerSpeedY * TOOL::FPStime);
		//}
		//else {//以玩家朝向为坐标轴移动
		//	glm::vec2 WSSpeedXY = SquarePhysics::vec2angle(glm::vec2((PlayerSpeedX * TOOL::FPStime), 0), m_angle);
		//	glm::vec2 ADSpeedXY = SquarePhysics::vec2angle(glm::vec2(0, (PlayerSpeedY * TOOL::FPStime)), m_angle);
		//	mCamera.setSpeedX(WSSpeedXY.x + ADSpeedXY.x);
		//	mCamera.setSpeedY(WSSpeedXY.y + ADSpeedXY.y);
		//}
		//mCamera.moveXY();
		
		m_angle = std::atan2(960 - CursorPosX, 540 - CursorPosY);
		mGamePlayer->mObjectCollision->SetAngle(m_angle + 1.57f);
		mGamePlayer->mObjectCollision->SetPos({ mCamera.getCameraPos().x, mCamera.getCameraPos().y });
		mSquarePhysics->PhysicsSimulation(TOOL::FPStime);//物理事件
		mGamePlayer->mObjectCollision->SetSpeed(0);
		mCamera.setCameraPos(mGamePlayer->mObjectCollision->GetPos());

		mVPMatrices.mViewMatrix = mCamera.getViewMatrix();//获取ViewMatrix数据
		mVPMatrices.mProjectionMatrix = mCamera.getProjectMatrix();//获取ProjectionMatrix数据
		mCameraVPMatricesBuffer[mCurrentFrame]->updateBufferByMap((void*)(&mVPMatrices), sizeof(VPMatrices));//更新Camera变换矩阵

		
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		mGamePlayer->setGamePlayerMatrix(
			glm::rotate(
				glm::translate(glm::mat4(1.0f), glm::vec3(mCamera.getCameraPos().x, mCamera.getCameraPos().y, 0.0f)),
				glm::radians(float(m_angle * 180 / M_PI)),
				glm::vec3(0.0f, 0.0f, 1.0f)
			),
			mCurrentFrame
		);


		int Lzuojian = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
		if ((Lzuojian == GLFW_PRESS) && zuojian != Lzuojian)
		{
			unsigned char color[4] = { 0,255,0,125 };
			mArms->ShootBullets(mCamera.getCameraPos().x, mCamera.getCameraPos().y, color, m_angle + 1.57f, 50);
			client_Fire = true;
		}
		zuojian = Lzuojian;

		if (ServerToClient) {
			ServerPos* dataVec = server.GetKeyData(0);
			for (size_t i = 0; i < server.GetKeyNumber(); i++)
			{
				if (dataVec[i].Fire) {
					dataVec[i].Fire = false;
					unsigned char color[4] = { 0,255,0,125 };
					mArms->ShootBullets(dataVec[i].X, dataVec[i].Y, color, dataVec[i].ang / 180.0f * 3.14159265359f, 5000);
				}
			}
		}
		else {

		}


		mArms->BulletsEvent();
		
		
		mLabyrinth->UpDateMaps();

		//战争迷雾
		TOOL::StartTiming(u8"战争迷雾耗时", true);
		mLabyrinth->UpdataMist(int(mCamera.getCameraPos().x), int(mCamera.getCameraPos().y), m_angle + 0.7853981633975f);
		TOOL::StartEnd();

		//ImGui显示录制
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin(u8"监视器");
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(400, 600));
		ImGui::Text(u8"相机位置：%10.1f  |  %10.1f  |  %10.1f", mCamera.getCameraPos().x, mCamera.getCameraPos().y, mCamera.getCameraPos().z);
		ImGui::Text(u8"鼠标角度：%10.3f", m_angle * 180 / M_PI);
		ImGui::Text(u8"玩家速度：%10.3f  |  %10.3f", PlayerSpeedX, PlayerSpeedY);
		ImGui::Text(u8"剩余粒子：%d", mParticleSystem->mParticle->GetNumber());
		ImGui::Text(u8"S C M：%d  |  %d  |  %d", server.GetNumber(), client.GetNumber(), MapPlayerS->GetNumber());
		InterFace->ImGuiShowFPS();
		InterFace->ImGuiShowTiming();
		ImGui::End();

		//ImGui::ShowDemoWindow();

		ImGui::Render();
	}

	void Application::GameCommandBuffers(unsigned int Format_i, VkCommandBufferInheritanceInfo info) {
		
		//加到显示数组中
		/*for (int i = 0; i < mBlockS->ThreadCommandBufferNumber; i++)
		{
			ThreadCommandBufferS.push_back(mBlockS->getDoubleCommandBuffer(Format_i, i));
		}*/
		mLabyrinth->GetCommandBuffer(&ThreadCommandBufferS, Format_i);
		mParticleSystem->GetCommandBuffer(&ThreadCommandBufferS, Format_i);
		//加到显示数组中
		if (ServerToClient) {
			ServerPos* Serpos = server.GetKeyData(0);
			for (size_t i = 0; i < server.GetKeyNumber(); i++)
			{
				if (MapPlayerS->Get(Serpos[i].Key) != nullptr) {
					ThreadCommandBufferS.push_back((*MapPlayerS->Get(Serpos[i].Key))->getCommandBuffer(Format_i));
				}
			}
		}
		else {
			ClientPos* Serpos = client.GetData();
			for (size_t i = 0; i < client.GetNumber(); i++)
			{
				if (MapPlayerS->Get(Serpos[i].Key) != nullptr) {
					ThreadCommandBufferS.push_back((*MapPlayerS->Get(Serpos[i].Key))->getCommandBuffer(Format_i));
				}
			}
		}
		MapPlayerS->TimeoutDetection();
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
		TOOL::StartTiming(u8"录制指令");
		createCommandBuffers(imageIndex);
		TOOL::StartEnd();

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
		TOOL::StartTiming(u8"等待渲染完成");
		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to submit renderCommand");
		}
		mImGuuiCommandBuffers->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);
		TOOL::StartEnd();

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = {mSwapChain->getSwapChain()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		//如果开了帧数限制，GPU会在这里等待，让当前循环符合GPU设置的帧数
		TOOL::StartTiming(u8"垂直同步");
		result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);//开始渲染
		TOOL::StartEnd();

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

	void Application::RenderInterFace() {
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
		createCommandBuffers(imageIndex);

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

		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame]->getSemaphore() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		mFences[mCurrentFrame]->resetFence();
		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to submit renderCommand");
		}

		mImGuuiCommandBuffers->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);


		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { mSwapChain->getSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		//如果开了帧数限制，GPU会在这里等待，让当前循环符合GPU设置的帧数
		result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);//开始渲染

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

		GamePlayer** Pr = MapPlayerS->GetData();
		for (size_t i = 0; i < MapPlayerS->GetNumber(); i++)
		{
			delete Pr[i];
		}


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