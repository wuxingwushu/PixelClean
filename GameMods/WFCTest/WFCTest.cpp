#include "WFCTest.h"
#include "../../GlobalVariable.h"
#include "../../DebugLog.h"

namespace GAME
{

	// ==================== 构造 / 析构 ====================

	WFCTest::WFCTest(Configuration wConfiguration) : Configuration{wConfiguration}, mInputPattern(10, 10)
	{
		LOGD("WFCTest::WFCTest constructor");

		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 80000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		// 创建输入图案：10x10 的城镇地图
		// 0=草地  1=楼房  2=水域  3=道路(横)  4=道路(竖)  5=桥梁  6=树林
		//
		// 布局示意：
		//   树林 树林 树林 草地 草地 草地 草地 草地 树林 树林
		//   树林 树林 树林 草地 草地 草地 草地 草地 树林 树林
		//   草地 草地 草地 草地 道路 楼房 楼房 草地 草地 草地
		//   草地 草地 草地 草地 道路 楼房 楼房 草地 草地 草地
		//   草地 草地 草地 草地 道路 草地 草地 草地 草地 草地
		//   道路 道路 道路 道路 路口 道路 道路 道路 道路 道路
		//   草地 草地 草地 草地 道路 草地 草地 草地 草地 草地
		//   草地 草地 草地 草地 道路 楼房 楼房 草地 草地 草地
		//   草地 草地 水域 水域 桥梁 水域 水域 水域 草地 草地
		//   草地 草地 水域 水域 水域 水域 水域 水域 草地 草地
		unsigned patternData[10][10] = {
			{6, 6, 6, 0, 0, 0, 0, 0, 6, 6},
			{0, 6, 0, 0, 4, 1, 1, 0, 6, 6},
			{0, 0, 0, 0, 4, 1, 1, 0, 0, 0},
			{0, 0, 0, 0, 4, 0, 0, 0, 0, 0},
			{3, 3, 3, 3, 4, 3, 3, 3, 3, 3},
			{0, 0, 0, 0, 4, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 4, 1, 1, 0, 0, 0},
			{0, 0, 2, 2, 5, 2, 2, 2, 0, 0},
			{6, 0, 2, 2, 5, 2, 2, 2, 0, 6},
			{6, 0, 0, 0, 0, 0, 0, 0, 0, 6},
		};
		for (unsigned y = 0; y < 10; y++) {
			for (unsigned x = 0; x < 10; x++) {
				mInputPattern.get(y, x) = patternData[y][x];
			}
		}

		mCamera->setCameraPos({0, 0, 70});
		GenerateWFC();
	}

	WFCTest::~WFCTest()
	{
		LOGD("WFCTest::~WFCTest destructor");
		delete mAuxiliaryVision;
	}

	// ==================== WFC 生成 ====================

	void WFCTest::GenerateWFC()
	{
		OverlappingWFCOptions options;
		options.periodic_input = true;
		options.periodic_output = mPeriodicOutput;
		options.out_height = mOutHeight;
		options.out_width = mOutWidth;
		options.symmetry = mSymmetry;
		options.ground = false;
		options.pattern_size = mPatternSize;

		OverlappingWFC<unsigned> wfc(mInputPattern, options, mSeed);

		mOutput = wfc.run();

		if (mOutput.has_value()) {
			LOGD("WFCTest: WFC generation succeeded (%dx%d)", mOutWidth, mOutHeight);
		} else {
			LOGD("WFCTest: WFC generation failed, retrying with new seed...");
			mSeed++;
			OverlappingWFC<unsigned> wfc_retry(mInputPattern, options, mSeed);
			mOutput = wfc_retry.run();
			if (mOutput.has_value()) {
				LOGD("WFCTest: WFC retry succeeded");
			} else {
				LOGD("WFCTest: WFC retry also failed");
			}
		}

		mSeed++;
		mNeedRegenerate = false;
	}

	// ==================== 颜色映射 ====================

	glm::vec4 WFCTest::GetColorForPixel(unsigned pixel) const
	{
		switch (pixel) {
		case 0: return {0.25f, 0.70f, 0.25f, 1.0f}; // 草地 — 鲜绿色
		case 1: return {0.55f, 0.50f, 0.42f, 1.0f}; // 楼房 — 暖灰色
		case 2: return {0.18f, 0.45f, 0.85f, 1.0f}; // 水域 — 深蓝色
		case 3: return {0.35f, 0.35f, 0.35f, 1.0f}; // 道路(横) — 深灰色
		case 4: return {0.35f, 0.35f, 0.35f, 1.0f}; // 道路(竖) — 深灰色
		case 5: return {0.60f, 0.45f, 0.25f, 1.0f}; // 桥梁 — 木棕色
		case 6: return {0.10f, 0.50f, 0.15f, 1.0f}; // 树林 — 深绿色
		default: return {0.5f, 0.5f, 0.5f, 1.0f};  // 未知 — 灰色
		}
	}

	const char* WFCTest::GetTerrainName(unsigned pixel) const
	{
		switch (pixel) {
		case 0: return u8"草地";
		case 1: return u8"楼房";
		case 2: return u8"水域";
		case 3: return u8"道路";
		case 4: return u8"道路";
		case 5: return u8"桥梁";
		case 6: return u8"树林";
		default: return u8"未知";
		}
	}

	// ==================== 渲染 ====================

	void WFCTest::RenderWFCMap()
	{
		if (!mOutput.has_value()) return;

		auto& output = mOutput.value();

		// 将地图居中，每个格子大小为 1x1
		float offsetX = -(float)output.width / 2.0f;
		float offsetY = -(float)output.height / 2.0f;

		for (unsigned y = 0; y < output.height; y++) {
			for (unsigned x = 0; x < output.width; x++) {
				unsigned pixel = output.get(y, x);
				glm::vec4 color = GetColorForPixel(pixel);

				float cx = offsetX + (float)x + 0.5f;
				float cy = offsetY + (float)y + 0.5f;

				// 绘制实心方块
				glm::dvec3 p0 = {cx - 0.5, cy - 0.5, 0};
				glm::dvec3 p1 = {cx + 0.5, cy - 0.5, 0};
				glm::dvec3 p2 = {cx + 0.5, cy + 0.5, 0};
				glm::dvec3 p3 = {cx - 0.5, cy + 0.5, 0};

				mAuxiliaryVision->Line(p0, color, p1, color);
				mAuxiliaryVision->Line(p1, color, p2, color);
				mAuxiliaryVision->Line(p2, color, p3, color);
				mAuxiliaryVision->Line(p3, color, p0, color);
				mAuxiliaryVision->Line(p0, color, p2, color);
				mAuxiliaryVision->Line(p1, color, p3, color);
			}
		}

		// 绘制输入图案缩略图在右下角
		float inputOffsetX = (float)mOutWidth / 2.0f + 2.0f;
		float inputOffsetY = -(float)mOutHeight / 2.0f + (float)mInputPattern.height;

		for (unsigned y = 0; y < mInputPattern.height; y++) {
			for (unsigned x = 0; x < mInputPattern.width; x++) {
				unsigned pixel = mInputPattern.get(y, x);
				glm::vec4 color = GetColorForPixel(pixel);
				color.a = 0.85f;

				float cx = inputOffsetX + (float)x + 0.5f;
				float cy = inputOffsetY + (float)y + 0.5f;

				glm::dvec3 p0 = {cx - 0.5, cy - 0.5, 0};
				glm::dvec3 p1 = {cx + 0.5, cy - 0.5, 0};
				glm::dvec3 p2 = {cx + 0.5, cy + 0.5, 0};
				glm::dvec3 p3 = {cx - 0.5, cy + 0.5, 0};

				mAuxiliaryVision->Line(p0, color, p1, color);
				mAuxiliaryVision->Line(p1, color, p2, color);
				mAuxiliaryVision->Line(p2, color, p3, color);
				mAuxiliaryVision->Line(p3, color, p0, color);
				mAuxiliaryVision->Line(p0, color, p2, color);
				mAuxiliaryVision->Line(p1, color, p3, color);
			}
		}
	}

	// ==================== 输入事件 ====================

	void WFCTest::MouseMove(double xpos, double ypos)
	{
	}

	void WFCTest::MouseRoller(int z)
	{
		if (Global::ClickWindow) return;

		glm::vec3 camPos = mCamera->getCameraPos();
		if (camPos.z <= 10) {
			camPos.z += (camPos.z / 2) * z;
		} else {
			camPos.z += z * 5;
		}
		if (camPos.z <= 0.1f) camPos.z = 0.1f;
		if (camPos.z > 200.0f) camPos.z = 200.0f;
		mCameraTarget.z = camPos.z;
	}

	void WFCTest::KeyDown(GameKeyEnum moveDirection)
	{
		switch (moveDirection)
		{
		case GameKeyEnum::MOVE_LEFT:
			mMoveLeft = true;
			break;
		case GameKeyEnum::MOVE_RIGHT:
			mMoveRight = true;
			break;
		case GameKeyEnum::MOVE_FRONT:
			mMoveUp = true;
			break;
		case GameKeyEnum::MOVE_BACK:
			mMoveDown = true;
			break;
		case GameKeyEnum::Key_1:
			mPeriodicOutput = !mPeriodicOutput;
			mNeedRegenerate = true;
			break;
		case GameKeyEnum::Key_2:
			mNeedRegenerate = true;
			break;
		case GameKeyEnum::ESC:
			if (Global::ConsoleBool) {
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			} else {
				InterFace->SetInterFaceBool();
			}
			break;
		default:
			break;
		}
	}

	// ==================== 游戏主循环 ====================

	void WFCTest::GameCommandBuffers(unsigned int Format_i)
	{
		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void WFCTest::GameLoop(unsigned int mCurrentFrame)
	{
		mAuxiliaryVision->Begin();

		if (mNeedRegenerate) {
			GenerateWFC();
		}

		RenderWFCMap();

		mAuxiliaryVision->End();

		// 相机平移
		float camSpeed = 20.0f * (float)TOOL::FPStime;
		glm::vec3 camPos = mCamera->getCameraPos();
		if (mMoveLeft)  camPos.x -= camSpeed;
		if (mMoveRight) camPos.x += camSpeed;
		if (mMoveUp)    camPos.y += camSpeed;
		if (mMoveDown)  camPos.y -= camSpeed;
		mCameraTarget.x = camPos.x;
		mCameraTarget.y = camPos.y;

		// 平滑相机
		glm::vec3 currentCamPos = mCamera->getCameraPos();
		float smoothFactor = (float)(1.0 - std::exp(-20.0 * TOOL::FPStime));
		glm::vec3 smoothedPos = currentCamPos + (mCameraTarget - currentCamPos) * smoothFactor;
		mCamera->setCameraPos(smoothedPos);

		mCamera->update();
		VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[mCurrentFrame]->getPersistentMappedPtr();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();

		mMoveLeft = mMoveRight = mMoveUp = mMoveDown = false;
	}

	// ==================== 其他接口 ====================

	void WFCTest::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
	}

	void WFCTest::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
	}

	void WFCTest::GameTCPLoop()
	{
	}

	// ==================== ImGui UI ====================

	void WFCTest::GameUI()
	{
		ImGui::Begin(u8"WFC 波函数坍缩 — 城市地图生成");

		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		// ---- WFC 参数 ----
		ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.0f}, u8"── WFC 参数 ──");
		ImGui::Text(u8"输入图案: 10x10  输出尺寸: %d x %d", mOutWidth, mOutHeight);
		ImGui::Text(u8"图案大小: %d  对称性: %d  种子: %d", mPatternSize, mSymmetry, mSeed);
		ImGui::Text(u8"周期性输出: %s", mPeriodicOutput ? u8"是" : u8"否");

		ImGui::Separator();

		// ---- 生成状态 ----
		ImGui::TextColored({0.3f, 0.7f, 1.0f, 1.0f}, u8"── 生成状态 ──");
		if (mOutput.has_value()) {
			ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, u8"状态: 生成成功");
			auto& output = mOutput.value();

			// 统计各类型数量
			int counts[7] = {};
			for (unsigned i = 0; i < output.data.size(); i++) {
				unsigned v = output.data[i];
				if (v < 7) counts[v]++;
			}
			ImGui::Text(u8"草地:%d  楼房:%d  水域:%d", counts[0], counts[1], counts[2]);
			ImGui::Text(u8"道路:%d  桥梁:%d  树林:%d", counts[3] + counts[4], counts[5], counts[6]);
		} else {
			ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, u8"状态: 生成失败");
		}

		ImGui::Separator();

		// ---- 图例 ----
		ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, u8"── 图例 ──");
		ImGui::TextColored({0.25f, 0.70f, 0.25f, 1.0f}, u8"  ■ 草地");
		ImGui::TextColored({0.55f, 0.50f, 0.42f, 1.0f}, u8"  ■ 楼房");
		ImGui::TextColored({0.18f, 0.45f, 0.85f, 1.0f}, u8"  ■ 水域");
		ImGui::TextColored({0.35f, 0.35f, 0.35f, 1.0f}, u8"  ■ 道路");
		ImGui::TextColored({0.60f, 0.45f, 0.25f, 1.0f}, u8"  ■ 桥梁");
		ImGui::TextColored({0.10f, 0.50f, 0.15f, 1.0f}, u8"  ■ 树林");

		ImGui::Separator();

		// ---- 操作说明 ----
		ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, u8"── 操作说明 ──");
		ImGui::BulletText(u8"W/A/S/D — 平移相机");
		ImGui::BulletText(u8"滚轮 — 缩放相机");
		ImGui::BulletText(u8"1 — 切换周期性输出");
		ImGui::BulletText(u8"2 — 重新生成 WFC");
		ImGui::BulletText(u8"右下角显示输入图案");

		// ---- 重新生成按钮 ----
		ImGui::Separator();
		if (ImGui::Button(u8"重新生成", {120, 30})) {
			mNeedRegenerate = true;
		}

		ImGui::End();
	}

}