#include "Interface.h"
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
		ImFont* Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 16.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());


		



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
				throw std::runtime_error("Error: initImGui DescriptorPool 生成失败");
			}
		}

		Info.DescriptorPool = g_DescriptorPool;
		Info.MinImageCount = g_MinImageCount;

		ImGui_ImplVulkan_Init(&Info, Pass->getRenderPass());

		// 上传 DearImgui 字体
		commandbuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);//开始录制要提交的指令
		ImGui_ImplVulkan_CreateFontsTexture(commandbuffer->getCommandBuffer());//要录制的内容
		commandbuffer->end();//结束录制要提交的指令
		commandbuffer->submitSync(mDevice->getGraphicQueue());//等待指令上传结束
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

		ImGuiCommandPoolS = new VulKan::CommandPool* [FormatCount];
		ImGuiCommandBufferS = new VulKan::CommandBuffer* [FormatCount];
		for (int i = 0; i < FormatCount; i++)
		{
			ImGuiCommandPoolS[i] = new VulKan::CommandPool(mDevice);
			ImGuiCommandBufferS[i] = new VulKan::CommandBuffer(mDevice, ImGuiCommandPoolS[i], true);
		}
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
	}

	void ImGuiInterFace::InterFace()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		switch (InterfaceIndexes)
		{
		case 0:
			MainInterface();
			break;
		case 1:
			ViceInterface();
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

	void ImGuiInterFace::MainInterface()
	{
		ImGui::Begin(u8"游戏ESC界面", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(1920, 1080));
		if (ImGui::Button(u8"开始游戏")) {
			InterFaceBool = false;
			InterfaceIndexes = 1;
		}
		if (ImGui::Button(u8"游戏设置")) {

		}
		if (ImGui::Button(u8"退出游戏")) {
			mWindown->WindowClose();
		}
		ImGui::End();
	}

	void ImGuiInterFace::ViceInterface()
	{
		ImGui::Begin(u8"游戏ESC界面", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(1920, 1080));
		if (ImGui::Button(u8"继续游戏")) {
			InterFaceBool = false;
		}
		if (ImGui::Button(u8"游戏设置")) {

		}
		if (ImGui::Button(u8"返回主页")) {
			InterfaceIndexes = 0;
		}
		if (ImGui::Button(u8"退出游戏")) {
			mWindown->WindowClose();
		}
		ImGui::End();
	}

	void ImGuiInterFace::ImGuiShowFPS() {
		ImGui::Text(u8"FPS: %f", TOOL::FPSNumber);
		static int values_offset = 0;
		static char overlay[32];
		sprintf(overlay, u8"平均: %f", TOOL::Mean_values);
		ImGui::PlotLines("FPS", TOOL::values, IM_ARRAYSIZE(TOOL::values), values_offset, overlay, TOOL::Min_values - 10, TOOL::Max_values + 10, ImVec2(0, 100.0f));
	}

	void ImGuiInterFace::ImGuiShowTiming() {
		if (ImGui::BeginTable("table1", 3))//横排有三个元素
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(u8"名字");
			ImGui::TableNextColumn();
			ImGui::Text(u8"耗时");
			ImGui::TableNextColumn();
			ImGui::Text(u8"百分比");
			ImGui::SameLine();//让一个元素并排
			HelpMarker(u8"相对与一帧时间的占比");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(u8"帧时间");
			ImGui::TableNextColumn();
			ImGui::Text(u8"%f", (1.0f / TOOL::FPStime));
			for (int i = 0; i < TOOL::Quantity; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(TOOL::Consume_name[i]);
				ImGui::TableNextColumn();
				if (TOOL::SecondVectorBool[i]) {
					ImGui::PlotLines("",
						TOOL::Consume_SecondVector[i],
						TOOL::SecondVectorNumber,
						TOOL::SecondVectorIndex[i],
						nullptr,
						TOOL::Min_Secondvalues[i],
						TOOL::Max_Secondvalues[i],
						ImVec2(0, 25.0f)
					);
				}
				else {
					ImGui::Text(u8"%1.6f 秒", TOOL::Consume_Second[i]);
				}
				if (i < TOOL::ConsumeNumber) {
					ImGui::TableNextColumn();
					ImGui::Text("%3.3f %%", TOOL::Consume_time[i]);
				}

			}
			for (int i = 1; i < TOOL::MomentQuantity; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(TOOL::MomentConsume_name[i]);
				ImGui::TableNextColumn();
				ImGui::Text(u8"%1.6f 秒", TOOL::MomentConsume_Second[i]);
			}
			ImGui::EndTable();
		}
	}
}

