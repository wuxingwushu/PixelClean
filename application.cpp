#include "application.h"
#include "NetworkTCP/Server.h"
#include "NetworkTCP/Client.h"
#include "Opcode/OpcodeFunction.h"


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
	}

	//总初始化
	void Application::run() {
		TOOL::mTimer->MomentTiming(u8"总初始化 ");
		TOOL::mTimer->MomentTiming(u8"初始化窗口 ");
		initWindow();//初始化窗口
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentTiming(u8"初始化Vulkan ");
		initVulkan();//初始化Vulkan
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentTiming(u8"初始化ImGui ");
		initImGui();//初始化ImGui
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentTiming(u8"初始化游戏 ");
		initGame();//初始化游戏
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentEnd();
		mainLoop();//开启主循环main
		Global::Storage();
		cleanUp();//回收资源
	}

	GameMods* Application::GetGame(GameModsEnum Game) {
		switch (Game)
		{
		case Maze_:
			return new MazeMods(*this);
			break;
		case Infinite_:
			return new UnlimitednessMapMods(*this);
			break;
		case TankTrouble_:
			return new TankTrouble(*this);
			break;
		case PhysicsTest_:
			return new PhysicsTest(*this);
			break;
		default:
			break;
		}
	}

	void Application::DeleteGame(GameModsEnum Game) {
		switch (Game)
		{
		case Maze_:
			delete (MazeMods*)mGameMods;
			break;
		case Infinite_:
			delete (UnlimitednessMapMods*)mGameMods;
			break;
		case TankTrouble_:
			delete (TankTrouble*)mGameMods;
			break;
		default:
			break;
		}
		mGameMods = nullptr;
	}

	//窗口的初始化
	void Application::initWindow() {
		mWindow = new VulKan::Window(Global::mWidth, Global::mHeight, false, Global::FullScreen);
		//mWindow->setApp(shared_from_this());//把 application 本体指针传给 Window ，便于调用 Camera
		//设置摄像机   位置，朝向（后面两个 vec3 来决定）
		mCamera = new Camera();
		mCamera->lookAt(glm::vec3(20.0f, 20.0f, 400.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//设置Perpective
		mCamera->setPerpective(45.0f, (float)Global::mWidth / (float)Global::mHeight, 0.1f, 1000.0f);
	}

	//初始化Vulkan
	//1 rendePass 加入pipeline 2 生成FrameBuffer
	void Application::initVulkan() {
		mInstance = new VulKan::Instance(Global::VulKanValidationLayer);//实列化需要的VulKan功能APi
		if (Global::VulKanValidationLayer && !mInstance->getEnableValidationLayer()) {//如果设备不支持，强制性修改设置信息，关闭验证层
			Global::VulKanValidationLayer = false;
			Global::Storage();
		}
		mWindowSurface = new VulKan::WindowSurface(mInstance, mWindow);//获取你在什么平台运行调用不同的API（比如：Window，Android）
		mDevice = new VulKan::Device(mInstance, mWindowSurface);//获取电脑的硬件设备，vma内存分配器 也在这里被创建了
		mCommandPool = new VulKan::CommandPool(mDevice);//创建指令池，给CommandBuffer用的
		mSwapChain = new VulKan::SwapChain(mDevice, mWindow, mWindowSurface, mCommandPool);//设置VulKan的工作细节
		Global::mWidth = mSwapChain->getExtent().width;
		Global::mHeight = mSwapChain->getExtent().height;
		mRenderPass = new VulKan::RenderPass(mDevice);//创建GPU画布描述
		mImGuuiRenderPass = new VulKan::RenderPass(mDevice);
		mSampler = new VulKan::Sampler(mDevice);//采样器
		createRenderPass();//设置GPU画布描述
		mSwapChain->createFrameBuffers(mRenderPass);//把GPU画布描述传给交换链生成GPU画布

		mPipelineS = new VulKan::PipelineS(mDevice, mRenderPass);//创建渲染管线集合

		Global::CommandBufferSize = mSwapChain->getImageCount();
		Global::MainCommandBufferS = new bool[Global::CommandBufferSize];
		//主指令缓存录制
		mCommandBuffers.resize(mSwapChain->getImageCount());//用来给每个GPU画布都绑上单独的 主CommandBuffer
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			mCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mCommandPool);//创建主指令缓存
			Global::MainCommandBufferS[i] = true;
		}

		wThreadCommandBufferS = &ThreadCommandBufferS;

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
		init_info.Subpass = 0;
		init_info.ImageCount = mSwapChain->getImageCount();
		init_info.MSAASamples = mDevice->getMaxUsableSampleCount();
		init_info.Allocator = nullptr;//内存分配器
		init_info.CheckVkResultFn = check_vk_result;//错误处理
		

		mImGuuiCommandPool = new VulKan::CommandPool(mDevice);
		mImGuuiCommandBuffers = new VulKan::CommandBuffer(mDevice, mImGuuiCommandPool);

		InterFace = new ImGuiInterFace(mDevice, mWindow, init_info, mRenderPass, mImGuuiCommandBuffers,
			new ImGuiTexture(mDevice, mSwapChain, mCommandPool,mSampler, 1),
			mSwapChain->getImageCount());
	}

	

	//初始化游戏
	void Application::initGame() {
		//生成Camera Buffer
		mCameraVPMatricesBuffer.resize(mSwapChain->getImageCount());//每个GPU画布都要分配单独的 VkBuffer
		for (auto i = 0; i < mCameraVPMatricesBuffer.size(); ++i) {
			mCameraVPMatricesBuffer[i] = VulKan::Buffer::createUniformBuffer(mDevice, sizeof(VPMatrices), nullptr);
			VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[i]->getupdateBufferByMap();
			mVPMatrices->mProjectionMatrix = mCamera->getProjectMatrix();//获取ProjectionMatrix数据
			mCameraVPMatricesBuffer[i]->endupdateBufferByMap();
		}

		//创建粒子系统
		mParticleSystem = new ParticleSystem(mDevice, 1000);
		mParticleSystem->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mParticleSystem->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));

		mParticlesSpecialEffect = new ParticlesSpecialEffect(mParticleSystem, 1000);


		//创建物理系统
		mSquarePhysics = new SquarePhysics::SquarePhysics(400, 400);

		//创建武器系统
		mArms = new Arms(mParticleSystem, 1000);
		mArms->SetSpecialEffect(mParticlesSpecialEffect);
		mArms->SetSquarePhysics(mSquarePhysics);//武器系统导入物理系统

		//GIF库
		mGIFTextureLibrary = new TextureLibrary(mDevice, mCommandPool, mSampler, "./Resource/GifTexture/");
		//素材库
		mTextureLibrary = new TextureLibrary(mDevice, mCommandPool, mSampler, "./Resource/Material/", false);


		Opcode::OpApplication = this;
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

		//finalAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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
		renderBeginInfo.framebuffer = mSwapChain->getFrameBuffer(i);//设置是那个GPU画布
		renderBeginInfo.renderArea.offset = { 0, 0 };//画布从哪里开始画
		renderBeginInfo.renderArea.extent = mSwapChain->getExtent();//画到哪里结束

		//0：final   1：muti   2：depth
		std::vector< VkClearValue> clearColors{};//在使用画布前，用什么颜色清理他，
		VkClearValue finalClearColor{};
		finalClearColor.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		clearColors.push_back(finalClearColor);

		VkClearValue mutiClearColor{};
		mutiClearColor.color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearColors.push_back(mutiClearColor);

		VkClearValue depthClearColor{};
		depthClearColor.depthStencil = { 1.0f, 0 };
		clearColors.push_back(depthClearColor);

		renderBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());//有多少个
		renderBeginInfo.pClearValues = clearColors.data();//用来清理的颜色数据


		if (InterFace->GetInterFaceBool())
		{
			renderBeginInfo.renderPass = mRenderPass->getRenderPass();//获得画布信息
			mCommandBuffers[i]->begin();//开始录制主指令
			mCommandBuffers[i]->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);//关于画布信息  !!!这个只有主指令才有
			ThreadCommandBufferS.push_back(InterFace->GetCommandBuffer(i, InheritanceInfo));
			vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), ThreadCommandBufferS.size(), ThreadCommandBufferS.data());
			mCommandBuffers[i]->endRenderPass();//结束RenderPass
			mCommandBuffers[i]->end();//结束
			Global::MainCommandBufferS[i] = true;
			return;
		}
		else if(!Global::MonitorCompatibleMode){
			renderBeginInfo.renderPass = mImGuuiRenderPass->getRenderPass();//获得画布信息
			mImGuuiCommandBuffers->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			mImGuuiCommandBuffers->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mImGuuiCommandBuffers->getCommandBuffer());
			mImGuuiCommandBuffers->endRenderPass();//结束RenderPass
			mImGuuiCommandBuffers->end();//结束
		}

		if (!Global::MainCommandBufferS[i] && !Global::MonitorCompatibleMode) {
			return;
		}

		renderBeginInfo.renderPass = mRenderPass->getRenderPass();//获得画布信息

		mCommandBuffers[i]->begin();//开始录制主指令

		mCommandBuffers[i]->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);//关于画布信息  !!!这个只有主指令才有

		mGameMods->GameCommandBuffers(i);

		if (Global::MonitorCompatibleMode) {
			ThreadCommandBufferS.push_back(InterFace->GetCommandBuffer(i, InheritanceInfo));
		}
		
		//把全部二级指令绑定到主指令缓存
		vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), ThreadCommandBufferS.size(), ThreadCommandBufferS.data());

		mCommandBuffers[i]->endRenderPass();//结束RenderPass

		mCommandBuffers[i]->end();//结束

		Global::MainCommandBufferS[i] = false;
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
		TOOL::mTimer->MomentTiming(u8"窗口重构 ");

		int width = 0, height = 0;
		glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwWaitEvents();
			glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		}
		vkDeviceWaitIdle(mDevice->getDevice());

		mSwapChain->~SwapChain();
		mSwapChain->StructureSwapChain();
		Global::mWidth = mSwapChain->getExtent().width;
		Global::mHeight = mSwapChain->getExtent().height;
		mRenderPass->~RenderPass();
		mImGuuiRenderPass->~RenderPass();
		createRenderPass();
		mSwapChain->createFrameBuffers(mRenderPass);
		mPipelineS->ReconfigurationPipelineS();
		mCamera->setPerpective(45.0f, (float)Global::mWidth / (float)Global::mHeight, 0.1f, 1000.0f);
		for (size_t i = 0; i < mSwapChain->getImageCount(); i++)
		{
			VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[i]->getupdateBufferByMap();
			mVPMatrices->mProjectionMatrix = mCamera->getProjectMatrix();//获取ProjectionMatrix数据
			mCameraVPMatricesBuffer[i]->endupdateBufferByMap();
		}
		if (mGameMods != nullptr) {
			mGameMods->GameRecordCommandBuffers();
		}
		mParticleSystem->ThreadUpdateCommandBuffer();
		for (size_t i = 0; i < mSwapChain->getImageCount(); ++i)
		{
			Global::MainCommandBufferS[i] = true;
		}
		TOOL::mTimer->MomentEnd();
	}

	//主循环main
	void Application::mainLoop() {
		SoundEffect::SoundEffect::GetSoundEffect()->Play("夜に駆ける", MIDI, true, Global::MusicVolume);
		while (!mWindow->shouldClose()) {//窗口被关闭结束循环
			SoundEffect::SoundEffect::GetSoundEffect()->SoundEffectEvent();
			PlayerForce = { 0,0 };
			mWindow->pollEvents();//GLFW轮询事件
			//更新 ImGui 的 MousePos
			ImGuiIO& io = ImGui::GetIO();
			glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
			io.MousePos.x = CursorPosX;
			io.MousePos.y = CursorPosY;
			if (InterFace->GetInterFaceBool()) {
				mWindow->ImGuiKeyBoardEvent();//监听键盘
				InterFace->InterFace(mCurrentFrame);
				
				if (Global::GameResourceLoadingBool) {
					Global::GameResourceLoadingBool = false;
					mGameMods = GetGame(Global::GameMode);
					mWindow->setApp(mGameMods);
				}

				if (Global::GameResourceUninstallBool) {
					Global::GameResourceUninstallBool = false;
					DeleteGame(Global::GameMode);
					mWindow->ReleaseApp();
				}

				if (mGameMods != nullptr) {
					mGameMods->GameStopInterfaceLoop(mCurrentFrame);
				}
			}
			else {
				TOOL::mTimer->StartTiming(u8"游戏逻辑 ", true);
				mWindow->processEvent();//监听键盘
				GameLoop();
				TOOL::mTimer->StartEnd();

				TOOL::mTimer->RefreshTiming();
			}


			TOOL::mTimer->StartTiming(u8"TCP ");
			if (mGameMods != nullptr) {
				mGameMods->GameTCPLoop();
			}
			TOOL::mTimer->StartEnd();


			TOOL::FPS();//刷新帧数

			TOOL::mTimer->StartTiming(u8"画面渲染 ");
			Render();//根据录制的主指令缓存显示画面
			TOOL::mTimer->StartEnd();
		}
		SoundEffect::SoundEffect::GetSoundEffect()->~SoundEffect();
		//等待设备所以命令执行完毕才可以开始销毁，
		vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕
	}

	void Application::GameLoop() {

		mGameMods->GameLoop(mCurrentFrame);

		int winwidth, winheight;
		glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;

		//ImGui显示录制
		if (Global::Monitor) {
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			ImGui::PushFont(InterFace->Font);
			
			ImGui::Begin(u8"监视器 ");
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::Text(u8"相机位置：%10.1f  |  %10.1f  |  %10.1f", mCamera->getCameraPos().x, mCamera->getCameraPos().y, mCamera->getCameraPos().z);
			ImGui::Text(u8"鼠标角度：%10.3f", m_angle * 180 / M_PI);
			ImGui::Text(u8"鼠标获取位置：%10.3f - %10.3f - %10.3f", huoqdedian.x, huoqdedian.y, huoqdedian.z);
			ImGui::Text(u8"攻击模式：%d (1 / 2 上下切换) ", AttackType);
			//ImGui::Text(u8"玩家速度：%10.3f  |  %10.3f", mGamePlayer->GetObjectCollision()->GetSpeed().x, mGamePlayer->GetObjectCollision()->GetSpeed().y);
			ImGui::Text(u8"剩余粒子：%d", mParticleSystem->mParticle->GetNumber());
			/*if (Global::MultiplePeopleMode) {
				if (Global::ServerOrClient) {
					ImGui::Text(u8"S M：%d  |  %d", server::GetServer()->GetServerData()->GetNumber(), mCrowd->GetNumber());
				}
				else {
					ImGui::Text(u8"C M：%d  |  %d", client::GetClient()->GetClientData()->GetNumber(), mCrowd->GetNumber());
				}
			}*/
			InterFace->ImGuiShowFPS();
			InterFace->ImGuiShowTiming();
			ImGui::Text(u8"如果只看得到监视窗口去设置中关掉监视器 ! ");
			ImGui::End();

			if (Global::ConsoleBool) {
				InterFace->ConsoleInterface();//控制台
			}
			InterFace->DisplayTextS();

			//ImGui::ShowDemoWindow();
			ImGui::PopFont();
			ImGui::Render();
		}else if (Global::ConsoleBool) {
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::PushFont(InterFace->Font);
			InterFace->ConsoleInterface();//控制台
			ImGui::PopFont();
			ImGui::Render();
		}
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
		TOOL::mTimer->StartTiming(u8"录制指令 ");
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
		TOOL::mTimer->StartTiming(u8"等待渲染完成 ");
		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to submit renderCommand");
		}
		if ((!InterFace->GetInterFaceBool() && Global::Monitor) && !Global::MonitorCompatibleMode)
		{
			mImGuuiCommandBuffers->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);//这个功能部分电脑，不支持 分两次 vkQueueSubmit 导致 只看得见 监视器
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
		TOOL::mTimer->StartTiming(u8"垂直同步 ");
		result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);//更新画面
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

		delete mParticleSystem;
		delete mSquarePhysics;
		delete mParticlesSpecialEffect;
		delete mArms;
		

		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			delete mCameraVPMatricesBuffer[i];
			delete mCommandBuffers[i];//必须先销毁 CommandBuffer ，才可以销毁绑定的 CommandPool。
			delete mImageAvailableSemaphores[i];
			delete mRenderFinishedSemaphores[i];
			delete mFences[i];
		}

		delete mImGuuiCommandBuffers;
		delete mImGuuiCommandPool;
		delete mImGuuiRenderPass;
		delete InterFace;

		delete mSampler;
		delete mPipelineS;
		delete mRenderPass;
		delete mSwapChain;
		delete mCommandPool;
		delete mDevice;
		delete mWindowSurface;
		delete mInstance;
	}
}