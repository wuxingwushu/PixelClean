#include "PhysicsTest.h"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"
#include "../../Opcode/OpcodeFunction.h"
#include "../../Physics/DestroyMode.h"
#include "../../GlobalVariable.h"
#include "../../PhysicsBlock/BaseCalculate.hpp"
#include "../../PhysicsBlock/ImGuiPhysics.hpp"


namespace GAME {


	PhysicsTest::PhysicsTest(Configuration wConfiguration) : Configuration{ wConfiguration }
	{
		m_position.x = 0;
		m_position.y = 0;
		m_position.z = 50;

		// 添加视觉辅助
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 1000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer
		);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mPhysicsWorld = new PhysicsBlock::PhysicsWorld({0, 0}, false);
		mMapStatic = new PhysicsBlock::MapStatic(3,3);
		for (size_t i = 0; i < 9; i++)
		{
			mMapStatic->at(i).Entity = true;
			mMapStatic->at(i).Collision = true;
		}
		mMapStatic->at(0).Entity = false;
		mMapStatic->at(0).Collision = false;
		/*for (size_t x = 0; x < mMapStatic->width; x++)
		{
			for (size_t y = 0; y < mMapStatic->height; y++)
			{
				if (mMapStatic->at(x, y).Entity) {
					ShowSquare({ x, y });
				}
			}
		}*/

		PhysicsBlock::PhysicsShape* PhysicsShape1 = new PhysicsBlock::PhysicsShape({ 0, 0 }, { 2, 2 });
		for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); i++)
		{
			PhysicsShape1->at(i).Collision = true;
			PhysicsShape1->at(i).Entity = true;
			PhysicsShape1->at(i).mass = 3;
		}
		PhysicsShape1->UpdateInfo();
		PhysicsShape1->UpdateOutline();
		PhysicsShape1->CollisionR = 1.414;
		PhysicsShape1->angle = 3.1415926 / 4;
		PhysicsBlock::PhysicsShape* PhysicsShape2 = new PhysicsBlock::PhysicsShape({ 1, 2 }, { 2, 2 });
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); i++)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = 1;
		}
		PhysicsShape2->UpdateInfo();
		PhysicsShape2->UpdateOutline();
		PhysicsShape2->CollisionR = 1.414;
		PhysicsShape2->speed = { 0, -1 };

		mPhysicsWorld->AddObject(PhysicsShape1);
		mPhysicsWorld->AddObject(PhysicsShape2);
	}

	PhysicsTest::~PhysicsTest()
	{
		delete mAuxiliaryVision;
		delete mPhysicsWorld;
	}

	void PhysicsTest::MouseMove(double xpos, double ypos)
	{
		//mCamera->onMouseMove(xpos, ypos);
	}

	void PhysicsTest::MouseRoller(int z)
	{
		m_position.z += z * 10;
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
			if (Global::ConsoleBool) {
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			}else {
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

	void PhysicsTest::GameUI() {
		ImGui::Begin(u8"属性 ");
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((Global::mWidth - window_size.x) < CursorPosX) && (window_size.y > CursorPosY)) {
			Global::ClickWindow = true;
		}
		ImGui::SetWindowPos(ImVec2(Global::mWidth - window_size.x, 0));

		if (PhysicsFormworkPtr) {
			switch (PhysicsFormworkPtr->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::shape:
				ImGui::Text(u8"shape");
				PhysicsBlock::PhysicsShapeUI((PhysicsBlock::PhysicsShape*)PhysicsFormworkPtr);
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				ImGui::Text(u8"particle");
				PhysicsBlock::PhysicsShapeUI((PhysicsBlock::PhysicsParticle*)PhysicsFormworkPtr);
				break;
			default:
				ImGui::Text(u8"无");
				break;
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


		static glm::dvec2 beang{ 0 }, beang2{ 0 }, Opos{ 0 }, end{ 0 };

		// 不在窗口上才可以操作
		if (!Global::ClickWindow) {
			//点击左键
			static int Z_fangzhifanfuvhufa;// 避免反复触发
			int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
			if (Leftan == GLFW_PRESS) {
				if (Z_fangzhifanfuvhufa != Leftan) {
					beang = { huoqdedian.x, huoqdedian.y };
					PhysicsFormworkPtr = mPhysicsWorld->Get(beang);
					if (PhysicsFormworkPtr) {
						Opos = PhysicsFormworkPtr->PFGetPos();
					}
					else {
						glm::vec3 pmPos = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
						pmPos *= -mCamera->getCameraPos().z / pmPos.z;
						Opos = { mCamera->getCameraPos().x , mCamera->getCameraPos().y };
						beang = { pmPos.x, pmPos.y };
						Opos += beang;
					}
					
				}
				else {
					if (PhysicsFormworkPtr) {
						beang2 = { huoqdedian.x, huoqdedian.y };
						((PhysicsBlock::PhysicsParticle*)PhysicsFormworkPtr)->pos = Opos + (beang2 - beang);
					}
					else {
						glm::vec3 pmPos = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
						pmPos *= -mCamera->getCameraPos().z / pmPos.z;
						m_position.x = Opos.x - pmPos.x;
						m_position.y = Opos.y - pmPos.y;
					}
				}
			}Z_fangzhifanfuvhufa = Leftan;
			//点击右键
			static int fangzhifanfuvhufa;// 避免反复触发
			Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
			if (Leftan == GLFW_PRESS && fangzhifanfuvhufa != Leftan) {
				end = { huoqdedian.x, huoqdedian.y };
			}fangzhifanfuvhufa = Leftan;
		}
		

		mAuxiliaryVision->Begin();

		mPhysicsWorld->PhysicsEmulator(TOOL::FPStime);

		PhysicsBlock::PhysicsShape* PhysicsShapePtr;
		PhysicsBlock::PhysicsParticle* PhysicsParticlePtr;
		for (auto i : *mPhysicsWorld->GetPhysicsFormworkS())
		{
			switch (i->PFGetType())
			{
			case PhysicsBlock::PhysicsObjectEnum::shape:
				PhysicsShapePtr = (PhysicsBlock::PhysicsShape*)i;
				glm::dvec2 POS = PhysicsShapePtr->pos;
				for (size_t x = 0; x < PhysicsShapePtr->width; x++)
				{
					for (size_t y = 0; y < PhysicsShapePtr->height; y++)
					{
						if (PhysicsShapePtr->at(x, y).Entity) {
							ShowSquare(SquarePhysics::vec2angle(glm::dvec2{ x, y } - PhysicsShapePtr->CentreMass, PhysicsShapePtr->angle) + POS, PhysicsShapePtr->angle);
						}
					}
				}
				
				break;
			case PhysicsBlock::PhysicsObjectEnum::particle:
				PhysicsParticlePtr = (PhysicsBlock::PhysicsParticle*)i;
				mAuxiliaryVision->AddSpot({ PhysicsParticlePtr->pos, 0 }, { 1,0,0,1 });
				break;

			default:
				break;
			}
		}

		mAuxiliaryVision->End();

		//更新Camera变换矩阵
		mCamera->update();
		VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[mCurrentFrame]->getupdateBufferByMap();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();//获取ViewMatrix数据
		mCameraVPMatricesBuffer[mCurrentFrame]->endupdateBufferByMap();
	}

	void PhysicsTest::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
	}

	//游戏停止界面循环
	void PhysicsTest::GameStopInterfaceLoop(unsigned int mCurrentFrame) {
		if (InterFace->GetInterfaceIndexes() == InterFaceEnum_::ViceInterface_Enum && Global::MultiplePeopleMode && !Global::ServerOrClient) {
			
		}
	}

	//游戏 TCP事件
	void PhysicsTest::GameTCPLoop() {
		if (Global::MultiplePeopleMode)
		{
			if (Global::ServerOrClient) {
				event_base_loop(server::GetServer()->GetEvent_Base(), EVLOOP_NONBLOCK);
			}
			else {
				event_base_loop(client::GetClient()->GetEvent_Base(), EVLOOP_ONCE);
			}
		}
	}


	void PhysicsTest::ShowStaticSquare(glm::dvec2 pos, double angle) {
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({ 0,1 }, angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({ 1,0 }, angle);
		glm::dvec2 jiao3 = PhysicsBlock::vec2angle({ 1,1 }, angle);
		mAuxiliaryVision->AddStaticLine({ pos,0 }, { pos + jiao1,0 }, { 1,0,0,1 });
		mAuxiliaryVision->AddStaticLine({ pos,0 }, { pos + jiao2,0 }, { 1,0,0,1 });
		mAuxiliaryVision->AddStaticLine({ pos + jiao3,0 }, { pos + jiao1,0 }, { 1,0,0,1 });
		mAuxiliaryVision->AddStaticLine({ pos + jiao3,0 }, { pos + jiao2,0 }, { 1,0,0,1 });
	}

	void PhysicsTest::ShowSquare(glm::dvec2 pos, double angle) {
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({ 0,1 }, angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({ 1,0 }, angle);
		glm::dvec2 jiao3 = PhysicsBlock::vec2angle({ 1,1 }, angle);
		mAuxiliaryVision->AddLine({ pos,0 }, { pos + jiao1,0 }, { 1,0,0,1 });
		mAuxiliaryVision->AddLine({ pos,0 }, { pos + jiao2,0 }, { 1,0,0,1 });
		mAuxiliaryVision->AddLine({ pos + jiao3,0 }, { pos + jiao1,0 }, { 1,0,0,1 });
		mAuxiliaryVision->AddLine({ pos + jiao3,0 }, { pos + jiao2,0 }, { 1,0,0,1 });
	}

}
