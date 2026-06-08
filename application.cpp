#include "application.h"
#include "../DebugLog.h"
#include "Opcode/OpcodeFunction.h"
#include "GameMods/BlockWorld/BlockWorld.h"
#include "GameMods/FruitNinja/FruitNinja.h"
#if defined(__ANDROID__)
#include <android/native_window.h>
#include <imgui/backends/imgui_impl_android.h>
#endif

#ifndef VK_API_VERSION_1_1
#define VK_API_VERSION_1_1 VK_MAKE_API_VERSION(0, 1, 1, 0)
#endif


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
		LOGD("Application constructed");
	}

	//总初始化
	void Application::run() {
		LOGI("Application::run started");
		TOOL::mTimer->MomentTiming(u8"总初始化 ");
		TOOL::mTimer->MomentTiming(u8"初始化窗口 ");
		initWindow();//初始化窗口
		TOOL::mTimer->MomentEnd();
		LOGD("Window initialized");
		TOOL::mTimer->MomentTiming(u8"初始化Vulkan ");
		initVulkan();//初始化Vulkan
		TOOL::mTimer->MomentEnd();
		LOGD("Vulkan initialized");
		TOOL::mTimer->MomentTiming(u8"初始化ImGui ");
		initImGui();//初始化ImGui
		TOOL::mTimer->MomentEnd();
		LOGD("ImGui initialized");
		TOOL::mTimer->MomentTiming(u8"初始化游戏 ");
		initGame();//初始化游戏
		TOOL::mTimer->MomentEnd();
		LOGI("All initialization complete");
		TOOL::mTimer->MomentEnd();
		mainLoop();//开启主循环main
		Global::Storage();
		cleanUp();//回收资源
	}

	GameMods* Application::GetGame(GameModsEnum Game) {
		LOGI("GetGame: mode=%d", (int)Game);
		GameMods* GamePtr = nullptr;
		switch (Game)
		{
		case Maze_:
			GamePtr = new MazeMods(*this);
			break;
		case Infinite_:
			GamePtr = new UnlimitednessMapMods(*this);
			break;
		case TankTrouble_:
			GamePtr = new TankTrouble(*this);
			break;
		case PhysicsTest_:
			GamePtr = new PhysicsTest(*this);
			break;
		case SoloudTest_:
			GamePtr = new SoloudTest(*this);
			break;
		case RadianceCascades_:
			GamePtr = new RadianceCascades(*this);
			break;
		case FruitNinja_:
			GamePtr = new FruitNinja(*this);
			break;
		case WFCTest_:
		GamePtr = new WFCTest(*this);
		break;
	case BlockWorld_:
		GamePtr = new BlockWorld(*this);
		break;
	default:
		break;
		}
		TOOL::FPStime = 0.00001;
		return GamePtr;
	}

	void Application::DeleteGame(GameModsEnum Game) {
		LOGD("DeleteGame: mode=%d", (int)Game);
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
		case PhysicsTest_:
			delete (PhysicsTest*)mGameMods;
			break;
		case SoloudTest_:
			delete (SoloudTest*)mGameMods;
			break;
		case RadianceCascades_:
			delete (RadianceCascades*)mGameMods;
			break;
		case FruitNinja_:
			delete (FruitNinja*)mGameMods;
			break;
		case WFCTest_:
		delete (WFCTest*)mGameMods;
		break;
	case BlockWorld_:
		delete (BlockWorld*)mGameMods;
		break;
	default:
			break;
		}
		mGameMods = nullptr;
	}

	//窗口的初始化
	void Application::initWindow() {
		LOGD("initWindow: %dx%d fullscreen=%d", Global::mWidth, Global::mHeight, Global::FullScreen);
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
		LOGD("initVulkan: validationLayer=%d", Global::VulKanValidationLayer);
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
		mCamera->setPerpective(45.0f, (float)Global::mWidth / (float)Global::mHeight, 0.1f, 2000.0f);
		mRenderPass = new VulKan::RenderPass(mDevice);//创建GPU画布描述
		mImGuuiRenderPass = new VulKan::RenderPass(mDevice);
		mImGuiLoadRenderPass = new VulKan::RenderPass(mDevice);
		mSampler = new VulKan::Sampler(mDevice);//采样器
		createRenderPass();//设置GPU画布描述
		mSwapChain->createFrameBuffers(mRenderPass);//把GPU画布描述传给交换链生成GPU画布

		mPipelineS = new VulKan::PipelineS(mDevice, mRenderPass);//创建渲染管线集合

		Global::CommandBufferSize = mSwapChain->getImageCount();
		Global::MainCommandBufferS = new std::atomic<bool>[Global::CommandBufferSize];
		//主指令缓存录制
		mCommandBuffers.resize(mSwapChain->getImageCount());//用来给每个GPU画布都绑上单独的 主CommandBuffer
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			mCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mCommandPool);//创建主指令缓存
			Global::MainCommandBufferS[i].store(true, std::memory_order_release);
		}

		wThreadCommandBufferS = &ThreadCommandBufferS;

		createSyncObjects();//创建信号量（用于渲染同步）

		mImGuiLoadFrameBuffers.resize(mSwapChain->getImageCount());
		for (uint32_t i = 0; i < mSwapChain->getImageCount(); ++i) {
			std::array<VkImageView, 3> attachments = {
				mSwapChain->getImageView(i),
				mSwapChain->getMutiSampleImageView(i),
				mSwapChain->getDepthImageView(i)
			};
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mImGuiLoadRenderPass->getRenderPass();
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = mSwapChain->getExtent().width;
			framebufferInfo.height = mSwapChain->getExtent().height;
			framebufferInfo.layers = 1;
			if (vkCreateFramebuffer(mDevice->getDevice(), &framebufferInfo, nullptr, &mImGuiLoadFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Error: failed to create ImGui load frame buffer");
			}
		}

		VkEventCreateInfo eventCreateInfo{};
		eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		if (vkCreateEvent(mDevice->getDevice(), &eventCreateInfo, nullptr, &mGameToImGuiEvent) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to create GameToImGui event");
		}
	}
	
	void GAME::Application::initImGui()
	{
		LOGD("initImGui started");
		//准备填写需要的信息
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.ApiVersion = VK_API_VERSION_1_1;
		init_info.Instance = mInstance->getInstance();
		init_info.PhysicalDevice = mDevice->getPhysicalDevice();
		init_info.Device = mDevice->getDevice();
		init_info.QueueFamily = mDevice->getGraphicQueueFamily().value();
		init_info.Queue = mDevice->getGraphicQueue();
		init_info.DescriptorPool = VK_NULL_HANDLE;
		init_info.MinImageCount = 3;
		init_info.ImageCount = mSwapChain->getImageCount();
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.PipelineInfoMain.Subpass = 0;
		init_info.PipelineInfoMain.MSAASamples = mDevice->getMaxUsableSampleCount();
		init_info.UseDynamicRendering = false;
		init_info.Allocator = nullptr;
		init_info.CheckVkResultFn = check_vk_result;//错误处理
		

		mImGuuiCommandPool = new VulKan::CommandPool(mDevice);
		mImGuuiCommandBuffers = new VulKan::CommandBuffer(mDevice, mImGuuiCommandPool);
		mImGuuiFence = new VulKan::Fence(mDevice);

		InterFace = new ImGuiInterFace(mDevice, mWindow, init_info, mRenderPass, mImGuuiCommandBuffers,
			new ImGuiTexture(mDevice, mSwapChain, mCommandPool,mSampler, 1),
			mSwapChain->getImageCount());
	}

	

	//初始化游戏
	void Application::initGame() {
		LOGI("initGame: step 1 - creating Camera Buffers");
		mCameraVPMatricesBuffer.resize(mSwapChain->getImageCount());
		for (auto i = 0; i < mCameraVPMatricesBuffer.size(); ++i) {
			mCameraVPMatricesBuffer[i] = VulKan::Buffer::createUniformBuffer(mDevice, sizeof(VPMatrices), nullptr);
			VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[i]->getupdateBufferByMap();
			mVPMatrices->mProjectionMatrix = mCamera->getProjectMatrix();
			mCameraVPMatricesBuffer[i]->endupdateBufferByMap();
		}

		LOGI("initGame: step 2 - creating ParticleSystem");
		const int particleCount = 1000;
		LOGI("initGame: step 2 - particleCount=%d (reduced on Android for mobile GPU compatibility)", particleCount);
		mParticleSystem = new ParticleSystem(mDevice, particleCount);
		mParticleSystem->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mParticleSystem->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));
		LOGI("initGame: step 2 - ParticleSystem done");

		mParticlesSpecialEffect = new ParticlesSpecialEffect(mParticleSystem, particleCount);

		LOGI("initGame: step 3 - creating SquarePhysics");
		mSquarePhysics = new SquarePhysics::SquarePhysics(400, 400);

		LOGI("initGame: step 4 - creating Arms");
		mArms = new Arms(mParticleSystem, particleCount);
		mArms->SetSpecialEffect(mParticlesSpecialEffect);
		mArms->SetSquarePhysics(mSquarePhysics);

		LOGI("initGame: step 5 - creating GIF TextureLibrary");
		mGIFTextureLibrary = new TextureLibrary(mDevice, mCommandPool, mSampler, "./Resource/GifTexture/");
		LOGI("initGame: step 5 - GIF TextureLibrary done");

		LOGI("initGame: step 6 - creating Material TextureLibrary");
		mTextureLibrary = new TextureLibrary(mDevice, mCommandPool, mSampler, "./Resource/Material/", false);
		LOGI("initGame: step 6 - Material TextureLibrary done");

		Opcode::OpApplication = this;
		LOGI("initGame: ALL DONE");
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

		VkAttachmentDescription imguiFinalAttachmentDes = finalAttachmentDes;
		imguiFinalAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		mImGuuiRenderPass->addAttachment(imguiFinalAttachmentDes);
		MutiAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		mImGuuiRenderPass->addAttachment(MutiAttachmentDes);
		mImGuuiRenderPass->addAttachment(depthAttachmentDes);
		mImGuuiRenderPass->addSubPass(subPass);
		mImGuuiRenderPass->addDependency(dependency);
		mImGuuiRenderPass->buildRenderPass();

		VkAttachmentDescription loadFinalAttachmentDes = finalAttachmentDes;
		loadFinalAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		loadFinalAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		mImGuiLoadRenderPass->addAttachment(loadFinalAttachmentDes);

		VkAttachmentDescription loadMutiAttachmentDes{};
		loadMutiAttachmentDes.format = mSwapChain->getFormat();
		loadMutiAttachmentDes.samples = mDevice->getMaxUsableSampleCount();
		loadMutiAttachmentDes.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		loadMutiAttachmentDes.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		loadMutiAttachmentDes.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		loadMutiAttachmentDes.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		loadMutiAttachmentDes.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		loadMutiAttachmentDes.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		mImGuiLoadRenderPass->addAttachment(loadMutiAttachmentDes);

		mImGuiLoadRenderPass->addAttachment(depthAttachmentDes);
		mImGuiLoadRenderPass->addSubPass(subPass);
		mImGuiLoadRenderPass->addDependency(dependency);
		mImGuiLoadRenderPass->buildRenderPass();
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
			Global::MainCommandBufferS[i].store(true, std::memory_order_release);
			return;
		}
		else if(!Global::MonitorCompatibleMode){
			mImGuuiFence->block();

			mImGuuiCommandBuffers->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			VkMemoryBarrier memoryBarrier{};
			memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			vkCmdWaitEvents(mImGuuiCommandBuffers->getCommandBuffer(), 1, &mGameToImGuiEvent,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				1, &memoryBarrier, 0, nullptr, 0, nullptr);

			VkRenderPassBeginInfo imguiRenderBeginInfo{};
			imguiRenderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			imguiRenderBeginInfo.renderPass = mImGuiLoadRenderPass->getRenderPass();
			imguiRenderBeginInfo.framebuffer = mImGuiLoadFrameBuffers[i];
			imguiRenderBeginInfo.renderArea.offset = { 0, 0 };
			imguiRenderBeginInfo.renderArea.extent = mSwapChain->getExtent();
			std::vector<VkClearValue> imguiClearValues{};
			VkClearValue imguiFinalClear{};
			imguiFinalClear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
			imguiClearValues.push_back(imguiFinalClear);
			VkClearValue imguiMutiClear{};
			imguiMutiClear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
			imguiClearValues.push_back(imguiMutiClear);
			VkClearValue imguiDepthClear{};
			imguiDepthClear.depthStencil = { 1.0f, 0 };
			imguiClearValues.push_back(imguiDepthClear);
			imguiRenderBeginInfo.clearValueCount = static_cast<uint32_t>(imguiClearValues.size());
			imguiRenderBeginInfo.pClearValues = imguiClearValues.data();

			mImGuuiCommandBuffers->beginRenderPass(imguiRenderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mImGuuiCommandBuffers->getCommandBuffer());
			mImGuuiCommandBuffers->endRenderPass();
			mImGuuiCommandBuffers->end();

			if (Global::MainCommandBufferS[i].load(std::memory_order_acquire)) {
				mCommandBuffers[i]->begin();

				renderBeginInfo.renderPass = mRenderPass->getRenderPass();
				mCommandBuffers[i]->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

				mGameMods->GameCommandBuffers(i);

				if (!ThreadCommandBufferS.empty()) {
					vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), ThreadCommandBufferS.size(), ThreadCommandBufferS.data());
				}

				mCommandBuffers[i]->endRenderPass();

				vkCmdSetEvent(mCommandBuffers[i]->getCommandBuffer(), mGameToImGuiEvent, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

				mCommandBuffers[i]->end();
				Global::MainCommandBufferS[i].store(false, std::memory_order_release);
			}
			return;
		}

		if (!Global::MainCommandBufferS[i].load(std::memory_order_acquire) && !Global::MonitorCompatibleMode) {
			return;
		}

		renderBeginInfo.renderPass = mRenderPass->getRenderPass();//获得画布信息

		mCommandBuffers[i]->begin();//开始录制主指令

		mCommandBuffers[i]->beginRenderPass(renderBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);//关于画布信息  !!!这个只有主指令才有

		mGameMods->GameCommandBuffers(i);

		if (Global::MonitorCompatibleMode) {
			ThreadCommandBufferS.push_back(InterFace->GetCommandBuffer(i, InheritanceInfo));
		}
		
		if (!ThreadCommandBufferS.empty()) {
			vkCmdExecuteCommands(mCommandBuffers[i]->getCommandBuffer(), ThreadCommandBufferS.size(), ThreadCommandBufferS.data());
		}

		mCommandBuffers[i]->endRenderPass();//结束RenderPass

		mCommandBuffers[i]->end();//结束

		Global::MainCommandBufferS[i].store(false, std::memory_order_release);
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
		LOGD("recreateSwapChain started");
		TOOL::mTimer->MomentTiming(u8"窗口重构 ");

		int width = 0, height = 0;
#if defined(_WIN32)
		glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwWaitEvents();
			glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		}
#elif defined(__ANDROID__)
		width = mWindow->getWidth();
		height = mWindow->getHeight();
		if (width == 0 || height == 0) {
			return;
		}
#endif
		vkDeviceWaitIdle(mDevice->getDevice());

		mSwapChain->~SwapChain();
		mSwapChain->StructureSwapChain();
		Global::mWidth = mSwapChain->getExtent().width;
		Global::mHeight = mSwapChain->getExtent().height;

		for (auto& fb : mImGuiLoadFrameBuffers) {
			if (fb != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(mDevice->getDevice(), fb, nullptr);
				fb = VK_NULL_HANDLE;
			}
		}
		mImGuiLoadFrameBuffers.clear();

		mRenderPass->~RenderPass();
		mImGuuiRenderPass->~RenderPass();
		mImGuiLoadRenderPass->~RenderPass();
		createRenderPass();
		mSwapChain->createFrameBuffers(mRenderPass);

		mImGuiLoadFrameBuffers.resize(mSwapChain->getImageCount());
		for (uint32_t i = 0; i < mSwapChain->getImageCount(); ++i) {
			std::array<VkImageView, 3> attachments = {
				mSwapChain->getImageView(i),
				mSwapChain->getMutiSampleImageView(i),
				mSwapChain->getDepthImageView(i)
			};
			VkFramebufferCreateInfo fbInfo{};
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.renderPass = mImGuiLoadRenderPass->getRenderPass();
			fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			fbInfo.pAttachments = attachments.data();
			fbInfo.width = mSwapChain->getExtent().width;
			fbInfo.height = mSwapChain->getExtent().height;
			fbInfo.layers = 1;
			if (vkCreateFramebuffer(mDevice->getDevice(), &fbInfo, nullptr, &mImGuiLoadFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Error: failed to recreate ImGui load frame buffer");
			}
		}
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
		mParticleSystem->resetThreadCommandPools();
		mParticleSystem->ThreadUpdateCommandBuffer();
		for (size_t i = 0; i < mSwapChain->getImageCount(); ++i)
		{
			Global::MainCommandBufferS[i].store(true, std::memory_order_release);
		}
		TOOL::mTimer->MomentEnd();
	}

	//主循环main
	void Application::mainLoop() {
		LOGD("mainLoop started");
		{
			auto& engine = GAME::Audio::AudioEngine::Get();
			if (!engine.Initialize())
			{
				LOGE("AudioEngine initialization failed");
			}
			else
			{
				engine.GetSpatial().LoadAllResources();
				if (engine.IsMidiFontLoaded())
				{
					engine.GetSpatial().PlaySimple(u8"夜に駆ける", GAME::Audio::SimpleSoundType::MIDI, true, Global::MusicVolume);
				}
			}
		}
		while (!mWindow->shouldClose()) {//窗口被关闭结束循环
			GAME::Audio::AudioEngine::Get().Update(TOOL::FPStime);
			PlayerForce = { 0,0 };
			mWindow->pollEvents();//GLFW轮询事件
			// 更新 ImGui 的 MousePos
			ImGuiIO& io = ImGui::GetIO();
#if defined(_WIN32)
			glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
#elif defined(__ANDROID__)
			// CursorPosX, CursorPosY 由 JNI 触摸事件填充
#endif
			io.MousePos.x = CursorPosX;
			io.MousePos.y = CursorPosY;
			// 更新 ImGui 的 鼠标滚轮的值
			io.MouseWheel = -mWindow->MouseScroll;
			mWindow->MouseScroll = 0;

			if (InterFace->GetInterFaceBool()) {
				mWindow->ImGuiKeyBoardEvent();//监听键盘
#if defined(__ANDROID__)
				io.AddMousePosEvent((float)CursorPosX, (float)CursorPosY);
				if (mPendingMouseDown.exchange(false)) {
					io.AddMouseButtonEvent(0, true);
				} else if (mPendingMouseUp.exchange(false)) {
					io.AddMouseButtonEvent(0, false);
				}
#endif
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
			if (mGameMods != nullptr) {
				mGameMods->MouseMove(CursorPosX, CursorPosY);
#if defined(_WIN32)
				mGameMods->MouseButton(MouseBtn::Left,
					glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
						? InputState::Down : InputState::Up);
				mGameMods->MouseButton(MouseBtn::Right,
					glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
						? InputState::Down : InputState::Up);
				mGameMods->MouseButton(MouseBtn::Middle,
					glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS
						? InputState::Down : InputState::Up);
#elif defined(__ANDROID__)
				mGameMods->MouseButton(MouseBtn::Left,
					Global::TouchState == TouchStateEnum::PrimaryDown
						? InputState::Down : InputState::Up);
				mGameMods->MouseButton(MouseBtn::Right,
					Global::TouchState == TouchStateEnum::SecondaryDown
						? InputState::Down : InputState::Up);
				mGameMods->MouseButton(MouseBtn::Middle,
					Global::TouchState == TouchStateEnum::TertiaryDown
						? InputState::Down : InputState::Up);
#endif
			}
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
		GAME::Audio::AudioEngine::Get().Shutdown();
		//等待设备所以命令执行完毕才可以开始销毁，
		vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕
	}

	void Application::GameLoop() {
		
		mGameMods->GameLoop(mCurrentFrame);

		int winwidth, winheight;
#if defined(_WIN32)
		glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
#elif defined(__ANDROID__)
		winwidth = mWindow->getWidth();
		winheight = mWindow->getHeight();
#endif
		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;

		
		
		Global::ClickWindow = false;
		//ImGui显示录制
		if (Global::Monitor) {
			ImGui_ImplVulkan_NewFrame();
#if defined(_WIN32)
			ImGui_ImplGlfw_NewFrame();
#elif defined(__ANDROID__)
			ImGui_ImplAndroid_NewFrame();
			{
				ImGuiIO& io = ImGui::GetIO();
				io.AddMousePosEvent((float)CursorPosX, (float)CursorPosY);
				if (mPendingMouseDown.exchange(false)) {
					io.AddMouseButtonEvent(0, true);
				} else if (mPendingMouseUp.exchange(false)) {
					io.AddMouseButtonEvent(0, false);
				}
			}
#endif
			ImGui::NewFrame();

			ImGui::PushFont(InterFace->Font);
			
			ImGui::Begin(u8"监视器 ");
			ImVec2 window_size = ImGui::GetWindowSize();
			if ((CursorPosX < window_size.x) && (CursorPosY < window_size.y)) {
				Global::ClickWindow = true;
			}
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::Text(u8"相机位置：%10.1f  |  %10.1f  |  %10.1f", mCamera->getCameraPos().x, mCamera->getCameraPos().y, mCamera->getCameraPos().z);
			ImGui::Text(u8"鼠标角度：%10.3f", m_angle * 180 / M_PI);
			ImGui::Text(u8"鼠标获取位置：%10.3f - %10.3f - %10.3f", huoqdedian.x, huoqdedian.y, huoqdedian.z);
			ImGui::Text(u8"攻击模式：%d (1 / 2 上下切换) ", AttackType);
			//ImGui::Text(u8"玩家速度：%10.3f  |  %10.3f", mGamePlayer->GetObjectCollision()->GetSpeed().x, mGamePlayer->GetObjectCollision()->GetSpeed().y);
			ImGui::Text(u8"剩余粒子：%d", mParticleSystem->mParticle->GetNumber());
			InterFace->ImGuiShowFPS();
			InterFace->ImGuiShowTiming();
			ImGui::Text(u8"如果只看得到监视窗口去设置中关掉监视器 ! ");
			ImGui::End();

			mGameMods->GameUI();

			if (Global::ConsoleBool) {
				InterFace->ConsoleInterface();//控制台
			}
			InterFace->DisplayTextS();

			//ImGui::ShowDemoWindow();
			ImGui::PopFont();
			ImGui::Render();
		} else if (Global::ConsoleBool) {
			ImGui_ImplVulkan_NewFrame();
#if defined(_WIN32)
			ImGui_ImplGlfw_NewFrame();
#elif defined(__ANDROID__)
			ImGui_ImplAndroid_NewFrame();
			{
				ImGuiIO& io = ImGui::GetIO();
				io.AddMousePosEvent((float)CursorPosX, (float)CursorPosY);
				if (mPendingMouseDown.exchange(false)) {
					io.AddMouseButtonEvent(0, true);
				} else if (mPendingMouseUp.exchange(false)) {
					io.AddMouseButtonEvent(0, false);
				}
			}
#endif
			ImGui::NewFrame();
			ImGui::PushFont(InterFace->Font);
			InterFace->ConsoleInterface();//控制台
			ImGui::PopFont();
			ImGui::Render();
		}
		
	}

	void Application::Render() {
		mFences[mCurrentFrame]->block();
		
		TOOL::mTimer->StartTiming(u8"获取交换链图像 ");
		uint32_t imageIndex{ 0 };
		VkResult result = vkAcquireNextImageKHR(
			mDevice->getDevice(),
			mSwapChain->getSwapChain(),
			UINT64_MAX,
			mImageAvailableSemaphores[mCurrentFrame]->getSemaphore(),
			VK_NULL_HANDLE,
			&imageIndex);

		static int renderLogCount = 0;
		if (renderLogCount < 5) {
			LOGI("Render: vkAcquireNextImageKHR result=%d, imageIndex=%u", (int)result, imageIndex);
			renderLogCount++;
		}

	
		//窗体发生了尺寸变化 （窗口大小改变了话，指令要全部重新录制）
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
			return;
		}//VK_SUBOPTIMAL_KHR得到了一张认为可用的图像，但是表面格式不一定匹配
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Error: failed to acquire next image");
		}

		TOOL::mTimer->StartEnd();

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

		VkSemaphore renderFinishedSemaphore = mRenderFinishedSemaphores[mCurrentFrame]->getSemaphore();
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
		bool imguiSeparateSubmit = (!InterFace->GetInterFaceBool() && Global::Monitor) && !Global::MonitorCompatibleMode;

		if (imguiSeparateSubmit) {
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
		} else {
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		mFences[mCurrentFrame]->resetFence();
		TOOL::mTimer->StartTiming(u8"等待渲染完成 ");
		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to submit renderCommand");
		}
		TOOL::mTimer->StartEnd();

		if (imguiSeparateSubmit)
		{
			VkSubmitInfo imguiSubmitInfo{};
			imguiSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			imguiSubmitInfo.commandBufferCount = 1;
			VkCommandBuffer imguiCmdBuf = mImGuuiCommandBuffers->getCommandBuffer();
			imguiSubmitInfo.pCommandBuffers = &imguiCmdBuf;
			imguiSubmitInfo.signalSemaphoreCount = 1;
			imguiSubmitInfo.pSignalSemaphores = signalSemaphores;

			mImGuuiFence->resetFence();
			if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &imguiSubmitInfo, mImGuuiFence->getFence()) != VK_SUCCESS) {
				throw std::runtime_error("Error:failed to submit ImGui renderCommand");
			}
		}

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
#if defined(__ANDROID__)
		if (mWindow->mWindowResized) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
		}
#else
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mWindow->mWindowResized) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to present");
		}
#endif

		mCurrentFrame = (mCurrentFrame + 1) % mSwapChain->getImageCount();
	}

	//回收资源
	void Application::cleanUp() {
		LOGI("cleanUp started");
		mInitialized = false;

		if (mDevice) {
			vkDeviceWaitIdle(mDevice->getDevice());
		}

		delete mParticleSystem;
		mParticleSystem = nullptr;
		delete mSquarePhysics;
		mSquarePhysics = nullptr;
		delete mParticlesSpecialEffect;
		mParticlesSpecialEffect = nullptr;
		delete mArms;
		mArms = nullptr;

		if (mSwapChain) {
			for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
				delete mCameraVPMatricesBuffer[i];
				delete mCommandBuffers[i];
				delete mImageAvailableSemaphores[i];
				delete mRenderFinishedSemaphores[i];
				delete mFences[i];
			}
		}
		mCameraVPMatricesBuffer.clear();
		mCommandBuffers.clear();
		mImageAvailableSemaphores.clear();
		mRenderFinishedSemaphores.clear();
		mFences.clear();

		delete mImGuuiCommandBuffers;
		mImGuuiCommandBuffers = nullptr;
		delete mImGuuiCommandPool;
		mImGuuiCommandPool = nullptr;
		delete mImGuuiFence;
		mImGuuiFence = nullptr;

		if (mGameToImGuiEvent != VK_NULL_HANDLE) {
			vkDestroyEvent(mDevice->getDevice(), mGameToImGuiEvent, nullptr);
			mGameToImGuiEvent = VK_NULL_HANDLE;
		}

		for (auto& fb : mImGuiLoadFrameBuffers) {
			if (fb != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(mDevice->getDevice(), fb, nullptr);
				fb = VK_NULL_HANDLE;
			}
		}
		mImGuiLoadFrameBuffers.clear();
		delete mImGuiLoadRenderPass;
		mImGuiLoadRenderPass = nullptr;

		delete mImGuuiRenderPass;
		mImGuuiRenderPass = nullptr;
		delete InterFace;
		InterFace = nullptr;

		delete mSampler;
		mSampler = nullptr;
		delete mPipelineS;
		mPipelineS = nullptr;
		delete mRenderPass;
		mRenderPass = nullptr;
		delete mSwapChain;
		mSwapChain = nullptr;
		delete mCommandPool;
		mCommandPool = nullptr;
		delete mDevice;
		mDevice = nullptr;
		delete mWindowSurface;
		mWindowSurface = nullptr;
		delete mInstance;
		mInstance = nullptr;
		delete mCamera;
		mCamera = nullptr;
		delete mWindow;
		mWindow = nullptr;
	}

#if defined(__ANDROID__)
	void Application::initBeforeSurface() {
		LOGD("initBeforeSurface started");
		TOOL::mTimer->MomentTiming(u8"总初始化 ");
		TOOL::mTimer->MomentTiming(u8"初始化窗口 ");
		initWindow();
		TOOL::mTimer->MomentEnd();
		TOOL::mTimer->MomentEnd();
	}

	void Application::initAfterSurface(ANativeWindow* nativeWindow) {
		LOGI("initAfterSurface: START (window=%p)", nativeWindow);
		mWindow->setAndroidWindow(nativeWindow);

		TOOL::mTimer->MomentTiming(u8"初始化Vulkan ");
		initVulkan();
		TOOL::mTimer->MomentEnd();
		LOGI("initAfterSurface: initVulkan done");

		TOOL::mTimer->MomentTiming(u8"初始化ImGui ");
		initImGui();
		TOOL::mTimer->MomentEnd();
		LOGI("initAfterSurface: initImGui done");

		TOOL::mTimer->MomentTiming(u8"初始化游戏 ");
		initGame();
		TOOL::mTimer->MomentEnd();
		LOGI("initAfterSurface: initGame done");

		mInitialized = true;
		LOGI("initAfterSurface: mInitialized = true, initialization COMPLETE!");

		auto& engine = GAME::Audio::AudioEngine::Get();
		if (!engine.Initialize())
		{
			LOGE("initAfterSurface: AudioEngine initialization failed");
		}
		else
		{
			engine.GetSpatial().LoadAllResources();
			if (engine.IsMidiFontLoaded())
			{
				engine.GetSpatial().PlaySimple(u8"夜に駆ける", GAME::Audio::SimpleSoundType::MIDI, true, Global::MusicVolume);
			}
		}
	}

	void Application::frameStep() {
		if (!mInitialized) {
			return;
		}
		if (mWindow->shouldClose()) return;

		static bool firstFrame = true;
		if (firstFrame) {
			LOGI("frameStep: first frame rendering");
			firstFrame = false;
		}

		GAME::Audio::AudioEngine::Get().Update(TOOL::FPStime);
		PlayerForce = { 0,0 };
		mWindow->pollEvents();
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos.x = CursorPosX;
		io.MousePos.y = CursorPosY;
		io.MouseWheel = -mWindow->MouseScroll;
		mWindow->MouseScroll = 0;

		if (InterFace->GetInterFaceBool()) {
			mWindow->ImGuiKeyBoardEvent();
#if defined(__ANDROID__)
			io.AddMousePosEvent((float)CursorPosX, (float)CursorPosY);
			if (mPendingMouseDown.exchange(false)) {
				io.AddMouseButtonEvent(0, true);
			} else if (mPendingMouseUp.exchange(false)) {
				io.AddMouseButtonEvent(0, false);
			}
#endif
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
			mWindow->processEvent();
			if (mGameMods != nullptr) {
				mGameMods->MouseMove(CursorPosX, CursorPosY);
				mGameMods->MouseButton(MouseBtn::Left,
					Global::TouchState == TouchStateEnum::PrimaryDown
						? InputState::Down : InputState::Up);
				mGameMods->MouseButton(MouseBtn::Right,
					Global::TouchState == TouchStateEnum::SecondaryDown
						? InputState::Down : InputState::Up);
				mGameMods->MouseButton(MouseBtn::Middle,
					Global::TouchState == TouchStateEnum::TertiaryDown
						? InputState::Down : InputState::Up);
				int scrollDelta = mPendingScroll.exchange(0);
				if (scrollDelta != 0) {
					mGameMods->MouseRoller(scrollDelta > 0 ? 1 : -1);
				}
			}
			GameLoop();
			TOOL::mTimer->StartEnd();

			TOOL::mTimer->RefreshTiming();
		}


		TOOL::mTimer->StartTiming(u8"TCP ");
		if (mGameMods != nullptr) {
			mGameMods->GameTCPLoop();
		}
		TOOL::mTimer->StartEnd();

		TOOL::FPS();

		TOOL::mTimer->StartTiming(u8"画面渲染 ");
		Render();
		TOOL::mTimer->StartEnd();
	}
#endif
}