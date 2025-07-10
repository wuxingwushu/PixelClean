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
		m_position.x = 0;
		m_position.y = 0;
		m_position.z = 50;

		// 添加视觉辅助
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 10000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		PhysicsBlock::DemoFunS[0](&mPhysicsWorld, mCamera);
		mMapStatic = (PhysicsBlock::MapStatic*)mPhysicsWorld->GetMapFormwork();
		for (size_t x = 0; x < mMapStatic->width; x++)
		{
			for (size_t y = 0; y < mMapStatic->height; y++)
			{
				if (mMapStatic->at(x, y).Entity)
				{
					ShowStaticSquare(glm::dvec2{x, y} - mMapStatic->centrality, 0, {0, 1, 0, 1});
				}
			}
		}
	}

	PhysicsTest::~PhysicsTest()
	{
		delete mAuxiliaryVision;
		delete mPhysicsWorld;
	}

	void PhysicsTest::MouseMove(double xpos, double ypos)
	{
		// mCamera->onMouseMove(xpos, ypos);
	}

	void PhysicsTest::MouseRoller(int z)
	{
		if (m_position.z <= 10) {
			m_position.z += (m_position.z / 2) * z;
		}
		else {
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
		ImGui::SetWindowPos(ImVec2(Global::mWidth - window_size.x, 0));

		
		static int item_current = 0;
		static int item_Demo_idx = 0;
		ImGui::Combo("Demo", &item_Demo_idx, PhysicsBlock::DemoNameS, IM_ARRAYSIZE(PhysicsBlock::DemoNameS));
		if (item_current != item_Demo_idx)
		{
			item_current = item_Demo_idx;
			PhysicsBlock::DemoFunS[item_Demo_idx](&mPhysicsWorld, mCamera);
			mMapStatic = (PhysicsBlock::MapStatic*)mPhysicsWorld->GetMapFormwork();
			PhysicsFormworkPtr = nullptr;
			mAuxiliaryVision->ClearStaticLine();// 清空上一个地图的静态网格
			for (size_t x = 0; x < mMapStatic->width; x++)
			{
				for (size_t y = 0; y < mMapStatic->height; y++)
				{
					if (mMapStatic->at(x, y).Entity)
					{
						ShowStaticSquare(glm::dvec2{x, y} - mMapStatic->centrality, 0, {0, 1, 0, 1});
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
		if (PhysicsAssistantInformation) {
			ImGui::Checkbox(u8"显示物理碰撞点", &PhysicsCollisionDrop); HelpMarker2("两个物体的接触点");
			ImGui::Checkbox(u8"显示分离法向量", &PhysicsSeparateNormalVector); HelpMarker2("两个物体间分离轴的法向量");
			ImGui::Checkbox(u8"碰撞点指向重心", &PhysicsCollisionDropToCenterOfGravity); HelpMarker2("碰撞点到两个物体重心的连线");
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
			if (PhysicsBlock::PhysicsShapeUI(mPhysicsWorld)) {
				mAuxiliaryVision->ClearStaticLine();// 清空上一个地图的静态网格
				for (size_t x = 0; x < mMapStatic->width; x++)
				{
					for (size_t y = 0; y < mMapStatic->height; y++)
					{
						if (mMapStatic->at(x, y).Entity)
						{
							ShowStaticSquare(glm::dvec2{ x, y } - mMapStatic->centrality, 0, { 0, 1, 0, 1 });
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

		static glm::dvec2 beang{0}, beang2{0}, Opos{0}, end{0};

		mAuxiliaryVision->Begin();

		// 不在窗口上才可以操作
		if (!Global::ClickWindow)
		{
			// 点击左键
			static int Z_fangzhifanfuvhufa; // 避免反复触发
			static glm::dvec2 PhysicsShapeArm; //
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
						if (PhysicsFormworkPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::shape) {
							PhysicsBlock::AngleMat lAngleMat(((PhysicsBlock::PhysicsShape*)PhysicsFormworkPtr)->angle);
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
						if (PhysicsSwitch) {
							// 开启物理了，就以受力的方式改变位置
							if (PhysicsFormworkPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::shape) {
								PhysicsBlock::AngleMat lAngleMat(((PhysicsBlock::PhysicsShape*)PhysicsFormworkPtr)->angle);
								((PhysicsBlock::PhysicsShape *)PhysicsFormworkPtr)->AddForce(lAngleMat.Rotary(PhysicsShapeArm) + PhysicsFormworkPtr->PFGetPos(), (beang2 - (lAngleMat.Rotary(PhysicsShapeArm) + PhysicsFormworkPtr->PFGetPos())) * 100.0);
								mAuxiliaryVision->Line({ lAngleMat.Rotary(PhysicsShapeArm) + PhysicsFormworkPtr->PFGetPos(), 0 }, { 1, 0, 0, 1 }, { (beang2), 0 }, { 0, 1, 0, 1 });
							}else if (PhysicsFormworkPtr->PFGetType() == PhysicsBlock::PhysicsObjectEnum::particle) {
								((PhysicsBlock::PhysicsParticle *)PhysicsFormworkPtr)->AddForce((Opos + beang2) - ((PhysicsBlock::PhysicsParticle *)PhysicsFormworkPtr)->pos);
							}
						}
						else {
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

		

		if (PhysicsSwitch) {
			mPhysicsWorld->PhysicsEmulator(TOOL::FPStime);
		}
		else {
			mPhysicsWorld->PhysicsInformationUpdate();
		}
			

		//mAuxiliaryVision->Line({beang, 0}, {1, 0, 0, 1}, {end, 0}, {1, 0, 0, 1});

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

			for (size_t x = 0; x < i->width; x++)
			{
				for (size_t y = 0; y < i->height; y++)
				{
					if (i->at(x, y).Entity)
					{
						ShowSquare(SquarePhysics::vec2angle(glm::dvec2{ x, y } - i->CentreMass, i->angle) + i->pos, i->angle, { 0, 1, 0, 1 });
					}
				}
			}
		}
		// 渲染物理点
		for (auto i : mPhysicsWorld->GetPhysicsParticle())
		{
			mAuxiliaryVision->Spot({ i->pos, 0 }, { 0, 1, 0, 1 });
		}
		// 渲染绳子
		for (auto i : mPhysicsWorld->GetPhysicsJoint())
		{
			
			mAuxiliaryVision->Line({i->body1->pos, 0 }, {0, 1, 0, 1}, {i->body1->pos + i->r1, 0 }, {0, 1, 0, 1});
			mAuxiliaryVision->Line({i->body2->pos, 0 }, {0, 1, 0, 1}, {i->body2->pos + i->r2, 0 }, {0, 1, 0, 1});
		}

		// 渲染物理信息
		if (PhysicsAssistantInformation) {
			for (auto& i : mPhysicsWorld->CollideGroupS)
			{
				for (int j = 0; j < i.second->numContacts; ++j)
				{
					// 碰撞点
					if (PhysicsCollisionDrop) mAuxiliaryVision->Spot({ i.second->contacts[j].position, 0 }, { 1,0,0,1 });
					// 碰撞点 分离 法向量
					if (PhysicsSeparateNormalVector) mAuxiliaryVision->Line({ i.second->contacts[j].position, 0 }, { 0,0,1,1 }, { i.second->contacts[j].position + i.second->contacts[j].normal, 0 }, { 0,1,1,1 });
					// 指向重心
					if (PhysicsCollisionDropToCenterOfGravity) {
						mAuxiliaryVision->Line({ i.second->contacts[j].position, 0 }, { 0,0,1,1 }, { i.second->contacts[j].position - i.second->contacts[j].r1, 0 }, { 0,0,1,1 });
						mAuxiliaryVision->Line({ i.second->contacts[j].position, 0 }, { 0,0,1,1 }, { i.second->contacts[j].position - i.second->contacts[j].r2, 0 }, { 0,0,1,1 });
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
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, angle);
		glm::dvec2 jiao3 = PhysicsBlock::vec2angle({1, 1}, angle);
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao2, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao2, 0}, color);
	}

	void PhysicsTest::ShowSquare(glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, angle);
		glm::dvec2 jiao3 = PhysicsBlock::vec2angle({1, 1}, angle);
		mAuxiliaryVision->Line({pos, 0}, color, {pos + jiao1, 0}, color);
		mAuxiliaryVision->Line({pos, 0}, color, {pos + jiao2, 0}, color);
		mAuxiliaryVision->Line({pos + jiao3, 0}, color, {pos + jiao1, 0}, color);
		mAuxiliaryVision->Line({pos + jiao3, 0}, color, {pos + jiao2, 0}, color);
	}

}
