#include "Interface.h"
#include "../SoundEffect/SoundEffect.h"
#include "../Opcode/Opcode.h"
#include "../Tool/trie.h"
#include "../GlobalStructural.h"

#include "../NetworkTCP/Server.h"
#include "../NetworkTCP/Client.h"

namespace GAME {
	ImGuiInterFace::ImGuiInterFace(
		VulKan::Device* device, 
		VulKan::Window* Win, 
		ImGui_ImplVulkan_InitInfo Info, 
		VulKan::RenderPass* Pass,
		VulKan::CommandBuffer* commandbuffer,
		int FormatCount
	)
	{
		mWindown = Win;
		mDevice = device;
		mFormatCount = FormatCount;
		mInfo = Info;
		mRenderPass = Pass;
		mCommandBuffer = commandbuffer;

		StructureImGuiInterFace();
	}

	void ImGuiInterFace::StructureImGuiInterFace() {
		// 安装 Dear ImGui 上下文
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// 安装 Dear ImGui 风格
		ImGui::StyleColorsDark();


		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& Style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			Style.WindowRounding = 0.0f;
			Style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// 设置字体
		ImFontConfig Font_cfg;
		Font_cfg.FontDataOwnedByAtlas = false;
		//Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 16.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		Font = io.Fonts->AddFontFromFileTTF("./Minecraft_AE.ttf", 16, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
		Font2 = io.Fonts->AddFontFromFileTTF("./Minecraft_AE.ttf", 32, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());


		


		// 安装 Platform/渲染器 backends
		ImGui_ImplGlfw_InitForVulkan(mWindown->getWindow(), true);

		// Create Descriptor Pool
		{
			VkDescriptorPoolSize pool_sizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
			pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			if (vkCreateDescriptorPool(mDevice->getDevice(), &pool_info, nullptr, &g_DescriptorPool)) {
				throw std::runtime_error("Error: initImGui DescriptorPool 生成失败 ");
			}
		}

		mInfo.DescriptorPool = g_DescriptorPool;
		mInfo.MinImageCount = g_MinImageCount;

		ImGui_ImplVulkan_Init(&mInfo, mRenderPass->getRenderPass());

		// 上传 DearImgui 字体
		mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);//开始录制要提交的指令
		ImGui_ImplVulkan_CreateFontsTexture(mCommandBuffer->getCommandBuffer());//要录制的内容
		mCommandBuffer->end();//结束录制要提交的指令
		mCommandBuffer->submitSync(mDevice->getGraphicQueue());//等待指令上传结束
		ImGui_ImplVulkan_DestroyFontUploadObjects();


		//ImGui 风格设置
		auto Color = Style.Colors;


		Style.ChildRounding = 0.0f;
		Style.FrameRounding = 0.0f;//是否圆润按键

		Color[ImGuiCol_Button] = ImColor(10, 105, 56, 255);//按键颜色
		Color[ImGuiCol_ButtonHovered] = ImColor(30, 125, 76, 255);//鼠标悬停颜色
		Color[ImGuiCol_ButtonActive] = ImColor(0, 95, 46, 255);//鼠标点击颜色

		Color[ImGuiCol_FrameBg] = ImColor(54, 54, 54, 150);
		Color[ImGuiCol_FrameBgActive] = ImColor(42, 42, 42, 150);
		Color[ImGuiCol_FrameBgHovered] = ImColor(100, 100, 100, 150);

		Color[ImGuiCol_CheckMark] = ImColor(10, 105, 56, 255);

		Color[ImGuiCol_SliderGrab] = ImColor(10, 105, 56, 255);
		Color[ImGuiCol_SliderGrabActive] = ImColor(0, 95, 46, 255);

		Color[ImGuiCol_Header] = ImColor(10, 105, 56, 255);
		Color[ImGuiCol_HeaderHovered] = ImColor(30, 125, 76, 255);
		Color[ImGuiCol_HeaderActive] = ImColor(0, 95, 46, 255);

		ImGuiCommandPoolS = new VulKan::CommandPool * [mFormatCount];
		ImGuiCommandBufferS = new VulKan::CommandBuffer * [mFormatCount];
		for (int i = 0; i < mFormatCount; i++)
		{
			ImGuiCommandPoolS[i] = new VulKan::CommandPool(mDevice);
			ImGuiCommandBufferS[i] = new VulKan::CommandBuffer(mDevice, ImGuiCommandPoolS[i], true);
		}


		mChatBoxStr = new QueueData<ChatBoxStr>(100);
	}

	ImGuiInterFace::~ImGuiInterFace()
	{
		//销毁ImGui
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		if (g_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(mDevice->getDevice(), g_DescriptorPool, nullptr);//销毁专门创建给ImGui用的DescriptorPool
		}

		for (int i = 0; i < mFormatCount; i++)
		{
			delete ImGuiCommandBufferS[i];
			delete ImGuiCommandPoolS[i];
		}
		delete ImGuiCommandBufferS;
		delete ImGuiCommandPoolS;
	}

	void ImGuiInterFace::InterFace()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::PushFont(Font2);
		switch (InterfaceIndexes)
		{
		case 0:
			MainInterface();
			break;
		case 1:
			ViceInterface();
			break;
		case 2:
			MultiplePeopleInterface();
			break;
		case 3:
			SetInterface();
			break;
		}
		ImGui::PopFont();
		ImGui::Render();
	}

	VkCommandBuffer ImGuiInterFace::GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info) {
		ImGuiCommandBufferS[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, info);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ImGuiCommandBufferS[i]->getCommandBuffer());
		ImGuiCommandBufferS[i]->end();
		return ImGuiCommandBufferS[i]->getCommandBuffer();
	}

	void ImGuiInterFace::MainInterface()
	{
		ImGui::Begin(u8"游戏ESC界面 ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));
		ImGui::SetWindowFontScale(1.0f);

		int gao = (Global::mHeight / 7) - 4;
		float kuan = Global::mWidth / 3;
		float Bgao = Global::mHeight / 10;

		ImGui::BeginTable("table_row_height", 1);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();


		ImGui::SetWindowFontScale(4.0f);

		ImVec2 textSize = ImGui::CalcTextSize(u8"素  净");
		float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

		float posX = (windowWidth - textSize.x) * 0.5f;
		ImGui::SetCursorPosX(posX);
		ImGui::Text(u8"素  净");




		ImGui::SetWindowFontScale(1.0f);


		posX = (windowWidth - kuan) * 0.5f;




		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"开始游戏 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterFaceBool = false;
			Global::GameResourceLoadingBool = true;//加载游戏资源
			InterfaceIndexes = ViceInterface_Enum;
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"多人游戏 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterfaceIndexes = MultiplePeopleInterface_Enum;
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"游戏设置 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetBool = true;
			InterfaceIndexes = SetInterface_Enum;
			PreviousLayerInterface = MainInterface_Enum;
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"退出游戏 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			mWindown->WindowClose();
		}

		ImGui::EndTable();
		
		ImGui::End();
	}

	void ImGuiInterFace::ViceInterface()
	{
		ImGui::Begin(u8"游戏时界面 ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));

		int gao = (Global::mHeight / 7) - 4;
		float kuan = Global::mWidth / 3;
		float Bgao = Global::mHeight / 10;
		float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

		float posX = (windowWidth - kuan) * 0.5f;
		

		ImGui::BeginTable("table_row_height2", 1);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"继续游戏 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterFaceBool = false;
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"游戏设置 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetBool = true;
			InterfaceIndexes = SetInterface_Enum;
			PreviousLayerInterface = ViceInterface_Enum;
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		
		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"返回主页 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			Global::GameResourceUninstallBool = true;//返回主页 随便卸载资源
			InterfaceIndexes = MainInterface_Enum;
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"退出游戏 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			mWindown->WindowClose();
		}

		ImGui::EndTable();

		ImGui::End();
	}

	void ImGuiInterFace::MultiplePeopleInterface() {
		ImGui::Begin(u8"多人界面 ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));

		int gao = (Global::mHeight / 7) - 4;
		float kuan = Global::mWidth / 3;
		float Bgao = Global::mHeight / 10;
		float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

		float posX = (windowWidth - kuan) * 0.5f;

		ImGui::BeginTable("table_row_height2", 1);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"服务器 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterFaceBool = false;
			InterfaceIndexes = ViceInterface_Enum;
			Global::MultiplePeopleMode = true;
			Global::ServerOrClient = true;
			Global::GameResourceLoadingBool = true;//加载游戏资源
			server::GetServer();
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"客户端 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterFaceBool = false;
			InterfaceIndexes = ViceInterface_Enum;
			Global::MultiplePeopleMode = true;
			Global::ServerOrClient = false;
			Global::GameResourceLoadingBool = true;//加载游戏资源
			client::GetClient();
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, gao);
		ImGui::TableNextColumn();

		ImGui::SetCursorPosX(posX);
		if (ImGui::Button(u8"返回 ", { kuan, Bgao })) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterfaceIndexes = MainInterface_Enum;
		}

		ImGui::EndTable();

		ImGui::End();
	}

	void ImGuiInterFace::SetInterface() {
		static int SetServerPort;
		static int SetClientPort;
		static char SetClientIP[16];
		static bool SetVulKanValidationLayer;
		static bool SetMonitor;
		static bool SetFullScreen;
		static float SetMusicVolume;
		static float SetSoundEffectsVolume;
		static bool SetMonitorCompatibleMode;

		static char SetKeyW[2];//截止符
		static char SetKeyS[2];
		static char SetKeyA[2];
		static char SetKeyD[2];

		struct TextFilters
		{
			// Return 0 (pass) if the character is 'i' or 'm' or 'g' or 'u' or 'i'
			static int FilterImGuiLetters(ImGuiInputTextCallbackData* data)
			{
				if (data->EventChar < 256 && strchr("1234567890.", (char)data->EventChar))
					return 0;
				return 1;
			}
		};

		if (SetBool) {
			SetBool = false;

			SetKeyW[0] = Global::KeyW;
			SetKeyS[0] = Global::KeyS;
			SetKeyA[0] = Global::KeyA;
			SetKeyD[0] = Global::KeyD;
			SetMusicVolume = Global::MusicVolume;
			SetSoundEffectsVolume = Global::SoundEffectsVolume;
			SetFullScreen = Global::FullScreen;
			SetServerPort = Global::ServerPort;
			SetClientPort = Global::ClientPort;
			memcpy(SetClientIP, Global::ClientIP.c_str(), Global::ClientIP.size());
			SetVulKanValidationLayer = Global::VulKanValidationLayer;
			SetMonitor = Global::Monitor;
			SetMonitorCompatibleMode = Global::MonitorCompatibleMode;
		}

		ImGui::Begin(u8"设置界面 ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));
		ImGui::DragFloat(u8"音乐音量 ", &SetMusicVolume, 0.001f, 0.0f, 10.0f);
		ImGui::DragFloat(u8"音效音量 ", &SetSoundEffectsVolume, 0.001f, 0.0f, 10.0f);
		ImGui::DragInt(u8"服务器端口 ", &SetServerPort, 0.5f, 0, 65535, "%d", ImGuiSliderFlags_None); HelpMarker("玩家开设在本地的服务器端口 ");
		ImGui::InputText(u8"客户端链接IP", SetClientIP, 16, ImGuiInputTextFlags_CallbackCharFilter, TextFilters::FilterImGuiLetters); HelpMarker("玩家链接服务器IP");
		ImGui::DragInt(u8"客户端链接端口)", &SetClientPort, 0.5f, 0, 65535, "%d", ImGuiSliderFlags_None); HelpMarker("玩家链接服务器端口");
		ImGui::Checkbox(u8"VulKan 验证层 ", &SetVulKanValidationLayer); HelpMarker("VulKan 校验层 （部分设备不支持） ");

		ImGui::Checkbox(u8"监视器 ", &SetMonitor); 
		if(SetMonitor){ ImGui::SameLine(); ImGui::Checkbox(u8"兼容模式 ", &SetMonitorCompatibleMode); } 
		HelpMarker("监视器（部分设备不支持） \n兼容模式（会牺牲性能） ");

		ImGui::Checkbox(u8"全屏 ", &SetFullScreen);

		ImGui::Text(u8"按键 ");
		ImGui::InputText(u8"上 ", SetKeyW, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::InputText(u8"下 ", SetKeyS, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::InputText(u8"左 ", SetKeyA, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::InputText(u8"右 ", SetKeyD, 2, ImGuiInputTextFlags_CharsUppercase);

		if (ImGui::Button(u8"保存 ")) {
			if (SetMusicVolume > 10.0f) {
				SetMusicVolume = 10.0f;
			}
			if (SetSoundEffectsVolume > 10.0f) {
				SetSoundEffectsVolume = 10.0f;
			}
			SoundEffect::SoundEffect::GetSoundEffect()->SetVolume(SetMusicVolume);
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, SetSoundEffectsVolume);
			if (Global::FullScreen != SetFullScreen) {
				Global::FullScreen = SetFullScreen;
				mWindown->SetWindow(SetFullScreen);
			}
			Global::KeyW = SetKeyW[0];
			Global::KeyS = SetKeyS[0];
			Global::KeyA = SetKeyA[0];
			Global::KeyD = SetKeyD[0];
			Global::MonitorCompatibleMode = SetMonitor?SetMonitorCompatibleMode:false;
			Global::MusicVolume = SetMusicVolume;
			Global::SoundEffectsVolume = SetSoundEffectsVolume;
			Global::ServerPort = SetServerPort;
			Global::ClientPort = SetClientPort;
			Global::ClientIP = SetClientIP;
			Global::VulKanValidationLayer = SetVulKanValidationLayer;
			Global::Monitor = SetMonitor;

			Global::Storage();
			InterfaceIndexes = PreviousLayerInterface;
		}
		if (ImGui::Button(u8"返回 ")) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterfaceIndexes = PreviousLayerInterface;
		}
		ImGui::End();
	}

	int InputTextCallbackData(ImGuiInputTextCallbackData* data){
		if (data->HasSelection()) {//文本被选中时取消被选中
			data->SelectionEnd = 0;
			data->SelectionStart = 0;
		}
		return 0;
	}

	void ImGuiInterFace::ConsoleInterface() {
		static char LOpCode[200] = { '\0' };
		static unsigned int OpodeSize = 0;
		static std::string LOpcodeStr;
		static bool PromptBool = false;
		static bool PromptTriggerBool = false;//触发过提示词
		static int PromptSelected = -1;
		static std::vector<const std::string*> Vvector;

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowBorderSize = 0.0f;                // 设置窗口边框大小为0，即无边框
		style.Colors[ImGuiCol_WindowBg].w = 0.0f;     // 将窗口背景颜色的Alpha通道设置为0，即完全透明

		ImGui::Begin(u8"控制台 ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImVec2 OpBoxsize = { 400, 100 };
		int CodeSize = ImGui::GetTextLineHeight() + 22 + (PromptBool ? 104 : 0);
		ImGui::SetWindowPos(ImVec2(0, Global::mHeight - CodeSize));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, CodeSize));
		
		
		
		
		//ImGui::SetNextItemWidth(Global::mWidth - 18);
		
		if (PromptBool && ImGui::BeginListBox(" ", OpBoxsize)) {
			for (int n = 0; n < Vvector.size(); n++)
			{
				if (ImGui::Selectable(Vvector[n]->c_str(), PromptSelected == n)) {
					PromptSelected = n;
					memcpy(&LOpCode[1], Vvector[n]->c_str(), Vvector[n]->size());
					LOpCode[Vvector[n]->size() + 1] = '\0';
					ConsoleFocusHere = true;
					PromptTriggerBool = true;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					ImGui::TextUnformatted(Opcode::GetOpAnnotation(Vvector[n]->c_str()));
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			}
			ImGui::EndListBox();
		}
		ImGui::SetNextItemWidth(Global::mWidth - 18);
		if (ConsoleFocusHere) {
			ImGui::SetKeyboardFocusHere();
			ConsoleFocusHere = false;
		}
		if (ImGui::InputText(u8"  Code  ", LOpCode, 200, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways, InputTextCallbackData)) {
			if (LOpCode[0] == '/') {
				Opcode::Opcode(&LOpCode[1]);
			}
			else {
				mChatBoxStr->add({ LOpCode, clock() });
			}

			if (Global::MultiplePeopleMode) {
				LOpcodeStr = LOpCode;
				OpodeSize = LOpcodeStr.size();
				char* NewChar = new char[OpodeSize + 1];
				memcpy(NewChar, LOpCode, OpodeSize + 1);

				LimitUse<ChatStrStruct*>* LUse;
				ContinuousMap<evutil_socket_t, RoleSynchronizationData>* LRoleMap;
				RoleSynchronizationData* LRoleData;

				ChatStrStruct* LChatStrStruct = new ChatStrStruct;
				LChatStrStruct->str = NewChar;
				LChatStrStruct->size = OpodeSize + 1;

				if (Global::ServerOrClient) {
					LRoleMap = server::GetServer()->GetServerData();
					LRoleData = LRoleMap->GetKeyData(0);
					LUse = new LimitUse<ChatStrStruct*>(LChatStrStruct, LRoleMap->GetKeyNumber());
					for (size_t i = 0; i < LRoleMap->GetKeyNumber(); i++)
					{
						LRoleData[i].mBufferEventSingleData->mStr->add(LUse);
					}
				}
				else
				{
					LRoleData = client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData();
					LUse = new LimitUse<ChatStrStruct*>(LChatStrStruct, 1);
					LRoleData->mBufferEventSingleData->mStr->add(LUse);
				}
			}

			OpodeSize = 0;
			ConsoleFocusHere = true;
			Global::ConsoleBool = false;
			LOpCode[0] = '\0';
		}
		LOpcodeStr = LOpCode;
		if (LOpCode[0] == '/' && LOpcodeStr.size() > 2 && OpodeSize != LOpcodeStr.size()) {
			OpodeSize = LOpcodeStr.size();
			Vvector = Opcode::GetOpCodePrompt(&LOpCode[1]);
			PromptBool = Vvector.size() != 0;
			PromptSelected = -1;
		}
		if(PromptTriggerBool)
		{
			PromptTriggerBool = false;
			PromptBool = false;
		}
		
		ImGui::End();

		style.WindowBorderSize = 1.0f;                // 设置窗口边框大小为0，即无边框
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;     // 将窗口背景颜色的Alpha通道设置为0，即完全透明
	}

	void ImGuiInterFace::DisplayTextS() {
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowBorderSize = 0.0f;                // 设置窗口边框大小为0，即无边框
		style.Colors[ImGuiCol_WindowBg].w = 0.0f;     // 将窗口背景颜色的Alpha通道设置为0，即完全透明

		ImGui::Begin(u8"TEXT ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));

		ImVec2 textSize = ImGui::CalcTextSize(u8"道生");
		ImGui::SetCursorPos({ (Global::mWidth / 2.0f - textSize.x / 2.0f),(Global::mHeight / 2.0f - textSize.y / 2.0f) });
		ImGui::Text(u8"道生");

		float LHeight = Global::mHeight - 60.0f;
		mChatBoxStr->InitData();
		ChatBoxStr* LChatBoxStr;
		for (size_t i = 0; i < mChatBoxStr->GetNumber(); i++)
		{
			LChatBoxStr = mChatBoxStr->addData();
			if ((i == (mChatBoxStr->GetNumber() - 1)) && ((clock() - LChatBoxStr->timr) > 10000)) {
				LChatBoxStr = mChatBoxStr->pop();
				continue;
			}
			ImGui::SetCursorPos({ 30.0f,LHeight });
			ImGui::Text(LChatBoxStr->str.c_str());
			LHeight -= 20.0f;
		}

		ImGui::End();
		style.WindowBorderSize = 1.0f;                // 设置窗口边框大小为0，即无边框
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;     // 将窗口背景颜色的Alpha通道设置为0，即完全透明
	}

	void ImGuiInterFace::ImGuiShowFPS() {
		ImGui::Text(u8"FPS: %f", TOOL::FPSNumber);
		static int values_offset = 0;
		static char overlay[32];
		sprintf(overlay, u8"平均: %f", TOOL::Mean_values);
		ImGui::PlotLines("FPS", TOOL::values, IM_ARRAYSIZE(TOOL::values), 0, overlay, TOOL::Min_values - 10, TOOL::Max_values + 10, ImVec2(0, 100.0f));
	}

	void ImGuiInterFace::ImGuiShowTiming() {
		if (ImGui::BeginTable("table1", 3))//横排有三个元素
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(u8"名字 ");
			ImGui::TableNextColumn();
			ImGui::Text(u8"耗时 ");
			ImGui::TableNextColumn();
			ImGui::Text(u8"百分比 ");
			ImGui::SameLine();//让一个元素并排
			HelpMarker(u8"相对与一帧时间的占比 ");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(u8"帧时间 ");
			ImGui::TableNextColumn();
			ImGui::Text(u8"%f", TOOL::FPStime);
			for (int i = 0; i < TOOL::mTimer->Count; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(TOOL::mTimer->mNameS[i]);
				ImGui::TableNextColumn();
				if (TOOL::mTimer->mHeapBoolS[i]) {
					ImGui::PlotLines("",
						TOOL::mTimer->mTimeHeapS[i],
						TOOL::mTimer->mHeapNumber,
						TOOL::mTimer->mTimeHeapIndexS,
						nullptr,
						TOOL::mTimer->mTimeMax[i],
						TOOL::mTimer->mTimeMin[i],
						ImVec2(0, 25.0f)
					);
				}
				else {
					ImGui::Text(u8"%1.6f 秒 ", TOOL::mTimer->mTimerS[i]);
				}
				ImGui::TableNextColumn();
				ImGui::Text("%3.3f %%", TOOL::mTimer->mTimerPercentageS[i]);
			}
			for (int i = 0; i < TOOL::mTimer->MomentCount; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(TOOL::mTimer->mMomentNameS[i]);
				ImGui::TableNextColumn();
				ImGui::Text(u8"%d", TOOL::mTimer->mMomentTimerS[i]);
			}
			ImGui::EndTable();
		}
	}
}

