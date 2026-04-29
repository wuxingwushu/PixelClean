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
		ImGuiTexture* imGuiTexture,
		int FormatCount
	)
	{
		mWindown = Win;
		mDevice = device;
		mFormatCount = FormatCount;
		mInfo = Info;
		mRenderPass = Pass;
		mCommandBuffer = commandbuffer;
		mImGuiTexture = imGuiTexture;

		StructureImGuiInterFace();
		VulKan::CommandPool* commandPool = new VulKan::CommandPool(mDevice);
		VulKan::Sampler* sampler = new VulKan::Sampler(device);
		mShaderTexture = new VulKan::ShaderTexture(_2D_GI_spv, mDevice, commandPool, 1920, 1080, 4, sampler);

		mImGuiTexture->AddTexture("Shader", mShaderTexture->getTexture());
	}

	void ImGuiInterFace::StructureImGuiInterFace() {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		ImGuiStyle& Style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			Style.WindowRounding = 0.0f;
			Style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImFontConfig Font_cfg;
		Font_cfg.OversampleH = 1;
		Font_cfg.FontDataOwnedByAtlas = false;
		Font = io.Fonts->AddFontFromFileTTF("./Minecraft_AE.ttf", 16.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());

		ImGui_ImplGlfw_InitForVulkan(mWindown->getWindow(), true);

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

		mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		ImGui_ImplVulkan_CreateFontsTexture(mCommandBuffer->getCommandBuffer());
		mCommandBuffer->end();
		mCommandBuffer->submitSync(mDevice->getGraphicQueue());
		ImGui_ImplVulkan_DestroyFontUploadObjects();

		auto Color = Style.Colors;

		Style.ChildRounding = 4.0f;
		Style.FrameRounding = 4.0f;
		Style.WindowRounding = 6.0f;
		Style.PopupRounding = 4.0f;
		Style.GrabRounding = 4.0f;
		Style.ScrollbarRounding = 4.0f;
		Style.TabRounding = 4.0f;

		Style.WindowPadding = ImVec2(12, 12);
		Style.FramePadding = ImVec2(8, 6);
		Style.ItemSpacing = ImVec2(8, 6);
		Style.ItemInnerSpacing = ImVec2(6, 4);
		Style.WindowBorderSize = 0.0f;
		Style.FrameBorderSize = 0.0f;

		Color[ImGuiCol_Button] = ImColor(10, 105, 56, 255);
		Color[ImGuiCol_ButtonHovered] = ImColor(30, 135, 76, 255);
		Color[ImGuiCol_ButtonActive] = ImColor(0, 85, 36, 255);

		Color[ImGuiCol_FrameBg] = ImColor(40, 40, 40, 180);
		Color[ImGuiCol_FrameBgActive] = ImColor(30, 30, 30, 200);
		Color[ImGuiCol_FrameBgHovered] = ImColor(60, 60, 60, 200);

		Color[ImGuiCol_CheckMark] = ImColor(10, 165, 86, 255);

		Color[ImGuiCol_SliderGrab] = ImColor(10, 135, 66, 255);
		Color[ImGuiCol_SliderGrabActive] = ImColor(0, 115, 46, 255);

		Color[ImGuiCol_Header] = ImColor(10, 105, 56, 200);
		Color[ImGuiCol_HeaderHovered] = ImColor(30, 135, 76, 255);
		Color[ImGuiCol_HeaderActive] = ImColor(0, 85, 36, 255);

		Color[ImGuiCol_WindowBg] = ImColor(18, 18, 18, 230);
		Color[ImGuiCol_PopupBg] = ImColor(24, 24, 24, 240);
		Color[ImGuiCol_Border] = ImColor(50, 50, 50, 128);

		ImGuiCommandPoolS = new VulKan::CommandPool * [mFormatCount];
		ImGuiCommandBufferS = new VulKan::CommandBuffer * [mFormatCount];
		for (int i = 0; i < mFormatCount; ++i)
		{
			ImGuiCommandPoolS[i] = new VulKan::CommandPool(mDevice);
			ImGuiCommandBufferS[i] = new VulKan::CommandBuffer(mDevice, ImGuiCommandPoolS[i], true);
		}

		mChatBoxStr = new Queue<ChatBoxStr>(100);
	}

	ImGuiInterFace::~ImGuiInterFace()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		if (g_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(mDevice->getDevice(), g_DescriptorPool, nullptr);
		}

		for (int i = 0; i < mFormatCount; ++i)
		{
			delete ImGuiCommandBufferS[i];
			delete ImGuiCommandPoolS[i];
		}
		delete ImGuiCommandBufferS;
		delete ImGuiCommandPoolS;
	}

	void ImGuiInterFace::InterFace(unsigned int CurrentFrame)
	{
		mCurrentFrame = CurrentFrame;
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		mShaderTexture->CalculationScreen(TOOL::FPStime);
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
		ImGui::Render();
	}

	VkCommandBuffer ImGuiInterFace::GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info) {
		ImGuiCommandBufferS[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, info);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ImGuiCommandBufferS[i]->getCommandBuffer());
		ImGuiCommandBufferS[i]->end();
		return ImGuiCommandBufferS[i]->getCommandBuffer();
	}

	static void DrawBackground(ImGuiTexture* tex, unsigned int frame, unsigned int w, unsigned int h) {
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##bg", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoFocusOnAppearing
		);
		ImGui::SetWindowPos(ImVec2(-1, -1));
		ImGui::SetWindowSize(ImVec2(w + 2, h + 2));
		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)tex->GetTexture("Shader")->getDescriptorSet(frame),
			ImVec2(0, 0), ImVec2(w, h),
			ImVec2(0, 0), ImVec2(1, 1)
		);
		ImGui::End();
	}

	static float GetUIScale() {
		float sx = Global::mWidth / 1920.0f;
		float sy = Global::mHeight / 1080.0f;
		return sx < sy ? sx : sy;
	}

	void ImGuiInterFace::MainInterface()
	{
		DrawBackground(mImGuiTexture, mCurrentFrame, Global::mWidth, Global::mHeight);

		float scale = GetUIScale();
		float btnW = 420.0f * scale;
		float btnH = 60.0f * scale;
		float arrowBtn = 60.0f * scale;
		float spacing = 16.0f * scale;
		float titleFontScale = 3.0f * scale;
		float normalFontScale = Global::FontZoomRatio * scale;

		float contentW = btnW + arrowBtn * 2 + spacing * 2;
		float titleH = 80.0f * scale;
		int numButtons = 4;
		float totalH = titleH + spacing * 2 + (btnH + spacing) * numButtons;

		float startX = (Global::mWidth - contentW) * 0.5f;
		float startY = (Global::mHeight - totalH) * 0.5f;

		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin(u8"##main", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));

		float curY = startY;

		ImGui::SetWindowFontScale(titleFontScale);
		ImVec2 textSize = ImGui::CalcTextSize(u8"素  净");
		ImGui::SetCursorPos(ImVec2((Global::mWidth - textSize.x) * 0.5f, curY + (titleH - textSize.y) * 0.5f));
		ImGui::Text(u8"素  净");

		curY += titleH + spacing * 2;

		ImGui::SetWindowFontScale(normalFontScale);

		float rowX = startX;
		ImGui::SetCursorPos(ImVec2(rowX, curY));
		if (ImGui::Button(u8"<", ImVec2(arrowBtn, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			int x = Global::GameMode - 1;
			if (x < 0) {
				Global::GameMode = GameModsEnum::Infinite_;
			}
			else {
				Global::GameMode = (GameModsEnum)x;
			}
		}
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button(GameModsEnumName[Global::GameMode], ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetInterFaceBool();
			Global::GameResourceLoadingBool = true;
			InterfaceIndexes = ViceInterface_Enum;
		}
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button(u8">", ImVec2(arrowBtn, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			int x = Global::GameMode + 1;
			if (x > GameModsEnum::Infinite_) {
				Global::GameMode = (GameModsEnum)0;
			}
			else {
				Global::GameMode = (GameModsEnum)x;
			}
		}

		curY += btnH + spacing;

		float singleBtnX = (Global::mWidth - btnW) * 0.5f;

		ImGui::SetCursorPos(ImVec2(singleBtnX, curY));
		if (ImGui::Button(u8"多人游戏", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterfaceIndexes = MultiplePeopleInterface_Enum;
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(singleBtnX, curY));
		if (ImGui::Button(u8"游戏设置", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetBool = true;
			InterfaceIndexes = SetInterface_Enum;
			PreviousLayerInterface = MainInterface_Enum;
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(singleBtnX, curY));
		if (ImGui::Button(u8"退出游戏", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			mWindown->WindowClose();
		}

		ImGui::End();
	}

	void ImGuiInterFace::ViceInterface()
	{
		DrawBackground(mImGuiTexture, mCurrentFrame, Global::mWidth, Global::mHeight);

		float scale = GetUIScale();
		float btnW = 420.0f * scale;
		float btnH = 60.0f * scale;
		float spacing = 16.0f * scale;
		float normalFontScale = Global::FontZoomRatio * scale;

		int numButtons = 4;
		float totalH = (btnH + spacing) * numButtons - spacing;

		float startX = (Global::mWidth - btnW) * 0.5f;
		float startY = (Global::mHeight - totalH) * 0.5f;

		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin(u8"##vice", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));
		ImGui::SetWindowFontScale(normalFontScale);

		float curY = startY;

		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"继续游戏", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterFaceBool = false;
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"游戏设置", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetBool = true;
			InterfaceIndexes = SetInterface_Enum;
			PreviousLayerInterface = ViceInterface_Enum;
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"返回主页", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			Global::GameResourceUninstallBool = true;
			InterfaceIndexes = MainInterface_Enum;
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"退出游戏", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			mWindown->WindowClose();
		}

		ImGui::End();
	}

	void ImGuiInterFace::MultiplePeopleInterface() {
		DrawBackground(mImGuiTexture, mCurrentFrame, Global::mWidth, Global::mHeight);

		float scale = GetUIScale();
		float btnW = 420.0f * scale;
		float btnH = 60.0f * scale;
		float spacing = 16.0f * scale;
		float normalFontScale = Global::FontZoomRatio * scale;

		int numButtons = 3;
		float totalH = (btnH + spacing) * numButtons - spacing;

		float startX = (Global::mWidth - btnW) * 0.5f;
		float startY = (Global::mHeight - totalH) * 0.5f;

		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin(u8"##multi", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));
		ImGui::SetWindowFontScale(normalFontScale);

		float curY = startY;

		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"服务器", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetInterFaceBool();
			InterfaceIndexes = ViceInterface_Enum;
			Global::MultiplePeopleMode = true;
			Global::ServerOrClient = true;
			Global::GameMode = GameModsEnum::Maze_;
			Global::GameResourceLoadingBool = true;
			server::GetServer();
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"客户端", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			SetInterFaceBool();
			InterfaceIndexes = ViceInterface_Enum;
			Global::MultiplePeopleMode = true;
			Global::ServerOrClient = false;
			Global::GameMode = GameModsEnum::Maze_;
			Global::GameResourceLoadingBool = true;
			client::GetClient();
		}

		curY += btnH + spacing;
		ImGui::SetCursorPos(ImVec2(startX, curY));
		if (ImGui::Button(u8"返回", ImVec2(btnW, btnH))) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterfaceIndexes = MainInterface_Enum;
		}

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
		static float SetFontZoomRatio;
		static bool SetMonitorCompatibleMode;

		static char SetKeyW[2];
		static char SetKeyS[2];
		static char SetKeyA[2];
		static char SetKeyD[2];

		struct TextFilters
		{
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
			SetFontZoomRatio = Global::FontZoomRatio;
			SetMonitor = Global::Monitor;
			SetMonitorCompatibleMode = Global::MonitorCompatibleMode;
		}

		DrawBackground(mImGuiTexture, mCurrentFrame, Global::mWidth, Global::mHeight);

		float scale = GetUIScale();
		float panelW = 900.0f * scale;
		float btnW = 180.0f * scale;
		float btnH = 50.0f * scale;
		float inputW = 200.0f * scale;
		float spacing = 10.0f * scale;
		float fontScale = Global::FontZoomRatio * scale;

		float panelX = (Global::mWidth - panelW) * 0.5f;
		float panelY = 40.0f * scale;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * scale, 16 * scale));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8 * scale, 6 * scale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scale, 8 * scale));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(20.0f/255.0f, 20.0f/255.0f, 20.0f/255.0f, 220.0f/255.0f));

		ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
		ImGui::SetNextWindowSize(ImVec2(panelW, Global::mHeight - panelY * 2));
		ImGui::Begin(u8"##settings", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse
		);
		ImGui::SetWindowFontScale(fontScale);

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);

		float contentW = ImGui::GetContentRegionAvail().x;

		ImGui::TextUnformatted(u8"游戏设置");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushItemWidth(inputW);
		ImGui::DragFloat(u8"字体大小", &Global::FontZoomRatio, 0.001f, 1.0f, 10.0f);
		ImGui::Spacing();
		ImGui::DragFloat(u8"音乐音量", &SetMusicVolume, 0.001f, 0.0f, 10.0f);
		ImGui::Spacing();
		ImGui::DragFloat(u8"音效音量", &SetSoundEffectsVolume, 0.001f, 0.0f, 10.0f);
		ImGui::Spacing();
		ImGui::DragInt(u8"服务器端口", &SetServerPort, 0.5f, 0, 65535, "%d", ImGuiSliderFlags_None);
		HelpMarker2(u8"玩家开设在本地的服务器端口");
		ImGui::Spacing();
		ImGui::InputText(u8"客户端链接IP", SetClientIP, 16, ImGuiInputTextFlags_CallbackCharFilter, TextFilters::FilterImGuiLetters);
		HelpMarker2(u8"玩家链接服务器IP");
		ImGui::Spacing();
		ImGui::DragInt(u8"客户端链接端口", &SetClientPort, 0.5f, 0, 65535, "%d", ImGuiSliderFlags_None);
		HelpMarker2(u8"玩家链接服务器端口");
		ImGui::Spacing();
		ImGui::Checkbox(u8"VulKan 验证层", &SetVulKanValidationLayer);
		HelpMarker2(u8"VulKan 校验层（部分设备不支持）");
		ImGui::Spacing();
		ImGui::Checkbox(u8"监视器", &SetMonitor);
		if (SetMonitor) {
			ImGui::SameLine();
			ImGui::Checkbox(u8"兼容模式", &SetMonitorCompatibleMode);
			HelpMarker2(u8"兼容模式（会牺牲性能）");
		}
		else {
			HelpMarker2(u8"监视器（部分设备不支持）\n兼容模式（会牺牲性能）");
		}
		ImGui::Spacing();
		ImGui::Checkbox(u8"全屏", &SetFullScreen);
		ImGui::Spacing();

		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextUnformatted(u8"按键设置");
		ImGui::Spacing();

		float keyInputW = 60.0f * scale;
		ImGui::PushItemWidth(keyInputW);
		ImGui::InputText(u8"上", SetKeyW, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::Spacing();
		ImGui::InputText(u8"下", SetKeyS, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::Spacing();
		ImGui::InputText(u8"左", SetKeyA, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::Spacing();
		ImGui::InputText(u8"右", SetKeyD, 2, ImGuiInputTextFlags_CharsUppercase);
		ImGui::PopItemWidth();
		ImGui::PopItemWidth();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float btnStartX = (contentW - btnW * 2 - spacing) * 0.5f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnStartX);
		if (ImGui::Button(u8"保存", ImVec2(btnW, btnH))) {
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
			Global::MonitorCompatibleMode = SetMonitor ? SetMonitorCompatibleMode : false;
			Global::MusicVolume = SetMusicVolume;
			Global::SoundEffectsVolume = SetSoundEffectsVolume;
			SetFontZoomRatio = Global::FontZoomRatio;
			Global::ServerPort = SetServerPort;
			Global::ClientPort = SetClientPort;
			Global::ClientIP = SetClientIP;
			Global::VulKanValidationLayer = SetVulKanValidationLayer;
			Global::Monitor = SetMonitor;

			Global::Storage();
			InterfaceIndexes = PreviousLayerInterface;
		}
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button(u8"返回", ImVec2(btnW, btnH))) {
			Global::FontZoomRatio = SetFontZoomRatio;
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Tap1", MP3, false, Global::SoundEffectsVolume);
			InterfaceIndexes = PreviousLayerInterface;
		}

		ImGui::PopStyleVar();
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(4);
	}

	int InputTextCallbackData(ImGuiInputTextCallbackData* data){
		if (data->HasSelection()) {
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
		static bool PromptTriggerBool = false;
		static int PromptSelected = -1;
		static std::vector<const std::string*> Vvector;

		float scale = GetUIScale();

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowBorderSize = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.0f;

		ImGui::Begin(u8"控制台 ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImVec2 OpBoxsize = { 400.0f * scale, 100.0f * scale };
		int CodeSize = (int)(ImGui::GetTextLineHeight() + 22 * scale + (PromptBool ? 104 * scale : 0));
		ImGui::SetWindowPos(ImVec2(0, Global::mHeight - CodeSize));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, CodeSize));
		ImGui::SetWindowFontScale(Global::FontZoomRatio * scale);

		if (PromptBool && ImGui::BeginListBox(" ", OpBoxsize)) {
			for (int n = 0; n < Vvector.size(); ++n)
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
		ImGui::SetNextItemWidth(Global::mWidth - 18 * scale);
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
					for (size_t i = 0; i < LRoleMap->GetKeyNumber(); ++i)
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

		style.WindowBorderSize = 1.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	void ImGuiInterFace::DisplayTextS() {
		float scale = GetUIScale();

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowBorderSize = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.0f;

		ImGui::Begin(u8"TEXT ", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(Global::mWidth, Global::mHeight));
		ImGui::SetWindowFontScale(Global::FontZoomRatio * scale);

		float LHeight = Global::mHeight - 60.0f * scale;
		mChatBoxStr->InitData();
		ChatBoxStr* LChatBoxStr;
		for (size_t i = 0; i < mChatBoxStr->GetNumber(); ++i)
		{
			LChatBoxStr = mChatBoxStr->addData();
			if ((i == (mChatBoxStr->GetNumber() - 1)) && ((clock() - LChatBoxStr->timr) > 10000)) {
				LChatBoxStr = mChatBoxStr->pop();
				continue;
			}
			ImGui::SetCursorPos({ 30.0f * scale, LHeight });
			ImGui::Text(LChatBoxStr->str.c_str());
			LHeight -= 20.0f * scale;
		}

		ImGui::End();
		style.WindowBorderSize = 1.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	void ImGuiInterFace::ImGuiShowFPS() {
		ImGui::Text(u8"FPS: %f", TOOL::FPSNumber);
		ImGui::Text(u8"FPS振幅: %f, Max: %f, Min: %f", TOOL::FrameAmplitude, TOOL::Max_FrameAmplitude, TOOL::Min_FrameAmplitude);
		static int values_offset = 0;
		static char overlay[32];
		sprintf(overlay, u8"平均: %f", TOOL::Mean_values);
		ImGui::PlotLines("FPS", TOOL::values, IM_ARRAYSIZE(TOOL::values), 0, overlay, TOOL::Min_values - 10, TOOL::Max_values + 10, ImVec2(0, 100.0f));
	}

	void ImGuiInterFace::ImGuiShowTiming() {
		if (ImGui::BeginTable("table1", 3))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(u8"名字 ");
			ImGui::TableNextColumn();
			ImGui::Text(u8"耗时 ");
			ImGui::TableNextColumn();
			ImGui::Text(u8"百分比 ");
			ImGui::SameLine();
			HelpMarker2(u8"相对与一帧时间的占比 ");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(u8"帧时间 ");
			ImGui::TableNextColumn();
			ImGui::Text(u8"%f", TOOL::FPStime);
			for (int i = 0; i < TOOL::mTimer->mTimerTimeS.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(TOOL::mTimer->mTimerTimeS[i].Name);
				ImGui::TableNextColumn();
				if (TOOL::mTimer->mTimerTimeS[i].HeapBool) {
					ImGui::PlotLines("",
						TOOL::mTimer->mTimerTimeS[i].TimeHeap.data(),
						TOOL::mTimer->mHeapNumber,
						TOOL::mTimer->mTimeHeapIndexS,
						nullptr,
						TOOL::mTimer->mTimerTimeS[i].TimeMax,
						TOOL::mTimer->mTimerTimeS[i].TimeMin,
						ImVec2(0, 25.0f)
					);
				}
				else {
					ImGui::Text(u8"%1.6f 秒 ", TOOL::mTimer->mTimerTimeS[i].Time);
				}
				ImGui::TableNextColumn();
				ImGui::Text("%3.3f %%", TOOL::mTimer->mTimerTimeS[i].Percentage);
			}
			for (int i = 0; i < TOOL::mTimer->mMomentS.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(TOOL::mTimer->mMomentS[i].Name);
				ImGui::TableNextColumn();
				ImGui::Text(u8"%d", TOOL::mTimer->mMomentS[i].Timer);
			}
			ImGui::EndTable();
		}
	}
}
