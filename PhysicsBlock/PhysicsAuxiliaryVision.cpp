#include "PhysicsAuxiliaryVision.hpp"
#include "MapDynamic.hpp"  // RenderMapOutline 中 _MapDynamic 分支需要具体类型
#include "ImGuiPhysics.hpp" // ColorToVec4 宏 + Auxiliary_*Bool / Auxiliary_*Color 全局开关
#include "BaseCalculate.hpp" // vec2angle / AngleFloatToAngleVec / AngleMat
#include "../VulkanTool/AuxiliaryVision.h" // 完整的 inline 绘制方法
// ImGui（drawUI 用）—— 与 ImGuiPhysics.cpp 保持一致的两种 include 路径
#if TranslatorLocality
#include "../ImGui/imgui.h"
#else
#include "../extern/imgui/imgui.h"
#endif
#include "../GlobalVariable.h" // Global::mWidth（drawUI 窗口定位）

namespace PhysicsBlock
{
	// ─────────────────────────────────────────────────────────────
	// 网格轮廓去重绘制（内部辅助）
	//
	// 相邻两个实体格子的共享边会被画两次（A 的右边 == B 的左边），完全重合，
	// 大网格里近一半线段是冗余的。这里改为只画"外轮廓边"——某条边仅在邻格非实体
	// （或越界）时才画，视觉效果完全一致，线段数接近减半。
	//
	// 实体判定 + 邻格查询统一走 IsEntity(x,y) 谓词（调用方负责越界处理：越界返回 false）。
	// 坐标变换：每个格子 (gx,gy) 的左下角世界坐标 = origin + rotate({gx,gy} - centreMass, angle)。
	// angle=0 时退化为轴对齐（地图轮廓的常见情况），仍正确。
	// ─────────────────────────────────────────────────────────────
	namespace {

		// 画一个格子的"外轮廓"四条边，每条边仅在邻格非实体时才画。
		// 四个角点在旋转后的世界坐标系下计算一次，按 (gx,gy)/(gx+1,gy+1) 映射。
		template <typename IsEntityFn>
		void DrawCellOutline(VulKan::AuxiliaryVision *av,
							 int gx, int gy,
							 glm::dvec2 origin, FLOAT_ angle, glm::vec2 centreMass,
							 glm::vec4 color, bool isStatic,
							 IsEntityFn IsEntity)
		{
			// 局部坐标（格子坐标系）四角，再旋转+平移到世界坐标
			glm::dvec2 A = origin + glm::dvec2(vec2angle(Vec2_{(FLOAT_)gx, (FLOAT_)gy} - centreMass, angle));
			glm::dvec2 B = origin + glm::dvec2(vec2angle(Vec2_{(FLOAT_)(gx + 1), (FLOAT_)gy} - centreMass, angle));
			glm::dvec2 C = origin + glm::dvec2(vec2angle(Vec2_{(FLOAT_)gx, (FLOAT_)(gy + 1)} - centreMass, angle));
			glm::dvec2 D = origin + glm::dvec2(vec2angle(Vec2_{(FLOAT_)(gx + 1), (FLOAT_)(gy + 1)} - centreMass, angle));

			// 底边 (A-B)：邻格 (gx, gy-1) 非实体才画
			if (!IsEntity(gx, gy - 1))
				(isStatic ? av->AddStaticLine({A, 0}, {B, 0}, color) : av->Line({A, 0}, color, {B, 0}, color));
			// 顶边 (C-D)：邻格 (gx, gy+1) 非实体才画
			if (!IsEntity(gx, gy + 1))
				(isStatic ? av->AddStaticLine({C, 0}, {D, 0}, color) : av->Line({C, 0}, color, {D, 0}, color));
			// 左边 (A-C)：邻格 (gx-1, gy) 非实体才画
			if (!IsEntity(gx - 1, gy))
				(isStatic ? av->AddStaticLine({A, 0}, {C, 0}, color) : av->Line({A, 0}, color, {C, 0}, color));
			// 右边 (B-D)：邻格 (gx+1, gy) 非实体才画
			if (!IsEntity(gx + 1, gy))
				(isStatic ? av->AddStaticLine({B, 0}, {D, 0}, color) : av->Line({B, 0}, color, {D, 0}, color));
		}

	} // anonymous namespace

	// ─────────────────────────────────────────────────────────────
	// 基本图元
	// ─────────────────────────────────────────────────────────────

	void DrawStaticSquare(VulKan::AuxiliaryVision *av, glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = vec2angle({0, 1}, Angle);
		glm::dvec2 jiao2 = vec2angle({1, 0}, Angle);
		glm::dvec2 jiao3 = jiao2 + jiao1;
		av->AddStaticLine({pos, 0}, {pos + jiao1, 0}, color);
		av->AddStaticLine({pos, 0}, {pos + jiao2, 0}, color);
		av->AddStaticLine({pos + jiao3, 0}, {pos + jiao1, 0}, color);
		av->AddStaticLine({pos + jiao3, 0}, {pos + jiao2, 0}, color);
	}

	void DrawSquare(VulKan::AuxiliaryVision *av, glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = vec2angle({0, 1}, Angle);
		glm::dvec2 jiao2 = vec2angle({1, 0}, Angle);
		glm::dvec2 jiao3 = jiao2 + jiao1;
		av->Line({pos, 0}, color, {pos + jiao1, 0}, color);
		av->Line({pos, 0}, color, {pos + jiao2, 0}, color);
		av->Line({pos + jiao3, 0}, color, {pos + jiao1, 0}, color);
		av->Line({pos + jiao3, 0}, color, {pos + jiao2, 0}, color);
	}

	glm::vec4 ObjectBodyColor(unsigned StaticNum)
	{
		return {0, (StaticNum < 10 ? 1 : 0.2), 0, 1};
	}

	// ─────────────────────────────────────────────────────────────
	// 单体物体：本体绘制
	// ─────────────────────────────────────────────────────────────

	void DrawShape(VulKan::AuxiliaryVision *av, PhysicsShape *obj, glm::vec4 color)
	{
		// 邻格实体判定（带越界保护：越界视为非实体，从而画出外边界）
		auto IsEntity = [&](int x, int y) -> bool {
			if (x < 0 || y < 0 || x >= (int)obj->width || y >= (int)obj->height)
				return false;
			return obj->at(x, y).Entity;
		};
		glm::dvec2 origin = obj->pos;
		glm::vec2 centreMass = obj->CentreMass;
		for (size_t x = 0; x < obj->width; ++x)
		{
			for (size_t y = 0; y < obj->height; ++y)
			{
				if (obj->at(x, y).Entity)
				{
					DrawCellOutline(av, (int)x, (int)y, origin, obj->angle, centreMass, color, /*isStatic=*/false, IsEntity);
				}
			}
		}
	}

	void DrawParticle(VulKan::AuxiliaryVision *av, PhysicsParticle *obj, glm::vec4 color)
	{
		av->Spot({obj->pos, 0}, 0.05f, color);
	}

	void DrawCircle(VulKan::AuxiliaryVision *av, PhysicsCircle *obj, glm::vec4 color)
	{
		av->Circle({obj->pos, 0}, obj->radius, color);
	}

	void DrawLine(VulKan::AuxiliaryVision *av, PhysicsLine *obj, glm::vec4 color)
	{
		Vec2_ pR = vec2angle({obj->radius, 0}, obj->angle);
		av->Line({obj->pos + pR, 0}, color, {obj->pos - pR, 0}, color);
	}

	// ─────────────────────────────────────────────────────────────
	// 单体物体：辅助信息绘制
	// ─────────────────────────────────────────────────────────────

	void DrawShapeAuxiliary(VulKan::AuxiliaryVision *av, PhysicsShape *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool)
			av->Spot({i->pos, 0}, 0.05f, ColorToVec4(Auxiliary_PosColor));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool)
			av->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(Auxiliary_OldPosColor));
		// 辅助显示角度
		if (Auxiliary_AngleBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_AngleColor), {i->pos + vec2angle({1, 0}, i->angle), 0}, ColorToVec4(Auxiliary_AngleColor));
		// 辅助显示速度
		if (Auxiliary_SpeedBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(Auxiliary_SpeedColor));
		// 辅助显示受力
		if (Auxiliary_ForceBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(Auxiliary_ForceColor));
		// 辅助显示质心
		if (Auxiliary_CentreMassBool)
			av->Spot({i->pos, 0}, 0.05f, ColorToVec4(Auxiliary_CentreMassColor));
		// 辅助显示几何中心
		if (Auxiliary_CentreShapeBool)
			av->Spot({i->pos - vec2angle(i->CentreMass - i->CentreShape, i->angle), 0}, 0.05f, ColorToVec4(Auxiliary_CentreShapeColor));
		// 辅助显示外骨骼点
		if (Auxiliary_OutlineBool)
		{
			AngleMat angleMat(i->angle);
			for (size_t d = 0; d < i->OutlineSize; ++d)
			{
				av->Spot({i->pos + angleMat.Rotary(i->OutlineSet[d]), 0}, 0.05f, ColorToVec4(Auxiliary_OutlineColor));
			}
		}
		// 辅助显示最大外骨骼点质心
		if (Auxiliary_MaxOutlineCentreMassBool)
		{
			AngleMat angleMat(i->angle);
			for (size_t d = 0; d < i->OutlineSize; ++d)
			{
				av->Spot({i->pos + angleMat.Rotary(i->MaxOutlineCentreMass[d]), 0}, 0.05f, ColorToVec4(Auxiliary_MaxOutlineCentreMassColor));
			}
		}
		if (Auxiliary_MaxOutlineCentreMassBool & Auxiliary_OutlineBool)
		{
			AngleMat angleMat(i->angle);
			for (size_t d = 0; d < i->OutlineSize; ++d)
			{
				av->Line({i->pos + angleMat.Rotary(i->MaxOutlineCentreMass[d]), 0}, ColorToVec4(Auxiliary_ForceColor), {i->pos + angleMat.Rotary(i->OutlineSet[d]), 0}, ColorToVec4(Auxiliary_ForceColor));
			}
		}
	}

	void DrawParticleAuxiliary(VulKan::AuxiliaryVision *av, PhysicsParticle *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool)
			av->Spot({i->pos, 0}, 0.05f, ColorToVec4(Auxiliary_PosColor));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool)
			av->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(Auxiliary_OldPosColor));
		// 辅助显示速度
		if (Auxiliary_SpeedBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(Auxiliary_SpeedColor));
		// 辅助显示受力
		if (Auxiliary_ForceBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(Auxiliary_ForceColor));
	}

	void DrawCircleAuxiliary(VulKan::AuxiliaryVision *av, PhysicsCircle *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool)
			av->Spot({i->pos, 0}, 0.05f, ColorToVec4(Auxiliary_PosColor));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool)
			av->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(Auxiliary_OldPosColor));
		// 辅助显示角度
		if (Auxiliary_AngleBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_AngleColor), {i->pos + vec2angle({i->radius, 0}, i->angle), 0}, ColorToVec4(Auxiliary_AngleColor));
		// 辅助显示速度
		if (Auxiliary_SpeedBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(Auxiliary_SpeedColor));
		// 辅助显示受力
		if (Auxiliary_ForceBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(Auxiliary_ForceColor));
	}

	void DrawLineAuxiliary(VulKan::AuxiliaryVision *av, PhysicsLine *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool)
			av->Spot({i->pos, 0}, 0.05f, ColorToVec4(Auxiliary_PosColor));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool)
			av->Spot({i->OldPos, 0}, 0.05f, ColorToVec4(Auxiliary_OldPosColor));
		// 辅助显示角度
		if (Auxiliary_AngleBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_AngleColor), {i->pos + vec2angle({i->radius, 0}, i->angle), 0}, ColorToVec4(Auxiliary_AngleColor));
		// 辅助显示速度
		if (Auxiliary_SpeedBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_SpeedColor), {i->pos + i->speed, 0}, ColorToVec4(Auxiliary_SpeedColor));
		// 辅助显示受力
		if (Auxiliary_ForceBool)
			av->Line({i->pos, 0}, ColorToVec4(Auxiliary_ForceColor), {i->pos + i->force, 0}, ColorToVec4(Auxiliary_ForceColor));
	}

	// ─────────────────────────────────────────────────────────────
	// 集合级绘制
	// ─────────────────────────────────────────────────────────────

	void DrawJoints(VulKan::AuxiliaryVision *av, PhysicsWorld *world)
	{
		for (auto i : world->GetPhysicsJoint())
		{
			av->Line({i->body1->pos, 0}, {0, 1, 0, 1}, {i->body1->pos + i->r1, 0}, {0, 1, 0, 1});
			av->Line({i->body2->pos, 0}, {0, 1, 0, 1}, {i->body2->pos + i->r2, 0}, {0, 1, 0, 1});
		}
	}

	void DrawJunctions(VulKan::AuxiliaryVision *av, PhysicsWorld *world)
	{
		for (auto j : world->GetBaseJunction())
		{
			av->Line({j->GetA(), 0}, {0, 1, 0, 1}, {j->GetB(), 0}, {0, 1, 0, 1});
		}
	}

	void DrawCollisions(VulKan::AuxiliaryVision *av, PhysicsWorld *world)
	{
		for (auto &i : world->CollideGroupVector)
		{
			for (int j = 0; j < i->numContacts; ++j)
			{
				// 碰撞点
				if (Auxiliary_CollisionDropBool)
					av->Spot({i->contacts[j].position, 0}, 0.05f, ColorToVec4(Auxiliary_CollisionDropColor));
				// 碰撞点 分离 法向量
				if (Auxiliary_SeparateNormalVectorBool)
					av->Line({i->contacts[j].position, 0}, ColorToVec4(Auxiliary_SeparateNormalVectorColor), {i->contacts[j].position + i->contacts[j].normal, 0}, ColorToVec4(Auxiliary_SeparateNormalVectorColor));
				// 指向重心
				if (Auxiliary_CollisionDropToCenterOfGravityBool)
				{
					av->Line({i->contacts[j].position, 0}, ColorToVec4(Auxiliary_CollisionDropToCenterOfGravityColor), {i->contacts[j].position - i->contacts[j].r1, 0}, ColorToVec4(Auxiliary_CollisionDropToCenterOfGravityColor));
					av->Line({i->contacts[j].position, 0}, ColorToVec4(Auxiliary_CollisionDropToCenterOfGravityColor), {i->contacts[j].position - i->contacts[j].r2, 0}, ColorToVec4(Auxiliary_CollisionDropToCenterOfGravityColor));
				}
			}
		}
	}

	void DrawTriggers(VulKan::AuxiliaryVision *av, PhysicsWorld *world)
	{
		for (const auto &[handle, config] : world->mTrigger.GetConfigs())
		{
			Vec2_ min = config.TriggerBounds.Center - config.TriggerBounds.HalfSize;
			Vec2_ max = config.TriggerBounds.Center + config.TriggerBounds.HalfSize;
			glm::vec4 color = {1.0f, 0.5f, 0.0f, 1.0f};
			av->Line({min.x, min.y, 0}, color, {max.x, min.y, 0}, color);
			av->Line({max.x, min.y, 0}, color, {max.x, max.y, 0}, color);
			av->Line({max.x, max.y, 0}, color, {min.x, max.y, 0}, color);
			av->Line({min.x, max.y, 0}, color, {min.x, min.y, 0}, color);
		}
	}

	void DrawGridDivided(VulKan::AuxiliaryVision *av, PhysicsWorld *world, PhysicsFormwork *selected)
	{
		if (!Auxiliary_GridDividedBool || selected == nullptr)
			return;

		std::vector<Vec2_> vision = world->mGridSearch.GetDividedVision(selected);
		for (size_t i = 0; i < (vision.size() / 2); ++i)
		{
			av->Line({vision[i * 2], 0}, ColorToVec4(Auxiliary_GridDividedColor), {vision[i * 2 + 1].x, vision[i * 2].y, 0}, ColorToVec4(Auxiliary_GridDividedColor));
			av->Line({vision[i * 2], 0}, ColorToVec4(Auxiliary_GridDividedColor), {vision[i * 2].x, vision[i * 2 + 1].y, 0}, ColorToVec4(Auxiliary_GridDividedColor));
			av->Line({vision[i * 2 + 1], 0}, ColorToVec4(Auxiliary_GridDividedColor), {vision[i * 2 + 1].x, vision[i * 2].y, 0}, ColorToVec4(Auxiliary_GridDividedColor));
			av->Line({vision[i * 2 + 1], 0}, ColorToVec4(Auxiliary_GridDividedColor), {vision[i * 2].x, vision[i * 2 + 1].y, 0}, ColorToVec4(Auxiliary_GridDividedColor));
		}
	}

	void RenderMapOutline(VulKan::AuxiliaryVision *av, PhysicsWorld *world)
	{
		MapFormwork *mMapFormwork = world->GetMapFormwork();
		glm::uvec2 mapSize = mMapFormwork->FMGetMapSize();
		if (mapSize.x == 0 || mapSize.y == 0)
			return;
		av->ClearStaticLine();

		if (world->GetMapFormwork()->FMGetType() == PhysicsObjectEnum::_MapDynamic)
		{
			MapDynamic *dynMap = (MapDynamic *)(world->GetMapFormwork());

			int startX = -(int)(dynMap->GetPlatePos().x);
			int startY = -(int)(dynMap->GetPlatePos().y);
			int endX = startX + (int)mapSize.x;
			int endY = startY + (int)mapSize.y;

			// 邻格实体判定（带越界保护：板块外视为非实体，画出外边界）
			auto IsEntity = [&](int x, int y) -> bool {
				if (x < startX || y < startY || x >= endX || y >= endY)
					return false;
				return mMapFormwork->FMGetGridBlock({x, y}).Collision;
			};
			glm::vec4 color = {0, 1, 0, 1};
			for (int x = startX; x < endX; x++)
			{
				for (int y = startY; y < endY; y++)
				{
					if (IsEntity(x, y))
					{
						// 动态地图格子直接用世界坐标，origin/centreMass 均为 0
						DrawCellOutline(av, x, y, glm::dvec2{0, 0}, (FLOAT_)0, glm::vec2{0, 0}, color, /*isStatic=*/true, IsEntity);
					}
				}
			}
		}
		else
		{
			Vec2_ mapCentrality = mMapFormwork->FMGetCentrality();
			int endX = (int)mapSize.x;
			int endY = (int)mapSize.y;

			// 邻格实体判定（带越界保护：地图外视为非实体，画出外边界）
			auto IsEntity = [&](int x, int y) -> bool {
				if (x < 0 || y < 0 || x >= endX || y >= endY)
					return false;
				return mMapFormwork->FMGetGridBlock({x, y}).Collision;
			};
			glm::dvec2 origin = glm::dvec2(-mapCentrality);
			glm::vec4 color = {0, 1, 0, 1};
			for (int x = 0; x < endX; x++)
			{
				for (int y = 0; y < endY; y++)
				{
					if (IsEntity(x, y))
					{
						// 静态地图世界坐标 = (x,y) - mapCentrality；centreMass=0
						DrawCellOutline(av, x, y, origin, (FLOAT_)0, glm::vec2{0, 0}, color, /*isStatic=*/true, IsEntity);
					}
				}
			}
		}
	}

	// ─────────────────────────────────────────────────────────────
	// 高层聚合
	// ─────────────────────────────────────────────────────────────

	void DrawPhysicsWorld(VulKan::AuxiliaryVision *av, PhysicsWorld *world,
						  bool showAuxiliary, PhysicsFormwork *selected)
	{
		// 注意：本函数不管理 av->Begin()/End()，由调用方在录制区间内调用。
		// 这样可以让鼠标拖拽反馈线等其它绘制与本帧物理内容共用同一个录制区间。

		// 查看分配到的网格树位置（仅显示辅助信息时）
		if (showAuxiliary)
			DrawGridDivided(av, world, selected);

		// 渲染物理网格
		for (auto i : world->GetPhysicsShape())
		{
			DrawShape(av, i, ObjectBodyColor(i->StaticNum));
			if (showAuxiliary)
				DrawShapeAuxiliary(av, i);
		}
		// 渲染物理点
		for (auto i : world->GetPhysicsParticle())
		{
			DrawParticle(av, i, ObjectBodyColor(i->StaticNum));
			if (showAuxiliary)
				DrawParticleAuxiliary(av, i);
		}
		// 渲染圆
		for (auto i : world->GetPhysicsCircle())
		{
			DrawCircle(av, i, ObjectBodyColor(i->StaticNum));
			if (showAuxiliary)
				DrawCircleAuxiliary(av, i);
		}
		// 渲染线
		for (auto i : world->GetPhysicsLine())
		{
			DrawLine(av, i, ObjectBodyColor(i->StaticNum));
			if (showAuxiliary)
				DrawLineAuxiliary(av, i);
		}

		// 渲染关节 / 连接点
		DrawJoints(av, world);
		DrawJunctions(av, world);

		// 渲染碰撞信息（仅显示辅助信息时）
		if (showAuxiliary)
			DrawCollisions(av, world);

		// 渲染触发器区域
		DrawTriggers(av, world);
	}

	void DrawPhysicsWorldFrame(VulKan::AuxiliaryVision *av, PhysicsWorld *world,
							   bool showAuxiliary, PhysicsFormwork *selected)
	{
		av->Begin();
		DrawPhysicsWorld(av, world, showAuxiliary, selected);
		av->End();
	}

	// ─────────────────────────────────────────────────────────────
	// 物理辅助显示控制器 PhysicsDebugView
	// ─────────────────────────────────────────────────────────────

	void PhysicsDebugView::setup(VulKan::AuxiliaryVision *a, PhysicsWorld *w)
	{
		av = a;
		world = w;
		// 首帧重写静态地图缓冲；动态地图的 lastPlatePos 由 refreshMap() 首次比对时自然触发
		mapDirty = true;
	}

	void PhysicsDebugView::refreshMap()
	{
		if (world == nullptr || av == nullptr || !showMap)
			return;

		MapFormwork *m = world->GetMapFormwork();
		if (m == nullptr)
			return;

		// 动态地图：板块位置变化即视为脏，需重写静态缓冲
		if (m->FMGetType() == PhysicsObjectEnum::_MapDynamic)
		{
			glm::ivec2 platePos = ((MapDynamic *)m)->GetPlatePos();
			if (platePos != lastPlatePos)
			{
				mapDirty = true;
				lastPlatePos = platePos;
			}
		}
		if (mapDirty)
		{
			RenderMapOutline(av, world);
			mapDirty = false;
		}
	}

	void PhysicsDebugView::drawWorld()
	{
		if (av == nullptr || world == nullptr)
			return;
		DrawPhysicsWorld(av, world, showAuxiliary, selected);
	}

	void PhysicsDebugView::render()
	{
		refreshMap();
		if (av == nullptr || world == nullptr)
			return;
		av->Begin();
		drawWorld();
		av->End();
	}

	void PhysicsDebugView::drawUI(const char *windowTitle)
	{
		ImGui::Begin(windowTitle == nullptr ? u8"物理辅助显示" : windowTitle);
		ImGui::SetWindowSize(ImVec2(360, 220), ImGuiCond_FirstUseEver);
		ImGui::SetWindowPos(ImVec2(Global::mWidth - 360, 0), ImGuiCond_FirstUseEver);

		if (ImGui::Button(showAuxiliary ? u8"关闭辅助视觉" : u8"开启辅助视觉"))
		{
			showAuxiliary = !showAuxiliary;
		}
		ImGui::Checkbox(u8"显示地图轮廓", &showMap);

		if (showAuxiliary)
		{
			ImGui::Spacing();
			ImGui::TextDisabled(u8"辅助视觉参数");
			PhysicsAuxiliaryColorUI();
		}

		ImGui::End();
	}

} // namespace PhysicsBlock
