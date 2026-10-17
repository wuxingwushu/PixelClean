#include "PhysicsTest3D.h"
#include "PhysicsDemo3D.h"
// ── 3D 版物理控件（与 2D 的 ImGuiPhysics.hpp 同名函数但不同命名空间，可共存）──
//    本文件**全程使用全限定名** `PhysicsBlock3D::Xxx()`，且不写任何
//    `using namespace PhysicsBlock3D;`，以免与 2D 的 `PhysicsBlock::Xxx` 歧义。
#include "../../PhysicsBlock3D/ImGuiPhysics3D.hpp"
// 体素编辑需要直接读地图/形状的体素表与体素结构
#include "../../PhysicsBlock3D/MapFormwork3D.hpp"
#include "../../PhysicsBlock3D/BaseGrid3D.hpp"
// LOGD/LOGE
#include "../../DebugLog.h"
// Global::ClickWindow / Global::mWidth / Global::mHeight / Global::ConsoleBool
#include "../../GlobalVariable.h"
// TOOL::FPStime（帧时间）、TOOL::mTimer
#include "../../Tool/Tool.h"
#include "../../Camera/Camera.h"
#include <cmath>

namespace GAME
{

	namespace
	{
		/* ═══════════════════════════════════════════════════════════════════
		 * 匿名命名空间：本 TU 私有的小工具
		 * ───────────────────────────────────────────────────────────────────
		 * 3D 版相较 2D 新增/改写的三个"几何原语"，都放在这里：
		 *   1. VoxelCenter3D —— 体素坐标 → 世界坐标（形状局部坐标 + 旋转 + 质心偏移）
		 *   2. WriteGridBlock3D —— 整块体素赋值（**不改**结构体定义，见下方注释）
		 *   3. MakeBoxCorners3D / MakeBoxFaces3D —— 线框盒与实心盒的顶点生成
		 *      （2D 有现成的 DrawSquare 可直接用，3D 没有等价的"立方体"辅助，
		 *      所以自己按 8 顶点 / 12 棱 / 6 面拼）
		 * ═══════════════════════════════════════════════════════════════════ */

		/** @brief 体素坐标 → 世界坐标（体素中心）
		 *  @details 形状的 `pos` 是**质心**的世界坐标，而网格原点的局部坐标是
		 *           `-CentreMass`（质心在网格下的位置）。因此：
		 *             世界 = pos + QuatRotate(rotation, (cell - CentreMass))      */
		glm::vec3 VoxelCenter3D(const PhysicsBlock3D::PhysicsShape3D *shape, const glm::ivec3 &cell)
		{
			const glm::vec3 local = glm::vec3(cell) - shape->CentreMass;
			return shape->pos + PhysicsBlock3D::QuatRotate(shape->rotation, local);
		}

		/**
		 * @brief 把一个体素整块写成指定的调试参数
		 * @note 3D 的 `GridBlock3D::~GridBlock3D` 是**有实际语义**的
		 *       （见 BaseStruct3D.hpp：它会把 Entity/Collision/Event 清零并
		 *       重置 type），所以**不能**用 `*dst = src` 或 placement-new
		 *       的方式整体赋值 —— 那会析构掉 dst 现有的位域并丢失语义。
		 *       这里改用"逐字节复制到 Entity 起始位置"的写法：
		 *       位域布局在同一个类型内是确定的，`unsigned char Healthpoint`
		 *       与 `FLOAT_ mass` 共用一个 union，把 `GridBlock3D` 的前
		 *       `sizeof(GridBlock3D)` 个字节复制过去即可完整搬运所有字段，
		 *       且完全不触碰任何析构函数。这也是本文件**唯一**的
		 *       "绕开类接口"的地方，特此注明。
		 */
		void WriteGridBlock3D(PhysicsBlock3D::GridBlock3D *dst, const PhysicsBlock3D::GridBlock3D &src)
		{
			if (dst == nullptr)
			{
				return;
			}
			// 逐个字段赋值，语义与 src 完全一致，且不依赖内存布局假设。
			dst->FrictionFactor = src.FrictionFactor;
			dst->mass = src.mass;
			dst->Entity = src.Entity;
			dst->Collision = src.Collision;
			dst->Event = src.Event;
			dst->type = src.type;
		}

		/** @brief 生成绕中心 ±half 的立方体 8 个角（world 坐标，未旋转） */
		void MakeBoxCorners3D(const glm::vec3 &center, const glm::vec3 &half, glm::vec3 out[8])
		{
			int i = 0;
			for (int dx = -1; dx <= 1; dx += 2)
			{
				for (int dy = -1; dy <= 1; dy += 2)
				{
					for (int dz = -1; dz <= 1; dz += 2)
					{
						out[i++] = center + glm::vec3(dx * half.x, dy * half.y, dz * half.z);
					}
				}
			}
		}

		/** @brief 立方体 12 条棱的顶点索引对（与 MakeBoxCorners3D 的枚举顺序配套） */
		const int kBoxEdges3D[12][2] = {
		    {0, 1}, {2, 3}, {4, 5}, {6, 7}, // 沿 X 的四条棱
		    {0, 2}, {1, 3}, {4, 6}, {5, 7}, // 沿 Y 的四条棱
		    {0, 4}, {1, 5}, {2, 6}, {3, 7}, // 沿 Z 的四条棱
		};
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 构造 / 析构
	// ═══════════════════════════════════════════════════════════════════════

	PhysicsTest3D::PhysicsTest3D(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		LOGD("PhysicsTest3D::PhysicsTest3D constructor");

		// ── 1. 物理辅助视觉后端 ────────────────────────────────────────
		// 构造方式与 2D 完全一致（`mAuxiliaryVision` 的类型同样是
		// VulKan::AuxiliaryVision，`PhysicsAuxiliaryVision3D.hpp` 用的就是它）。
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 150000);
		mAuxiliaryVision->initUniformManager(
		    mSwapChain->getImageCount(),
		    mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		// ── 1.5 「面」渲染器 ──────────────────────────────────────────
		// 复用 BlockWorld 的三角形管线（取舍说明见 PhysicsFaceRenderer3D.hpp）。
		// 初始化失败不致命：置空即退化为"只有线框"，与改造前行为一致。
		mFaceRenderer = new PhysicsFaceRenderer3D(this);
		if (!mFaceRenderer->Init())
		{
			LOGE("PhysicsTest3D: 面渲染器初始化失败，本次运行只能使用线框渲染");
			delete mFaceRenderer;
			mFaceRenderer = nullptr;
		}

		// ── 2. 首个 Demo ──────────────────────────────────────────────
		// DemoFunS3D 的约定（见 PhysicsDemo3D.h）：函数内部**自己**销毁旧世界、
		// 新建世界、并把相机摆好。所以传进去的 mPhysicsWorld 初值 null 是合法的。
		PhysicsBlock3D::DemoFunS3D[0](&mPhysicsWorld, mCamera);
		mMapFormwork = mPhysicsWorld->GetMapFormwork();
		mDynamicMap3D = dynamic_cast<PhysicsBlock3D::MapDynamic3D *>(mMapFormwork);

		// 从 Demo 摆好的视图反推 (Yaw, Pitch)，否则第一次鼠标拖拽会把视角
		// 猛地甩回成员默认值。
		// 注意：`ApplyCameraView3D` 已经把相机摆正了，这里只是把角度读回来。
		SyncCameraAnglesFromViewLocal();
		mCameraPos = mCamera->getCameraPos();

		// ── 3. 地图轮廓静态缓冲 ────────────────────────────────────────
		// 注意顺序：`RenderMapOutline3D` 把轮廓写进 av 的**静态**缓冲，
		// 属于"录制外"调用（见 PhysicsAuxiliaryVision3D.hpp 顶部注释），
		// 所以要在任何 Begin()/End() 之外、且在世界建立之后调用。
		PhysicsBlock3D::RenderMapOutline3D(mAuxiliaryVision, mPhysicsWorld);

		// ── 4. 读取辅助显示的 ini 配置 ────────────────────────────────
		PhysicsBlock3D::AuxiliaryInfoRead();

		// ── 5. 首帧缓存窗口尺寸与拾取落点 ─────────────────────────────
		mWinWidth = mWindow->getWidth();
		mWinHeight = mWindow->getHeight();
		mPickHit = mCameraPos + glm::vec3(0.0f, 0.0f, -1.0f) * mSpawnDistance;
		mSpawnPoint = mPickHit;

		Log3D(u8"[PhysicsTest3D] 初始化完成：共 %d 个示例场景\n", PhysicsBlock3D::DemoCount3D);
		Log3D(u8"[PhysicsTest3D] 相机初始位置 (%.1f, %.1f, %.1f)，yaw=%.1f pitch=%.1f\n",
		      (double)mCameraPos.x, (double)mCameraPos.y, (double)mCameraPos.z,
		      (double)mYaw, (double)mPitch);
	}

	PhysicsTest3D::~PhysicsTest3D()
	{
		LOGD("PhysicsTest3D::~PhysicsTest3D destructor");

		// 保存辅助显示参数（与 2D 一致：析构时回写 ini）
		PhysicsBlock3D::AuxiliaryInfoStorage();

		// ── 释放顺序：**先世界，后视觉**（与 2D 相反，理由见下）─────────
		// 2D 是 `delete mAuxiliaryVision; delete mPhysicsWorld;`。
		// 3D 反过来，因为：
		//   · `RenderMapOutline3D(av, world)` 会把"地图轮廓线段"写进 av 的
		//     静态缓冲。那份顶点数据本身不持有 world 的对象指针，所以先删
		//     world 不会让 av 悬垂；但如果先删 av，紧接着 delete world 时
		//     world 的析构里若还要访问任何已被 av 记录的语义（例如将来把
		//     轮廓所有权移交给 av），就会踩已释放内存。
		//   · 更关键的是**本类自己的成员**：析构会在删 world 时级联删除所有
		//     物体，此时 `PhysicsFormworkPtr / GridEditShape / DragObject /
		//     mLastCreated / mPickObject / RightClickObjectPtr` 全部变成
		//     悬垂指针。所以第一步必须先把这些缓存指针清空。
		// 注意：**不要**在这里调用 InterFace->SetInterFaceBool()。
		// 该函数只是把 InterFaceBool 取反（Interface.h:66-68），而 InterFaceBool
		// 同时决定 application.cpp 主循环走哪条分支：
		//   true  → InterFace 分支（只画界面，不碰 mGameMods）
		//   false → 游戏分支（createCommandBuffers 会在 application.cpp:545 调
		//           mGameMods->GameCommandBuffers(i)，且 mainLoop 会在 :827 调
		//           mGameMods->GameLoop()）
		// 从"返回主页"删除本模式时，本析构是在 InterFace 分支里被调用的；若在此
		// 把 InterFaceBool 从 true 翻成 false，同一帧后面的 Render() 就会改走游戏
		// 分支，而此时 DeleteGame() 已把 mGameMods 置空 → 空指针解引用
		// （实测崩溃：0xC0000005，偏移 0x82b3b = application.cpp:545）。
		// 2D 的 PhysicsTest 析构（GameMods/PhysicsTest/PhysicsTest.cpp:51-61）也
		// 没有这一步，界面收起由 InterFace 自己的状态机负责。
		PhysicsFormworkPtr = nullptr;
		GridEditShape = nullptr;
		DragObject = nullptr;
		mLastCreated = nullptr;
		mPickObject = nullptr;
		RightClickObjectPtr = nullptr;
		mMapFormwork = nullptr;
		mDynamicMap3D = nullptr;

		// 面渲染器持有 Vulkan 资源（顶点缓冲 / 描述符 / 指令缓冲），
		// 必须在世界与 av 之前释放，而且此时 mDevice 仍然有效。
		delete mFaceRenderer;
		mFaceRenderer = nullptr;

		delete mPhysicsWorld;
		mPhysicsWorld = nullptr;

		// 世界已经不存在了，av 里残留的静态轮廓线段已经没有意义，
		// 但释放 av 本身不依赖它，直接删即可。
		delete mAuxiliaryVision;
		mAuxiliaryVision = nullptr;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 相机（照 GameMods/BlockWorld 的自由飞行模式）
	// ═══════════════════════════════════════════════════════════════════════

	void PhysicsTest3D::SyncCameraAnglesFromViewLocal()
	{
		// `CameraTransform::rebuildViewMatrix()` 用的是 `glm::lookAt(pos, pos+front, up)`，
		// glm 的 mat4 是**列主序**：`m[0]=(s.x,u.x,-f.x,0)`、`m[1]=(s.y,u.y,-f.y,0)`、
		// `m[2]=(s.z,u.z,-f.z,0)`。所以视线方向 f 的三个分量分散在**三列的第 2 行**上：
		//   f = ( -V[0][2], -V[1][2], -V[2][2] )
		// 而**不是**同一列里的 `(-V[2][0], -V[2][1], -V[2][2])`（那是转置读法，取到的是
		// (-s.z, -u.z, f.z)，会让视角偏离约 85°，Demo 摆好的画面直接被甩出视野）。
		//
		// yaw/pitch 直接用 `UpdateCamera()` 的正向公式反解即可：
		//   front = ( cos(pitch)sin(yaw), sin(pitch), cos(pitch)cos(yaw) )
		//   → yaw = atan2(front.x, front.z)，pitch = asin(front.y)
		// 这比"从 right 向量反解 yaw"少一层符号约定（glm 的 right = cross(front, up)
		// = (-cos(yaw), 0, sin(yaw))，与旧注释里写的 (cos, 0, -sin) 正好反向）。
		const glm::mat4 V = mCamera->getViewMatrix();
		const glm::vec3 fRaw(-V[0][2], -V[1][2], -V[2][2]);

		const float len = glm::length(fRaw);
		if (len > 1e-6f)
		{
			const glm::vec3 front = fRaw / len;
			mYaw = glm::degrees(std::atan2(front.x, front.z));
			mPitch = glm::degrees(std::asin(glm::clamp(front.y, -1.0f, 1.0f)));
		}
	}

	void PhysicsTest3D::UpdateCamera()
	{
		// 朝向由 yaw/pitch 唯一确定，与 BlockWorld::UpdateCamera() 完全一致
		const float radYaw = glm::radians(mYaw);
		const float radPitch = glm::radians(mPitch);
		glm::vec3 front;
		front.x = std::cos(radPitch) * std::sin(radYaw);
		front.y = std::sin(radPitch);
		front.z = std::cos(radPitch) * std::cos(radYaw);
		front = glm::normalize(front);

		// 键位平移（本帧累积的输入，按相机局部轴解释）
		if (glm::length(mCameraMoveInput) > 1e-6f)
		{
			const glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
			const glm::vec3 up = glm::normalize(glm::cross(right, front));

			// 归一化斜向移动，避免两个键同按跑得比单键快
			const float mag = glm::length(mCameraMoveInput);
			const glm::vec3 dir = mCameraMoveInput / mag;

			mCameraPos += right * (dir.x * mag) * mCameraSpeed * (float)TOOL::FPStime;
			mCameraPos += up * (dir.y * mag) * mCameraSpeed * (float)TOOL::FPStime;
			mCameraPos += front * (dir.z * mag) * mCameraSpeed * (float)TOOL::FPStime;
			mCameraMoveInput = glm::vec3(0.0f);
		}

		mCamera->setCameraPos(mCameraPos);
		// 注意第二个参数是**方向**不是目标点：`Camera::lookAt(_pos, _front, _up)`
		// 内部执行 `m_transform.lookAt(_pos + _front, _up)`（Camera.h:90-94），
		// 而 `CameraTransform::lookAt(target)` 再算 `front = normalize(target - pos)`。
		// 所以这里只能传 `front` 本身。传 `mCameraPos + front` 会让真实视线变成
		// `normalize(mCameraPos + front)`：|mCameraPos| = 28 时它几乎是"从世界原点朝外"，
		// 场景恰好落在相机背后 —— 这就是"相机看不到物体"的直接原因。
		// 参照实现：`BlockWorld::UpdateCamera()`（GameMods/BlockWorld/BlockWorld.cpp:911）
		// 传的就是 `front`。
		mCamera->lookAt(mCameraPos, front, glm::vec3(0.0f, 1.0f, 0.0f));
		mCamera->update();
	}

	void PhysicsTest3D::MoveCameraLocal(glm::vec3 localDir)
	{
		mCameraMoveInput += localDir;
	}

	void PhysicsTest3D::LogCPUTimeProfile()
	{
		// 3D 引擎没有 GPU 路径（BaseDefine3D.h 明确排除 PhysicsGPU），
		// 所以这里的"CPU 用时"就是全部耗时，直接取引擎公开的五个阶段计时。
		const float total = mPhysicsWorld->GetCollisionDetectionTimeMS() +
		                    mPhysicsWorld->GetPreStepTimeMS() +
		                    mPhysicsWorld->GetApplyImpulseCPUTimeMS() +
		                    mPhysicsWorld->GetPositionUpdateTimeMS() +
		                    mPhysicsWorld->GetPostProcessTimeMS();
		mCPUTimeMS = total;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 3D 射线拾取（取代 2D 的"射线打到 z=0 平面"）
	// ═══════════════════════════════════════════════════════════════════════

	bool PhysicsTest3D::PickWorldPoint3D(glm::vec3 &outHit, PhysicsBlock3D::PhysicsFormwork3D *&outObj)
	{
		outObj = nullptr;

		const int winW = (mWinWidth > 0) ? mWinWidth : mWindow->getWidth();
		const int winH = (mWinHeight > 0) ? mWinHeight : mWindow->getHeight();
		if (winW <= 0 || winH <= 0)
		{
			outHit = glm::vec3(0.0f);
			return false;
		}

		// Camera::screenToWorld 返回的是**归一化方向向量**（Camera.cpp:69-81）。
		// 注意它不返回起点，起点必须自己从 getCameraPos() 取。
		glm::vec3 dir = mCamera->screenToWorld((int)CursorPosX, (int)CursorPosY, winW, winH);
		const float dirLen = glm::length(dir);
		if (dirLen < 1e-8f)
		{
			outHit = glm::vec3(0.0f);
			return false;
		}
		dir /= dirLen;

		const glm::vec3 origin = mCamera->getCameraPos();

		// ── 逐步进：每步先问物体（网格粗筛 + 精确判定），再问地图体素 ──
		for (int i = 1; i <= PickMaxStep3D; ++i)
		{
			const glm::vec3 p = origin + dir * (PickStep3D * (float)i);

			if (PhysicsBlock3D::PhysicsFormwork3D *obj = mPhysicsWorld->Get(p))
			{
				outObj = obj;
				outHit = p;
				return true;
			}

			if (mMapFormwork != nullptr)
			{
				// 用浮点重载：它内部会做 Centrality 偏移与取整，
				// 直接用整数重载在世界坐标为负时会取错格子。
				// FMGetCollide(Vec3_) 直接返回该世界点体素的碰撞位（bool），
				// 内部自带 centrality 偏移，故必须用浮点重载。
				const bool mapHit = mMapFormwork->FMGetCollide(p);
				if (mapHit)
				{
					outHit = p;
					return true;
				}
			}
		}

		// ── 未命中：回落到地面平面 y = 0（等价于 2D 的 z = 0 平面）──────
		if (std::fabs(dir.y) > 1e-6f)
		{
			const float t = -origin.y / dir.y;
			if (t > 0.0f)
			{
				outHit = origin + dir * t;
				return false;
			}
		}

		// 相机几乎水平、或地面交点在身后：退化为"相机前方 mSpawnDistance 处"
		outHit = origin + dir * mSpawnDistance;
		return false;
	}

	bool PhysicsTest3D::WorldToShapeVoxel3D(PhysicsBlock3D::PhysicsShape3D *shape, glm::vec3 worldPt,
	                                        glm::ivec3 &outCell)
	{
		if (shape == nullptr)
		{
			return false;
		}
		const glm::vec3 d = worldPt - shape->pos;
		// 3D 没有标量角：先把世界偏移用四元数逆旋转回物体局部系（2D 只需
		// 反向旋转一个标量角，3D 必须用 rotation 的逆）。
		const glm::vec3 local = glm::vec3(PhysicsBlock3D::QuatInverseRotate(shape->rotation, d)) + shape->CentreMass;
		outCell = glm::ivec3((int)std::floor(local.x),
		                     (int)std::floor(local.y),
		                     (int)std::floor(local.z));
		return shape->InBounds(outCell.x, outCell.y, outCell.z);
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 输入事件
	// ═══════════════════════════════════════════════════════════════════════

	void PhysicsTest3D::MouseMove(double xpos, double ypos)
	{
		// ── 坐标换算：完全照抄 2D 的 PhysicsTest.cpp:66-80 ──────────────
		// Android 下事件坐标已经是逻辑分辨率；桌面下是窗口像素，需要缩放。
		mWinWidth = mWindow->getWidth();
		mWinHeight = mWindow->getHeight();
#ifndef __ANDROID__
		{
			int winW = mWinWidth, winH = mWinHeight;
			double scaleX = (winW > 0) ? (double)Global::mWidth / (double)winW : 1.0;
			double scaleY = (winH > 0) ? (double)Global::mHeight / (double)winH : 1.0;
			CursorPosX = xpos * scaleX;
			CursorPosY = ypos * scaleY;
		}
		mWinWidth = Global::mWidth;
		mWinHeight = Global::mHeight;
#else
		CursorPosX = xpos;
		CursorPosY = ypos;
#endif

		// ── 相机拖拽（3D 新增；2D 的 CameraLeftEvent 只做平面平移）──────
		if (mFirstMouseMove)
		{
			mLastMouseX = CursorPosX;
			mLastMouseY = CursorPosY;
			mFirstMouseMove = false;
			return;
		}

		const double dx = CursorPosX - mLastMouseX;
		const double dy = CursorPosY - mLastMouseY;
		mLastMouseX = CursorPosX;
		mLastMouseY = CursorPosY;

		if (Global::ClickWindow)
		{
			// 光标在 ImGui 窗口上：不旋转，但仍更新缓存坐标
			return;
		}

		if (mLeftMouseDown && !EditorModeBool && GridEditShape == nullptr)
		{
			// 左键拖拽 = 旋转（与 BlockWorld 一致，也与 2D 的"左键拖相机"对应）
			mYaw -= (float)dx * mCameraSensitivity;
			mPitch -= (float)dy * mCameraSensitivity;
		}
		else if (mRightMouseDown)
		{
			// 右键拖拽 = 旋转（2D 的 CameraRightEvent 是空的，3D 复用为旋转）
			mYaw -= (float)dx * mCameraSensitivity;
			mPitch -= (float)dy * mCameraSensitivity;
		}

		// 俯仰限幅（留 1 度余量避免 lookAt 与 up 共线）
		if (mPitch > 89.0f) mPitch = 89.0f;
		if (mPitch < -89.0f) mPitch = -89.0f;
	}

	void PhysicsTest3D::MouseRoller(int z)
	{
		// 在 ImGui 窗口上不响应（照 2D）
		if (Global::ClickWindow)
		{
			return;
		}

		if (GridEditShape != nullptr)
		{
			// 体素编辑模式：滚轮切 Z 层
			const int maxLayer = (int)GridEditShape->depth - 1;
			GridEditLayer += (z > 0) ? 1 : -1;
			if (GridEditLayer < 0) GridEditLayer = 0;
			if (GridEditLayer > maxLayer) GridEditLayer = maxLayer;
			Log3D(u8"[体素编辑] 切换到 Z 层 %d / %d\n", GridEditLayer, maxLayer);
			return;
		}

		// 2D 的滚轮调的是"相机高度 m_position.z"；3D 没有高度平面，
		// 改为调"自由飞行速度"（Ctrl 按住时调"无命中时的创建距离"）。
		if (mRightMouseDown)
		{
			// 右键按住时滚轮调创建距离，方便在空处摆放物体
			mSpawnDistance *= (z > 0) ? 1.1f : (1.0f / 1.1f);
			if (mSpawnDistance < 0.5f) mSpawnDistance = 0.5f;
			if (mSpawnDistance > 500.0f) mSpawnDistance = 500.0f;
			Log3D(u8"[相机] 创建/落点距离 = %.2f\n", (double)mSpawnDistance);
			return;
		}

		mCameraSpeed *= (z > 0) ? 1.15f : (1.0f / 1.15f);
		if (mCameraSpeed < 0.5f) mCameraSpeed = 0.5f;
		if (mCameraSpeed > 2000.0f) mCameraSpeed = 2000.0f;
		Log3D(u8"[相机] 飞行速度 = %.1f 单位/秒\n", (double)mCameraSpeed);
	}

	void PhysicsTest3D::MouseButton(MouseBtn button, InputState State)
	{
		switch (button)
		{
		case MouseBtn::Left:
			if (State == InputState::Down || State == InputState::Hold)
			{
				mLeftMouseDown = true;
			}
			else
			{
				mLeftMouseDown = false;
				Dragging = false;
				DragObject = nullptr;
			}
			break;
		case MouseBtn::Right:
			if (State == InputState::Down || State == InputState::Hold)
			{
				mRightMouseDown = true;
			}
			else
			{
				mRightMouseDown = false;
				RightDragging = false;
			}
			break;
		default:
			break;
		}
	}

	void PhysicsTest3D::KeyDown(GameKeyEnum moveDirection)
	{
		switch (moveDirection)
		{
		// ── WASD + QE 自由飞行（取代 2D 的平面平移）──────────────────
		case GameKeyEnum::MOVE_LEFT:
			MoveCameraLocal(glm::vec3(-1.0f, 0.0f, 0.0f));
			break;
		case GameKeyEnum::MOVE_RIGHT:
			MoveCameraLocal(glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case GameKeyEnum::MOVE_FRONT:
			MoveCameraLocal(glm::vec3(0.0f, 0.0f, 1.0f));
			break;
		case GameKeyEnum::MOVE_BACK:
			MoveCameraLocal(glm::vec3(0.0f, 0.0f, -1.0f));
			break;
		case GameKeyEnum::SPACE:
			// 上升（3D 新增：2D 没有上下轴）
			MoveCameraLocal(glm::vec3(0.0f, 1.0f, 0.0f));
			break;
		case GameKeyEnum::MOVE_DOWN:
			// 下降（GameKeyEnum 没有 SHIFT，用 MOVE_DOWN 承担）
			MoveCameraLocal(glm::vec3(0.0f, -1.0f, 0.0f));
			break;

		case GameKeyEnum::ESC:
			// 与 2D 完全一致
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
			// 3D 新增：快捷切换 demo（1/2 前后翻）
			SwitchDemo3D((mCurrentDemo + PhysicsBlock3D::DemoCount3D - 1) % PhysicsBlock3D::DemoCount3D);
			break;
		case GameKeyEnum::Key_2:
			SwitchDemo3D((mCurrentDemo + 1) % PhysicsBlock3D::DemoCount3D);
			break;

		case GameKeyEnum::MOVE_UP:
			// 3D 新增：删除当前选中物体。
			// 说明：GameKeyEnum 没有 DELETE / R（键位表由游戏层统一定义，
			// 这里无权扩展），借用语义闲置的 MOVE_UP 承担"移除"；
			// 上升已并入 SPACE，下降并入 MOVE_DOWN。右键菜单里同样有删除项。
			if (PhysicsFormworkPtr != nullptr && mPhysicsWorld != nullptr)
			{
				if (PhysicsFormworkPtr == mLastCreated) mLastCreated = nullptr;
				if (PhysicsFormworkPtr == GridEditShape) GridEditShape = nullptr;
				if (PhysicsFormworkPtr == DragObject) DragObject = nullptr;
				if (PhysicsFormworkPtr == mPickObject) mPickObject = nullptr;
				if (PhysicsFormworkPtr == RightClickObjectPtr) RightClickObjectPtr = nullptr;
				Log3D(u8"[编辑] 删除选中物体（类型 %d）\n", (int)PhysicsFormworkPtr->PFGetType());
				mPhysicsWorld->RemoveObject(PhysicsFormworkPtr);
				PhysicsFormworkPtr = nullptr;
			}
			break;

		default:
			break;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 渲染 / 物理循环
	// ═══════════════════════════════════════════════════════════════════════

	void PhysicsTest3D::GameCommandBuffers(unsigned int Format_i)
	{
		// 与 2D 同构
		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		// 「面」渲染（实心三角形）也挂在同一个线程指令缓冲里
		if (mFaceRenderer != nullptr)
			mFaceRenderer->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void PhysicsTest3D::GameRecordCommandBuffers()
	{
		// 与 2D 同构
		mAuxiliaryVision->initCommandBuffer();
		// 面渲染的二级指令缓冲。
		// 这里**必须**每帧重录（force=true）：引擎里其它渲染器也是每帧重录
		// （见 VulkanTool/AuxiliaryVision.cpp::initCommandBuffer），
		// 若只在"顶点数变化"时录一次，指令缓冲会在后续帧变成未录制状态，
		// 校验层会报 "is unrecorded and contains no commands" 且画不出任何东西。
		if (mFaceRenderer != nullptr)
			mFaceRenderer->RecordCommandBuffers(true);
	}

	void PhysicsTest3D::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
		// 2D 版这里也只是读了一下 InterFace 状态、没有实际动作（多人模式的
		// 占位）。3D 沙盒明确不支持多人，故保持与 2D 相同的"无操作"语义，
		// 但把未使用参数显式忽略，避免 -Wunused-parameter。
		(void)mCurrentFrame;
	}

	void PhysicsTest3D::GameTCPLoop()
	{
		// 2D 版为空实现；3D 沙盒同样不做网络同步（GameMods.h 里
		// PhysicsTest3D_ 标记为不支持多人）。
	}

	void PhysicsTest3D::GameLoop(unsigned int mCurrentFrame)
	{
		(void)mCurrentFrame;

		// ── 0. 窗口逻辑尺寸兜底（照 2D）─────────────────────────────
		int winwidth = mWinWidth, winheight = mWinHeight;
		if (winwidth == 0 || winheight == 0)
		{
#if defined(__ANDROID__)
			winwidth = mWindow->getWidth();
			winheight = mWindow->getHeight();
#else
			winwidth = Global::mWidth;
			winheight = Global::mHeight;
#endif
		}

		// ── 1. 3D 射线拾取（取代 2D 的"打到 z=0 平面"）──────────────
		mPickHitValid = PickWorldPoint3D(mPickHit, mPickObject);
		mSpawnPoint = mPickHit;

		// ── 2. 动态地图滚动 ──────────────────────────────────────────
		// 2D：MapDynamic::Updata({camX, camY}) 返回 bool；
		// 3D：MapDynamic3D::Updata(Vec3_) 返回 MovePlateInfo3D{.UpData, .X, .Y, .Z}
		if (mMapFormwork != nullptr &&
		    mMapFormwork->FMGetType() == PhysicsBlock3D::PhysicsObjectEnum::_MapDynamic)
		{
			auto *dyn = dynamic_cast<PhysicsBlock3D::MapDynamic3D *>(mMapFormwork);
			if (dyn != nullptr)
			{
				const PhysicsBlock3D::MovePlateInfo3D plate = dyn->Updata(mCamera->getCameraPos());
				if (plate.UpData)
				{
					// 地图平移了：轮廓静态缓冲必须重写
					RefreshMapOutline();
				}
			}
		}

		// ── 3. 绘制物理世界 ──────────────────────────────────────────
		// 3.0 「面」渲染重建：在 Begin() 之前按当前渲染方式生成面几何并上传，
		//     这样线框与实心面用的是同一份物理状态（不会差一帧）。
		if (mFaceRenderer != nullptr)
			mFaceRenderer->Update(mPhysicsWorld, mRenderModes);

		// 注意：`DrawPhysicsWorld3D` 是"录进 av 的**动态**缓冲"，必须放在
		// Begin() 与 End() 之间（见 PhysicsAuxiliaryVision3D.hpp 顶部注释）。
		mAuxiliaryVision->Begin();

		{
			// 3.1 拖拽反馈：从抓取点到光标的黄绿连线（对应 2D 的
			//     `mAuxiliaryVision->Line({grabPos,0},{1,0,0,1},{e,0},{0,1,0,1})`）
			if (DragObject != nullptr && Dragging && mLeftMouseDown)
			{
				const glm::vec3 grip = (glm::vec3)DragObject->PFGetPos();
				mAuxiliaryVision->Line(grip, {1.0f, 0.0f, 0.0f, 1.0f},
				                       mSpawnPoint, {0.0f, 1.0f, 0.0f, 1.0f});
			}

			// 3.2 体素编辑的三维提示（当前 Z 层框 + 笔刷落点框）
			if (GridEditShape != nullptr)
			{
				RenderGridEditPreview3D();
			}

			// 3.3 整帧物理世界（物体本体 + 辅助信息 + 关节 + 碰撞 + 触发器）
			//     走「面」渲染的物体/地形会跳过线框本体（见 mRenderModes）
			PhysicsBlock3D::DrawPhysicsWorld3D(mAuxiliaryVision, mPhysicsWorld,
			                                   PhysicsAssistantInformation, PhysicsFormworkPtr,
			                                   &mRenderModes);

			// 3.4 选中物体的白色包围盒提示（3D 新增；2D 只有外骨骼点）
			if (PhysicsFormworkPtr != nullptr)
			{
				glm::vec3 corners[8];
				const glm::vec3 half(0.25f);
				MakeBoxCorners3D((glm::vec3)PhysicsFormworkPtr->PFGetPos(), half, corners);
				for (int e = 0; e < 12; ++e)
				{
					mAuxiliaryVision->Line(corners[kBoxEdges3D[e][0]], {1.0f, 1.0f, 1.0f, 0.7f},
					                       corners[kBoxEdges3D[e][1]], {1.0f, 1.0f, 1.0f, 0.7f});
				}
			}
		}

		mAuxiliaryVision->End();

		// 3.3 「面」渲染的二级指令缓冲：**每帧在这里重录**。
		// 注意：`GameRecordCommandBuffers()` 只在交换链重建时被调用一次
		// （application.cpp:696 在 ReconfigurationPipelineS 之后），不是每帧钩子；
		// 引擎里真正的"每帧重录"惯例在渲染器自己的更新函数里
		// （见 VulkanTool/AuxiliaryVision.cpp::End -> initCommandBuffer）。
		// 放在这里正好在 Update() 填好顶点缓冲之后、下一帧提交之前。
		if (mFaceRenderer != nullptr)
			mFaceRenderer->RecordCommandBuffers(true);

		// ── 4. 鼠标事件 / 编辑 ───────────────────────────────────────
		EditorMode(mPickHit, mPickHitValid);

		// ── 5. 物理步进（保持 2D 的 1/100 秒累加器写法）──────────────
		if (PhysicsSwitch)
		{
			PhysicsAccumulator += TOOL::FPStime;
			if (PhysicsAccumulator > PhysicsTick)
			{
				PhysicsAccumulator -= PhysicsTick;

				// 3D 没有 PhysicsGPU：ApplyImpulse 只有 CPU 路径。
				// 2D 那个 `if (mUseGPUApplyImpulse && mGPUInitialized)` 分支
				// 在这里被明确删除（BaseDefine3D.h 排除了 PhysicsGPU）。
				mPhysicsWorld->PhysicsEmulator((FLOAT_)PhysicsTick);

				// 与 2D 相同的"追不上就丢弃、避免死亡螺旋"保护
				if (PhysicsAccumulator > 0.02)
				{
					PhysicsAccumulator = 0.01;
				}
			}
		}
		else
		{
			mPhysicsWorld->PhysicsInformationUpdate();
		}

		LogCPUTimeProfile();

		// ── 6. 更新相机变换矩阵 ──────────────────────────────────────
		UpdateCamera();
	}

	// ═══════════════════════════════════════════════════════════════════════
	// Demo 切换 / 地图轮廓
	// ═══════════════════════════════════════════════════════════════════════

	void PhysicsTest3D::RefreshMapOutline()
	{
		if (mAuxiliaryVision == nullptr || mPhysicsWorld == nullptr)
		{
			return;
		}
		// 必须在 Begin()/End() 之外调用（静态缓冲，见 av 头文件说明）
		mAuxiliaryVision->ClearStaticLine();

		// 地形内容变了：面渲染缓存的那份几何要标记为脏
		if (mFaceRenderer != nullptr)
			mFaceRenderer->InvalidateTerrain();

		// 地形走「面」渲染时**不能**再写线框轮廓，否则线框会叠在实心地形上
		if (ShowMapOutline && mRenderModes.TerrainMode() == PhysicsBlock3D::RenderMode3D::Line)
		{
			PhysicsBlock3D::RenderMapOutline3D(mAuxiliaryVision, mPhysicsWorld);
		}
	}

	void PhysicsTest3D::SwitchDemo3D(int index)
	{
		if (PhysicsBlock3D::DemoCount3D <= 0)
		{
			return;
		}
		if (index < 0) index = 0;
		if (index >= PhysicsBlock3D::DemoCount3D) index = PhysicsBlock3D::DemoCount3D - 1;
		mCurrentDemo = index;

		// 切换前把本类持有的物体指针全部失效化 —— Demo 函数会 `delete` 整个
		// 旧世界，这些指针会立刻悬垂。
		PhysicsFormworkPtr = nullptr;
		GridEditShape = nullptr;
		DragObject = nullptr;
		mLastCreated = nullptr;
		mPickObject = nullptr;
		RightClickObjectPtr = nullptr;
		Dragging = false;
		RightDragging = false;
		mDynamicMap3D = nullptr;

		// 整个世界的物体都换了：单物体渲染方式覆盖表里的指针全部失效，必须清空
		mRenderModes.ClearAllObjectModes();

		// Demo 函数自己负责 delete 旧世界 + new 新世界 + 摆相机
		PhysicsBlock3D::DemoFunS3D[index](&mPhysicsWorld, mCamera);

		mMapFormwork = mPhysicsWorld->GetMapFormwork();
		mDynamicMap3D = dynamic_cast<PhysicsBlock3D::MapDynamic3D *>(mMapFormwork);

		// 从 Demo 摆好的视图反推 yaw/pitch，使后续鼠标旋转从正确姿态开始
		SyncCameraAnglesFromViewLocal();
		mCameraPos = mCamera->getCameraPos();

		// 重建地图轮廓静态缓冲
		RefreshMapOutline();

		PhysicsAccumulator = 0.0;

		Log3D(u8"[Demo] 切换到 #%d %s\n", mCurrentDemo, PhysicsBlock3D::DemoNameS3D[mCurrentDemo]);
	}

	// ═══════════════════════════════════════════════════════════════════════
	// ImGui 界面
	// ═══════════════════════════════════════════════════════════════════════

	void PhysicsTest3D::GameUI()
	{
		// ── 主控制面板（与 2D 相同的"一个窗口 + 三个标签页"结构）──────
		ImGui::Begin(u8"物理模拟控制面板 3D");
		const ImVec2 window_pos = ImGui::GetWindowPos();
		const ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
		    ((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY)))
		{
			// 与 BlockWorld/2D 相同的写法：光标压在面板上就置位，
			// GameLoop 的 EditorMode 与事件层据此屏蔽场景操作。
			Global::ClickWindow = true;
		}
		// 面板要给「渲染方式」这一节留出位置，所以比原来(620)更高；内容超出仍可滚动
		ImGui::SetWindowSize(ImVec2(430, 820), ImGuiCond_FirstUseEver);
		ImGui::SetWindowPos(ImVec2((float)Global::mWidth - 430.0f, 0.0f), ImGuiCond_FirstUseEver);

		if (ImGui::BeginTabBar("MainTabBar3D"))
		{
			// ══════════ 标签页 1: 编辑 ══════════
			if (ImGui::BeginTabItem(u8"编辑"))
			{
				if (ImGui::Button(EditorModeBool ? u8"关闭编辑" : u8"开启编辑"))
				{
					EditorModeBool = !EditorModeBool;
					if (EditorModeBool && GridEditShape != nullptr)
					{
						// 编辑模式与体素编辑互斥
						ExitGridEdit();
					}
					Log3D(u8"[编辑] 刚体编辑模式 = %s\n", EditorModeBool ? "ON" : "OFF");
				}
				ImGui::SameLine();
				ImGui::Checkbox(u8"物理运行", &PhysicsSwitch);

				ImGui::SeparatorText(u8"创建物体");
				ImGui::Combo(u8"类型", &item_object, objectName, IM_ARRAYSIZE(objectName));
				ImGui::TextDisabled(u8"在当前拾取点创建（左键单击）");

				switch (item_object)
				{
				case 0: // 球
					ImGui::SliderFloat(u8"半径", &CreateRadius, 0.1f, 8.0f, "%.2f");
					break;
				case 1: // 点
					ImGui::TextDisabled(u8"点质量由质量滑块决定，半径为 0.25");
					break;
				case 2: // 网格形状
					ImGui::SliderInt(u8"宽度 X", &CreateShapeSize.x, 1, 12);
					ImGui::SliderInt(u8"高度 Y", &CreateShapeSize.y, 1, 12);
					ImGui::SliderInt(u8"深度 Z", &CreateShapeSize.z, 1, 12);
					break;
				case 3: // 线段
					ImGui::SliderFloat(u8"半长度", &CreateLineHalfLength, 0.5f, 12.0f, "%.2f");
					ImGui::SliderFloat(u8"厚度", &CreateLineThickness, 0.05f, 1.0f, "%.3f");
					break;
				default:
					break;
				}
				ImGui::SliderFloat(u8"质量", &CreateMass, 0.1f, 100.0f, "%.2f");
				ImGui::SliderFloat(u8"摩擦", &CreateFriction, 0.0f, 2.0f, "%.3f");

				ImGui::SeparatorText(u8"拾取 / 相机");
				ImGui::Text(u8"拾取点: (%.1f, %.1f, %.1f)%s",
				            (double)mPickHit.x, (double)mPickHit.y, (double)mPickHit.z,
				            mPickHitValid ? u8"  [命中]" : u8"  [地面回落]");
				ImGui::Text(u8"相机: (%.1f, %.1f, %.1f)", (double)mCameraPos.x,
				            (double)mCameraPos.y, (double)mCameraPos.z);
				ImGui::Text(u8"yaw=%.1f  pitch=%.1f  速度=%.1f", (double)mYaw, (double)mPitch,
				            (double)mCameraSpeed);
				ImGui::SliderFloat(u8"鼠标灵敏度", &mCameraSensitivity, 0.02f, 1.0f, "%.3f");
				ImGui::SliderFloat(u8"飞行速度", &mCameraSpeed, 0.5f, 300.0f, "%.1f");
				ImGui::SliderFloat(u8"落点距离", &mSpawnDistance, 0.5f, 200.0f, "%.1f");

				ImGui::SeparatorText(u8"辅助视觉");
				if (ImGui::Button(PhysicsAssistantInformation ? u8"关闭辅助视觉" : u8"开启辅助视觉"))
				{
					PhysicsAssistantInformation = !PhysicsAssistantInformation;
				}
				if (PhysicsAssistantInformation)
				{
					ImGui::Spacing();
					// 3D 版函数名在 namespace PhysicsBlock3D 内，**必须全限定**
					PhysicsBlock3D::PhysicsAuxiliaryColorUI();
				}
				if (ImGui::Checkbox(u8"显示地图轮廓", &ShowMapOutline))
				{
					RefreshMapOutline();
				}

				// ────────── 渲染方式（线 / 面）──────────
				// 线 = 现在的线框轮廓；面 = BlockWorld 同款实心三角形。
				// 优先级：单物体覆盖 > 地形单独 > 全局默认。
				ImGui::SeparatorText(u8"渲染方式（线 / 面）");

				int globalMode = (int)mRenderModes.global;
				if (ImGui::RadioButton(u8"全局：线框", &globalMode, (int)PhysicsBlock3D::RenderMode3D::Line))
				{
					mRenderModes.global = PhysicsBlock3D::RenderMode3D::Line;
					RefreshMapOutline(); // 地形可能要从面切回线框
				}
				ImGui::SameLine();
				if (ImGui::RadioButton(u8"全局：实心面", &globalMode, (int)PhysicsBlock3D::RenderMode3D::Face))
				{
					mRenderModes.global = PhysicsBlock3D::RenderMode3D::Face;
					RefreshMapOutline(); // 地形可能要从线框切成实心面
				}

				// 地形单独设置（当前场景没有地图样板时禁用这一组控件）
				const bool hasMapFormwork =
				    mPhysicsWorld != nullptr && mPhysicsWorld->GetMapFormwork() != nullptr;
				if (!hasMapFormwork)
					ImGui::BeginDisabled();
				bool terrainOverride = mRenderModes.terrainOverride;
				if (ImGui::Checkbox(u8"地形单独设置（不勾=跟随全局）", &terrainOverride))
				{
					// 勾选/取消时保持当前实际显示方式不变，避免视觉突变
					const PhysicsBlock3D::RenderMode3D currentTerrainMode = mRenderModes.TerrainMode();
					mRenderModes.terrainOverride = terrainOverride;
					mRenderModes.terrain = terrainOverride ? currentTerrainMode : mRenderModes.global;
					RefreshMapOutline();
				}
				if (mRenderModes.terrainOverride)
				{
					int terrainMode = (int)mRenderModes.terrain;
					if (ImGui::RadioButton(u8"地形：线框", &terrainMode, (int)PhysicsBlock3D::RenderMode3D::Line))
					{
						mRenderModes.terrain = PhysicsBlock3D::RenderMode3D::Line;
						RefreshMapOutline();
					}
					ImGui::SameLine();
					if (ImGui::RadioButton(u8"地形：实心面", &terrainMode, (int)PhysicsBlock3D::RenderMode3D::Face))
					{
						mRenderModes.terrain = PhysicsBlock3D::RenderMode3D::Face;
						RefreshMapOutline();
					}
				}
				if (!hasMapFormwork)
				{
					ImGui::EndDisabled();
					ImGui::TextDisabled(u8"当前场景没有地形（地图样板）");
				}

				// 选中物体的单独设置
				if (PhysicsFormworkPtr == nullptr)
				{
					ImGui::TextDisabled(u8"未选中物体：右键物体可单独设置渲染方式");
				}
				else
				{
					ImGui::Text(u8"选中物体类型: %d", (int)PhysicsFormworkPtr->PFGetType());
					const bool faceSupported =
					    PhysicsBlock3D::RenderModeState3D::SupportsFace(PhysicsFormworkPtr);
					if (!faceSupported)
					{
						ImGui::TextDisabled(u8"点/线没有体面，恒为线框");
						ImGui::BeginDisabled();
					}
					bool objectOverride = mRenderModes.HasObjectOverride(PhysicsFormworkPtr);
					if (ImGui::Checkbox(u8"该物体单独设置", &objectOverride))
					{
						if (objectOverride)
							mRenderModes.SetObjectMode(PhysicsFormworkPtr,
							                           mRenderModes.ObjectMode(PhysicsFormworkPtr));
						else
							mRenderModes.ClearObjectMode(PhysicsFormworkPtr);
					}
					if (objectOverride)
					{
						int objectMode = (int)mRenderModes.ObjectMode(PhysicsFormworkPtr);
						if (ImGui::RadioButton(u8"物体：线框", &objectMode, (int)PhysicsBlock3D::RenderMode3D::Line))
						{
							mRenderModes.SetObjectMode(PhysicsFormworkPtr, PhysicsBlock3D::RenderMode3D::Line);
						}
						ImGui::SameLine();
						if (ImGui::RadioButton(u8"物体：实心面", &objectMode, (int)PhysicsBlock3D::RenderMode3D::Face))
						{
							mRenderModes.SetObjectMode(PhysicsFormworkPtr, PhysicsBlock3D::RenderMode3D::Face);
						}
					}
					if (!faceSupported)
						ImGui::EndDisabled();
				}

				if (mFaceRenderer != nullptr)
					ImGui::TextDisabled(u8"面顶点数: %u", mFaceRenderer->GetVertexCount());
				else
					ImGui::TextDisabled(u8"面渲染器不可用（本次运行只有线框）");

				ImGui::SeparatorText(u8"体素编辑（3D 简化版）");
				if (GridEditShape == nullptr)
				{
					ImGui::TextDisabled(u8"流程：先左键点选一个网格形状，再点下面的按钮");
					if (ImGui::Button(u8"进入体素编辑") && PhysicsFormworkPtr != nullptr &&
					    PhysicsFormworkPtr->PFGetType() == PhysicsBlock3D::PhysicsObjectEnum::shape)
					{
						EnterGridEdit((PhysicsBlock3D::PhysicsShape3D *)PhysicsFormworkPtr);
					}
				}
				else
				{
					ImGui::Text(u8"正在编辑网格: 尺寸 %ux%ux%u", GridEditShape->width,
					            GridEditShape->height, GridEditShape->depth);
					if (ImGui::Button(u8"退出体素编辑"))
					{
						ExitGridEdit();
					}
				}
				ImGui::TextDisabled(u8"简化说明见 PhysicsTest3D.cpp 的 EnterGridEdit 注释");

				ImGui::EndTabItem();
			}

			// ══════════ 标签页 2: 属性 ══════════
			if (ImGui::BeginTabItem(u8"属性"))
			{
				// ── Demo 选择（照 2D 的 (item_Demo_idx, item_current) 双变量模式）
				ImGui::Combo(u8"示例场景", &mCurrentDemo, PhysicsBlock3D::DemoNameS3D,
				             PhysicsBlock3D::DemoCount3D);
				// 上面的 Combo 一旦改动 mCurrentDemo 就需要真正重建场景；
				// 用一个"已应用索引"来检测变化，等价于 2D 的 item_current。
				static int appliedDemo3D = 0;
				if (appliedDemo3D != mCurrentDemo)
				{
					const int wanted = mCurrentDemo;
					SwitchDemo3D(wanted);
					appliedDemo3D = wanted;
				}
				ImGui::SameLine();
				if (ImGui::Button(u8"重置场景"))
				{
					SwitchDemo3D(mCurrentDemo);
				}
				ImGui::TextDisabled(u8"快捷键：1/2 切换上下一个场景，R 重置，Delete 删除选中");

				// ── 世界参数（等价于 2D 的 PhysicsUI(PhysicsWorld*)）────────
				ImGui::Spacing();
				ImGui::SeparatorText(u8"世界参数");
				ImGui::InputFloat3(u8"重力", &mGravity.x, "%.2f");
				if (ImGui::Button(u8"应用重力"))
				{
					mPhysicsWorld->GravityAcceleration = mGravity;
					Log3D(u8"[世界] 重力 = (%.2f, %.2f, %.2f)\n", (double)mGravity.x,
					      (double)mGravity.y, (double)mGravity.z);
				}
				ImGui::SameLine();
				if (ImGui::Button(u8"读回重力"))
				{
					mGravity = mPhysicsWorld->GravityAcceleration;
				}
				// WindBool 是 const（构造后不可变），只能显示不能改
				ImGui::Text(u8"风场: %s", mPhysicsWorld->WindBool ? u8"启用（构造时固定）" : u8"未启用");
				if (mPhysicsWorld->WindBool)
				{
					ImGui::InputFloat3(u8"风向量", &mPhysicsWorld->Wind.x, "%.2f");
				}
				ImGui::Text(u8"迭代次数: %u（物体 %u 个时自适应）",
				            mPhysicsWorld->ApplyImpulseSize, mPhysicsWorld->ObjectSize);

				// ── 性能统计（只剩 CPU；GPU 相关 UI 已按要求全部删除）────
				ImGui::Spacing();
				ImGui::SeparatorText(u8"性能统计（CPU）");
				const ImVec4 yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
				const ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
				ImGui::TextColored(yellow, u8"碰撞检测  耗时: %.3f ms", mPhysicsWorld->GetCollisionDetectionTimeMS());
				ImGui::TextColored(yellow, u8"预处理    耗时: %.3f ms", mPhysicsWorld->GetPreStepTimeMS());
				ImGui::TextColored(yellow, u8"冲量求解  耗时: %.3f ms", mPhysicsWorld->GetApplyImpulseCPUTimeMS());
				ImGui::TextColored(yellow, u8"位置更新  耗时: %.3f ms", mPhysicsWorld->GetPositionUpdateTimeMS());
				ImGui::TextColored(yellow, u8"后处理    耗时: %.3f ms", mPhysicsWorld->GetPostProcessTimeMS());
				ImGui::TextColored(green, u8"CPU 合计  耗时: %.3f ms（3D 无 GPU 路径）", mCPUTimeMS);

				// ── 场景统计 ────────────────────────────────────────────
				ImGui::Spacing();
				ImGui::SeparatorText(u8"场景统计");
				ImGui::Text(u8"物体总数: %d",
				            (int)(mPhysicsWorld->PhysicsShapeS.size() +
				                  mPhysicsWorld->PhysicsCircleS.size() +
				                  mPhysicsWorld->PhysicsParticleS.size() +
				                  mPhysicsWorld->PhysicsLineS.size()));
				ImGui::Text(u8"  网格 %d / 球 %d / 点 %d / 线段 %d",
				            (int)mPhysicsWorld->PhysicsShapeS.size(),
				            (int)mPhysicsWorld->PhysicsCircleS.size(),
				            (int)mPhysicsWorld->PhysicsParticleS.size(),
				            (int)mPhysicsWorld->PhysicsLineS.size());
				ImGui::Text(u8"碰撞对: %d    关节: %d    连接点: %d",
				            (int)mPhysicsWorld->CollideGroupVector.size(),
				            (int)mPhysicsWorld->PhysicsJointS.size(),
				            (int)mPhysicsWorld->BaseJunctionS.size());
				ImGui::Text(u8"世界总动能: %.6f", mPhysicsWorld->GetWorldEnergy());

				// ── 液体（3D 无液体渲染，只显示统计；见文件头差异说明 5）──
				if (mPhysicsWorld->GetLiquid() != nullptr)
				{
					ImGui::Text(u8"液体粒子: %d   密度参数: %.2f",
					            (int)mPhysicsWorld->GetLiquid()->Particles().size(),
					            (double)mPhysicsWorld->GetLiquid()->param.restDensity);
					ImGui::TextDisabled(u8"（渲染层不画液体，只统计）");
				}

				// ── 选中对象属性 ────────────────────────────────────────
				ImGui::Spacing();
				ImGui::SeparatorText(u8"选中对象属性");
				if (PhysicsFormworkPtr != nullptr)
				{
					const char *typeName = u8"未知";
					switch (PhysicsFormworkPtr->PFGetType())
					{
					case PhysicsBlock3D::PhysicsObjectEnum::shape: typeName = u8"网格形状"; break;
					case PhysicsBlock3D::PhysicsObjectEnum::particle: typeName = u8"粒子"; break;
					case PhysicsBlock3D::PhysicsObjectEnum::circle: typeName = u8"圆球"; break;
					case PhysicsBlock3D::PhysicsObjectEnum::line: typeName = u8"线段"; break;
					default: break;
					}
					ImGui::Text(u8"类型: %s", typeName);
					ImGui::Spacing();

					bool changed = false;
					switch (PhysicsFormworkPtr->PFGetType())
					{
					case PhysicsBlock3D::PhysicsObjectEnum::shape:
						changed = PhysicsBlock3D::PhysicsUI((PhysicsBlock3D::PhysicsShape3D *)PhysicsFormworkPtr);
						break;
					case PhysicsBlock3D::PhysicsObjectEnum::particle:
						changed = PhysicsBlock3D::PhysicsUI((PhysicsBlock3D::PhysicsParticle3D *)PhysicsFormworkPtr);
						break;
					case PhysicsBlock3D::PhysicsObjectEnum::circle:
						changed = PhysicsBlock3D::PhysicsUI((PhysicsBlock3D::PhysicsCircle3D *)PhysicsFormworkPtr);
						break;
					case PhysicsBlock3D::PhysicsObjectEnum::line:
						changed = PhysicsBlock3D::PhysicsUI((PhysicsBlock3D::PhysicsLine3D *)PhysicsFormworkPtr);
						break;
					default:
						// 也可用基类重载直接查看公共字段（位置/速度/旋转）
						changed = PhysicsBlock3D::PhysicsUI((PhysicsBlock3D::PhysicsAngle3D *)PhysicsFormworkPtr);
						break;
					}

					// 修改了尺寸/质量等内容后需要重建网格、质心、转动惯量、
					// 外轮廓与碰撞体缓存。
					if (changed)
					{
						auto *shp = dynamic_cast<PhysicsBlock3D::PhysicsShape3D *>(PhysicsFormworkPtr);
						if (shp != nullptr)
						{
							shp->UpdateAll();
						}
					}
				}
				else
				{
					ImGui::Text(u8"当前选中: 整个物理世界");
					if (PhysicsBlock3D::PhysicsUI(mPhysicsWorld))
					{
						// 世界的重力/风等改完后地图轮廓不变，但重画一次更稳
						RefreshMapOutline();
					}
				}

				ImGui::EndTabItem();
			}

			// ══════════ 标签页 3: 日志输出 ══════════
			if (ImGui::BeginTabItem(u8"日志输出"))
			{
				ImGui::Checkbox(u8"自动滚动", &AutoScrollLog);
				ImGui::SameLine();
				if (ImGui::Button(u8"清空日志"))
				{
					PhysicsBlock3D::PhysicsLogBuffer3D::GetInstance().Clear();
				}
				ImGui::SameLine();
				if (ImGui::Button(u8"记录 CPU 用时"))
				{
					LogCPUTimeProfile();
					Log3D(u8"[CPU] 合计 %.3f ms（碰撞 %.3f / 预处理 %.3f / 求解 %.3f / 位置 %.3f / 后处理 %.3f）\n",
					      (double)mCPUTimeMS,
					      (double)mPhysicsWorld->GetCollisionDetectionTimeMS(),
					      (double)mPhysicsWorld->GetPreStepTimeMS(),
					      (double)mPhysicsWorld->GetApplyImpulseCPUTimeMS(),
					      (double)mPhysicsWorld->GetPositionUpdateTimeMS(),
					      (double)mPhysicsWorld->GetPostProcessTimeMS());
				}
				ImGui::Separator();

				static ImGuiTextFilter logFilter;
				logFilter.Draw(u8"过滤");

				const float footerHeightToReserve =
				    ImGui::GetStyle().ItemSpacing.y + ImGui::GetStyle().ScrollbarSize;
				ImGui::BeginChild("LogScrollRegion3D", ImVec2(0, -footerHeightToReserve), false,
				                  ImGuiWindowFlags_HorizontalScrollbar);

				auto &logBuffer = PhysicsBlock3D::PhysicsLogBuffer3D::GetInstance();
				static std::vector<std::string> cachedLogs3D;
				static size_t lastLogCount3D = 0;

				if (logBuffer.GetLogCount() != lastLogCount3D)
				{
					cachedLogs3D.clear();
					for (size_t i = 0; i < logBuffer.GetLogCount(); ++i)
					{
						cachedLogs3D.push_back(logBuffer.GetLog(i));
					}
					lastLogCount3D = cachedLogs3D.size();
				}

				for (const auto &log : cachedLogs3D)
				{
					if (logFilter.PassFilter(log.c_str()))
					{
						ImGui::TextUnformatted(log.c_str());
					}
				}

				if (AutoScrollLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				{
					ImGui::SetScrollHereY(1.0f);
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();

		// 右键菜单与体素编辑面板（3D 没有 2D 的"单元格右键菜单"，
		// 笔刷参数直接列在体素编辑面板里）
		RenderRightClickMenu();
		RenderGridEditUI();
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 编辑 / 拖拽（2D EditorMode 系列的 3D 改写）
	// ═══════════════════════════════════════════════════════════════════════

	void PhysicsTest3D::EditorMode(glm::vec3 worldPos, bool haveRay)
	{
		// 静态状态与本文件内保存的"上一帧按键"一致：ImGui 遮挡时同步状态，
		// 避免遮挡解除后补触发一次。
		static bool sLastLeftDown = false;
		static bool sLastRightDown = false;
		static bool sStaleState = false;

		const bool leftDown = mLeftMouseDown;
		const bool rightDown = mRightMouseDown;

		if (Global::ClickWindow)
		{
			// 光标在 ImGui 面板上：只同步状态，不产生任何场景交互
			sStaleState = true;
			sLastLeftDown = false;
			sLastRightDown = false;
			return;
		}

		const bool leftPress = leftDown && !sLastLeftDown;
		const bool leftRelease = !leftDown && sLastLeftDown;
		const bool rightPress = rightDown && !sLastRightDown;

		if (sStaleState)
		{
			// 遮挡解除的第一帧：把"按下"作为新的一帧起点，不补事件
			sStaleState = false;
			sLastLeftDown = leftDown;
			sLastRightDown = rightDown;
			return;
		}

		// ── 体素编辑模式：笔刷涂抹 ─────────────────────────────────────
		if (GridEditShape != nullptr)
		{
			// 先算落点供预览用
			if (haveRay)
			{
				glm::ivec3 cell;
				if (WorldToShapeVoxel3D(GridEditShape, worldPos, cell))
				{
					GridEditHoverCell = cell;
					GridEditHoverValid = true;
				}
				else
				{
					GridEditHoverValid = false;
				}
			}
			else
			{
				GridEditHoverValid = false;
			}

			if (leftPress)
			{
				// 单击也要立即生效（2D 是"首次按下 + 松开各涂一次，中间拖动连续涂"）
				GridEditPaint(GridEditShape, worldPos, GridEditBrushType == GridEditBrush3D::Erase);
			}
			else if (leftDown && !leftPress)
			{
				// 按住拖动 = 连续涂抹
				GridEditPaint(GridEditShape, worldPos, GridEditBrushType == GridEditBrush3D::Erase);
			}

			if (rightPress)
			{
				// 右键按下 = 强制擦除（2D 是"弹单元格菜单"，3D 简化为快速擦除）
				GridEditPaint(GridEditShape, worldPos, GridEditBrushType != GridEditBrush3D::Erase);
			}

			sLastLeftDown = leftDown;
			sLastRightDown = rightDown;
			return;
		}

		// ── 普通模式 ───────────────────────────────────────────────────
		if (leftPress)
		{
			// 选中的对象在拾取时已经确定
			PhysicsFormworkPtr = mPickObject;
			Dragging = false;
			DragObject = nullptr;

			if (mPickObject == nullptr)
			{
				if (EditorModeBool)
				{
					SquareLeftEvent(nullptr, true, true, DragStart, worldPos);
				}
				else
				{
					// 相机左键旋转在 MouseMove 里按增量做，这里无需处理
				}
			}
			else
			{
				DragStart = worldPos;
				if (mPickObject->PFGetType() == PhysicsBlock3D::PhysicsObjectEnum::particle)
				{
					ParticleLeftEvent((PhysicsBlock3D::PhysicsParticle3D *)mPickObject, true, true,
					                  DragStart, worldPos);
				}
				else
				{
					SquareLeftEvent((PhysicsBlock3D::PhysicsAngle3D *)mPickObject, true, true,
					                DragStart, worldPos);
				}
				Log3D(u8"[选中] 物体类型 %d\n", (int)mPickObject->PFGetType());
			}
		}
		else if (leftDown && (DragObject != nullptr))
		{
			// 拖动中
			if (DragObject->PFGetType() == PhysicsBlock3D::PhysicsObjectEnum::particle)
			{
				ParticleLeftEvent((PhysicsBlock3D::PhysicsParticle3D *)DragObject, true, false,
				                  DragStart, worldPos);
			}
			else
			{
				SquareLeftEvent((PhysicsBlock3D::PhysicsAngle3D *)DragObject, true, false,
				                DragStart, worldPos);
			}
		}
		else if (leftRelease)
		{
			Dragging = false;
			DragObject = nullptr;
		}

		if (rightPress)
		{
			if (mPickObject != nullptr)
			{
				RightClickObjectPtr = mPickObject;
				ShowRightClickMenu = true;
				// 右键也进入拖拽旋转（与 2D 的 SquareRightEvent 不同：3D 的
				// 右键旋转在 MouseMove 里做，这里只负责弹菜单）
			}
		}

		sLastLeftDown = leftDown;
		sLastRightDown = rightDown;
	}

	void PhysicsTest3D::CameraLeftEvent(bool Click, bool First, glm::dvec2 s, glm::dvec2 e)
	{
		// 2D 用世界平面坐标做"抓取点 - 相机"的平移；3D 的相机旋转改在
		// MouseMove 里用鼠标增量直接改 yaw/pitch，故这里保留函数只为
		// 与 2D 的调用结构对应，并明确说明为什么是空的。
		(void)Click;
		(void)First;
		(void)s;
		(void)e;
	}

	void PhysicsTest3D::CameraRightEvent(bool Click, bool First, glm::dvec2 s, glm::dvec2 e)
	{
		// 2D 是空实现；3D 的右键旋转同样在 MouseMove 里按增量处理。
		(void)Click;
		(void)First;
		(void)s;
		(void)e;
	}

	void PhysicsTest3D::SquareLeftEvent(PhysicsBlock3D::PhysicsAngle3D *Ptr, bool Click, bool First,
	                                    glm::dvec3 s, glm::dvec3 e)
	{
		if (!Click)
		{
			return;
		}

		if (First)
		{
			// 首次按下：
			//  - 如果目标是"线段/胶囊"，2D 会把抓取点投影到线段轴上并夹紧到
			//    半长度内；3D 的 `PhysicsLine3D` 同样有半长度（`radius` 字段
			//    在 3D 被用作半长度，见 PhysicsLine3D.hpp），所以照搬这个逻辑。
			//  - 其他形状：记录"抓取点在物体局部坐标系下的位置"作为力臂。
			if (Ptr->PFGetType() == PhysicsBlock3D::PhysicsObjectEnum::line)
			{
				const glm::vec3 dir = glm::vec3(PhysicsBlock3D::QuatRotate(Ptr->rotation, glm::vec3(1, 0, 0)));
				const glm::vec3 v = glm::vec3(s) - Ptr->pos;
				FLOAT_ t = glm::dot(v, dir);
				FLOAT_ half = Ptr->radius;
				if (t > half) t = half;
				if (t < -half) t = -half;
				const glm::vec3 grabPoint = Ptr->pos + t * dir;
				DragLocalAnchor = glm::vec3(PhysicsBlock3D::QuatInverseRotate(
				    Ptr->rotation, grabPoint - Ptr->pos));
			}
			else
			{
				// Opos = 物体质心 - 抓取点（世界系偏移）；转到局部系后是常量，
				// 物体旋转时抓取点会跟着转（等价于 2D 的 PhysicsShapeArm）。
				const glm::vec3 armWorld = Ptr->pos - glm::vec3(s);
				DragLocalAnchor = glm::vec3(PhysicsBlock3D::QuatInverseRotate(Ptr->rotation, armWorld));
			}

			DragObject = Ptr;
			Dragging = true;
			return;
		}

		// 拖动中：用弹簧力把"抓取点"拉向光标（与 2D 同样的 k/c 与上限）
		const glm::vec3 armWorld =
		    glm::vec3(PhysicsBlock3D::QuatRotate(Ptr->rotation, DragLocalAnchor));
		const glm::vec3 grabPos = Ptr->pos + armWorld;
		const glm::vec3 displacement = glm::vec3(e) - grabPos;

		if (!PhysicsSwitch)
		{
			// 物理关闭时直接摆位置（2D 的 `Ptr->pos = Opos + e` 对应物）
			Ptr->pos = glm::vec3(e) + armWorld;
			Ptr->speed = glm::vec3(0.0f);
			return;
		}

		const FLOAT_ k = 300.0f;
		const FLOAT_ mass = (Ptr->PFGetMass() > 0.0f) ? Ptr->PFGetMass() : 1.0f;
		const FLOAT_ c = 2.0f * std::sqrt(k * mass);

		// 抓取点速度 = 线速度 + ω × r（3D 的 ω 是向量，直接叉积）
		const glm::vec3 grabVelocity = Ptr->PFSpeed() + glm::cross(Ptr->PFAngleSpeed(), armWorld);

		FLOAT_ nCalls = (FLOAT_)(PhysicsTick / ((TOOL::FPStime > 1e-6) ? TOOL::FPStime : 1e-6));
		if (nCalls < 1.0f) nCalls = 1.0f;

		glm::vec3 springForce = (k / nCalls) * displacement - (c / nCalls) * grabVelocity;

		// 拖拽力上限（照 2D：质量 × 500）
		const FLOAT_ maxForce = mass * 500.0f;
		const FLOAT_ forceMag = glm::length(springForce);
		if (forceMag > maxForce && forceMag > 0.0f)
		{
			springForce *= maxForce / forceMag;
		}

		Ptr->AddForce(grabPos, springForce);
	}

	void PhysicsTest3D::ParticleLeftEvent(PhysicsBlock3D::PhysicsParticle3D *Ptr, bool Click, bool First,
	                                      glm::dvec3 s, glm::dvec3 e)
	{
		if (!Click)
		{
			return;
		}

		if (First)
		{
			// 点没有旋转，力臂是纯世界偏移（照 2D 的 Opos）
			DragLocalAnchor = Ptr->pos - glm::vec3(s);
			DragObject = Ptr;
			Dragging = true;
			return;
		}

		if (!PhysicsSwitch)
		{
			Ptr->pos = glm::vec3(e) + glm::vec3(DragLocalAnchor);
			Ptr->speed = glm::vec3(0.0f);
			return;
		}

		// 点在拖动时力臂不随旋转变化，等价于把"抓取点"固定在质心
		const glm::vec3 displacement = glm::vec3(e) - Ptr->pos;

		const FLOAT_ k = 300.0f;
		const FLOAT_ mass = (Ptr->PFGetMass() > 0.0f) ? Ptr->PFGetMass() : 1.0f;
		const FLOAT_ c = 2.0f * std::sqrt(k * mass);

		FLOAT_ nCalls = (FLOAT_)(PhysicsTick / ((TOOL::FPStime > 1e-6) ? TOOL::FPStime : 1e-6));
		if (nCalls < 1.0f) nCalls = 1.0f;

		glm::vec3 springForce = (k / nCalls) * displacement - (c / nCalls) * Ptr->PFSpeed();

		const FLOAT_ maxForce = mass * 500.0f;
		const FLOAT_ forceMag = glm::length(springForce);
		if (forceMag > maxForce && forceMag > 0.0f)
		{
			springForce *= maxForce / forceMag;
		}

		Ptr->AddForce(springForce);
	}

	void PhysicsTest3D::FoundLeftEvent(bool Click, bool First, glm::dvec3 s, glm::dvec3 e)
	{
		(void)s;
		if (!Click || !First)
		{
			return;
		}

		// 创建位置：优先用真实命中的拾取点；如果只是地面回落点，
		// 也不要贴着地面创建（那会立刻穿透），抬高 0.5。
		glm::vec3 spawn = glm::vec3(e);
		if (!mPickHitValid)
		{
			spawn.y += 0.5f;
		}

		switch (item_object)
		{
		case 0: // 球
		{
			auto *obj = new PhysicsBlock3D::PhysicsCircle3D(spawn, CreateRadius, CreateMass);
			obj->friction = CreateFriction;
			mPhysicsWorld->AddObject(obj);
			mLastCreated = obj;
			Log3D(u8"[创建] 球 半径 %.2f 质量 %.2f @ (%.1f, %.1f, %.1f)\n",
			      (double)CreateRadius, (double)CreateMass,
			      (double)spawn.x, (double)spawn.y, (double)spawn.z);
			break;
		}
		case 1: // 点
		{
			auto *obj = new PhysicsBlock3D::PhysicsParticle3D(spawn, CreateMass, CreateFriction);
			mPhysicsWorld->AddObject(obj);
			mLastCreated = obj;
			Log3D(u8"[创建] 点 质量 %.2f @ (%.1f, %.1f, %.1f)\n",
			      (double)CreateMass, (double)spawn.x, (double)spawn.y, (double)spawn.z);
			break;
		}
		case 2: // 网格形状
		{
			// 拾取点是"表面"，把质心抬高半个高度，避免生成即嵌入地面/物体
			const glm::vec3 center = spawn + glm::vec3(0.0f, (float)CreateShapeSize.y * 0.5f + 0.5f, 0.0f);
			auto *obj = PhysicsBlock3D::MakeSolidShape3D(
			    center, CreateShapeSize.x, CreateShapeSize.y, CreateShapeSize.z,
			    CreateMass, CreateFriction);
			if (obj != nullptr)
			{
				mPhysicsWorld->AddObject(obj);
				mLastCreated = obj;
				Log3D(u8"[创建] 网格 %dx%dx%d 质量 %.2f @ (%.1f, %.1f, %.1f)\n",
				      CreateShapeSize.x, CreateShapeSize.y, CreateShapeSize.z,
				      (double)CreateMass, (double)center.x, (double)center.y, (double)center.z);
			}
			break;
		}
		case 3: // 线段（胶囊）
		{
			// PhysicsLine3D(begin, end, mass, friction, thickness)：
			// 以拾取点为中点、沿 X 轴摆一根半长 CreateLineHalfLength 的胶囊
			const Vec3_ half(CreateLineHalfLength, 0.0f, 0.0f);   // Vec3_ 是宏（= glm::vec3）
			auto *obj = new PhysicsBlock3D::PhysicsLine3D(spawn - half, spawn + half,
			                                              CreateMass, CreateFriction,
			                                              CreateLineThickness);
			mPhysicsWorld->AddObject(obj);
			mLastCreated = obj;
			Log3D(u8"[创建] 线段 半长 %.2f 厚度 %.3f 质量 %.2f @ (%.1f, %.1f, %.1f)\n",
			      (double)CreateLineHalfLength, (double)CreateLineThickness, (double)CreateMass,
			      (double)spawn.x, (double)spawn.y, (double)spawn.z);
			break;
		}
		default:
			break;
		}
	}

	void PhysicsTest3D::RenderRightClickMenu()
	{
		if (!ShowRightClickMenu)
		{
			return;
		}

		ImGui::SetNextWindowPos(ImVec2((float)CursorPosX, (float)CursorPosY), ImGuiCond_Appearing);
		if (ImGui::Begin(u8"物体操作", &ShowRightClickMenu, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (RightClickObjectPtr != nullptr)
			{
				ImGui::Text(u8"类型: %d", (int)RightClickObjectPtr->PFGetType());
				const glm::vec3 p = RightClickObjectPtr->PFGetPos();
				ImGui::Text(u8"位置: (%.2f, %.2f, %.2f)", (double)p.x, (double)p.y, (double)p.z);

				if (ImGui::MenuItem(u8"选中该物体"))
				{
					PhysicsFormworkPtr = RightClickObjectPtr;
					ShowRightClickMenu = false;
				}
				if (ImGui::MenuItem(u8"移到拾取点"))
				{
					RightClickObjectPtr->PFGetPos() = mPickHit;
					ShowRightClickMenu = false;
				}
				if (ImGui::MenuItem(u8"速度清零"))
				{
					RightClickObjectPtr->PFSpeed() = glm::vec3(0.0f);
					RightClickObjectPtr->PFAngleSpeed() = glm::vec3(0.0f);
					ShowRightClickMenu = false;
				}
				if (RightClickObjectPtr->PFGetType() == PhysicsBlock3D::PhysicsObjectEnum::shape &&
				    ImGui::MenuItem(u8"进入体素编辑"))
				{
					EnterGridEdit((PhysicsBlock3D::PhysicsShape3D *)RightClickObjectPtr);
					ShowRightClickMenu = false;
				}
				// ── 单独设置该物体的渲染方式（线 / 面）──
				// 只有体素网格与球有"体面"，点/线不提供该菜单（它们的 SupportsFace 为 false）
				if (PhysicsBlock3D::RenderModeState3D::SupportsFace(RightClickObjectPtr))
				{
					if (ImGui::BeginMenu(u8"渲染方式"))
					{
						const bool objectOverride = mRenderModes.HasObjectOverride(RightClickObjectPtr);
						const PhysicsBlock3D::RenderMode3D objectMode =
						    mRenderModes.ObjectMode(RightClickObjectPtr);

						if (ImGui::MenuItem(u8"跟随全局", nullptr, !objectOverride))
						{
							mRenderModes.ClearObjectMode(RightClickObjectPtr);
							ShowRightClickMenu = false;
						}
						if (ImGui::MenuItem(u8"线框", nullptr,
						                    objectOverride &&
						                        objectMode == PhysicsBlock3D::RenderMode3D::Line))
						{
							mRenderModes.SetObjectMode(RightClickObjectPtr,
							                           PhysicsBlock3D::RenderMode3D::Line);
							ShowRightClickMenu = false;
						}
						if (ImGui::MenuItem(u8"实心面", nullptr,
						                    objectOverride &&
						                        objectMode == PhysicsBlock3D::RenderMode3D::Face))
						{
							mRenderModes.SetObjectMode(RightClickObjectPtr,
							                           PhysicsBlock3D::RenderMode3D::Face);
							ShowRightClickMenu = false;
						}
						ImGui::EndMenu();
					}
				}
				if (ImGui::MenuItem(u8"删除该物体"))
				{
					if (RightClickObjectPtr == PhysicsFormworkPtr) PhysicsFormworkPtr = nullptr;
					if (RightClickObjectPtr == mLastCreated) mLastCreated = nullptr;
					if (RightClickObjectPtr == mPickObject) mPickObject = nullptr;
					// 单物体渲染方式覆盖表里存的是裸指针，删除前必须清掉
					mRenderModes.ClearObjectMode(RightClickObjectPtr);
					mPhysicsWorld->RemoveObject(RightClickObjectPtr);
					RightClickObjectPtr = nullptr;
					ShowRightClickMenu = false;
				}
			}
			else
			{
				ImGui::TextDisabled(u8"（未指向任何物体）");
				if (ImGui::MenuItem(u8"关闭"))
				{
					ShowRightClickMenu = false;
				}
			}
		}
		ImGui::End();
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 体素编辑（3D 简化版）
	// ═══════════════════════════════════════════════════════════════════════

	/*
	 * ─────────────────────────────────────────────────────────────────────
	 * 与 2D 版 `EnterGridEdit` / `RenderGridEditUI` / `GridEditPaint` 的差异
	 * ─────────────────────────────────────────────────────────────────────
	 * 2D 的编辑器是"把地图上的一个矩形区域抠出来，放进 GameMods 侧维护的
	 * `std::map<std::pair<int,int>, GridBlock>` 里独立编辑"：
	 *   · `GridEditData` 是一份**脱离世界**的副本，退出编辑时整体写回地图；
	 *   · 有 `GridEditMin/Max` 选区、`GridEditCellRightClick` 单元格菜单、
	 *     `GridEditSavedMap` 整块 map 快照用于回滚；
	 *   · 坐标只有 (x, y)，没有层。
	 *
	 * 3D 版**只保留最核心的"选中物体 → 在选定 Z 层上画笔切换体素"**，
	 * 原因有三：
	 *   1. 三维选区（AABB 框选 + 复制/粘贴/回滚）会让本类规模翻倍，
	 *      而 3D 引擎本身有 `MapStatic3D` 的体素接口，用户完全可以在 demo 层
	 *      直接构建地图；编辑器的价值主要在"随手试单个物体"。
	 *   2. 直接改**物体自身的体素表**（`shape->at(x,y,z)`）比维护一份外部
	 *      副本更不容易出错：不需要写回、不会与物理世界脱节，改完调一次
	 *      `UpdateAll()` 即可。
	 *   3. `ImGuiPhysics3D.hpp` 已经提供了单个体素的完整调试控件
	 *      `GridImGui3D(name, GridBlock3D*)`，质量/摩擦/血量/位标志都能改，
	 *      所以"精确编辑某一个体素"的能力没有损失，损失的是"批量选区"。
	 *
	 * 因此本文件的编辑器提供：
	 *   · 一个 Z 层滑块（滚轮可直接切层）；
	 *   · 画笔模式（绘制 / 擦除）与要写入的 Entity/Collision/Event 位；
	 *   · 是否同时写入质量/摩擦；
	 *   · 鼠标左键涂抹、右键反向涂抹（绘制↔擦除）；
	 *   · `GridImGui3D` 对"当前落点体素"做逐字段编辑；
	 *   · 三维预览：当前 Z 层用绿色线框标出，笔刷落点用黄色线框标出。
	 * 不提供：三维 AABB 框选、复制粘贴、整块快照回滚、单元格右键菜单、
	 * "从地图抠一块进编辑器"。
	 * ─────────────────────────────────────────────────────────────────────
	 */

	void PhysicsTest3D::EnterGridEdit(PhysicsBlock3D::PhysicsShape3D *shape)
	{
		if (shape == nullptr)
		{
			return;
		}
		GridEditShape = shape;
		GridEditLayer = (int)shape->depth / 2;
		GridEditHoverValid = false;
		GridEditSavedPhysicsSwitch = PhysicsSwitch;
		// 编辑网格时暂停物理，避免边改边抖动（2D 也隐含这个效果：
		// 编辑期不步进物理更符合直觉）
		PhysicsSwitch = false;
		PhysicsFormworkPtr = shape;

		Log3D(u8"[体素编辑] 进入：网格 %ux%ux%u，初始 Z 层 %d\n",
		      shape->width, shape->height, shape->depth, GridEditLayer);
	}

	void PhysicsTest3D::ExitGridEdit()
	{
		if (GridEditShape == nullptr)
		{
			return;
		}
		Log3D(u8"[体素编辑] 退出，重建物体信息\n");
		// 位标志/质量/摩擦改了以后必须重建：总质量、质心、几何中心、
		// 转动惯量张量、外轮廓点集、碰撞体缓存。
		GridEditShape->UpdateAll();
		GridEditShape = nullptr;
		GridEditHoverValid = false;
		PhysicsSwitch = GridEditSavedPhysicsSwitch;
	}

	void PhysicsTest3D::GridEditPaint(PhysicsBlock3D::PhysicsShape3D *shape, glm::vec3 worldPt, bool erase)
	{
		if (shape == nullptr)
		{
			return;
		}

		glm::ivec3 cell;
		if (!WorldToShapeVoxel3D(shape, worldPt, cell))
		{
			return;
		}

		// 强制落在当前选定的 Z 层上：画笔是"层面"操作。
		// （这是本简化版的核心约定：Z 由滑块决定，X/Y 由鼠标决定。）
		cell.z = GridEditLayer;
		GridEditHoverCell = cell;
		GridEditHoverValid = true;

		PhysicsBlock3D::GridBlock3D &voxel = shape->at(cell.x, cell.y, cell.z);

		if (erase || GridEditBrushType == GridEditBrush3D::Erase)
		{
			voxel.Entity = false;
			voxel.Collision = false;
			voxel.Event = false;
			voxel.mass = 0.0f;
		}
		else
		{
			PhysicsBlock3D::GridBlock3D brush;
			brush.Entity = GridEditEntity;
			brush.Collision = GridEditCollision;
			brush.Event = GridEditEvent;
			brush.FrictionFactor = GridEditFriction;
			brush.mass = GridEditWriteMass ? GridEditMass : voxel.mass;
			WriteGridBlock3D(&voxel, brush);
		}

		// 立刻重建几何/质量/轮廓，让下一次拾取就反映改动。
		// 体素规模（≤ 12³ = 1728）下这个代价可接受；不做的话外轮廓与
		// 碰撞体缓存会和体素表脱节，出现"看得见打不着"。
		shape->UpdateAll();
	}

	void PhysicsTest3D::RenderGridEditUI()
	{
		if (GridEditShape == nullptr)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(340, 420), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(u8"体素编辑 3D", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(u8"网格尺寸: %ux%ux%u", GridEditShape->width, GridEditShape->height,
			            GridEditShape->depth);
			ImGui::Text(u8"质心(局部): (%.2f, %.2f, %.2f)", (double)GridEditShape->CentreMass.x,
			            (double)GridEditShape->CentreMass.y, (double)GridEditShape->CentreMass.z);

			ImGui::SeparatorText(u8"Z 层");
			const int maxLayer = (int)GridEditShape->depth - 1;
			if (ImGui::SliderInt(u8"层", &GridEditLayer, 0, maxLayer))
			{
				GridEditHoverValid = false;
			}
			ImGui::TextDisabled(u8"滚轮可直接切层");

			ImGui::SeparatorText(u8"画笔");
			int brush = (GridEditBrushType == GridEditBrush3D::Draw) ? 0 : 1;
			if (ImGui::RadioButton(u8"绘制", &brush, 0)) GridEditBrushType = GridEditBrush3D::Draw;
			ImGui::SameLine();
			if (ImGui::RadioButton(u8"擦除", &brush, 1)) GridEditBrushType = GridEditBrush3D::Erase;

			ImGui::Checkbox(u8"实体 Entity", &GridEditEntity);
			ImGui::Checkbox(u8"碰撞 Collision", &GridEditCollision);
			ImGui::Checkbox(u8"事件 Event", &GridEditEvent);
			ImGui::Checkbox(u8"写入质量", &GridEditWriteMass);
			if (GridEditWriteMass)
			{
				ImGui::SliderFloat(u8"体素质量", &GridEditMass, 0.0f, 10.0f, "%.3f");
			}
			ImGui::SliderFloat(u8"体素摩擦", &GridEditFriction, 0.0f, 2.0f, "%.3f");
			ImGui::TextDisabled(u8"左键涂抹 / 右键反向 / 松开后自动重建");

			// ── 落点体素的逐字段调试（复用 ImGuiPhysics3D 的控件）────────
			ImGui::SeparatorText(u8"落点体素");
			if (GridEditHoverValid &&
			    GridEditShape->InBounds(GridEditHoverCell.x, GridEditHoverCell.y, GridEditHoverCell.z))
			{
				ImGui::Text(u8"坐标: (%d, %d, %d)", GridEditHoverCell.x, GridEditHoverCell.y,
				            GridEditHoverCell.z);
				PhysicsBlock3D::GridBlock3D &voxel =
				    GridEditShape->at(GridEditHoverCell.x, GridEditHoverCell.y, GridEditHoverCell.z);
				if (PhysicsBlock3D::GridImGui3D(u8"体素", &voxel))
				{
					GridEditShape->UpdateAll();
				}
			}
			else
			{
				ImGui::TextDisabled(u8"（光标未落在网格上）");
			}

			// ── 体素统计 ───────────────────────────────────────────────
			ImGui::SeparatorText(u8"统计");
			int solidCount = 0, collisionCount = 0;
			for (unsigned int x = 0; x < GridEditShape->width; ++x)
			{
				for (unsigned int y = 0; y < GridEditShape->height; ++y)
				{
					for (unsigned int z = 0; z < GridEditShape->depth; ++z)
					{
						const PhysicsBlock3D::GridBlock3D &v = GridEditShape->GridAt((int)x, (int)y, (int)z);
						if (v.Entity) ++solidCount;
						if (v.Collision) ++collisionCount;
					}
				}
			}
			ImGui::Text(u8"实体体素: %d    碰撞体素: %d", solidCount, collisionCount);
			ImGui::Text(u8"总质量: %.3f", (double)GridEditShape->PFGetMass());

			ImGui::Separator();
			if (ImGui::Button(u8"退出体素编辑"))
			{
				ExitGridEdit();
			}
		}
		ImGui::End();
	}

	void PhysicsTest3D::RenderGridEditPreview3D()
	{
		if (GridEditShape == nullptr)
		{
			return;
		}

		// 当前 Z 层：沿网格包围盒在该层的"外框"画一圈绿线。
		// 用体素中心的整数坐标范围即可（体素是单位立方体，中心在整数 +
		// 0.5 处；这里直接用 (x, y) 的 0..width/0..height 作为角点）。
		if (GridEditShowLayerBox)
		{
			const float z = (float)GridEditLayer + 0.5f;
			const glm::ivec3 c0(0, 0, GridEditLayer);
			const glm::vec3 minLocal = glm::vec3(c0) - GridEditShape->CentreMass;
			const glm::vec3 maxLocal = glm::vec3((float)GridEditShape->width,
			                                     (float)GridEditShape->height,
			                                     (float)GridEditLayer + 1.0f) -
			                           GridEditShape->CentreMass;
			const glm::vec3 base = GridEditShape->pos;
			const glm::vec4 layerColor{0.2f, 1.0f, 0.3f, 0.65f};

			// 该层的 4 个角（局部 → 世界）
			const glm::vec3 corners[4] = {
			    glm::vec3(minLocal.x, minLocal.y, z - GridEditShape->CentreMass.z),
			    glm::vec3(maxLocal.x, minLocal.y, z - GridEditShape->CentreMass.z),
			    glm::vec3(maxLocal.x, maxLocal.y, z - GridEditShape->CentreMass.z),
			    glm::vec3(minLocal.x, maxLocal.y, z - GridEditShape->CentreMass.z),
			};
			glm::vec3 w[4];
			for (int i = 0; i < 4; ++i)
			{
				w[i] = base + glm::vec3(PhysicsBlock3D::QuatRotate(GridEditShape->rotation, corners[i]));
			}
			for (int i = 0; i < 4; ++i)
			{
				mAuxiliaryVision->Line(w[i], layerColor, w[(i + 1) % 4], layerColor);
			}
		}

		// 笔刷落点：黄色线框立方体
		if (GridEditHoverValid)
		{
			glm::ivec3 cell = GridEditHoverCell;
			cell.z = GridEditLayer;
			const glm::vec3 center = VoxelCenter3D(GridEditShape, cell);
			const glm::vec3 half(0.5f, 0.5f, 0.5f);
			glm::vec4 color{1.0f, 1.0f, 0.0f, 0.9f};
			if (GridEditBrushType == GridEditBrush3D::Erase)
			{
				color = glm::vec4(1.0f, 0.2f, 0.2f, 0.9f);
			}

			// 未旋转的轴对齐立方体边（体素本身就是轴对齐的，旋转由
			// 顶点变换体现 —— 这里为了直观，把 8 个角用物体旋转变换一下）
			glm::vec3 corners[8];
			MakeBoxCorners3D(center, half, corners);
			glm::vec3 worldCorners[8];
			for (int i = 0; i < 8; ++i)
			{
				// corners 已经是世界坐标（含 base + 旋转后的中心），
				// 只需把 8 个偏移量按物体旋转方向重排
				const glm::vec3 offset = corners[i] - center;
				worldCorners[i] = center + glm::vec3(PhysicsBlock3D::QuatRotate(
				                                GridEditShape->rotation, offset));
			}
			for (int e = 0; e < 12; ++e)
			{
				mAuxiliaryVision->Line(worldCorners[kBoxEdges3D[e][0]], color,
				                       worldCorners[kBoxEdges3D[e][1]], color);
			}
		}
	}

}
