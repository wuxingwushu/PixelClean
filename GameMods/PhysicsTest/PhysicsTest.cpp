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
		if (m_position.z <= 10)
		{
			m_position.z += (m_position.z / 2) * z;
		}
		else
		{
			m_position.z += z * 5;
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
		ImGui::ShowDemoWindow();

		ImGui::Begin(u8"属性 ");
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((Global::mWidth - window_size.x) < CursorPosX) && (window_size.y > CursorPosY))
		{
			Global::ClickWindow = true;
		}
		// 设置位置，让窗口靠右
		ImGui::SetWindowPos(ImVec2(Global::mWidth - window_size.x, 0));

		static int item_Demo_idx = IM_ARRAYSIZE(PhysicsBlock::DemoNameS) - 1; // ImGui::Combo Demo序号
		static int item_current = item_Demo_idx + 1;  // 储存当前Demo序号
		ImGui::Combo("Demo", &item_Demo_idx, PhysicsBlock::DemoNameS, IM_ARRAYSIZE(PhysicsBlock::DemoNameS));
		// Demo序号 是否发生改变
		if (item_current != item_Demo_idx)
		{
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

		if (ImGui::Button(PhysicsSwitch ? "关闭物理" : "开启物理"))
		{
			PhysicsSwitch = !PhysicsSwitch;
		}
		if (ImGui::Button(PhysicsAssistantInformation ? "关闭物理辅助视觉" : "开启物理辅助视觉"))
		{
			PhysicsAssistantInformation = !PhysicsAssistantInformation;
		}
		if (PhysicsAssistantInformation)
		{
			// 物理辅助显示参数的控制UI
			PhysicsBlock::PhysicsAuxiliaryColorUI();
		}

		ImGui::Text("世界总动能：%f", mPhysicsWorld->GetWorldEnergy());

		if (PhysicsFormworkPtr)
		{
			switch (PhysicsFormworkPtr->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::shape:
				ImGui::Text(u8"Shape");
				PhysicsBlock::PhysicsShapeUI((PhysicsBlock::PhysicsShape *)PhysicsFormworkPtr);
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				ImGui::Text(u8"Particle");
				PhysicsBlock::PhysicsShapeUI((PhysicsBlock::PhysicsParticle *)PhysicsFormworkPtr);
				break;
			default:
				ImGui::Text(u8"nullptr");
				break;
			}
		}
		else
		{
			ImGui::Text(u8"World");
			if (PhysicsBlock::PhysicsShapeUI(mPhysicsWorld))
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

		static Vec2_ beang{0}, beang2{0}, Opos{0}, end{0};

		mAuxiliaryVision->Begin();

		// 不在窗口上才可以操作
		if (!Global::ClickWindow)
		{
			// 点击左键
			static int Z_fangzhifanfuvhufa;	   // 避免反复触发
			static Vec2_ PhysicsShapeArm; //
			int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
			if (Leftan == GLFW_PRESS)
			{
				if (Z_fangzhifanfuvhufa != Leftan)
				{
					beang = {huoqdedian.x, huoqdedian.y};
					PhysicsFormworkPtr = mPhysicsWorld->Get(beang);
					if (PhysicsFormworkPtr)
					{
						Opos = PhysicsFormworkPtr->PFGetPos() - beang;
						if (PhysicsFormworkPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::shape)
						{
							PhysicsBlock::AngleMat lAngleMat(((PhysicsBlock::PhysicsShape *)PhysicsFormworkPtr)->angle);
							PhysicsShapeArm = -lAngleMat.Anticlockwise(Opos);
						}
					}
					else
					{
						glm::vec3 pmPos = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
						pmPos *= -mCamera->getCameraPos().z / pmPos.z;
						Opos = {mCamera->getCameraPos().x, mCamera->getCameraPos().y};
						Opos += glm::dvec2{pmPos.x, pmPos.y};
					}
				}
				else
				{
					if (PhysicsFormworkPtr)
					{
						beang2 = {huoqdedian.x, huoqdedian.y};
						if (PhysicsSwitch)
						{
							// 开启物理了，就以受力的方式改变位置
							if (PhysicsFormworkPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::shape)
							{
								PhysicsBlock::AngleMat lAngleMat(((PhysicsBlock::PhysicsShape *)PhysicsFormworkPtr)->angle);
								((PhysicsBlock::PhysicsShape*)PhysicsFormworkPtr)->AddForce(lAngleMat.Rotary(PhysicsShapeArm) + PhysicsFormworkPtr->PFGetPos(), (beang2 - (lAngleMat.Rotary(PhysicsShapeArm) + PhysicsFormworkPtr->PFGetPos())) * FLOAT_(100.0));
								mAuxiliaryVision->Line({lAngleMat.Rotary(PhysicsShapeArm) + PhysicsFormworkPtr->PFGetPos(), 0}, {1, 0, 0, 1}, {(beang2), 0}, {0, 1, 0, 1});
							}
							else if (PhysicsFormworkPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::particle)
							{
								((PhysicsBlock::PhysicsParticle*)PhysicsFormworkPtr)->AddForce((beang2 - ((PhysicsBlock::PhysicsParticle*)PhysicsFormworkPtr)->pos) * FLOAT_(100.0));
								mAuxiliaryVision->Line({ ((PhysicsBlock::PhysicsParticle*)PhysicsFormworkPtr)->pos, 0 }, { 1, 0, 0, 1 }, { (beang2), 0 }, { 0, 1, 0, 1 });
							}
						}
						else
						{
							// 没有开启物理，就直接修改位置
							((PhysicsBlock::PhysicsParticle *)PhysicsFormworkPtr)->pos = Opos + beang2;
						}
					}
					else
					{
						glm::vec3 pmPos = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
						pmPos *= -mCamera->getCameraPos().z / pmPos.z;
						m_position.x = Opos.x - pmPos.x;
						m_position.y = Opos.y - pmPos.y;
					}
				}
			}
			Z_fangzhifanfuvhufa = Leftan;
			// 点击右键
			static int fangzhifanfuvhufa; // 避免反复触发
			Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
			if (Leftan == GLFW_PRESS && fangzhifanfuvhufa != Leftan)
			{
				end = {huoqdedian.x, huoqdedian.y};
			}
			fangzhifanfuvhufa = Leftan;
		}

		TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
		if (PhysicsSwitch)
		{
			mPhysicsWorld->PhysicsEmulator(1 ? 0.01 : TOOL::FPStime);
		}
		else
		{
			mPhysicsWorld->PhysicsInformationUpdate();
		}
		TOOL::mTimer->StartEnd();

		// mAuxiliaryVision->Line({beang, 0}, {1, 0, 0, 1}, {end, 0}, {1, 0, 0, 1});

		PhysicsBlock::PhysicsShape *PhysicsShapePtr;
		PhysicsBlock::PhysicsParticle *PhysicsParticlePtr;
		PhysicsBlock::CollisionInfoD info;

		// 渲染物理网格
		for (auto i : mPhysicsWorld->GetPhysicsShape())
		{
			/*
			PhysicsBlock::CollisionInfoD info = i->PsBresenhamDetection(beang, end);
			if (info.Collision) {
				mAuxiliaryVision->Spot({ info.pos, 0 }, { 0, 1, 1, 1 });
			}*/
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
				// 辅助显示质心
				if (PhysicsBlock::Auxiliary_CentreMassBool)
					mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_CentreMassColor));
				// 辅助显示几何中心
				if (PhysicsBlock::Auxiliary_CentreShapeBool)
					mAuxiliaryVision->Spot({i->pos - PhysicsBlock::vec2angle(i->CentreMass - i->CentreShape, i->angle), 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_CentreShapeColor));
				// 辅助显示外骨骼点
				if (PhysicsBlock::Auxiliary_OutlineBool) {
					PhysicsBlock::AngleMat angleMat(i->angle);
					for	(size_t d = 0; d < i->OutlineSize; ++d) {
						mAuxiliaryVision->Spot({ i->pos + angleMat.Rotary(i->OutlineSet[d]), 0 }, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_OutlineColor));
					}
				}
			}

			for (size_t x = 0; x < i->width; x++)
			{
				for (size_t y = 0; y < i->height; y++)
				{
					if (i->at(x, y).Entity)
					{
						ShowSquare(PhysicsBlock::vec2angle(Vec2_{x, y} - i->CentreMass, i->angle) + i->pos, i->angle, {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});
					}
				}
			}
		}
		// 渲染物理点
		for (auto i : mPhysicsWorld->GetPhysicsParticle())
		{
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

			mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});
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
			for (auto &i : mPhysicsWorld->CollideGroupS)
			{
				for (int j = 0; j < i.second->numContacts; ++j)
				{
					// 碰撞点
					if (PhysicsBlock::Auxiliary_CollisionDropBool)
						mAuxiliaryVision->Spot({i.second->contacts[j].position, 0}, 0.05f, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropColor));
					// 碰撞点 分离 法向量
					if (PhysicsBlock::Auxiliary_SeparateNormalVectorBool)
						mAuxiliaryVision->Line({i.second->contacts[j].position, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SeparateNormalVectorColor), {i.second->contacts[j].position + i.second->contacts[j].normal, 0}, ColorToVec4(PhysicsBlock::Auxiliary_SeparateNormalVectorColor));
					// 指向重心
					if (PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityBool)
					{
						mAuxiliaryVision->Line({i.second->contacts[j].position, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor), {i.second->contacts[j].position - i.second->contacts[j].r1, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor));
						mAuxiliaryVision->Line({i.second->contacts[j].position, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor), {i.second->contacts[j].position - i.second->contacts[j].r2, 0}, ColorToVec4(PhysicsBlock::Auxiliary_CollisionDropToCenterOfGravityColor));
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

}
