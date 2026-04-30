#include "PhysicsTest.h"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"
#include "../../Opcode/OpcodeFunction.h"
#include "../../Physics/DestroyMode.h"
#include "../../GlobalVariable.h"
#include "../../PhysicsBlock/BaseCalculate.hpp"
#include "../../PhysicsBlock/ImGuiPhysics.hpp"

#include "../../ImGui/imgui_demo.cpp"
#include "PhysicsDemo.h"
#include <fstream>
#include <sstream>

namespace GAME
{

	PhysicsTest::PhysicsTest(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		// 添加视觉辅助
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 100000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		PhysicsBlock::DemoFunS[0](&mPhysicsWorld, mCamera);
		mMapStatic = (PhysicsBlock::MapStatic *)mPhysicsWorld->GetMapFormwork();
		for (size_t x = 0; x < mMapStatic->width; x++)
		{
			for (size_t y = 0; y < mMapStatic->height; y++)
			{
				if (mMapStatic->at(x, y).Entity)
				{
					ShowStaticSquare(Vec2_{x, y} - mMapStatic->centrality, 0, {0, 1, 0, 1});
				}
			}
		}

		PhysicsBlock::AuxiliaryInfoRead();
	}

	PhysicsTest::~PhysicsTest()
	{
		PhysicsBlock::AuxiliaryInfoStorage();
		delete mAuxiliaryVision;
		delete mPhysicsWorld;
	}

	void PhysicsTest::MouseMove(double xpos, double ypos)
	{
		// mCamera->onMouseMove(xpos, ypos);
	}

	void PhysicsTest::MouseRoller(int z)
	{
		// 在 imgui 窗口上所以不响应
		if (Global::ClickWindow)
			return;

		if (m_position.z <= 10)
		{
			m_position.z += (m_position.z / 2) * z;
		}
		else
		{
			m_position.z += z * 5;
		}
		if (m_position.z <= 0.1)
		{
			m_position.z = 0.1;
		}
	}

	void PhysicsTest::KeyDown(GameKeyEnum moveDirection)
	{
		const int lidaxiao = 100;
		switch (moveDirection)
		{
		case GameKeyEnum::MOVE_LEFT:
			m_position.x -= TOOL::FPStime * 20;
			break;
		case GameKeyEnum::MOVE_RIGHT:
			m_position.x += TOOL::FPStime * 20;
			break;
		case GameKeyEnum::MOVE_FRONT:
			m_position.y += TOOL::FPStime * 20;
			break;
		case GameKeyEnum::MOVE_BACK:
			m_position.y -= TOOL::FPStime * 20;
			break;
		case GameKeyEnum::ESC:
			if (Global::ConsoleBool)
			{
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			}
			else
			{
				InterFace->SetInterFaceBool();
			}
			break;
		case GameKeyEnum::Key_1:
			break;
		case GameKeyEnum::Key_2:
			break;
		default:
			break;
		}
	}

	void PhysicsTest::GameCommandBuffers(unsigned int Format_i)
	{
		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void PhysicsTest::GameUI()
	{
		//ImGui::ShowDemoWindow();
		ImGui::Begin(u8"编辑");
		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true; // 鼠标在窗口上
		}
		if (ImGui::Button(EditorModeBool ? u8"关闭编辑" : u8"开启编辑"))
		{
			EditorModeBool = !EditorModeBool;
		}
		ImGui::Combo(u8"物体", &item_object, objectName, IM_ARRAYSIZE(objectName));
		ImGui::End();

		ImGui::Begin(u8"属性");
		window_pos = ImGui::GetWindowPos();
		window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true; // 鼠标在窗口上
		}
		// 设置位置，让窗口靠右
		ImGui::SetWindowPos(ImVec2(Global::mWidth - window_size.x, 0));

		static int item_Demo_idx = IM_ARRAYSIZE(PhysicsBlock::DemoNameS) - 1; // ImGui::Combo Demo序号
		static int item_current = item_Demo_idx + 1;						  // 储存当前Demo序号
		ImGui::Combo("Demo", &item_Demo_idx, PhysicsBlock::DemoNameS, IM_ARRAYSIZE(PhysicsBlock::DemoNameS));
		// Demo序号 是否发生改变
		static bool ResetBool = false;
		if ((item_current != item_Demo_idx) || ResetBool)
		{
			ResetBool = false;
			item_current = item_Demo_idx;
			PhysicsBlock::DemoFunS[item_Demo_idx](&mPhysicsWorld, mCamera);
			mMapStatic = (PhysicsBlock::MapStatic *)mPhysicsWorld->GetMapFormwork();
			PhysicsFormworkPtr = nullptr;
			mAuxiliaryVision->ClearStaticLine(); // 清空上一个地图的静态网格
			for (size_t x = 0; x < mMapStatic->width; x++)
			{
				for (size_t y = 0; y < mMapStatic->height; y++)
				{
					if (mMapStatic->at(x, y).Entity)
					{
						ShowStaticSquare(Vec2_{x, y} - mMapStatic->centrality, 0, {0, 1, 0, 1});
					}
				}
			}
		}
		if (ImGui::Button("重置")) {
			ResetBool = true;
		}
		if (ImGui::Button("保存"))
		{
			auto jsondata = nlohmann::json::parse(R"({})");
			mPhysicsWorld->JsonSerialization(jsondata);
			std::ofstream outFile("./JSON.json", std::ios::out | std::ios::trunc); // 文件覆盖模式
			outFile << jsondata.dump();
			outFile.close();
		}
		if (ImGui::Button("读取"))
		{
			PhysicsFormworkPtr = nullptr;
			if (mPhysicsWorld != nullptr)
			{
				delete mPhysicsWorld;
			}
			std::ofstream outFile("./JSON.json", std::ios::in);
			std::stringstream buff{};
			buff << outFile.rdbuf();
			outFile.close();
			auto jsondata = nlohmann::json::parse(buff);
			mPhysicsWorld = new PhysicsBlock::PhysicsWorld(jsondata);
		}

		if (ImGui::Button(PhysicsSwitch ? u8"关闭物理" : u8"开启物理"))
		{
			PhysicsSwitch = !PhysicsSwitch;
		}
		if (ImGui::Button(PhysicsAssistantInformation ? u8"关闭物理辅助视觉" : u8"开启物理辅助视觉"))
		{
			PhysicsAssistantInformation = !PhysicsAssistantInformation;
		}
		if (PhysicsAssistantInformation)
		{
			// 物理辅助显示参数的控制UI
			PhysicsBlock::PhysicsAuxiliaryColorUI();
		}

		ImGui::Text(u8"世界总动能：%f", mPhysicsWorld->GetWorldEnergy());

		if (PhysicsFormworkPtr)
		{
			switch (PhysicsFormworkPtr->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::shape:
				ImGui::Text(u8"Shape");
				PhysicsBlock::PhysicsUI((PhysicsBlock::PhysicsShape *)PhysicsFormworkPtr);
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				ImGui::Text(u8"Particle");
				PhysicsBlock::PhysicsUI((PhysicsBlock::PhysicsParticle *)PhysicsFormworkPtr);
				break;
			case PhysicsBlock::PhysicsObjectEnum::circle:
				ImGui::Text(u8"Circle");
				PhysicsBlock::PhysicsUI((PhysicsBlock::PhysicsCircle *)PhysicsFormworkPtr);
				break;
			case PhysicsBlock::PhysicsObjectEnum::line:
				ImGui::Text(u8"Line");
				PhysicsBlock::PhysicsUI((PhysicsBlock::PhysicsLine *)PhysicsFormworkPtr);
				break;
			default:
				ImGui::Text(u8"nullptr");
				break;
			}
		}
		else
		{
			ImGui::Text(u8"World");
			if (PhysicsBlock::PhysicsUI(mPhysicsWorld))
			{
				// 地图发生变动
				mAuxiliaryVision->ClearStaticLine(); // 清空上一个地图的静态网格
				for (size_t x = 0; x < mMapStatic->width; x++)
				{
					for (size_t y = 0; y < mMapStatic->height; y++)
					{
						if (mMapStatic->at(x, y).Entity)
						{
							ShowStaticSquare(Vec2_{x, y} - mMapStatic->centrality, 0, {0, 1, 0, 1});
						}
					}
				}
			}
		}

		ImGui::End();

		RenderRightClickMenu();
		RenderGridEditUI();
		RenderGridCellMenu();
	}

	void PhysicsTest::GameLoop(unsigned int mCurrentFrame)
	{
		int winwidth, winheight;
		glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;

		mAuxiliaryVision->Begin();

		// 鼠标事件
		EditorMode({huoqdedian.x, huoqdedian.y});

		TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
		if (PhysicsSwitch)
		{
			#define PhysicsTick (1.0 / 100.0)
			static float AddUpTime = 0;
			AddUpTime += TOOL::FPStime;
			if (AddUpTime > PhysicsTick) {
				AddUpTime -= PhysicsTick;
				mPhysicsWorld->PhysicsEmulator(PhysicsTick);
				if (AddUpTime > 1.0) {
					AddUpTime = 0.1;
				}
			}
		}
		else
		{
			mPhysicsWorld->PhysicsInformationUpdate();
		}
		TOOL::mTimer->StartEnd();

		// 查看分配到的网格树位置
		if (PhysicsAssistantInformation && PhysicsBlock::Auxiliary_GridDividedBool && (PhysicsFormworkPtr != nullptr))
		{
			std::vector<Vec2_> vision = mPhysicsWorld->mGridSearch.GetDividedVision(PhysicsFormworkPtr);
			for (size_t i = 0; i < (vision.size() / 2); ++i)
			{
				mAuxiliaryVision->Line({vision[i * 2], 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor), {vision[i * 2 + 1].x, vision[i * 2].y, 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor));
				mAuxiliaryVision->Line({vision[i * 2], 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor), {vision[i * 2].x, vision[i * 2 + 1].y, 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor));
				mAuxiliaryVision->Line({vision[i * 2 + 1], 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor), {vision[i * 2 + 1].x, vision[i * 2].y, 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor));
				mAuxiliaryVision->Line({vision[i * 2 + 1], 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor), {vision[i * 2].x, vision[i * 2 + 1].y, 0}, ColorToVec4(PhysicsBlock::Auxiliary_GridDividedColor));
			}
		}

		// 渲染物理网格
		for (auto i : mPhysicsWorld->GetPhysicsShape())
		{
			for (size_t x = 0; x < i->width; ++x)
			{
				for (size_t y = 0; y < i->height; ++y)
				{
					if (i->at(x, y).Entity)
					{
						ShowSquare(PhysicsBlock::vec2angle(Vec2_{x, y} - i->CentreMass, i->angle) + i->pos, i->angle, {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});
					}
				}
			}
			if (PhysicsAssistantInformation)
			{
				// 辅助显示位置
				if (PhysicsBlock::Auxiliary_PosBool)
					mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_PosColor));
				// 辅助显示旧位置
				if (PhysicsBlock::Auxiliary_OldPosBool)
					mAuxiliaryVision->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_OldPosColor));
				// 辅助显示角度
				if (PhysicsBlock::Auxiliary_AngleBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_AngleColor), {i->pos + PhysicsBlock::vec2angle({1, 0}, i->angle), 0}, ColorToVec4(PhysicsBlock::Auxiliary_AngleColor));
				// 辅助显示速度
				if (PhysicsBlock::Auxiliary_SpeedBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor));
				// 辅助显示受力
				if (PhysicsBlock::Auxiliary_ForceBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor));
				// 辅助显示质心
				if (PhysicsBlock::Auxiliary_CentreMassBool)
					mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_CentreMassColor));
				// 辅助显示几何中心
				if (PhysicsBlock::Auxiliary_CentreShapeBool)
					mAuxiliaryVision->Spot({i->pos - PhysicsBlock::vec2angle(i->CentreMass - i->CentreShape, i->angle), 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_CentreShapeColor));
				// 辅助显示外骨骼点
				if (PhysicsBlock::Auxiliary_OutlineBool)
				{
					PhysicsBlock::AngleMat angleMat(i->angle);
					for (size_t d = 0; d < i->OutlineSize; ++d)
					{
						mAuxiliaryVision->Spot({i->pos + angleMat.Rotary(i->OutlineSet[d]), 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_OutlineColor));
					}
				}
			}
		}
		// 渲染物理点
		for (auto i : mPhysicsWorld->GetPhysicsParticle())
		{
			mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});
			if (PhysicsAssistantInformation)
			{
				// 辅助显示位置
				if (PhysicsBlock::Auxiliary_PosBool)
					mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_PosColor));
				// 辅助显示旧位置
				if (PhysicsBlock::Auxiliary_OldPosBool)
					mAuxiliaryVision->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_OldPosColor));
				// 辅助显示速度
				if (PhysicsBlock::Auxiliary_SpeedBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor));
				// 辅助显示受力
				if (PhysicsBlock::Auxiliary_ForceBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor));
			}
		}
		// 渲染圆
		for (auto i : mPhysicsWorld->GetPhysicsCircle())
		{
			mAuxiliaryVision->Circle({i->pos, 0}, i->radius, {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});
			if (PhysicsAssistantInformation)
			{
				// 辅助显示位置
				if (PhysicsBlock::Auxiliary_PosBool)
					mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_PosColor));
				// 辅助显示旧位置
				if (PhysicsBlock::Auxiliary_OldPosBool)
					mAuxiliaryVision->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_OldPosColor));
				// 辅助显示角度
				if (PhysicsBlock::Auxiliary_AngleBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_AngleColor), {i->pos + PhysicsBlock::vec2angle({i->radius, 0}, i->angle), 0}, ColorToVec4(PhysicsBlock::Auxiliary_AngleColor));
				// 辅助显示速度
				if (PhysicsBlock::Auxiliary_SpeedBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor));
				// 辅助显示受力
				if (PhysicsBlock::Auxiliary_ForceBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor));
			}
		}
		// 渲染线
		for (auto i : mPhysicsWorld->GetPhysicsLine())
		{
			Vec2_ pR = PhysicsBlock::vec2angle({i->radius, 0}, i->angle);
			mAuxiliaryVision->Line({i->pos + pR, 0}, {0, 1, 0, 1}, {i->pos - pR, 0}, {0, 1, 0, 1});
			if (PhysicsAssistantInformation)
			{
				// 辅助显示位置
				if (PhysicsBlock::Auxiliary_PosBool)
					mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_PosColor));
				// 辅助显示旧位置
				if (PhysicsBlock::Auxiliary_OldPosBool)
					mAuxiliaryVision->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_OldPosColor));
				// 辅助显示角度
				if (PhysicsBlock::Auxiliary_AngleBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_AngleColor), {i->pos + PhysicsBlock::vec2angle({i->radius, 0}, i->angle), 0}, ColorToVec4(PhysicsBlock::Auxiliary_AngleColor));
				// 辅助显示速度
				if (PhysicsBlock::Auxiliary_SpeedBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SpeedColor));
				// 辅助显示受力
				if (PhysicsBlock::Auxiliary_ForceBool)
					mAuxiliaryVision->Line({i->pos, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(PhysicsBlock::Auxiliary_ForceColor));
			}
			
		}
		// 渲染绳子
		for (auto i : mPhysicsWorld->GetPhysicsJoint())
		{
			mAuxiliaryVision->Line({i->body1->pos, 0}, {0, 1, 0, 1}, {i->body1->pos + i->r1, 0}, {0, 1, 0, 1});
			mAuxiliaryVision->Line({i->body2->pos, 0}, {0, 1, 0, 1}, {i->body2->pos + i->r2, 0}, {0, 1, 0, 1});
		}
		for (auto j : mPhysicsWorld->GetBaseJunction())
		{
			mAuxiliaryVision->Line({j->GetA(), 0}, {0, 1, 0, 1}, {j->GetB(), 0}, {0, 1, 0, 1});
		}
		// 渲染物理信息
		if (PhysicsAssistantInformation)
		{
			for (auto &i : mPhysicsWorld->CollideGroupVector)
			{
				for (int j = 0; j < i->numContacts; ++j)
				{
					// 碰撞点
					if (PhysicsBlock::Auxiliary_CollisionDropBool)
						mAuxiliaryVision->Spot({i->contacts[j].position, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropColor));
					// 碰撞点 分离 法向量
					if (PhysicsBlock::Auxiliary_SeparateNormalVectorBool)
						mAuxiliaryVision->Line({i->contacts[j].position, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SeparateNormalVectorColor), {i->contacts[j].position + i->contacts[j].normal, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SeparateNormalVectorColor));
					// 指向重心
					if (PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityBool)
					{
						mAuxiliaryVision->Line({i->contacts[j].position, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor), {i->contacts[j].position - i->contacts[j].r1, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor));
						mAuxiliaryVision->Line({i->contacts[j].position, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor), {i->contacts[j].position - i->contacts[j].r2, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor));
					}
				}
			}
		}

		mAuxiliaryVision->End();

		if (GridEditShape != nullptr)
		{
			mAuxiliaryVision->Begin();
			glm::vec2 Angle = PhysicsBlock::AngleFloatToAngleVec(GridEditShape->angle);
			glm::vec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, Angle);
			glm::vec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, Angle);
			glm::vec2 jiao3 = jiao2 + jiao1;
			for (auto &kv : GridEditData)
			{
				int gx = kv.first.first;
				int gy = kv.first.second;
				Vec2_ cellPos = PhysicsBlock::vec2angle(Vec2_{gx, gy} - GridEditShape->CentreMass, GridEditShape->angle) + GridEditShape->pos;
				if (kv.second.Entity)
				{
					ShowSquare(cellPos, GridEditShape->angle, {0, 1, 0, 0.8f});
				}
				else
				{
					glm::vec4 lineColor = {1, 0.3f, 0.3f, 0.3f};
					mAuxiliaryVision->Line({cellPos.x, cellPos.y, 0.0}, lineColor, {cellPos.x + jiao1.x, cellPos.y + jiao1.y, 0.0}, lineColor);
					mAuxiliaryVision->Line({cellPos.x, cellPos.y, 0.0}, lineColor, {cellPos.x + jiao2.x, cellPos.y + jiao2.y, 0.0}, lineColor);
					mAuxiliaryVision->Line({cellPos.x + jiao3.x, cellPos.y + jiao3.y, 0.0}, lineColor, {cellPos.x + jiao1.x, cellPos.y + jiao1.y, 0.0}, lineColor);
					mAuxiliaryVision->Line({cellPos.x + jiao3.x, cellPos.y + jiao3.y, 0.0}, lineColor, {cellPos.x + jiao2.x, cellPos.y + jiao2.y, 0.0}, lineColor);
				}
			}
			int winwidth, winheight;
			glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
			glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
			huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
			huoqdedian.x += mCamera->getCameraPos().x;
			huoqdedian.y += mCamera->getCameraPos().y;
			glm::ivec2 hoverCell = WorldToGridCell(GridEditShape, {huoqdedian.x, huoqdedian.y});
			Vec2_ hoverPos = PhysicsBlock::vec2angle(Vec2_{hoverCell.x, hoverCell.y} - GridEditShape->CentreMass, GridEditShape->angle) + GridEditShape->pos;
			glm::vec4 hoverColor = {1, 1, 0, 0.8f};
			mAuxiliaryVision->Line({hoverPos.x, hoverPos.y, 0.0}, hoverColor, {hoverPos.x + jiao1.x, hoverPos.y + jiao1.y, 0.0}, hoverColor);
			mAuxiliaryVision->Line({hoverPos.x, hoverPos.y, 0.0}, hoverColor, {hoverPos.x + jiao2.x, hoverPos.y + jiao2.y, 0.0}, hoverColor);
			mAuxiliaryVision->Line({hoverPos.x + jiao3.x, hoverPos.y + jiao3.y, 0.0}, hoverColor, {hoverPos.x + jiao1.x, hoverPos.y + jiao1.y, 0.0}, hoverColor);
			mAuxiliaryVision->Line({hoverPos.x + jiao3.x, hoverPos.y + jiao3.y, 0.0}, hoverColor, {hoverPos.x + jiao2.x, hoverPos.y + jiao2.y, 0.0}, hoverColor);
			mAuxiliaryVision->End();
		}

		// 更新Camera变换矩阵
		mCamera->update();
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[mCurrentFrame]->getupdateBufferByMap();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix(); // 获取ViewMatrix数据
		mCameraVPMatricesBuffer[mCurrentFrame]->endupdateBufferByMap();
	}

	void PhysicsTest::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
	}

	// 游戏停止界面循环
	void PhysicsTest::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
		if (InterFace->GetInterfaceIndexes() == InterFaceEnum_::ViceInterface_Enum && Global::MultiplePeopleMode && !Global::ServerOrClient)
		{
		}
	}

	// 游戏 TCP事件
	void PhysicsTest::GameTCPLoop()
	{
		if (Global::MultiplePeopleMode)
		{
			if (Global::ServerOrClient)
			{
				event_base_loop(server::GetServer()->GetEvent_Base(), EVLOOP_NONBLOCK);
			}
			else
			{
				event_base_loop(client::GetClient()->GetEvent_Base(), EVLOOP_ONCE);
			}
		}
	}

	void PhysicsTest::ShowStaticSquare(glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = PhysicsBlock::AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, Angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, Angle);
		glm::dvec2 jiao3 = jiao2 + jiao1;
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao2, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao2, 0}, color);
	}

	void PhysicsTest::ShowSquare(glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = PhysicsBlock::AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, Angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, Angle);
		glm::dvec2 jiao3 = jiao2 + jiao1;
		mAuxiliaryVision->Line({pos, 0}, color, {pos + jiao1, 0}, color);
		mAuxiliaryVision->Line({pos, 0}, color, {pos + jiao2, 0}, color);
		mAuxiliaryVision->Line({pos + jiao3, 0}, color, {pos + jiao1, 0}, color);
		mAuxiliaryVision->Line({pos + jiao3, 0}, color, {pos + jiao2, 0}, color);
	}

	void PhysicsTest::EditorMode(glm::vec2 huoqdedian)
	{
		// 鼠标在窗口上，不处理按键事件
		if (Global::ClickWindow)
			return;

		if (GridEditShape != nullptr)
		{
			bool zb = false, yb = false;
			static glm::vec2 z1, z2, y1, y2;

			static int Z_fangzhifanfuvhufa;
			int Z_Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
			if (Z_Leftan == GLFW_PRESS)
			{
				if (Z_fangzhifanfuvhufa != Z_Leftan)
				{
					zb = true;
					z1 = huoqdedian;
					z2 = huoqdedian;
				}
				else
				{
					z2 = huoqdedian;
					GridEditPaint(huoqdedian);
				}
			}
			else if (Z_fangzhifanfuvhufa != Z_Leftan)
			{
				zb = true;
			}
			Z_fangzhifanfuvhufa = Z_Leftan;

			if (zb && Z_Leftan == GLFW_PRESS)
			{
				GridEditPaint(huoqdedian);
			}

			static int fangzhifanfuvhufa;
			int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
			if (Leftan == GLFW_PRESS)
			{
				if (fangzhifanfuvhufa != Leftan)
				{
					yb = true;
					y1 = huoqdedian;
					y2 = huoqdedian;
				}
				else
				{
					y2 = huoqdedian;
				}
			}
			else if (fangzhifanfuvhufa != Leftan)
			{
				yb = true;
			}
			fangzhifanfuvhufa = Leftan;

			if (yb && Leftan == GLFW_PRESS)
			{
				glm::ivec2 cell = WorldToGridCell(GridEditShape, huoqdedian);
				GridEditCellRightClick = cell;
				ShowGridCellMenu = true;
			}
			return;
		}

		bool zb = false, yb = false;
		static glm::vec2 z1, z2, y1, y2;

		static int Z_fangzhifanfuvhufa; // 避免反复触发
		int Z_Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
		if (Z_Leftan == GLFW_PRESS)
		{
			if (Z_fangzhifanfuvhufa != Z_Leftan)
			{
				zb = true;
				z1 = huoqdedian;
				z2 = huoqdedian;
			}
			else
			{
				z2 = huoqdedian;
			}
		}
		else if (Z_fangzhifanfuvhufa != Z_Leftan)
		{
			zb = true;
		}
		Z_fangzhifanfuvhufa = Z_Leftan;
		static int fangzhifanfuvhufa; // 避免反复触发
		int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
		if (Leftan == GLFW_PRESS)
		{
			if (fangzhifanfuvhufa != Leftan)
			{
				yb = true;
				y1 = huoqdedian;
				y2 = huoqdedian;
			}
			else
			{
				y2 = huoqdedian;
			}
		}
		else if (fangzhifanfuvhufa != Leftan)
		{
			yb = true;
		}
		fangzhifanfuvhufa = Leftan;

		// 事件分类
		static PhysicsBlock::PhysicsFormwork *Z_MousePhysicsFormworkPtr = nullptr; // 选择的物理对象
		if ((Z_Leftan == GLFW_PRESS) && zb)
		{
			Z_MousePhysicsFormworkPtr = mPhysicsWorld->Get(huoqdedian);
			PhysicsFormworkPtr = Z_MousePhysicsFormworkPtr;
		}
		static PhysicsBlock::PhysicsFormwork *MousePhysicsFormworkPtr = nullptr; // 选择的物理对象
		if ((Leftan == GLFW_PRESS) && yb)
		{
			MousePhysicsFormworkPtr = mPhysicsWorld->Get(huoqdedian);
		}

		// 执行对应事件
		if (Z_MousePhysicsFormworkPtr != nullptr)
		{
			switch (Z_MousePhysicsFormworkPtr->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::line:
			case PhysicsBlock::PhysicsObjectEnum::circle:
			case PhysicsBlock::PhysicsObjectEnum::shape:
				SquareLeftEvent((PhysicsBlock::PhysicsShape *)Z_MousePhysicsFormworkPtr, Z_Leftan == GLFW_PRESS, zb, z1, z2);
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				ParticleLeftEvent((PhysicsBlock::PhysicsParticle *)Z_MousePhysicsFormworkPtr, Z_Leftan == GLFW_PRESS, zb, z1, z2);
				break;

			default:
				break;
			}
		}
		else
		{
			if (EditorModeBool)
			{
				FoundLeftEvent(Z_Leftan == GLFW_PRESS, zb, z1, z2);
			}
			else
			{
				CameraLeftEvent(Z_Leftan == GLFW_PRESS, zb, z1, z2);
			}
		}

		if (MousePhysicsFormworkPtr != nullptr)
		{
			switch (MousePhysicsFormworkPtr->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::line:
			case PhysicsBlock::PhysicsObjectEnum::circle:
			case PhysicsBlock::PhysicsObjectEnum::shape:
				SquareRightEvent((PhysicsBlock::PhysicsShape *)MousePhysicsFormworkPtr, Leftan == GLFW_PRESS, yb, y1, y2);
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				ParticleRightEvent((PhysicsBlock::PhysicsParticle *)MousePhysicsFormworkPtr, Leftan == GLFW_PRESS, yb, y1, y2);
				break;

			default:
				break;
			}
		}
		else
		{
			CameraRightEvent(Z_Leftan == GLFW_PRESS, zb, z1, z2);
		}


		if ((Z_Leftan != GLFW_PRESS) && zb)
		{
			Z_MousePhysicsFormworkPtr = nullptr;
		}
		if ((Leftan != GLFW_PRESS) && yb)
		{
			MousePhysicsFormworkPtr = nullptr;
		}
	}

	void PhysicsTest::CameraLeftEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		static Vec2_ Opos, OCamera;
		if (Click)
		{
			if (First)
			{
				// 初次点击
				Opos = {m_position.x, m_position.y};
				OCamera.x = s.x - m_position.x;
				OCamera.y = s.y - m_position.y;
			}
			else
			{
				glm::vec2 CameraPos = OCamera - Vec2_{e.x - m_position.x, e.y - m_position.y} + Opos;
				m_position.x = CameraPos.x;
				m_position.y = CameraPos.y;
			}
		}
		else
		{
		}
	}

	void PhysicsTest::CameraRightEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
	}

	void PhysicsTest::SquareLeftEvent(PhysicsBlock::PhysicsAngle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		static Vec2_ Opos;
		static Vec2_ PhysicsShapeArm;
		if (Click)
		{
			if (First)
			{
				if (Ptr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::line)
				{
					Vec2_ dir = PhysicsBlock::vec2angle({1, 0}, Ptr->angle);
					Vec2_ v = s - Ptr->pos;
					FLOAT_ t = v.x * dir.x + v.y * dir.y;
					if (t > Ptr->radius) t = Ptr->radius;
					if (t < -Ptr->radius) t = -Ptr->radius;
					Vec2_ grabPoint = Ptr->pos + t * dir;
					Opos = Ptr->pos - grabPoint;
					PhysicsShapeArm = {t, 0};
				}
				else
				{
					Opos = Ptr->PFGetPos() - s;
					PhysicsBlock::AngleMat lAngleMat(Ptr->angle);
					PhysicsShapeArm = -lAngleMat.Anticlockwise(Opos);
				}
			}
			else
			{
				if (PhysicsSwitch)
				{
					PhysicsBlock::AngleMat lAngleMat(Ptr->angle);
					Vec2_ armWorld = lAngleMat.Rotary(PhysicsShapeArm);
					Vec2_ grabPos = armWorld + Ptr->PFGetPos();
					Vec2_ displacement = e - grabPos;

					FLOAT_ k = 300.0;
					FLOAT_ c = 2.0 * sqrt(k * Ptr->PFGetMass());

					Vec2_ grabVelocity = Ptr->speed + Ptr->angleSpeed * Vec2_{-armWorld.y, armWorld.x};
					Vec2_ springForce = k * displacement - c * grabVelocity;

					// 设置拖拽力上限
					FLOAT_ maxForce = Ptr->PFGetMass() * 500.0;
					FLOAT_ forceMag = PhysicsBlock::Modulus(springForce);
					if (forceMag > maxForce && forceMag > 0)
						springForce *= maxForce / forceMag;

					Ptr->AddForce(grabPos, springForce);
					mAuxiliaryVision->Line({grabPos, 0}, {1, 0, 0, 1}, {e, 0}, {0, 1, 0, 1});
				}
				else
				{
					Ptr->pos = Opos + e;
				}
			}
		}
		else
		{
		}
	}

	void PhysicsTest::SquareRightEvent(PhysicsBlock::PhysicsAngle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		if (Click && First)
		{
			RightClickObjectPtr = Ptr;
			ShowRightClickMenu = true;
		}
	}

	void PhysicsTest::ParticleLeftEvent(PhysicsBlock::PhysicsParticle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		static Vec2_ Opos;
		if (Click)
		{
			if (First)
			{
				Opos = Ptr->PFGetPos() - s;
			}
			else
			{
				if (PhysicsSwitch)
				{
					Vec2_ displacement = e - Ptr->pos;
					FLOAT_ k = 300.0;
					FLOAT_ c = 2.0 * sqrt(k * Ptr->PFGetMass());
					Vec2_ springForce = k * displacement - c * Ptr->speed;

					// 设置拖拽力上限
					FLOAT_ maxForce = Ptr->PFGetMass() * 500.0;
					FLOAT_ forceMag = PhysicsBlock::Modulus(springForce);
					if (forceMag > maxForce && forceMag > 0)
						springForce *= maxForce / forceMag;

					Ptr->AddForce(springForce);
					mAuxiliaryVision->Line({Ptr->pos, 0}, {1, 0, 0, 1}, {e, 0}, {0, 1, 0, 1});
				}
				else
				{
					Ptr->pos = Opos + e;
				}
			}
		}
		else
		{
		}
	}

	void PhysicsTest::ParticleRightEvent(PhysicsBlock::PhysicsParticle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		if (Click && First)
		{
			RightClickObjectPtr = Ptr;
			ShowRightClickMenu = true;
		}
	}

	void PhysicsTest::FoundLeftEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		if ((s == e) && (item_object != 1)) {
			return;
		}
		Vec2_ pos;
		glm::ivec2 size;
		if (Click)
		{
			if (First)
			{
			}
			else
			{

				// u8"圆", u8"点", u8"网格", u8"线"
				switch (item_object)
				{
				case 0:
					pos = (s + e) / FLOAT_(2);
					mAuxiliaryVision->Circle({pos, FLOAT_(0)}, PhysicsBlock::Modulus(s - pos), {0, 0.5, 0, 1});
					break;
				case 1:
					mAuxiliaryVision->Spot({e, FLOAT_(0)}, 0.05f, {0, 0.5, 0, 1});
					break;
				case 2:
					if (s.x > e.x)
						std::swap(s.x, e.x);
					if (s.y > e.y)
						std::swap(s.y, e.y);
					size = e - s;
					size += 1;
					for (int x = 0; x < size.x; ++x)
					{
						for (int y = 0; y < size.y; ++y)
						{
							ShowSquare(s + glm::vec2{x, y}, 0, {0, 0.5, 0, 1});
						}
					}
					break;
				case 3:
					mAuxiliaryVision->Line({s, FLOAT_(0)}, {0, 0.5, 0, 1}, {e, FLOAT_(0)}, {0, 0.5, 0, 1});
					break;

				default:
					break;
				}
			}
		}
		else
		{
			if (First)
			{
				PhysicsBlock::PhysicsShape *PhysicsShape;
				switch (item_object)
				{
				case 0:
					pos = (s + e) / FLOAT_(2);
					mPhysicsWorld->AddObject(new PhysicsBlock::PhysicsCircle(pos, PhysicsBlock::Modulus(s - pos), 3.14 * PhysicsBlock::ModulusLength(s - pos)));
					break;
				case 1:
					mPhysicsWorld->AddObject(new PhysicsBlock::PhysicsParticle(e, 1));
					break;
				case 2:
					if (s.x > e.x)
						std::swap(s.x, e.x);
					if (s.y > e.y)
						std::swap(s.y, e.y);
					size = e - s;
					size += 1;
					PhysicsShape = new PhysicsBlock::PhysicsShape(s, size);
					for (size_t i = 0; i < (PhysicsShape->width * PhysicsShape->height); ++i)
					{
						PhysicsShape->at(i).Collision = true;
						PhysicsShape->at(i).Entity = true;
						PhysicsShape->at(i).mass = 1;
						PhysicsShape->at(i).FrictionFactor = 0.2;
					}
					PhysicsShape->UpdateAll();
					PhysicsShape->angle = 0;
					mPhysicsWorld->AddObject(PhysicsShape);
					break;
				case 3:
					mPhysicsWorld->AddObject(new PhysicsBlock::PhysicsLine(s, e, 1));
					break;

				default:
					break;
				}
			}
		}
	}

	void PhysicsTest::RenderRightClickMenu()
	{
		if (ShowRightClickMenu && RightClickObjectPtr != nullptr)
		{
			ImGui::OpenPopup("##RightClickMenu");
			ShowRightClickMenu = false;
		}

		if (RightClickObjectPtr == nullptr)
			return;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		if (ImGui::BeginPopup("##RightClickMenu"))
		{
			ImVec2 window_size = ImGui::GetWindowSize();// 获取窗口大小
			ImVec2 window_pos = ImGui::GetWindowPos();// 获取窗口位置
			if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
				((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
				Global::ClickWindow = true; // 鼠标在窗口上
			}

			const char *typeName = u8"未知";
			switch (RightClickObjectPtr->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::shape:
				typeName = u8"网格形状";
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				typeName = u8"粒子";
				break;
			case PhysicsBlock::PhysicsObjectEnum::circle:
				typeName = u8"圆形";
				break;
			case PhysicsBlock::PhysicsObjectEnum::line:
				typeName = u8"线段";
				break;
			default:
				break;
			}
			ImGui::Text(u8"类型: %s", typeName);
			ImGui::Separator();

			if (ImGui::MenuItem(u8"删除物体"))
			{
				if (PhysicsFormworkPtr == RightClickObjectPtr)
					PhysicsFormworkPtr = nullptr;
				mPhysicsWorld->RemoveObject(RightClickObjectPtr);
				RightClickObjectPtr = nullptr;
				ImGui::CloseCurrentPopup();
			}

			if (RightClickObjectPtr != nullptr && RightClickObjectPtr->PFGetType() != PhysicsBlock::PhysicsObjectEnum::particle)
			{
				PhysicsBlock::PhysicsAngle *anglePtr = (PhysicsBlock::PhysicsAngle *)RightClickObjectPtr;
				if (ImGui::MenuItem(u8"设为静态"))
				{
					anglePtr->mass = FLOAT_MAX;
					anglePtr->invMass = 0;
					anglePtr->MomentInertia = FLOAT_MAX;
					anglePtr->invMomentInertia = 0;
					anglePtr->speed = {0, 0};
					anglePtr->angleSpeed = 0;
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem(u8"设为动态"))
				{
					anglePtr->mass = 1;
					anglePtr->invMass = 1;
					anglePtr->MomentInertia = 1;
					anglePtr->invMomentInertia = 1;
					ImGui::CloseCurrentPopup();
				}
			}

			if (RightClickObjectPtr != nullptr)
			{
				if (ImGui::MenuItem(u8"重置速度"))
				{
					RightClickObjectPtr->PFSpeed() = {0, 0};
					if (RightClickObjectPtr->PFGetType() != PhysicsBlock::PhysicsObjectEnum::particle)
					{
						((PhysicsBlock::PhysicsAngle *)RightClickObjectPtr)->angleSpeed = 0;
					}
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem(u8"选中此物体"))
				{
					PhysicsFormworkPtr = RightClickObjectPtr;
					ImGui::CloseCurrentPopup();
				}
			}

			if (RightClickObjectPtr != nullptr && RightClickObjectPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::shape)
			{
				ImGui::Separator();
				if (ImGui::MenuItem(u8"编辑网格"))
				{
					EnterGridEdit((PhysicsBlock::PhysicsShape *)RightClickObjectPtr);
					RightClickObjectPtr = nullptr;
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}
		else
		{
			RightClickObjectPtr = nullptr;
		}
		ImGui::PopStyleVar();
	}

	glm::ivec2 PhysicsTest::WorldToGridCell(PhysicsBlock::PhysicsShape *shape, glm::vec2 worldPos)
	{
		PhysicsBlock::AngleMat lAngleMat(shape->angle);
		Vec2_ localPos = lAngleMat.Anticlockwise(worldPos - shape->pos) + shape->CentreMass;
		return {(int)floor(localPos.x), (int)floor(localPos.y)};
	}

	void PhysicsTest::EnterGridEdit(PhysicsBlock::PhysicsShape *shape)
	{
		GridEditShape = shape;
		GridEditSavedPhysicsSwitch = PhysicsSwitch;
		PhysicsSwitch = false;
		GridEditData.clear();
		GridEditOrigin = {0, 0};
		for (unsigned int x = 0; x < shape->width; ++x)
		{
			for (unsigned int y = 0; y < shape->height; ++y)
			{
				GridEditData[{x, y}] = shape->at(x, y);
			}
		}
	}

	void PhysicsTest::ExitGridEdit()
	{
		if (GridEditShape == nullptr)
			return;

		bool hasEntity = false;
		for (auto &kv : GridEditData)
		{
			if (kv.second.Entity)
			{
				hasEntity = true;
				break;
			}
		}

		if (!hasEntity)
		{
			if (PhysicsFormworkPtr == GridEditShape)
				PhysicsFormworkPtr = nullptr;
			mPhysicsWorld->RemoveObject(GridEditShape);
			GridEditShape = nullptr;
			GridEditData.clear();
			PhysicsSwitch = GridEditSavedPhysicsSwitch;
			return;
		}

		int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
		for (auto &kv : GridEditData)
		{
			if (kv.second.Entity)
			{
				if (kv.first.first < minX) minX = kv.first.first;
				if (kv.first.second < minY) minY = kv.first.second;
				if (kv.first.first > maxX) maxX = kv.first.first;
				if (kv.first.second > maxY) maxY = kv.first.second;
			}
		}

		int newW = maxX - minX + 1;
		int newH = maxY - minY + 1;

		Vec2_ oldPos = GridEditShape->pos;
		FLOAT_ oldAngle = GridEditShape->angle;
		Vec2_ oldCentreMass = GridEditShape->CentreMass;
		Vec2_ offsetLocal = Vec2_{(FLOAT_)minX, (FLOAT_)minY} - oldCentreMass;
		Vec2_ worldOffset = PhysicsBlock::vec2angle(offsetLocal, oldAngle);

		PhysicsBlock::PhysicsShape *newShape = new PhysicsBlock::PhysicsShape(oldPos + worldOffset, {newW, newH});
		for (int x = 0; x < newW; ++x)
		{
			for (int y = 0; y < newH; ++y)
			{
				auto it = GridEditData.find({x + minX, y + minY});
				if (it != GridEditData.end())
				{
					newShape->at(x, y) = it->second;
				}
			}
		}
		newShape->angle = oldAngle;
		newShape->UpdateAll();

		if (PhysicsFormworkPtr == GridEditShape)
			PhysicsFormworkPtr = newShape;
		mPhysicsWorld->RemoveObject(GridEditShape);
		mPhysicsWorld->AddObject(newShape);

		GridEditShape = nullptr;
		GridEditData.clear();
		PhysicsSwitch = GridEditSavedPhysicsSwitch;
	}

	void PhysicsTest::GridEditPaint(glm::vec2 worldPos)
	{
		if (GridEditShape == nullptr)
			return;
		glm::ivec2 cell = WorldToGridCell(GridEditShape, worldPos);

		if (GridEditBrushType == GridEditBrush::Draw)
		{
			PhysicsBlock::GridBlock block;
			block.Entity = GridEditEntity;
			block.Collision = GridEditCollision;
			block.mass = GridEditMass;
			block.FrictionFactor = GridEditFriction;
			GridEditData[{cell.x, cell.y}] = block;
		}
		else
		{
			GridEditData.erase({cell.x, cell.y});
		}
	}

	void PhysicsTest::RenderGridEditUI()
	{
		// 被编辑对象不为空时才显示菜单
		if (GridEditShape == nullptr)
			return;

		ImGui::Begin(u8"网格编辑");
		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true; // 鼠标在窗口上
		}

		int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
		int entityCount = 0;
		for (auto &kv : GridEditData)
		{
			if (kv.first.first < minX) minX = kv.first.first;
			if (kv.first.second < minY) minY = kv.first.second;
			if (kv.first.first > maxX) maxX = kv.first.first;
			if (kv.first.second > maxY) maxY = kv.first.second;
			if (kv.second.Entity) ++entityCount;
		}
		if (GridEditData.empty())
		{
			minX = minY = maxX = maxY = 0;
		}
		ImGui::Text(u8"网格范围: (%d,%d) ~ (%d,%d)", minX, minY, maxX, maxY);
		ImGui::Text(u8"实体格子: %d / %d", entityCount, (int)GridEditData.size());

		const char *brushNames[] = {u8"绘制", u8"擦除"};
		int brushIdx = static_cast<int>(GridEditBrushType);
		if (ImGui::Combo(u8"画笔", &brushIdx, brushNames, IM_ARRAYSIZE(brushNames)))
		{
			GridEditBrushType = static_cast<GridEditBrush>(brushIdx);
		}

		if (GridEditBrushType == GridEditBrush::Draw)
		{
			ImGui::Checkbox(u8"实体", &GridEditCollision);
			ImGui::Checkbox(u8"碰撞", &GridEditEntity);
			ImGui::DragFloat(u8"质量", &GridEditMass, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat(u8"摩擦系数", &GridEditFriction, 0.01f, 0.0f, 1.0f);
		}

		ImGui::Separator();

		if (ImGui::Button(u8"填充全部"))
		{
			for (auto &kv : GridEditData)
			{
				kv.second.Entity = true;
				kv.second.Collision = true;
				kv.second.mass = GridEditMass;
				kv.second.FrictionFactor = GridEditFriction;
			}
		}

		if (ImGui::Button(u8"清空全部"))
		{
			for (auto &kv : GridEditData)
			{
				kv.second.Entity = false;
				kv.second.Collision = false;
			}
		}

		if (ImGui::Button(u8"反转网格"))
		{
			for (auto &kv : GridEditData)
			{
				kv.second.Entity = !kv.second.Entity;
				kv.second.Collision = !kv.second.Collision;
			}
		}

		ImGui::Separator();

		if (ImGui::Button(u8"镜像水平"))
		{
			std::map<std::pair<int,int>, PhysicsBlock::GridBlock> newData;
			for (auto &kv : GridEditData)
			{
				int mirrorX = minX + maxX - kv.first.first;
				newData[{mirrorX, kv.first.second}] = kv.second;
			}
			GridEditData = std::move(newData);
		}

		if (ImGui::Button(u8"镜像垂直"))
		{
			std::map<std::pair<int,int>, PhysicsBlock::GridBlock> newData;
			for (auto &kv : GridEditData)
			{
				int mirrorY = minY + maxY - kv.first.second;
				newData[{kv.first.first, mirrorY}] = kv.second;
			}
			GridEditData = std::move(newData);
		}

		ImGui::Separator();

		if (ImGui::Button(u8"退出编辑"))
		{
			ExitGridEdit();
		}

		ImGui::End();
	}

	void PhysicsTest::RenderGridCellMenu()
	{
		// 被编辑对象不为空时才显示菜单
		if (GridEditShape == nullptr)
			return;

		if (ShowGridCellMenu)
		{
			ImGui::OpenPopup("##GridCellMenu");
			ShowGridCellMenu = false;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		if (ImGui::BeginPopup("##GridCellMenu"))
		{

			ImVec2 window_size = ImGui::GetWindowSize();// 获取窗口大小
			ImVec2 window_pos = ImGui::GetWindowPos();// 获取窗口位置
			if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
				((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
				Global::ClickWindow = true; // 鼠标在窗口上
			}

			int cx = GridEditCellRightClick.x;
			int cy = GridEditCellRightClick.y;
			auto it = GridEditData.find({cx, cy});
			if (it != GridEditData.end())
			{
				PhysicsBlock::GridBlock &block = it->second;
				ImGui::Text(u8"格子 (%d, %d)", cx, cy);
				ImGui::Separator();

				bool entity = block.Entity;
				bool collision = block.Collision;
				if (ImGui::Checkbox(u8"实体", &entity))
				{
					block.Entity = entity;
				}
				if (ImGui::Checkbox(u8"碰撞", &collision))
				{
					block.Collision = collision;
				}
				ImGui::DragFloat(u8"质量", &block.mass, 0.1f, 0.0f, 100.0f);
				ImGui::DragFloat(u8"摩擦系数", &block.FrictionFactor, 0.01f, 0.0f, 1.0f);

				if (ImGui::Button(u8"应用质量到全部"))
				{
					for (auto &kv : GridEditData)
					{
						if (kv.second.Entity)
							kv.second.mass = block.mass;
					}
				}

				if (ImGui::Button(u8"应用摩擦到全部"))
				{
					for (auto &kv : GridEditData)
					{
						if (kv.second.Entity)
							kv.second.FrictionFactor = block.FrictionFactor;
					}
				}
			}
			else
			{
				ImGui::Text(u8"空格子 (%d, %d)", cx, cy);
				if (ImGui::MenuItem(u8"在此绘制"))
				{
					PhysicsBlock::GridBlock block;
					block.Entity = GridEditEntity;
					block.Collision = GridEditCollision;
					block.mass = GridEditMass;
					block.FrictionFactor = GridEditFriction;
					GridEditData[{cx, cy}] = block;
				}
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
	}

}
