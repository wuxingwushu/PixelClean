#include "PhysicsAuxiliaryVision3D.hpp"
#include "MapDynamic3D.hpp"  // RenderMapOutline3D 中 _MapDynamic 分支需要具体类型
#include "ImGuiPhysics3D.hpp" // ColorToVec4_3D 宏 + Auxiliary_*Bool3D / Auxiliary_*Color3D 全局开关

/*
 * 关于内存池：Vulkan 侧的 include 链会拉进 2D 的 `Tool/MemoryPool.h`（全局作用域
 * `class MemoryPool`），而本引擎用的是 `PhysicsBlock3D/MemoryPool3D.h`。两者已在
 * 命名空间层面隔离（3D 版放在 `namespace PhysicsBlock3D` 内），因此可以共存，
 * 不需要任何 include 顺序上的对冲。
 */
#include "../VulkanTool/AuxiliaryVision.h" // 完整的 inline 绘制方法
// ImGui（drawUI 用）—— 与 ImGuiPhysics3D.cpp 保持一致的两种 include 路径
#if TranslatorLocality3D
#include "../ImGui/imgui.h"
#else
#include "../extern/imgui/imgui.h"
#endif
#include "../GlobalVariable.h" // Global::mWidth（drawUI 窗口定位）

namespace PhysicsBlock3D
{
	// ─────────────────────────────────────────────────────────────
	// 内部辅助
	// ─────────────────────────────────────────────────────────────
	namespace
	{
		/**
		 * @brief 把 float[4] 颜色数组转成 glm::vec4
		 * @details 宏 ColorToVec4_3D 展开为 `glm::vec4{...}` 花括号初始化列表，
		 *          只在声明处可用；函数实参无法直接吃它，故这里包一层函数。 */
		inline glm::vec4 AuxColor3D(const float *color)
		{
			return ColorToVec4_3D(color);
		}

		/**
		 * @brief 网格体素外轮廓绘制（3D 版；判据与 2D 逐条同构）
		 *
		 * 2D 原判据（DrawCellOutline）：实体格子的 4 条边里，某条边仅当"该边外侧的
		 * 邻格非实体（或越界）"时才画。等价说法是：**只画格子朝外的面所属的边**，
		 * 于是相邻实体格共享的内部边不会重复出现，线段数接近减半。
		 *
		 * 3D 推广（本函数）：立方体格子有 6 个面，某个面仅当"该面法线方向的邻格
		 * 非实体（或越界）"时才是外露面；对外露的每个面画它自己的 4 条边。
		 * 这与 2D 判据逐条对应（2D 的"边"= 3D 的面），同样保证内部边不出现、
		 * 每条外露边由拥有它的格子画出（仅当两个外露面共棱时会被画两次，属可接受的
		 * 少量重复；实测比"12 条棱各判一次"的朴素写法更接近 2D 的"接近减半"效果）。
		 *
		 * 坐标变换：体素 (gx,gy,gz) 的角点 (i,j,k)∈{0,1}³ 世界坐标 =
		 *           origin + QuatRotate(rotation, Vec3_(gx+i,gy+j,gz+k) - centreMass)。
		 * rotation 为单位四元数时退化为轴对齐（地图轮廓的常见情形），仍然正确。
		 *
		 * @param isStatic true 时走 `av->AddStaticLine`（录制外静态缓冲），
		 *                 false 时走 `av->Line`（必须在 Begin/End 之间）
		 */
		template <typename IsEntityFn>
		void DrawCellOutline3D(VulKan::AuxiliaryVision *av,
							   int gx, int gy, int gz,
							   glm::dvec3 origin, Quat_ rotation, Vec3_ centreMass,
							   glm::vec4 color, bool isStatic,
							   IsEntityFn IsEntity)
		{
			// 体素 8 个角点：局部坐标 → 旋转 → 平移到世界坐标
			// 注意：角点局部坐标必须是「格子索引 + 角点偏移」= (gx+i, gy+j, gz+k)。
			// 若漏掉 gx/gy/gz，所有体素都会坍缩成同一个单位立方体：地图轮廓只会
			// 在地图原点处画出一个 1×1×1 的小盒子（视觉上等于"看不见地形"），
			// 物体轮廓也会失去自身尺寸。判据（邻格非实体才画该面）本身与 2D 同构，
			// 不受影响，所以这种写法只在"坐标"上出错、不会报错。
			auto Corner = [&](int i, int j, int k) -> Vec3_ {
				return Vec3_(origin) + QuatRotate(rotation, Vec3_((FLOAT_)(gx + i), (FLOAT_)(gy + j), (FLOAT_)(gz + k)) - centreMass);
			};
			const Vec3_ c000 = Corner(0, 0, 0);
			const Vec3_ c100 = Corner(1, 0, 0);
			const Vec3_ c010 = Corner(0, 1, 0);
			const Vec3_ c001 = Corner(0, 0, 1);
			const Vec3_ c110 = Corner(1, 1, 0);
			const Vec3_ c101 = Corner(1, 0, 1);
			const Vec3_ c011 = Corner(0, 1, 1);
			const Vec3_ c111 = Corner(1, 1, 1);

			// 统一的描边谓词：静态/动态两种后端在此分流
			auto Edge = [&](const Vec3_ &P, const Vec3_ &Q) {
				if (isStatic)
					av->AddStaticLine(glm::dvec3(P), glm::dvec3(Q), color);
				else
					av->Line(glm::dvec3(P), color, glm::dvec3(Q), color);
			};

			// 依次画四边形 p00→p10→p11→p01 的 4 条边
			auto Face = [&](const Vec3_ &p00, const Vec3_ &p10, const Vec3_ &p11, const Vec3_ &p01) {
				Edge(p00, p10);
				Edge(p10, p11);
				Edge(p11, p01);
				Edge(p01, p00);
			};

			// 每个外露面画 4 条边（判据：该面法线方向的邻格非实体/越界）
			if (!IsEntity(gx - 1, gy, gz)) // -X 面
				Face(c000, c010, c011, c001);
			if (!IsEntity(gx + 1, gy, gz)) // +X 面
				Face(c100, c110, c111, c101);
			if (!IsEntity(gx, gy - 1, gz)) // -Y 面
				Face(c000, c100, c101, c001);
			if (!IsEntity(gx, gy + 1, gz)) // +Y 面
				Face(c010, c110, c111, c011);
			if (!IsEntity(gx, gy, gz - 1)) // -Z 面
				Face(c000, c100, c110, c010);
			if (!IsEntity(gx, gy, gz + 1)) // +Z 面
				Face(c001, c101, c111, c011);
		}

		/**
		 * @brief 画一条线段的两个端点标记（辅助信息用，便于在 3D 中看清线的两端）
		 * @details 2D 是一张平面图，线段端点天然可见；3D 下线段可能朝向任意方向，
		 *          加上两端的小球标记可显著提升可读性。 */
		inline void SpotBothEnds(VulKan::AuxiliaryVision *av, const Vec3_ &P, const Vec3_ &Q, glm::vec4 color)
		{
			av->Spot(glm::dvec3(P), AuxiliaryMarkSize3D, color);
			av->Spot(glm::dvec3(Q), AuxiliaryMarkSize3D, color);
		}
	} // anonymous namespace

	// ─────────────────────────────────────────────────────────────
	// 基本图元
	// ─────────────────────────────────────────────────────────────

	void DrawStaticSquare3D(VulKan::AuxiliaryVision *av, glm::dvec3 pos, Quat_ rotation, glm::vec4 color)
	{
		// 2D 用 AngleFloatToAngleVec(angle) 得到旋转矩阵；3D 直接用四元数旋转基向量。
		const Vec3_ jiao1 = QuatRotate(rotation, Vec3_(0, 1, 0));
		const Vec3_ jiao2 = QuatRotate(rotation, Vec3_(1, 0, 0));
		const Vec3_ jiao3 = jiao2 + jiao1;
		av->AddStaticLine(pos, pos + glm::dvec3(jiao1), color);
		av->AddStaticLine(pos, pos + glm::dvec3(jiao2), color);
		av->AddStaticLine(pos + glm::dvec3(jiao3), pos + glm::dvec3(jiao1), color);
		av->AddStaticLine(pos + glm::dvec3(jiao3), pos + glm::dvec3(jiao2), color);
	}

	void DrawSquare3D(VulKan::AuxiliaryVision *av, glm::dvec3 pos, Quat_ rotation, glm::vec4 color)
	{
		const Vec3_ jiao1 = QuatRotate(rotation, Vec3_(0, 1, 0));
		const Vec3_ jiao2 = QuatRotate(rotation, Vec3_(1, 0, 0));
		const Vec3_ jiao3 = jiao2 + jiao1;
		av->Line(pos, color, pos + glm::dvec3(jiao1), color);
		av->Line(pos, color, pos + glm::dvec3(jiao2), color);
		av->Line(pos + glm::dvec3(jiao3), color, pos + glm::dvec3(jiao1), color);
		av->Line(pos + glm::dvec3(jiao3), color, pos + glm::dvec3(jiao2), color);
	}

	glm::vec4 ObjectBodyColor3D(unsigned StaticNum)
	{
		// 判据与 2D 完全一致：StaticNum < 10 视为动态（亮绿），否则静止（暗绿）
		return {0, (StaticNum < 10 ? 1 : 0.2), 0, 1};
	}

	// ─────────────────────────────────────────────────────────────
	// 单体物体：本体绘制
	// ─────────────────────────────────────────────────────────────

	void DrawShape3D(VulKan::AuxiliaryVision *av, PhysicsShape3D *obj, glm::vec4 color)
	{
		// 邻格实体判定（带越界保护：越界视为非实体，从而画出外边界）
		auto IsEntity = [&](int x, int y, int z) -> bool {
			if (x < 0 || y < 0 || z < 0 ||
				x >= (int)obj->width || y >= (int)obj->height || z >= (int)obj->depth)
				return false;
			return obj->at(x, y, z).Entity;
		};
		const glm::dvec3 origin = glm::dvec3(obj->pos);
		for (unsigned int x = 0; x < obj->width; ++x)
		{
			for (unsigned int y = 0; y < obj->height; ++y)
			{
				for (unsigned int z = 0; z < obj->depth; ++z)
				{
					if (obj->at((int)x, (int)y, (int)z).Entity)
					{
						DrawCellOutline3D(av, (int)x, (int)y, (int)z, origin,
										  obj->rotation, obj->CentreMass, color,
										  /*isStatic=*/false, IsEntity);
					}
				}
			}
		}
	}

	void DrawParticle3D(VulKan::AuxiliaryVision *av, PhysicsParticle3D *obj, glm::vec4 color)
	{
		// 2D: av->Spot({obj->pos, 0}, 0.05f, color) —— z=0 压平；
		// 3D: 直接传真实三维位置（AuxiliaryVision 的 Spot 收 glm::dvec3）
		av->Spot(glm::dvec3(obj->pos), AuxiliaryMarkSize3D, color);
	}

	void DrawCircle3D(VulKan::AuxiliaryVision *av, PhysicsCircle3D *obj, glm::vec4 color)
	{
		// ── 3D 对"2D 圆"的表示：三条大圆组成的线框球 ──
		// AuxiliaryVision 的 Circle 只是一个存 (Pos, Radius, Color) 的标记，
		// 没有真正的线框球图元；而 2D 的 Circle 是**圆的轮廓**。
		// 若退化成单个 Circle 标记，球体在 3D 下的空间占据就完全看不见。
		// 因此这里用与形状轮廓同一套线段管线（av->Line）画 XY / XZ / YZ 三个平面上
		// 的大圆，半径均为 obj->radius、圆心均为 obj->pos：它在任意视角下都能
		// 表达球的体积，且与 2D"画出物体边界"的意图一致。
		const Vec3_ c = obj->pos;
		const FLOAT_ r = obj->radius;
		const int Segments = 24;
		for (int i = 0; i < Segments; ++i)
		{
			const FLOAT_ a0 = (FLOAT_)(2.0 * M_PI * i / Segments);
			const FLOAT_ a1 = (FLOAT_)(2.0 * M_PI * (i + 1) / Segments);
			const FLOAT_ c0 = cosf(a0), s0 = sinf(a0);
			const FLOAT_ c1 = cosf(a1), s1 = sinf(a1);
			// XY 平面大圆
			av->Line(glm::dvec3(c + Vec3_(r * c0, r * s0, 0.0f)), color,
					 glm::dvec3(c + Vec3_(r * c1, r * s1, 0.0f)), color);
			// XZ 平面大圆
			av->Line(glm::dvec3(c + Vec3_(r * c0, 0.0f, r * s0)), color,
					 glm::dvec3(c + Vec3_(r * c1, 0.0f, r * s1)), color);
			// YZ 平面大圆
			av->Line(glm::dvec3(c + Vec3_(0.0f, r * c0, r * s0)), color,
					 glm::dvec3(c + Vec3_(0.0f, r * c1, r * s1)), color);
		}
	}

	void DrawLine3D(VulKan::AuxiliaryVision *av, PhysicsLine3D *obj, glm::vec4 color)
	{
		// 2D 手工用 vec2angle({obj->radius,0}, obj->angle) 重建端点；
		// 3D 由物理类直接给出真实线段端点（朝向已包含在内）。
		Vec3_ begin, end;
		obj->GetSegment(begin, end);
		av->Line(glm::dvec3(begin), color, glm::dvec3(end), color);
	}

	// ─────────────────────────────────────────────────────────────
	// 单体物体：辅助信息绘制
	// ─────────────────────────────────────────────────────────────

	void DrawShapeAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsShape3D *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool3D)
			av->Spot(glm::dvec3(i->pos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_PosColor3D));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool3D)
			av->Spot(glm::dvec3(i->OldPos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_OldPosColor3D));
		// 辅助显示角度（3D 下画"局部 X 轴朝向"：2D 的 angle 只有绕 Z 一轴，
		// 3D 用四元数表示任意朝向，用单位 X 轴的像表达朝向是最直接的对应物）
		if (Auxiliary_AngleBool3D)
		{
			const Vec3_ dir = QuatRotate(i->rotation, Vec3_(1, 0, 0));
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_AngleColor3D),
					 glm::dvec3(i->pos + dir), AuxColor3D(Auxiliary_AngleColor3D));
		}
		// 辅助显示速度
		if (Auxiliary_SpeedBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_SpeedColor3D),
					 glm::dvec3(i->pos + i->speed), AuxColor3D(Auxiliary_SpeedColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->speed, AuxColor3D(Auxiliary_SpeedColor3D));
		}
		// 辅助显示受力
		if (Auxiliary_ForceBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_ForceColor3D),
					 glm::dvec3(i->pos + i->force), AuxColor3D(Auxiliary_ForceColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->force, AuxColor3D(Auxiliary_ForceColor3D));
		}
		// 辅助显示质心（质心位置标记：2D 也画在 pos 上）
		if (Auxiliary_CentreMassBool3D)
			av->Spot(glm::dvec3(i->pos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_CentreMassColor3D));
		// 辅助显示几何中心（质心 → 几何中心的偏移反向旋转到世界系）
		if (Auxiliary_CentreShapeBool3D)
		{
			const Vec3_ centerShape = Vec3_(i->pos) - QuatRotate(i->rotation, i->CentreMass - i->CentreShape);
			av->Spot(glm::dvec3(centerShape), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_CentreShapeColor3D));
		}
		// 辅助显示外骨骼点
		if (Auxiliary_OutlineBool3D)
		{
			for (size_t d = 0; d < i->OutlineSet.size(); ++d)
			{
				const Vec3_ p = Vec3_(i->pos) + QuatRotate(i->rotation, i->OutlineSet[d].LocalPos);
				av->Spot(glm::dvec3(p), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_OutlineColor3D));
			}
		}
		// 辅助显示最大外骨骼点质心（3D 对应 OutlinePoint3D::SeedPos）
		if (Auxiliary_MaxOutlineCentreMassBool3D)
		{
			for (size_t d = 0; d < i->OutlineSet.size(); ++d)
			{
				const Vec3_ p = Vec3_(i->pos) + QuatRotate(i->rotation, i->OutlineSet[d].SeedPos);
				av->Spot(glm::dvec3(p), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_MaxOutlineCentreMassColor3D));
			}
		}
		if (Auxiliary_MaxOutlineCentreMassBool3D & Auxiliary_OutlineBool3D)
		{
			for (size_t d = 0; d < i->OutlineSet.size(); ++d)
			{
				const Vec3_ seed = Vec3_(i->pos) + QuatRotate(i->rotation, i->OutlineSet[d].SeedPos);
				const Vec3_ surface = Vec3_(i->pos) + QuatRotate(i->rotation, i->OutlineSet[d].LocalPos);
				av->Line(glm::dvec3(seed), AuxColor3D(Auxiliary_ForceColor3D),
						 glm::dvec3(surface), AuxColor3D(Auxiliary_ForceColor3D));
			}
		}
	}

	void DrawParticleAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsParticle3D *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool3D)
			av->Spot(glm::dvec3(i->pos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_PosColor3D));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool3D)
			av->Spot(glm::dvec3(i->OldPos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_OldPosColor3D));
		// 辅助显示速度
		if (Auxiliary_SpeedBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_SpeedColor3D),
					 glm::dvec3(i->pos + i->speed), AuxColor3D(Auxiliary_SpeedColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->speed, AuxColor3D(Auxiliary_SpeedColor3D));
		}
		// 辅助显示受力
		if (Auxiliary_ForceBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_ForceColor3D),
					 glm::dvec3(i->pos + i->force), AuxColor3D(Auxiliary_ForceColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->force, AuxColor3D(Auxiliary_ForceColor3D));
		}
	}

	void DrawCircleAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsCircle3D *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool3D)
			av->Spot(glm::dvec3(i->pos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_PosColor3D));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool3D)
			av->Spot(glm::dvec3(i->OldPos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_OldPosColor3D));
		// 辅助显示角度：2D 画 pos → pos + (radius,0) 旋转 angle；
		// 3D 画 pos → pos + QuatRotate(rotation,(radius,0,0))，表达"球的一个极轴"
		if (Auxiliary_AngleBool3D)
		{
			const Vec3_ dir = QuatRotate(i->rotation, Vec3_(i->radius, 0, 0));
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_AngleColor3D),
					 glm::dvec3(i->pos + dir), AuxColor3D(Auxiliary_AngleColor3D));
		}
		// 辅助显示速度
		if (Auxiliary_SpeedBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_SpeedColor3D),
					 glm::dvec3(i->pos + i->speed), AuxColor3D(Auxiliary_SpeedColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->speed, AuxColor3D(Auxiliary_SpeedColor3D));
		}
		// 辅助显示受力
		if (Auxiliary_ForceBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_ForceColor3D),
					 glm::dvec3(i->pos + i->force), AuxColor3D(Auxiliary_ForceColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->force, AuxColor3D(Auxiliary_ForceColor3D));
		}
	}

	void DrawLineAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsLine3D *i)
	{
		// 辅助显示位置
		if (Auxiliary_PosBool3D)
			av->Spot(glm::dvec3(i->pos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_PosColor3D));
		// 辅助显示旧位置
		if (Auxiliary_OldPosBool3D)
			av->Spot(glm::dvec3(i->OldPos), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_OldPosColor3D));
		// 辅助显示角度：2D 画 pos → pos + (radius,0) 旋转 angle；
		// 3D 直接画 pos → 线段的一个端点（由 GetSegment 给出）
		if (Auxiliary_AngleBool3D)
		{
			Vec3_ begin, end;
			i->GetSegment(begin, end);
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_AngleColor3D),
					 glm::dvec3(begin), AuxColor3D(Auxiliary_AngleColor3D));
		}
		// 辅助显示速度
		if (Auxiliary_SpeedBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_SpeedColor3D),
					 glm::dvec3(i->pos + i->speed), AuxColor3D(Auxiliary_SpeedColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->speed, AuxColor3D(Auxiliary_SpeedColor3D));
		}
		// 辅助显示受力
		if (Auxiliary_ForceBool3D)
		{
			av->Line(glm::dvec3(i->pos), AuxColor3D(Auxiliary_ForceColor3D),
					 glm::dvec3(i->pos + i->force), AuxColor3D(Auxiliary_ForceColor3D));
			SpotBothEnds(av, i->pos, i->pos + i->force, AuxColor3D(Auxiliary_ForceColor3D));
		}
	}

	// ─────────────────────────────────────────────────────────────
	// 集合级绘制
	// ─────────────────────────────────────────────────────────────

	void DrawJoints3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world)
	{
		for (auto i : world->PhysicsJointS)
		{
			if (i == nullptr)
				continue;
			if (i->body1 != nullptr)
				av->Line(glm::dvec3(i->body1->pos), {0, 1, 0, 1}, glm::dvec3(i->body1->pos + i->r1), {0, 1, 0, 1});
			if (i->body2 != nullptr)
				av->Line(glm::dvec3(i->body2->pos), {0, 1, 0, 1}, glm::dvec3(i->body2->pos + i->r2), {0, 1, 0, 1});
		}
	}

	void DrawJunctions3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world)
	{
		for (auto j : world->BaseJunctionS)
		{
			if (j == nullptr)
				continue;
			av->Line(glm::dvec3(j->GetA()), {0, 1, 0, 1}, glm::dvec3(j->GetB()), {0, 1, 0, 1});
		}
	}

	void DrawCollisions3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world)
	{
		for (auto &i : world->CollideGroupVector)
		{
			if (i == nullptr)
				continue;
			for (int j = 0; j < i->numContacts; ++j)
			{
				// 碰撞点
				if (Auxiliary_CollisionDropBool3D)
					av->Spot(glm::dvec3(i->contacts[j].position), AuxiliaryMarkSize3D, AuxColor3D(Auxiliary_CollisionDropColor3D));
				// 碰撞点 分离 法向量
				if (Auxiliary_SeparateNormalVectorBool3D)
					av->Line(glm::dvec3(i->contacts[j].position), AuxColor3D(Auxiliary_SeparateNormalVectorColor3D),
							 glm::dvec3(i->contacts[j].position + i->contacts[j].normal), AuxColor3D(Auxiliary_SeparateNormalVectorColor3D));
				// 指向重心
				if (Auxiliary_CollisionDropToCenterOfGravityBool3D)
				{
					av->Line(glm::dvec3(i->contacts[j].position), AuxColor3D(Auxiliary_CollisionDropToCenterOfGravityColor3D),
							 glm::dvec3(i->contacts[j].position - i->contacts[j].r1), AuxColor3D(Auxiliary_CollisionDropToCenterOfGravityColor3D));
					av->Line(glm::dvec3(i->contacts[j].position), AuxColor3D(Auxiliary_CollisionDropToCenterOfGravityColor3D),
							 glm::dvec3(i->contacts[j].position - i->contacts[j].r2), AuxColor3D(Auxiliary_CollisionDropToCenterOfGravityColor3D));
				}
			}
		}
	}

	void DrawTriggers3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world)
	{
		for (const auto &[handle, config] : world->mTrigger.GetConfigs())
		{
			(void)handle;
			// 2D 的触发器是矩形（4 条边）；3D 是 AABB，画 12 条棱的线框
			const Vec3_ min = config.TriggerBounds.Center - config.TriggerBounds.HalfSize;
			const Vec3_ max = config.TriggerBounds.Center + config.TriggerBounds.HalfSize;
			const glm::vec4 color = {1.0f, 0.5f, 0.0f, 1.0f};

			// 底面（z = min.z）
			av->Line(glm::dvec3(min.x, min.y, min.z), color, glm::dvec3(max.x, min.y, min.z), color);
			av->Line(glm::dvec3(max.x, min.y, min.z), color, glm::dvec3(max.x, max.y, min.z), color);
			av->Line(glm::dvec3(max.x, max.y, min.z), color, glm::dvec3(min.x, max.y, min.z), color);
			av->Line(glm::dvec3(min.x, max.y, min.z), color, glm::dvec3(min.x, min.y, min.z), color);
			// 顶面（z = max.z）
			av->Line(glm::dvec3(min.x, min.y, max.z), color, glm::dvec3(max.x, min.y, max.z), color);
			av->Line(glm::dvec3(max.x, min.y, max.z), color, glm::dvec3(max.x, max.y, max.z), color);
			av->Line(glm::dvec3(max.x, max.y, max.z), color, glm::dvec3(min.x, max.y, max.z), color);
			av->Line(glm::dvec3(min.x, max.y, max.z), color, glm::dvec3(min.x, min.y, max.z), color);
			// 4 条竖直棱
			av->Line(glm::dvec3(min.x, min.y, min.z), color, glm::dvec3(min.x, min.y, max.z), color);
			av->Line(glm::dvec3(max.x, min.y, min.z), color, glm::dvec3(max.x, min.y, max.z), color);
			av->Line(glm::dvec3(max.x, max.y, min.z), color, glm::dvec3(max.x, max.y, max.z), color);
			av->Line(glm::dvec3(min.x, max.y, min.z), color, glm::dvec3(min.x, max.y, max.z), color);
		}
	}

	void DrawGridDivided3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world, PhysicsFormwork3D *selected)
	{
		if (!Auxiliary_GridDividedBool3D || selected == nullptr)
			return;

		// GetDividedVision 返回的仍是"每两个 Vec3_ 构成一个盒对角点"的列表
		std::vector<Vec3_> vision = world->mGridSearch.GetDividedVision(selected);
		const glm::vec4 color = AuxColor3D(Auxiliary_GridDividedColor3D);
		for (size_t i = 0; i < (vision.size() / 2); ++i)
		{
			const Vec3_ &A = vision[i * 2];
			const Vec3_ &B = vision[i * 2 + 1];
			// 2D 只画 4 条边构成的矩形；3D 画 12 条棱构成的盒
			av->Line(glm::dvec3(A.x, A.y, A.z), color, glm::dvec3(B.x, A.y, A.z), color);
			av->Line(glm::dvec3(A.x, A.y, A.z), color, glm::dvec3(A.x, B.y, A.z), color);
			av->Line(glm::dvec3(B.x, B.y, A.z), color, glm::dvec3(B.x, A.y, A.z), color);
			av->Line(glm::dvec3(B.x, B.y, A.z), color, glm::dvec3(A.x, B.y, A.z), color);

			av->Line(glm::dvec3(A.x, A.y, B.z), color, glm::dvec3(B.x, A.y, B.z), color);
			av->Line(glm::dvec3(A.x, A.y, B.z), color, glm::dvec3(A.x, B.y, B.z), color);
			av->Line(glm::dvec3(B.x, B.y, B.z), color, glm::dvec3(B.x, A.y, B.z), color);
			av->Line(glm::dvec3(B.x, B.y, B.z), color, glm::dvec3(A.x, B.y, B.z), color);

			av->Line(glm::dvec3(A.x, A.y, A.z), color, glm::dvec3(A.x, A.y, B.z), color);
			av->Line(glm::dvec3(B.x, A.y, A.z), color, glm::dvec3(B.x, A.y, B.z), color);
			av->Line(glm::dvec3(B.x, B.y, A.z), color, glm::dvec3(B.x, B.y, B.z), color);
			av->Line(glm::dvec3(A.x, B.y, A.z), color, glm::dvec3(A.x, B.y, B.z), color);
		}
	}

	void RenderMapOutline3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world)
	{
		MapFormwork3D *mMapFormwork = world->GetMapFormwork();
		if (mMapFormwork == nullptr)
			return;
		UVec3_ mapSize = mMapFormwork->FMGetMapSize();
		if (mapSize.x == 0 || mapSize.y == 0 || mapSize.z == 0)
			return;
		av->ClearStaticLine();

		const glm::vec4 color = {0, 1, 0, 1};
		const Quat_ noRotation = IdentityQuat();

		if (mMapFormwork->FMGetType() == PhysicsObjectEnum::_MapDynamic)
		{
			MapDynamic3D *dynMap = (MapDynamic3D *)(mMapFormwork);

			const int startX = -(int)(dynMap->GetPlatePos().x);
			const int startY = -(int)(dynMap->GetPlatePos().y);
			const int startZ = -(int)(dynMap->GetPlatePos().z);
			const int endX = startX + (int)mapSize.x;
			const int endY = startY + (int)mapSize.y;
			const int endZ = startZ + (int)mapSize.z;

			// 邻格实体判定（带越界保护：板块外视为非实体，画出外边界）
			auto IsEntity = [&](int x, int y, int z) -> bool {
				if (x < startX || y < startY || z < startZ || x >= endX || y >= endY || z >= endZ)
					return false;
				return mMapFormwork->FMGetGridBlock(IVec3_{x, y, z}).Collision;
			};
			for (int x = startX; x < endX; x++)
			{
				for (int y = startY; y < endY; y++)
				{
					for (int z = startZ; z < endZ; z++)
					{
						if (IsEntity(x, y, z))
						{
							// 动态地图格子直接用世界坐标，origin/centreMass 均为 0
							DrawCellOutline3D(av, x, y, z, glm::dvec3(0.0), noRotation, Vec3_(0.0f),
											  color, /*isStatic=*/true, IsEntity);
						}
					}
				}
			}
		}
		else
		{
			const Vec3_ mapCentrality = mMapFormwork->FMGetCentrality();
			const int endX = (int)mapSize.x;
			const int endY = (int)mapSize.y;
			const int endZ = (int)mapSize.z;

			// 邻格实体判定（带越界保护：地图外视为非实体，画出外边界）
			auto IsEntity = [&](int x, int y, int z) -> bool {
				if (x < 0 || y < 0 || z < 0 || x >= endX || y >= endY || z >= endZ)
					return false;
				return mMapFormwork->FMGetGridBlock(IVec3_{x, y, z}).Collision;
			};
			const glm::dvec3 origin = glm::dvec3(-mapCentrality);
			for (int x = 0; x < endX; x++)
			{
				for (int y = 0; y < endY; y++)
				{
					for (int z = 0; z < endZ; z++)
					{
						if (IsEntity(x, y, z))
						{
							// 静态地图世界坐标 = (x,y,z) - mapCentrality；centreMass=0
							DrawCellOutline3D(av, x, y, z, origin, noRotation, Vec3_(0.0f),
											  color, /*isStatic=*/true, IsEntity);
						}
					}
				}
			}
		}
	}

	// ─────────────────────────────────────────────────────────────
	// 高层聚合
	// ─────────────────────────────────────────────────────────────

	void DrawPhysicsWorld3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world,
							bool showAuxiliary, PhysicsFormwork3D *selected,
							const RenderModeState3D *modes)
	{
		// 注意：本函数不管理 av->Begin()/End()，由调用方在录制区间内调用。
		// 这样可以让鼠标拖拽反馈线等其它绘制与本帧物理内容共用同一个录制区间。
		if (av == nullptr || world == nullptr)
			return;

		// 「面」渲染的物体/地形由 PhysicsFaceRenderer3D 画实心三角形，
		// 这里跳过它们的线框本体（否则实心面外还套一层线框，糊成一片）。
		// 辅助信息（速度/受力/碰撞点）仍照画——它是叠加在面之上的调试信息。
		// 判据与 ObjectBodyColor3D 一样用 StaticNum 无关的 SupportsFace：
		// 只有体素网格与球有"体面"，点/线永远走线框。
		auto SkipBody = [modes](PhysicsFormwork3D *obj) -> bool {
			return modes != nullptr &&
				   modes->ObjectMode(obj) == RenderMode3D::Face &&
				   RenderModeState3D::SupportsFace(obj);
		};

		// 查看分配到的网格树位置（仅显示辅助信息时）
		if (showAuxiliary)
			DrawGridDivided3D(av, world, selected);

		// 渲染物理网格
		for (auto i : world->PhysicsShapeS)
		{
			if (i == nullptr)
				continue;
			if (!SkipBody(i))
				DrawShape3D(av, i, ObjectBodyColor3D(i->StaticNum));
			if (showAuxiliary)
				DrawShapeAuxiliary3D(av, i);
		}
		// 渲染物理点
		// （2D 会跳过 IsLiquidParticle 并把液体粒子交给 DrawLiquid；3D 无液体子系统，
		//   该标记字段仍存在但恒为 false，故这里不做跳过。）
		for (auto i : world->PhysicsParticleS)
		{
			if (i == nullptr)
				continue;
			DrawParticle3D(av, i, ObjectBodyColor3D(i->StaticNum));
			if (showAuxiliary)
				DrawParticleAuxiliary3D(av, i);
		}
		// 渲染圆（3D 为线框球；面模式时为实心球，故跳过线框）
		for (auto i : world->PhysicsCircleS)
		{
			if (i == nullptr)
				continue;
			if (!SkipBody(i))
				DrawCircle3D(av, i, ObjectBodyColor3D(i->StaticNum));
			if (showAuxiliary)
				DrawCircleAuxiliary3D(av, i);
		}
		// 渲染线
		for (auto i : world->PhysicsLineS)
		{
			if (i == nullptr)
				continue;
			DrawLine3D(av, i, ObjectBodyColor3D(i->StaticNum));
			if (showAuxiliary)
				DrawLineAuxiliary3D(av, i);
		}

		// 渲染关节 / 连接点
		DrawJoints3D(av, world);
		DrawJunctions3D(av, world);

		// 渲染碰撞信息（仅显示辅助信息时）
		if (showAuxiliary)
			DrawCollisions3D(av, world);

		// 渲染触发器区域
		DrawTriggers3D(av, world);
	}

	void DrawPhysicsWorldFrame3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world,
								 bool showAuxiliary, PhysicsFormwork3D *selected,
								 const RenderModeState3D *modes)
	{
		av->Begin();
		DrawPhysicsWorld3D(av, world, showAuxiliary, selected, modes);
		av->End();
	}

	// ─────────────────────────────────────────────────────────────
	// 物理辅助显示控制器 PhysicsDebugView3D
	// ─────────────────────────────────────────────────────────────

	void PhysicsDebugView3D::setup(VulKan::AuxiliaryVision *a, PhysicsWorld3D *w)
	{
		av = a;
		world = w;
		// 首帧重写静态地图缓冲；动态地图的 lastPlatePos 由 refreshMap() 首次比对时自然触发
		mapDirty = true;
	}

	void PhysicsDebugView3D::refreshMap()
	{
		if (world == nullptr || av == nullptr || !showMap)
			return;

		MapFormwork3D *m = world->GetMapFormwork();
		if (m == nullptr)
			return;

		// 动态地图：板块位置变化即视为脏，需重写静态缓冲
		if (m->FMGetType() == PhysicsObjectEnum::_MapDynamic)
		{
			IVec3_ platePos = ((MapDynamic3D *)m)->GetPlatePos();
			if (platePos != lastPlatePos)
			{
				mapDirty = true;
				lastPlatePos = platePos;
			}
		}
		// 地形走「面」渲染时，实心地形由 PhysicsFaceRenderer3D 画，
		// 这里必须把静态缓冲里的地图轮廓清掉，否则线框会叠在实心面上。
		// mapDirty 保持 true：一旦切回线框，下一帧立刻重建轮廓。
		if (modes != nullptr && modes->TerrainMode() == RenderMode3D::Face)
		{
			av->ClearStaticLine();
			mapDirty = true;
			return;
		}

		if (mapDirty)
		{
			RenderMapOutline3D(av, world);
			mapDirty = false;
		}
	}

	void PhysicsDebugView3D::drawWorld()
	{
		if (av == nullptr || world == nullptr)
			return;
		DrawPhysicsWorld3D(av, world, showAuxiliary, selected, modes);
	}

	void PhysicsDebugView3D::render()
	{
		refreshMap();
		if (av == nullptr || world == nullptr)
			return;
		av->Begin();
		drawWorld();
		av->End();
	}

	void PhysicsDebugView3D::drawUI(const char *windowTitle)
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

} // namespace PhysicsBlock3D
