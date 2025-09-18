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
		ImVec2 window_size;
		ImGui::Begin(u8"编辑");
		ImVec2 window_pos = ImGui::GetWindowPos();
		window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}
		if (ImGui::Button(EditorModeBool ? u8"关闭编辑" : u8"开启编辑"))
		{
			EditorModeBool = !EditorModeBool;
		}
		ImGui::Combo(u8"物体", &item_object, objectName, IM_ARRAYSIZE(objectName));
		ImGui::End();

		ImGui::Begin(u8"属性");
		window_size = ImGui::GetWindowSize();
		if (((Global::mWidth - window_size.x) < CursorPosX) && (window_size.y > CursorPosY))
		{
			Global::ClickWindow = true;
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
		// 不在窗口上才可以操作
		if (Global::ClickWindow)
			return;

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
				// 初次点击
				Opos = Ptr->PFGetPos() - s;
				PhysicsBlock::AngleMat lAngleMat(Ptr->angle);
				PhysicsShapeArm = -lAngleMat.Anticlockwise(Opos);
			}
			else
			{
				if (PhysicsSwitch)
				{
					// 开启物理了
					PhysicsBlock::AngleMat lAngleMat(Ptr->angle);
					Ptr->AddForce(
						lAngleMat.Rotary(PhysicsShapeArm) + Ptr->PFGetPos(),
						10 * Ptr->PFGetMass() * (e - (lAngleMat.Rotary(PhysicsShapeArm) + Ptr->PFGetPos())));
					mAuxiliaryVision->Line({lAngleMat.Rotary(PhysicsShapeArm) + Ptr->PFGetPos(), 0}, {1, 0, 0, 1}, {(e), 0}, {0, 1, 0, 1});
				}
				else
				{
					// 没有开启物理，就直接修改位置
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
	}

	void PhysicsTest::ParticleLeftEvent(PhysicsBlock::PhysicsParticle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e)
	{
		static Vec2_ Opos;
		if (Click)
		{
			if (First)
			{
				// 初次点击
				Opos = Ptr->PFGetPos() - s;
			}
			else
			{
				if (PhysicsSwitch)
				{
					// 开启物理了e
					Ptr->AddForce(10 * Ptr->PFGetMass() * (e - Ptr->pos));
					mAuxiliaryVision->Line({Ptr->pos, 0}, {1, 0, 0, 1}, {(e), 0}, {0, 1, 0, 1});
				}
				else
				{
					// 没有开启物理，就直接修改位置
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

}
