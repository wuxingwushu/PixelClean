#include "PhysicsTest.h"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"
#include "../../Opcode/OpcodeFunction.h"
#include "../../Physics/DestroyMode.h"


namespace GAME {


	PhysicsTest::PhysicsTest(Configuration wConfiguration) : Configuration{ wConfiguration }
	{
		// 添加视觉辅助
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 1000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer
		);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mPhysicsWorld = new PhysicsBlock::PhysicsWorld({0, -9.8}, false);
		mMapStatic = new PhysicsBlock::MapStatic(3,3);
		for (size_t i = 0; i < 9; i++)
		{
			mMapStatic->at(i).Entity = true;
			mMapStatic->at(i).Collision = true;
		}
		mMapStatic->at(0).Entity = false;
		mMapStatic->at(0).Collision = false;
		for (size_t x = 0; x < mMapStatic->width; x++)
		{
			for (size_t y = 0; y < mMapStatic->height; y++)
			{
				if (mMapStatic->at(x, y).Entity) {
					ShowStop({ x, y });
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

	void PhysicsTest::GameLoop(unsigned int mCurrentFrame)
	{

		int winwidth, winheight;
		glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;


		static glm::dvec2 beang{ 0 }, end{ 0 };


		//点击左键
		if (glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			beang = { huoqdedian.x, huoqdedian.y };
		}
		//点击右键
		static int fangzhifanfuvhufa;// 避免反复触发
		int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
		if (Leftan == GLFW_PRESS && fangzhifanfuvhufa != Leftan) {
			end = { huoqdedian.x, huoqdedian.y };
		}fangzhifanfuvhufa = Leftan;

		mAuxiliaryVision->Begin();

		PhysicsBlock::CollisionInfoD stop = mMapStatic->FMBresenhamDetection(beang, end);
		if (stop.Collision) {
			mAuxiliaryVision->AddSpot({ stop.pos, 0 }, { 0,0,1,1 });
		}

		{
			glm::ivec2 start1 = beang, end1 = end;
			int dx = abs(end1.x - start1.x);
			int dy = abs(end1.y - start1.y);
			int sx = (start1.x < end1.x) ? 1 : -1;
			int sy = (start1.y < end1.y) ? 1 : -1;
			int err = dx - dy;
			int e2;
			while (true)
			{
				if (end1.x == start1.x && end1.y == start1.y)
				{
					break;
				}
				e2 = 2 * err;
				if (e2 > -dy)
				{
					err -= dy;
					start1.x += sx;
					mAuxiliaryVision->AddSpot({ (glm::dvec2)start1 + glm::dvec2{0.5,0.5}, 0 }, { 0,1,1,1 });
				}
				if (e2 < dx)
				{
					err += dx;
					start1.y += sy;
					mAuxiliaryVision->AddSpot({ (glm::dvec2)start1 + glm::dvec2{0.5,0.5}, 0 }, { 0,1,1,1 });
				}
			}
		}

		mAuxiliaryVision->Line({ beang,0 }, { 1,0,0,1 } , { end,0 }, {0,1,0,1});

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

}
