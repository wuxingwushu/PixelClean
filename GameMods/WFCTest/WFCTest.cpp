#include "WFCTest.h"
#include "../../GlobalVariable.h"
#include "../../DebugLog.h"
#include <algorithm>

namespace GAME
{

	// ==================== 地形名称与颜色 ====================

	static const char* sTerrainNames[] = {
		u8"草地",   // Grass
		u8"楼房",   // Building
		u8"水域",   // Water
		u8"道路",   // Road
		u8"桥梁",   // Bridge
		u8"树林",   // Forest
	};

	static const glm::vec4 sTerrainColors[] = {
		{0.25f, 0.70f, 0.25f, 1.0f}, // 草地 — 鲜绿色
		{0.55f, 0.50f, 0.42f, 1.0f}, // 楼房 — 暖灰色
		{0.18f, 0.45f, 0.85f, 1.0f}, // 水域 — 深蓝色
		{0.35f, 0.35f, 0.35f, 1.0f}, // 道路 — 深灰色
		{0.60f, 0.45f, 0.25f, 1.0f}, // 桥梁 — 木棕色
		{0.10f, 0.50f, 0.15f, 1.0f}, // 树林 — 深绿色
	};

	// ==================== 构造 / 析构 ====================

	WFCTest::WFCTest(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		LOGD("WFCTest::WFCTest constructor");

		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 80000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		InitTiles();
		InitNeighborRules();

		mCamera->setCameraPos({0, 0, 70});
		GenerateWFC();
	}

	WFCTest::~WFCTest()
	{
		LOGD("WFCTest::~WFCTest destructor");
		delete mAuxiliaryVision;
	}

	// ==================== 图块初始化 ====================

	void WFCTest::InitTiles()
	{
		// 每种地形是一个 1x1 的 Tile，使用 Symmetry::X（无旋转对称）
		// 桥梁权重较低，因为桥梁应该只在固定场景出现
		auto makeTile = [](unsigned id, double weight) -> Tile<unsigned> {
			Tile<unsigned> tile{{Array2D<unsigned>(1, 1)}, Symmetry::X, weight};
			tile.data[0].get(0, 0) = id;
			return tile;
		};

		mTiles.push_back(makeTile(TerrainType::Grass,    1.0));
		mTiles.push_back(makeTile(TerrainType::Building, 1.0));
		mTiles.push_back(makeTile(TerrainType::Water,    1.0));
		mTiles.push_back(makeTile(TerrainType::Road,     1.0));
		mTiles.push_back(makeTile(TerrainType::Bridge,   0.5));
		mTiles.push_back(makeTile(TerrainType::Forest,   1.0));
	}

	// ==================== 邻接规则定义 ====================
	//
	// 每条规则 (tile1, 0, tile2, 0, dirMask) 中的第 5 个参数是方向掩码，
	// 指定这条规则适用于哪些方向：
	//   DIR_UP(0x1)    — tile1 可以在 tile2 的上边
	//   DIR_LEFT(0x2)  — tile1 可以在 tile2 的左边
	//   DIR_RIGHT(0x4) — tile1 可以在 tile2 的右边
	//   DIR_DOWN(0x8)  — tile1 可以在 tile2 的下边
	// 用 | 组合：DIR_UP|DIR_DOWN 表示只在上边和下边。
	// DIR_ALL(0xF) 表示 4 个方向都允许。
	//
	// 因为 Symmetry::X 无旋转，规则的 tile1 和 tile2 只有 orientation 0。
	// ====================================================================

	void WFCTest::InitNeighborRules()
	{
		// 添加双向规则（四个方向都允许）
		auto AddRule = [this](unsigned a, unsigned b) {
			mNeighbors.emplace_back(a, 0u, b, 0u, DIR_ALL);
		};

		// 添加单向规则（只允许在指定方向相邻）
		auto AddRuleDir = [this](unsigned a, unsigned b, unsigned dirMask) {
			mNeighbors.emplace_back(a, 0u, b, 0u, dirMask);
		};

		// ── 草地：可以与任何地形相邻（充当过渡带） ──
		AddRule(Grass, Grass);
		AddRule(Grass, Building);
		AddRule(Grass, Water);
		AddRule(Grass, Road);
		AddRule(Grass, Bridge);
		AddRule(Grass, Forest);

		// ── 楼房：与草地、楼房、道路相邻 ──
		// 楼房只允许在道路的上下方（道路两侧是楼房，道路尽头是草地）
		AddRule(Building, Building);
		AddRule(Building, Grass);
		AddRuleDir(Building, Road, DIR_UP | DIR_DOWN);

		// ── 水域：与草地、水域、桥梁相邻 ──
		AddRule(Water, Water);
		AddRule(Water, Grass);
		AddRuleDir(Water, Bridge, DIR_LEFT | DIR_RIGHT);

		// ── 道路：与草地、楼房、道路、桥梁相邻 ──
		AddRule(Road, Road);
		AddRule(Road, Grass);
		// 道路只在左右方向连接桥梁（道路→桥梁→水域的过渡）
		AddRuleDir(Road, Bridge, DIR_LEFT | DIR_RIGHT);

		// ── 桥梁：与水域、道路、桥梁相邻 ──
		AddRule(Bridge, Bridge);
		// 桥梁只在水域的左右方向出现
		AddRuleDir(Bridge, Water, DIR_UP | DIR_DOWN);

		// ── 树林：与草地、树林相邻 ──
		AddRule(Forest, Forest);
		AddRule(Forest, Grass);

		// 从 mNeighbors 推导 mAllowedNeighbors（用于 UI 显示）
		mAllowedNeighbors.resize(TerrainCount);
		for (auto& [a, oa, b, ob, dm] : mNeighbors) {
			(void)oa; (void)ob; (void)dm;
			if (std::find(mAllowedNeighbors[a].begin(), mAllowedNeighbors[a].end(), b)
				== mAllowedNeighbors[a].end()) {
				mAllowedNeighbors[a].push_back(b);
			}
			if (a != b) {
				if (std::find(mAllowedNeighbors[b].begin(), mAllowedNeighbors[b].end(), a)
					== mAllowedNeighbors[b].end()) {
					mAllowedNeighbors[b].push_back(a);
				}
			}
		}
		for (auto& list : mAllowedNeighbors) {
			std::sort(list.begin(), list.end());
		}
	}

	// ==================== WFC 生成 ====================

	void WFCTest::GenerateWFC()
	{
		TilingWFCOptions options;
		options.periodic_output = mPeriodicOutput;

		TilingWFC<unsigned> wfc(mTiles, mNeighbors, mOutHeight, mOutWidth, options, mSeed);

		mOutput = wfc.run();

		if (mOutput.has_value()) {
			LOGD("WFCTest: TilingWFC generation succeeded (%dx%d)", mOutWidth, mOutHeight);
		} else {
			LOGD("WFCTest: TilingWFC generation failed, retrying with new seed...");
			mSeed++;
			TilingWFC<unsigned> wfc_retry(mTiles, mNeighbors, mOutHeight, mOutWidth, options, mSeed);
			mOutput = wfc_retry.run();
			if (mOutput.has_value()) {
				LOGD("WFCTest: TilingWFC retry succeeded");
			} else {
				LOGD("WFCTest: TilingWFC retry also failed");
			}
		}

		mSeed++;
		mNeedRegenerate = false;
	}

	// ==================== 颜色 / 名称 ====================

	glm::vec4 WFCTest::GetColorForPixel(unsigned pixel) const
	{
		if (pixel < TerrainCount) return sTerrainColors[pixel];
		return {0.5f, 0.5f, 0.5f, 1.0f};
	}

	const char* WFCTest::GetTerrainName(unsigned pixel) const
	{
		if (pixel < TerrainCount) return sTerrainNames[pixel];
		return u8"未知";
	}

	std::vector<unsigned> WFCTest::GetAllowedNeighbors(unsigned terrain) const
	{
		if (terrain < mAllowedNeighbors.size()) return mAllowedNeighbors[terrain];
		return {};
	}

	// ==================== 渲染 ====================

	void WFCTest::RenderWFCMap()
	{
		if (!mOutput.has_value()) return;

		auto& output = mOutput.value();

		float offsetX = -(float)output.width / 2.0f;
		float offsetY = -(float)output.height / 2.0f;

		for (unsigned y = 0; y < output.height; y++) {
			for (unsigned x = 0; x < output.width; x++) {
				unsigned pixel = output.get(y, x);
				glm::vec4 color = GetColorForPixel(pixel);

				float cx = offsetX + (float)x + 0.5f;
				float cy = offsetY + (float)y + 0.5f;

				mAuxiliaryVision->Spot({cx, cy, 0}, 0.9f, color);
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

		float camSpeed = 20.0f * (float)TOOL::FPStime;
		glm::vec3 camPos = mCamera->getCameraPos();
		if (mMoveLeft)  camPos.x -= camSpeed;
		if (mMoveRight) camPos.x += camSpeed;
		if (mMoveUp)    camPos.y += camSpeed;
		if (mMoveDown)  camPos.y -= camSpeed;
		mCameraTarget.x = camPos.x;
		mCameraTarget.y = camPos.y;

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
		ImGui::Begin(u8"WFC 波函数坍缩 — 城市地图生成 (TilingWFC)");

		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		// ---- WFC 参数 ----
		ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.0f}, u8"── WFC 参数 ──");
		ImGui::Text(u8"图块数: %d  输出尺寸: %d x %d  种子: %d",
			TerrainCount, mOutWidth, mOutHeight, mSeed);
		ImGui::Text(u8"周期性输出: %s", mPeriodicOutput ? u8"是" : u8"否");

		ImGui::Separator();

		// ---- 生成状态 ----
		ImGui::TextColored({0.3f, 0.7f, 1.0f, 1.0f}, u8"── 生成状态 ──");
		if (mOutput.has_value()) {
			ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, u8"状态: 生成成功");

			auto& output = mOutput.value();
			int counts[TerrainCount] = {};
			for (unsigned i = 0; i < output.data.size(); i++) {
				unsigned v = output.data[i];
				if (v < TerrainCount) counts[v]++;
			}
			for (unsigned i = 0; i < TerrainCount; i++) {
				auto& c = sTerrainColors[i];
				ImGui::TextColored(ImVec4(c.x, c.y, c.z, c.w), u8"  ■ %s: %d", sTerrainNames[i], counts[i]);
			}
		} else {
			ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, u8"状态: 生成失败");
		}

		ImGui::Separator();

		// ---- 邻接规则表 ----
		ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.0f}, u8"── 邻接规则 ──");

		auto DirMaskToString = [](unsigned dm) -> const char* {
			switch (dm) {
			case DIR_UP:                   return u8"↑ 上";
			case DIR_DOWN:                 return u8"↓ 下";
			case DIR_LEFT:                 return u8"← 左";
			case DIR_RIGHT:                return u8"→ 右";
			case DIR_UP | DIR_DOWN:        return u8"↑↓ 上下";
			case DIR_LEFT | DIR_RIGHT:     return u8"←→ 左右";
			case DIR_UP | DIR_DOWN | DIR_LEFT:  return u8"↑↓← 上/下/左";
			case DIR_UP | DIR_DOWN | DIR_RIGHT: return u8"↑↓→ 上/下/右";
			case DIR_UP | DIR_LEFT | DIR_RIGHT: return u8"↑←→ 上/左/右";
			case DIR_DOWN | DIR_LEFT | DIR_RIGHT: return u8"↓←→ 下/左/右";
			case DIR_ALL:                  return u8"↑↓←→ 四周";
			default:                       return u8"其他";
			}
		};

		if (ImGui::BeginTable("rules", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn(u8"A", ImGuiTableColumnFlags_WidthFixed, 60);
			ImGui::TableSetupColumn(u8"方向", ImGuiTableColumnFlags_WidthFixed, 90);
			ImGui::TableSetupColumn(u8"B", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (auto& [a, oa, b, ob, dm] : mNeighbors) {
				(void)oa; (void)ob;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				auto& ca = sTerrainColors[a];
				ImGui::TextColored(ImVec4(ca.x, ca.y, ca.z, ca.w), u8"■ %s", sTerrainNames[a]);

				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored({0.7f, 0.7f, 0.9f, 1.0f}, u8"%s", DirMaskToString(dm));

				ImGui::TableSetColumnIndex(2);
				auto& cb = sTerrainColors[b];
				ImGui::TextColored(ImVec4(cb.x, cb.y, cb.z, cb.w), u8"■ %s", sTerrainNames[b]);
			}
			ImGui::EndTable();
		}

		ImGui::Separator();

		// ---- 操作说明 ----
		ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, u8"── 操作说明 ──");
		ImGui::BulletText(u8"W/A/S/D — 平移相机");
		ImGui::BulletText(u8"滚轮 — 缩放相机");
		ImGui::BulletText(u8"1 — 切换周期性输出");
		ImGui::BulletText(u8"2 — 重新生成 WFC");

		ImGui::Separator();
		if (ImGui::Button(u8"重新生成", {120, 30})) {
			mNeedRegenerate = true;
		}

		ImGui::End();
	}

}