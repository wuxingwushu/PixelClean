// ═══════════════════════════════════════════════════════════════════════════
// PhysicsDemo3D.cpp —— 3D 物理沙盒的 24 个演示场景
// 逐条改写自 GameMods/PhysicsTest/PhysicsDemo.cpp（2D 版，1766 行）。
//
// 约定（与 2D 一致）：
//   · 每个 demo 自己 delete 旧世界、new 新世界 —— `PhysicsWorld3D` 析构会
//     级联删除地图、液体与全部挂入的物体，所以场景里 new 出来的对象
//     **一律不要再自己 delete**；
//   · 相机由 `ApplyCameraView3D` 摆到自由飞行视角（yaw/pitch 单位=度，
//     front=(cos p·sin y, sin p, cos p·cos y)，与 GameMods/BlockWorld 一致）；
//   · demo 函数体内不做任何渲染/ImGui 调用 —— 有无头测试直接驱动它们。
//
// 世界坐标约定（与 2D demo 的居中传统一致）：静态地图 centrality 设为
// (size/2, size/2, size/2)，本地体素 0..size 映射到世界 -size/2..+size/2，
// 于是地面顶面在世界 y ≈ -size/2 + 1，场景物体摆放在 y ∈ (-size/2, +size/2)。
// ═══════════════════════════════════════════════════════════════════════════

#include "PhysicsDemo3D.h"
#include "../../PhysicsBlock3D/BaseCalculate3D.hpp"
#include "../../PhysicsBlock3D/MapDynamic3D.hpp"
#include "../../PhysicsBlock3D/PhysicsJoint3D.hpp"
#include "../../PhysicsBlock3D/PhysicsJunction3D.hpp"
#include "../../PhysicsBlock3D/PhysicsAssembly3D.hpp"
#include "../../PhysicsBlock3D/PhysicsCollision3D.hpp"
#include "../../PhysicsBlock3D/PhysicsKinematic3D.hpp"
#include "../../PhysicsBlock3D/PhysicsTrigger3D.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>

namespace PhysicsBlock3D
{
	// ─────────────────────────────────────────────────────────────────────
	// 工具函数
	// ─────────────────────────────────────────────────────────────────────

	void ApplyCameraView3D(Camera *mCamera, const CameraView3D &view)
	{
		if (mCamera == nullptr)
		{
			return;
		}
		const FLOAT_ pitchRad = glm::radians((float)view.Pitch);
		const FLOAT_ yawRad = glm::radians((float)view.Yaw);
		Vec3_ front;
		front.x = cos(pitchRad) * sin(yawRad);
		front.y = sin(pitchRad);
		front.z = cos(pitchRad) * cos(yawRad);
		mCamera->lookAt(glm::vec3(view.Pos.x, view.Pos.y, view.Pos.z),
		                glm::vec3(front.x, front.y, front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		mCamera->update();
	}

	CameraView3D MakeOrbitView3D(Vec3_ center, FLOAT_ radius)
	{
		// 方位 45°、俯角 -25°，把场景中心收进视野
		CameraView3D view;
		const FLOAT_ yaw = glm::radians(45.0f);
		const FLOAT_ pitch = glm::radians(-25.0f);
		Vec3_ dir(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw));
		view.Pos = center - dir * radius; // 视线沿 dir，从反方向绕过去
		view.Yaw = 45.0f;
		view.Pitch = -25.0f;
		return view;
	}

	MapStatic3D *BuildBoxMap3D(int size, int height)
	{
		MapStatic3D *map = new MapStatic3D((unsigned int)size, (unsigned int)size, (unsigned int)size);
		for (int x = 0; x < size; ++x)
			for (int y = 0; y < size; ++y)
				for (int z = 0; z < size; ++z)
				{
					GridBlock3D &b = map->at(x, y, z);
					b.Entity = false;
					b.Collision = false;
					b.mass = 1.0f;
					b.Healthpoint = 24;
				}
		// 底面整层
		for (int x = 0; x < size; ++x)
			for (int z = 0; z < size; ++z)
			{
				map->at(x, 0, z).Entity = true;
				map->at(x, 0, z).Collision = true;
			}
		// 四周一圈墙（高 height 层）
		for (int y = 1; y <= height && y < size; ++y)
			for (int i = 0; i < size; ++i)
			{
				map->at(0, y, i).Entity = true;
				map->at(0, y, i).Collision = true;
				map->at(size - 1, y, i).Entity = true;
				map->at(size - 1, y, i).Collision = true;
				map->at(i, y, 0).Entity = true;
				map->at(i, y, 0).Collision = true;
				map->at(i, y, size - 1).Entity = true;
				map->at(i, y, size - 1).Collision = true;
			}
		map->SetCentrality(Vec3_(size / 2.0f, size / 2.0f, size / 2.0f));
		return map;
	}

	MapStatic3D *BuildFloorMap3D(int size)
	{
		MapStatic3D *map = new MapStatic3D((unsigned int)size, (unsigned int)size, (unsigned int)size);
		for (int x = 0; x < size; ++x)
			for (int z = 0; z < size; ++z)
			{
				map->at(x, 0, z).Entity = true;
				map->at(x, 0, z).Collision = true;
				map->at(x, 0, z).mass = 1.0f;
				map->at(x, 0, z).Healthpoint = 24;
			}
		map->SetCentrality(Vec3_(size / 2.0f, size / 2.0f, size / 2.0f));
		return map;
	}

	PhysicsShape3D *MakeSolidShape3D(Vec3_ pos, int w, int h, int d, FLOAT_ mass, FLOAT_ friction)
	{
		PhysicsShape3D *shape = new PhysicsShape3D(pos, IVec3_(w, h, d));
		for (int x = 0; x < w; ++x)
			for (int y = 0; y < h; ++y)
				for (int z = 0; z < d; ++z)
				{
					GridBlock3D &b = shape->at(x, y, z);
					b.Entity = true;
					b.Collision = true;
					b.mass = mass;
					b.FrictionFactor = friction;
				}
		shape->UpdateAll();
		return shape;
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 0 外骨骼计算：躯干 + 四肢，用关节连成骨架
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D0(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 6));

		PhysicsShape3D *torso = MakeSolidShape3D(Vec3_(0.0f, 4.0f, 0.0f), 2, 3, 1);
		PhysicsShape3D *armL = MakeSolidShape3D(Vec3_(-2.6f, 4.0f, 0.0f), 1, 3, 1);
		PhysicsShape3D *armR = MakeSolidShape3D(Vec3_(2.6f, 4.0f, 0.0f), 1, 3, 1);
		PhysicsShape3D *legL = MakeSolidShape3D(Vec3_(-0.7f, 0.4f, 0.0f), 1, 3, 1);
		PhysicsShape3D *legR = MakeSolidShape3D(Vec3_(0.7f, 0.4f, 0.0f), 1, 3, 1);
		// 躯干钉死当"吊架"（kinematic），四肢靠关节挂着 —— 演示多关节链的 3D 摆动
		(*myPhysicsWorld)->GetKinematic().SetIsKinematic(torso, true);
		(*myPhysicsWorld)->AddObject(torso);
		(*myPhysicsWorld)->AddObject(armL);
		(*myPhysicsWorld)->AddObject(armR);
		(*myPhysicsWorld)->AddObject(legL);
		(*myPhysicsWorld)->AddObject(legR);

		auto link = [&](PhysicsAngle3D *a, PhysicsAngle3D *b, const Vec3_ &anchor)
		{
			PhysicsJoint3D *j = new PhysicsJoint3D();
			j->Set(a, b, anchor);
			(*myPhysicsWorld)->AddObject(j);
		};
		link(torso, armL, Vec3_(-1.5f, 5.0f, 0.0f));
		link(torso, armR, Vec3_(1.5f, 5.0f, 0.0f));
		link(torso, legL, Vec3_(-0.7f, 2.2f, 0.0f));
		link(torso, legR, Vec3_(0.7f, 2.2f, 0.0f));

		// 给四肢一点初速度，骨架会转着摆起来
		armL->speed = Vec3_(0.0f, 0.0f, 3.0f);
		armR->speed = Vec3_(0.0f, 0.0f, -3.0f);

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 3.0f, 0.0f), 28.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 1 几何重心：三个偏心质量方块斜放，看它们倒向不同一侧
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D1(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 4));

		// 质量只堆在 +x 一侧 → 质心偏移 → 释放后会向重的一侧翻滚
		PhysicsShape3D *biased = new PhysicsShape3D(Vec3_(-6.0f, -6.0f, 6.0f), IVec3_(3, 3, 3));
		for (int x = 0; x < 3; ++x)
			for (int y = 0; y < 3; ++y)
				for (int z = 0; z < 3; ++z)
				{
					GridBlock3D &b = biased->at(x, y, z);
					b.Entity = true;
					b.Collision = true;
					b.mass = (x == 2) ? 5.0f : 0.2f;
				}
		biased->UpdateAll();
		biased->rotation = glm::angleAxis(glm::radians(20.0f), Vec3_(0.0f, 0.0f, 1.0f));

		// 质量只堆在顶部 → 高重心，站不稳
		PhysicsShape3D *toppy = new PhysicsShape3D(Vec3_(0.0f, -6.0f, 0.0f), IVec3_(3, 3, 3));
		for (int x = 0; x < 3; ++x)
			for (int y = 0; y < 3; ++y)
				for (int z = 0; z < 3; ++z)
				{
					GridBlock3D &b = toppy->at(x, y, z);
					b.Entity = true;
					b.Collision = true;
					b.mass = (y == 2) ? 5.0f : 0.2f;
				}
		toppy->UpdateAll();

		// 均匀质量作对照
		PhysicsShape3D *even = MakeSolidShape3D(Vec3_(6.0f, -6.0f, -6.0f), 3, 3, 3);
		even->rotation = glm::angleAxis(glm::radians(45.0f), Vec3_(1.0f, 0.0f, 0.0f));

		(*myPhysicsWorld)->AddObject(biased);
		(*myPhysicsWorld)->AddObject(toppy);
		(*myPhysicsWorld)->AddObject(even);

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -6.0f, 0.0f), 26.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 2 方块堆叠（2D 原版：10 个 2×2 错位落下 → 3D：沿 Z 也错位）
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D2(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 12));

		// 与 2D 原版同参数：±0.1 微错位、无初始旋转、底块贴着地面（不悬空下落）。
		// 地面顶面在 y=-15，2×2×2 的体素原点取 -15 时质心在 -14，底面正好贴地。
		for (int j = 0; j < 10; ++j)
		{
			PhysicsShape3D *box = MakeSolidShape3D(
			    Vec3_(Random(-0.1f, 0.1f), -15.0f + j * 2.1f, Random(-0.1f, 0.1f)), 2, 2, 2);
			(*myPhysicsWorld)->AddObject(box);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -4.0f, 0.0f), 24.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 3 方块堆叠金字塔：真三维 5 层金字塔（n×n 逐层收窄）
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D3(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(48, 16));

		const int base = 5; // 底层 5×5
		for (int layer = 0; layer < base; ++layer)
		{
			const int n = base - layer;
			const FLOAT_ offset = (base - 1) * 1.05f - (n - 1) * 1.05f; // 居中对齐
			for (int x = 0; x < n; ++x)
				for (int z = 0; z < n; ++z)
				{
					PhysicsShape3D *block = MakeSolidShape3D(
					    Vec3_(x * 2.1f - offset, -15.0f + layer * 2.1f, z * 2.1f - offset), 2, 2, 2);
					(*myPhysicsWorld)->AddObject(block);
				}
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -10.0f, 0.0f), 32.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 4 点和方块：一束粒子砸方块堆
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D4(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 10));

		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
			{
				(*myPhysicsWorld)->AddObject(MakeSolidShape3D(
				    Vec3_((i - 1.5f) * 2.2f, -10.0f, (j - 1.5f) * 2.2f), 2, 2, 2));
			}

		// 高速粒子束（无转动自由度，纯动量传递）
		for (int k = 0; k < 6; ++k)
		{
			PhysicsParticle3D *p = new PhysicsParticle3D(
			    Vec3_((k % 3 - 1) * 1.5f, 6.0f, (k / 3 - 0.5f) * 1.5f), 8.0f, 0.2f);
			p->speed = Vec3_(0.0f, -35.0f, 0.0f);
			(*myPhysicsWorld)->AddObject(p);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -6.0f, 0.0f), 26.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 5 关节：两个方块被一个关节栓住，重心失衡后绕锚点翻摆
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D5(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 8));

		PhysicsShape3D *big = MakeSolidShape3D(Vec3_(0.0f, -4.0f, 0.0f), 2, 2, 2);
		PhysicsShape3D *small = MakeSolidShape3D(Vec3_(5.0f, 2.0f, 0.0f), 1, 1, 1);
		(*myPhysicsWorld)->AddObject(big);
		(*myPhysicsWorld)->AddObject(small);

		PhysicsJoint3D *joint = new PhysicsJoint3D();
		joint->Set(big, small, Vec3_(2.5f, -1.0f, 0.0f));
		(*myPhysicsWorld)->AddObject(joint);

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(1.0f, -2.0f, 0.0f), 22.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 6 铁链：10 节粒子绳悬垂 + 底端挂重物
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D6(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 4));

		const FLOAT_ link = 1.4f;
		const Vec3_ ceiling(0.0f, 10.0f, 0.0f);
		// 初始整条链沿 50° 斜线拉直：每节间距恰好 = link，重力下像单摆甩向竖直
		const FLOAT_ theta = 0.9f; // rad，偏离竖直方向约 51°
		const Vec3_ dir(sin(theta), -cos(theta), 0.0f);
		PhysicsParticle3D *prev = nullptr;
		for (int i = 0; i < 10; ++i)
		{
			Vec3_ pos = ceiling + dir * (link * (i + 1));
			PhysicsParticle3D *p = new PhysicsParticle3D(pos, 1.0f, 0.2f);
			(*myPhysicsWorld)->AddObject(p);
			if (i == 0)
			{
				// 首节钉死在天花板（Length = |p - ceiling| = link）
				(*myPhysicsWorld)->AddObject(new PhysicsJunctionP3D(p, ceiling, cord3D));
			}
			else
			{
				(*myPhysicsWorld)->AddObject(new PhysicsJunctionPP3D(prev, p, cord3D));
			}
			prev = p;
		}
		// 底端挂一个重方块（绳末端自然垂着）
		PhysicsShape3D *weight = MakeSolidShape3D(prev->pos + Vec3_(0.6f, -2.2f, 0.0f), 2, 2, 2);
		(*myPhysicsWorld)->AddObject(weight);
		(*myPhysicsWorld)->AddObject(new PhysicsJunctionS3D(
		    weight, Vec3_(0.0f, 1.0f, 0.0f), prev->pos, cord3D));

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 0.0f, 0.0f), 26.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 7 杠杆连接：一根长条绕固定支点转，两端各挂不同重量
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D7(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 6));

		// 长条：9×1×1 体素，质心在几何中心
		PhysicsShape3D *plank = MakeSolidShape3D(Vec3_(0.0f, 0.0f, 0.0f), 9, 1, 1);
		(*myPhysicsWorld)->AddObject(plank);
		// lever3D = 刚体连接：把长条的 (0,0,0) 局部点钉死在支点世界坐标
		(*myPhysicsWorld)->AddObject(new PhysicsJunctionS3D(
		    plank, Vec3_(0.0f, 0.0f, 0.0f), Vec3_(0.0f, 1.0f, 0.0f), lever3D));

		PhysicsShape3D *heavy = MakeSolidShape3D(Vec3_(4.0f, 1.0f, 0.0f), 2, 2, 2);
		heavy->mass = 12.0f;
		heavy->invMass = 1.0f / heavy->mass;
		(*myPhysicsWorld)->AddObject(heavy);
		(*myPhysicsWorld)->AddObject(new PhysicsJunctionS3D(
		    heavy, Vec3_(0.0f, 1.0f, 0.0f), Vec3_(4.0f, 0.5f, 0.0f), cord3D));

		PhysicsShape3D *light = MakeSolidShape3D(Vec3_(-4.0f, 1.0f, 0.0f), 1, 1, 1);
		(*myPhysicsWorld)->AddObject(light);
		(*myPhysicsWorld)->AddObject(new PhysicsJunctionS3D(
		    light, Vec3_(0.0f, 0.5f, 0.0f), Vec3_(-4.0f, 0.5f, 0.0f), cord3D));

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 0.0f, 0.0f), 20.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 8 橡皮筋受力：两个球被橡皮筋拉向彼此，绕共同质心旋转
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D8(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 2));

		// Junction 的静止长度在**构造瞬间**由两端距离定死：
		// 先在相距 4 处建绳，再把球挪到 ±5 —— 拉开的 6 就是回拉力
		PhysicsCircle3D *a = new PhysicsCircle3D(Vec3_(-2.0f, 6.0f, 0.0f), 1.0f, 2.0f, 0.2f);
		PhysicsCircle3D *b = new PhysicsCircle3D(Vec3_(2.0f, 6.0f, 0.0f), 1.0f, 2.0f, 0.2f);
		(*myPhysicsWorld)->AddObject(a);
		(*myPhysicsWorld)->AddObject(b);
		PhysicsJunctionSS3D *band = new PhysicsJunctionSS3D(a, Vec3_(0.0f), b, Vec3_(0.0f), rubber3D);
		(*myPhysicsWorld)->AddObject(band);
		a->pos = Vec3_(-5.0f, 6.0f, 0.0f);
		b->pos = Vec3_(5.0f, 6.0f, 0.0f);
		a->speed = Vec3_(0.0f, 0.0f, 6.0f);
		b->speed = Vec3_(0.0f, 0.0f, -6.0f);

		// 另一组：弹簧（会稳定在平衡长度而不是松弛长度）
		PhysicsCircle3D *c = new PhysicsCircle3D(Vec3_(-5.0f, 0.0f, 0.0f), 0.8f, 2.0f, 0.2f);
		PhysicsCircle3D *d = new PhysicsCircle3D(Vec3_(5.0f, 0.0f, 0.0f), 0.8f, 2.0f, 0.2f);
		(*myPhysicsWorld)->AddObject(c);
		(*myPhysicsWorld)->AddObject(d);
		(*myPhysicsWorld)->AddObject(new PhysicsJunctionSS3D(c, Vec3_(0.0f), d, Vec3_(0.0f), spring3D));

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 3.0f, 0.0f), 22.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 9 绳子与地形碰撞：一条长绳拖在地形台阶上
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D9(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);

		// 地形：地面 + 一道横贯 Z 的台阶墙
		MapStatic3D *map = new MapStatic3D(32, 32, 32);
		for (int x = 0; x < 32; ++x)
			for (int z = 0; z < 32; ++z)
			{
				map->at(x, 0, z).Entity = true;
				map->at(x, 0, z).Collision = true;
				if (x >= 18 && x <= 21)
				{
					map->at(x, 1, z).Entity = true;
					map->at(x, 1, z).Collision = true;
					map->at(x, 2, z).Entity = true;
					map->at(x, 2, z).Collision = true;
				}
			}
		map->SetCentrality(Vec3_(16.0f, 16.0f, 16.0f));
		(*myPhysicsWorld)->SetMapFormwork(map);

		// 被拖的终端：一个重球，给它一个 +X 初速度，绳子从台阶棱上刮过去
		PhysicsCircle3D *puller = new PhysicsCircle3D(Vec3_(8.0f, -12.0f, 0.0f), 1.2f, 6.0f, 0.4f);
		puller->speed = Vec3_(12.0f, 1.5f, 0.0f);
		(*myPhysicsWorld)->AddObject(puller);

		const FLOAT_ link = 1.2f;
		PhysicsParticle3D *prev = puller;
		for (int i = 0; i < 14; ++i)
		{
			PhysicsParticle3D *p = new PhysicsParticle3D(
			    Vec3_(8.0f - (i + 1) * link * 0.7f, -10.5f + i * link * 0.5f, (i % 3 - 1) * 0.4f),
			    1.0f, 0.3f);
			(*myPhysicsWorld)->AddObject(p);
			(*myPhysicsWorld)->AddObject(new PhysicsJunctionPP3D(prev, p, cord3D));
			prev = p;
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(2.0f, -12.0f, 0.0f), 26.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 10 球：不同半径/质量的球同时落下，看反弹与滚动差异
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D10(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		// 地面留一点摩擦，球落下后会向前滚
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 8));

		const FLOAT_ radii[4] = {0.6f, 1.0f, 1.5f, 2.0f};
		for (int i = 0; i < 4; ++i)
		{
			PhysicsCircle3D *ball = new PhysicsCircle3D(
			    Vec3_((i - 1.5f) * 5.0f, -6.0f + i * 2.0f, 0.0f), radii[i], 1.0f + radii[i], 0.3f);
			ball->speed = Vec3_(3.0f, 0.0f, 0.0f);
			ball->angleSpeed = Vec3_(0.0f, 0.0f, -4.0f); // 绕 Z 的前旋
			(*myPhysicsWorld)->AddObject(ball);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -8.0f, 0.0f), 24.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 11 球压力测试：4×4×4=64 球阵列落下堆叠（验证宽相+热启动）
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D11(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(48, 16));

		const FLOAT_ spacing = 2.05f;
		for (int x = 0; x < 4; ++x)
			for (int y = 0; y < 4; ++y)
				for (int z = 0; z < 4; ++z)
				{
					PhysicsCircle3D *ball = new PhysicsCircle3D(
					    Vec3_((x - 1.5f) * spacing, -18.0f + (y + 4) * spacing, (z - 1.5f) * spacing),
					    1.0f, 1.0f, 0.4f);
					(*myPhysicsWorld)->AddObject(ball);
				}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -12.0f, 0.0f), 34.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 12 绳子牛顿摆：5 个绳悬粒子摆球排成一线，拉起一端释放
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D12(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildFloorMap3D(32));

		// PhysicsJunctionP3D(particle, RegularDrop=固定点世界坐标, type)：
		// 静止长度在构造瞬间取 |particle->pos - RegularDrop|，
		// 所以摆球要**初始摆在以固定点为心、rope 为半径的弧线上**。
		const FLOAT_ rope = 6.0f;
		const FLOAT_ pivotY = 9.0f;
		for (int i = 0; i < 5; ++i)
		{
			const FLOAT_ hangX = (i - 2.0f) * 1.35f; // 静止间距略小于球直径（r≈0.65），几乎相切
			const FLOAT_ theta = (i == 4) ? 0.5f : 0.0f; // 拉起最外侧一端 ~29°
			Vec3_ start(hangX + rope * sin(theta), pivotY - rope * cos(theta), 0.0f);
			PhysicsParticle3D *ball = new PhysicsParticle3D(start, 2.0f, 0.2f);
			(*myPhysicsWorld)->AddObject(ball);
			(*myPhysicsWorld)->AddObject(new PhysicsJunctionP3D(
			    ball, Vec3_(hangX, pivotY, 0.0f), cord3D));
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 4.0f, 0.0f), 22.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 13 关节牛顿摆：同 12，但用 Joint（刚性锚点，可推可拉）+ 方块摆球
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D13(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildFloorMap3D(32));

		// 横梁：kinematic 钉死的参考物（2D 版用 MapStatic 顶墙，3D 用运动学更直观）
		PhysicsShape3D *bar = MakeSolidShape3D(Vec3_(0.0f, 9.5f, 0.0f), 9, 1, 1);
		(*myPhysicsWorld)->AddObject(bar);
		(*myPhysicsWorld)->GetKinematic().SetIsKinematic(bar, true);

		const FLOAT_ rope = 6.0f;
		for (int i = 0; i < 5; ++i)
		{
			const FLOAT_ x = (i - 2.0f) * 1.05f; // 1³ 方块几乎相切，才能链式传冲量
			const FLOAT_ lift = (i == 0) ? -4.5f : 0.0f;
			PhysicsShape3D *bob = MakeSolidShape3D(Vec3_(x + lift, 9.0f - rope, 0.0f), 1, 1, 1);
			(*myPhysicsWorld)->AddObject(bob);
			// 锚点定在悬挂原点（静止弧线最低点正上方 rope 处）：lift 摆开的一侧
			// 被刚性拉回，摆球以锚点为球心做球面摆动
			PhysicsJoint3D *j = new PhysicsJoint3D();
			j->Set(bob, bar, Vec3_(x, 9.0f - rope, 0.0f));
			(*myPhysicsWorld)->AddObject(j);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 4.0f, 0.0f), 22.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 14 摩擦力：三种摩擦因数的球/方块在同一条缓坡上滑出三种距离
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D14(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);

		// 阶梯缓坡：x 每前进 4 格降 1（≈14°）。体素引擎画不出真斜面，
		// 台阶滚落正是它区别于 2D 平滑斜坡的看点。
		MapStatic3D *map = new MapStatic3D(48, 48, 48);
		for (int x = 0; x < 48; ++x)
			for (int z = 0; z < 48; ++z)
				for (int y = 0; y < 48; ++y)
				{
					GridBlock3D &b = map->at(x, y, z);
					b.mass = 1.0f;
					b.Healthpoint = 24;
					b.Entity = false;
					b.Collision = false;
				}
		for (int x = 0; x < 48; ++x)
		{
			const int surfaceY = 30 - x / 4;
			for (int z = 0; z < 48; ++z)
				for (int y = 0; y <= surfaceY; ++y)
				{
					map->at(x, y, z).Entity = true;
					map->at(x, y, z).Collision = true;
				}
		}
		map->SetCentrality(Vec3_(24.0f, 24.0f, 24.0f));
		(*myPhysicsWorld)->SetMapFormwork(map);

		const FLOAT_ mu[3] = {0.02f, 0.3f, 0.8f};
		for (int i = 0; i < 3; ++i)
		{
			PhysicsCircle3D *ball = new PhysicsCircle3D(
			    Vec3_(-15.0f, 10.5f, (i - 1.0f) * 4.0f), 1.0f, 2.0f, mu[i]);
			(*myPhysicsWorld)->AddObject(ball);
			// 方块组：低摩擦方块在台阶上"卡"与"滑"的差别更夸张
			(*myPhysicsWorld)->AddObject(MakeSolidShape3D(
			    Vec3_(-15.0f, 11.5f, (i - 1.0f) * 4.0f + 7.0f), 1, 1, 1, 1.0f, mu[i]));
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(-6.0f, 8.0f, 0.0f), 30.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 15 动态网格搜索测试：200 粒子高速乱飞，逼迫格网频繁挪动/重建
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D15(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(48, 46)); // 高墙，粒子飞不出去

		for (int i = 0; i < 200; ++i)
		{
			PhysicsParticle3D *p = new PhysicsParticle3D(
			    Vec3_(Random(-20.0f, 20.0f), Random(-10.0f, 15.0f), Random(-20.0f, 20.0f)),
			    1.0f, 0.2f);
			// 速度拉高：粒子在盒子里来回高速弹跳，宽相网格每帧都在挪动。
			// 竖直分量限幅 [-20,10]：峰值不过墙顶（BuildBoxMap3D 无天花板，
			// vy>14 左右就会抛出围墙变成自由落体，那就不是测网格而是测逃逸了）
			p->speed = Vec3_(Random(-40.0f, 40.0f), Random(-20.0f, 10.0f), Random(-40.0f, 40.0f));
			(*myPhysicsWorld)->AddObject(p);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 0.0f, 0.0f), 44.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 16 物理线段（胶囊）：六根不同厚度胶囊带着自旋斜落
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D16(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 10));

		// PhysicsLine3D(begin, end, mass, friction, thickness)
		for (int i = 0; i < 6; ++i)
		{
			const FLOAT_ ax = (i % 3 - 1.0f) * 6.0f;
			const FLOAT_ az = (i / 3 - 0.5f) * 5.0f;
			const Vec3_ begin(ax - 2.0f, 2.0f, az);
			const Vec3_ end(ax + 2.0f, 3.5f, az);
			PhysicsLine3D *rod = new PhysicsLine3D(begin, end, 3.0f, 0.3f, 0.15f + 0.12f * i);
			rod->angleSpeed = Vec3_(Random(-3.0f, 3.0f), Random(-3.0f, 3.0f), Random(-3.0f, 3.0f));
			(*myPhysicsWorld)->AddObject(rod);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -8.0f, 0.0f), 24.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 17 线段绳子：粒子结 + 胶囊杆交替，一条斜拉悬链
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D17(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 4));

		// 固定端：kinematic 粒子（不受重力、位置钉死）
		PhysicsParticle3D *anchor = new PhysicsParticle3D(Vec3_(-6.0f, 10.0f, 0.0f), 1.0f, 0.2f);
		(*myPhysicsWorld)->AddObject(anchor);
		(*myPhysicsWorld)->GetKinematic().SetIsKinematic(anchor, true);

		PhysicsParticle3D *prev = anchor;
		const FLOAT_ seg = 2.4f;
		for (int i = 0; i < 5; ++i)
		{
			const Vec3_ knot(-6.0f + (i + 1) * seg, 10.0f - (i + 1) * seg * 0.5f, (i % 2) * 0.6f);
			PhysicsParticle3D *p = new PhysicsParticle3D(knot, 1.5f, 0.2f);
			(*myPhysicsWorld)->AddObject(p);
			(*myPhysicsWorld)->AddObject(new PhysicsJunctionPP3D(prev, p, cord3D));
			// 装饰杆只是自由落体的胶囊（与绳链同起点）：看"杆各自摔落、
			// 绳链成悬垂曲线"的对比
			(*myPhysicsWorld)->AddObject(new PhysicsLine3D(prev->pos, knot, 1.0f, 0.3f, 0.08f));
			prev = p;
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, 4.0f, 0.0f), 24.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 18 组合测试：四类物体 + 关节 + 连接 + 组装体同场
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D18(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(48, 20));

		// 组装体：三球两两相贴成一簇 —— 同组装体成员互不碰撞，整体一起滚
		PhysicsAssembly3D *cluster = new PhysicsAssembly3D();
		cluster->Add(new PhysicsCircle3D(Vec3_(-8.0f, 4.0f, 0.0f), 1.0f, 1.0f, 0.4f));
		cluster->Add(new PhysicsCircle3D(Vec3_(-6.0f, 4.0f, 0.0f), 1.0f, 1.0f, 0.4f));
		cluster->Add(new PhysicsCircle3D(Vec3_(-7.0f, 5.7f, 0.0f), 1.0f, 1.0f, 0.4f));
		(*myPhysicsWorld)->AddObject(cluster);

		// 方块塔 + 一根斜靠的胶囊
		for (int i = 0; i < 5; ++i)
		{
			(*myPhysicsWorld)->AddObject(MakeSolidShape3D(
			    Vec3_(4.0f, -21.0f + i * 2.1f, 4.0f), 2, 2, 2));
		}
		(*myPhysicsWorld)->AddObject(new PhysicsLine3D(
		    Vec3_(6.0f, -22.0f, 6.0f), Vec3_(9.0f, -15.0f, 6.0f), 4.0f, 0.4f, 0.2f));

		// 关节连接的球对（高空落下后继续被拴在一起绕共同质心翻滚）
		PhysicsCircle3D *j1 = new PhysicsCircle3D(Vec3_(12.0f, 2.0f, -8.0f), 1.2f, 2.0f, 0.3f);
		PhysicsCircle3D *j2 = new PhysicsCircle3D(Vec3_(15.0f, 6.0f, -8.0f), 0.8f, 1.0f, 0.3f);
		(*myPhysicsWorld)->AddObject(j1);
		(*myPhysicsWorld)->AddObject(j2);
		PhysicsJoint3D *pair = new PhysicsJoint3D();
		pair->Set(j1, j2, Vec3_(13.5f, 4.0f, -8.0f));
		(*myPhysicsWorld)->AddObject(pair);

		// 高速粒子穿场
		PhysicsParticle3D *p = new PhysicsParticle3D(Vec3_(-2.0f, 8.0f, -4.0f), 3.0f, 0.2f);
		p->speed = Vec3_(10.0f, -6.0f, 2.0f);
		(*myPhysicsWorld)->AddObject(p);

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -10.0f, 0.0f), 36.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 19 碰撞回调：对撞球 + 落地方块，Enter/Exit 打进 stdout
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D19(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		MapStatic3D *map = BuildBoxMap3D(32, 12);
		(*myPhysicsWorld)->SetMapFormwork(map);

		PhysicsCircle3D *a = new PhysicsCircle3D(Vec3_(-8.0f, -6.0f, 0.0f), 1.2f, 3.0f, 0.2f);
		PhysicsCircle3D *b = new PhysicsCircle3D(Vec3_(8.0f, -6.0f, 0.0f), 1.2f, 1.0f, 0.2f);
		a->speed = Vec3_(14.0f, 0.0f, 0.0f);
		b->speed = Vec3_(-6.0f, 0.0f, 2.0f);
		(*myPhysicsWorld)->AddObject(a);
		(*myPhysicsWorld)->AddObject(b);

		// 物体↔物体：AddCollision*Listener 挂在物体上
		(*myPhysicsWorld)->GetCollision().AddCollisionEnterListener(
		    a, [](const PhysicsFormwork3D *, const PhysicsFormwork3D *other, const BaseArbiter3D *)
		    { printf("[Collision3D] Enter: 球A 撞上物体 %p\n", (const void *)other); });
		(*myPhysicsWorld)->GetCollision().AddCollisionExitListener(
		    a, [](const PhysicsFormwork3D *, const PhysicsFormwork3D *other, const BaseArbiter3D *)
		    { printf("[Collision3D] Exit : 球A 脱离物体 %p\n", (const void *)other); });

		// 物体↔地图：必须走 AddMapCollision*Listener（挂在地图上）——2D 同款分裂语义
		(*myPhysicsWorld)->GetCollision().AddMapCollisionEnterListener(
		    map, [](const PhysicsFormwork3D *obj, const MapFormwork3D *, const BaseArbiter3D *)
		    { printf("[Collision3D] MapEnter: 物体 %p 触地\n", (const void *)obj); });

		PhysicsShape3D *box = MakeSolidShape3D(Vec3_(0.0f, 2.0f, 0.0f), 2, 2, 2);
		(*myPhysicsWorld)->AddObject(box);

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -6.0f, 0.0f), 26.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 20 运动学物体：平移平台 + 旋转转盘，上面的球被摩擦带着走
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D20(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(48, 20));

		// 平移平台：kinematic 后不受重力，MoveTo 走直线轨迹（2 秒到目标点后停住）
		PhysicsShape3D *platform = MakeSolidShape3D(Vec3_(-6.0f, -12.0f, -4.0f), 6, 1, 6, 1.0f, 0.9f);
		(*myPhysicsWorld)->AddObject(platform);
		(*myPhysicsWorld)->GetKinematic().SetIsKinematic(platform, true);
		(*myPhysicsWorld)->GetKinematic().MoveTo(platform, Vec3_(8.0f, -8.0f, 6.0f), 2.0f);

		// 平台上的三个球 + 一个方块（高摩擦：被平台拖着走）
		for (int i = 0; i < 3; ++i)
		{
			(*myPhysicsWorld)->AddObject(new PhysicsCircle3D(
			    Vec3_(-6.0f + (i - 1.0f) * 1.8f, -10.0f, -4.0f), 0.7f, 1.0f, 0.7f));
		}
		(*myPhysicsWorld)->AddObject(MakeSolidShape3D(Vec3_(-6.0f, -9.0f, -1.0f), 1, 1, 1, 1.0f, 0.9f));

		// 转盘：RotateTo 绕 Y 转 180°（四元数 slerp —— 3D 版新语义）
		PhysicsShape3D *turntable = MakeSolidShape3D(Vec3_(10.0f, -12.0f, -10.0f), 4, 1, 4, 1.0f, 0.9f);
		(*myPhysicsWorld)->AddObject(turntable);
		(*myPhysicsWorld)->GetKinematic().SetIsKinematic(turntable, true);
		(*myPhysicsWorld)->GetKinematic().RotateTo(turntable,
		    glm::angleAxis(3.14159265f, Vec3_(0.0f, 1.0f, 0.0f)), 3.0f);
		(*myPhysicsWorld)->AddObject(new PhysicsCircle3D(
		    Vec3_(8.5f, -10.0f, -10.0f), 0.6f, 1.0f, 0.9f));

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(2.0f, -10.0f, -2.0f), 32.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 21 触发器系统：AABB 触发区 + 层级过滤区，穿过的球打日志
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D21(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 20));

		PhysicsTrigger3D &trg = (*myPhysicsWorld)->GetTrigger();
		const TriggerHandle3D zone = trg.CreateTrigger();
		trg.SetTriggerBounds(zone, Bounds3D(Vec3_(0.0f, -6.0f, 0.0f), Vec3_(3.0f, 3.0f, 3.0f)));
		trg.AddTriggerListener(zone, TriggerEventType::Enter,
		                       [](PhysicsFormwork3D *obj)
		                       { printf("[Trigger3D] Enter 物体 %p 进入触发盒\n", (void *)obj); });
		trg.AddTriggerListener(zone, TriggerEventType::Exit,
		                       [](PhysicsFormwork3D *obj)
		                       { printf("[Trigger3D] Exit  物体 %p 离开触发盒\n", (void *)obj); });

		// 第二个区带层级过滤（3D 修正：2D 的 TriggerLayers 是从未生效的死字段，
		// 3D 通过 SetLayerProvider 真正接上）。球在 default 层、方块在 layer1；
		// gated 只订阅 default → 只有球会打 Gated-Enter。
		const TriggerHandle3D gated = trg.CreateTrigger();
		trg.SetTriggerBounds(gated, Bounds3D(Vec3_(6.0f, -6.0f, 0.0f), Vec3_(2.0f, 6.0f, 2.0f)));
		trg.SetTriggerLayers(gated, LayerMaskDefault);
		trg.SetLayerProvider(
		    [](const PhysicsFormwork3D *obj) -> LayerMask
		    {
			    // PFGetType 非 const（2D 同款），provider 给的是 const 指针 → const_cast
			    PhysicsFormwork3D *m = const_cast<PhysicsFormwork3D *>(obj);
			    return (m && m->PFGetType() == circle) ? LayerMaskDefault : (LayerMaskDefault << 1);
		    });
		trg.AddTriggerListener(gated, TriggerEventType::Enter,
		                       [](PhysicsFormwork3D *obj)
		                       { printf("[Trigger3D] Gated-Enter %p\n", (void *)obj); });

		for (int i = 0; i < 3; ++i)
		{
			PhysicsCircle3D *ball = new PhysicsCircle3D(
			    Vec3_(-10.0f, -6.0f + i * 1.2f, i * 1.0f), 0.6f, 1.0f, 0.2f);
			ball->speed = Vec3_(16.0f, 0.3f, 0.0f);
			(*myPhysicsWorld)->AddObject(ball);
		}
		for (int i = 0; i < 2; ++i)
		{
			PhysicsShape3D *box = MakeSolidShape3D(Vec3_(-10.0f, -4.5f - i * 1.5f, -1.5f), 1, 1, 1);
			box->speed = Vec3_(15.0f, 0.0f, 0.0f);
			(*myPhysicsWorld)->AddObject(box);
		}

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(-2.0f, -5.0f, 0.0f), 28.0f));
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 22 动态地图（无限生成）：3D 值噪声丘陵 + 天降方块/球雨
	// ─────────────────────────────────────────────────────────────────────

	// 简易值噪声（2D 版链接了 PerlinNoise 库；这里自带一个等价物，
	// 避免 demo 拖第三方依赖。平滑插值 + 三线性，足以生成可滚落的丘陵）。
	static float ValueNoise3(float x, float y, float z)
	{
		auto h = [](int i, int j, int k) -> float
		{
			unsigned int n = (unsigned int)(i * 1619 + j * 31337 + k * 6971);
			n = (n << 13u) ^ n;
			return float((n * (n * n * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 1073741823.0f - 1.0f;
		};
		const int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
		const float fx = x - x0, fy = y - y0, fz = z - z0;
		const float sx = fx * fx * (3.0f - 2.0f * fx);
		const float sy = fy * fy * (3.0f - 2.0f * fy);
		const float sz = fz * fz * (3.0f - 2.0f * fz);
		const float n000 = h(x0, y0, z0), n100 = h(x0 + 1, y0, z0);
		const float n010 = h(x0, y0 + 1, z0), n110 = h(x0 + 1, y0 + 1, z0);
		const float n001 = h(x0, y0, z0 + 1), n101 = h(x0 + 1, y0, z0 + 1);
		const float n011 = h(x0, y0 + 1, z0 + 1), n111 = h(x0 + 1, y0 + 1, z0 + 1);
		const float nx00 = n000 + sx * (n100 - n000);
		const float nx10 = n010 + sx * (n110 - n010);
		const float nx01 = n001 + sx * (n101 - n001);
		const float nx11 = n011 + sx * (n111 - n011);
		const float nxy0 = nx00 + sy * (nx10 - nx00);
		const float nxy1 = nx01 + sy * (nx11 - nx01);
		return nxy0 + sz * (nxy1 - nxy0);
	}

	// 板块生成回调：世界重力沿 -y，体素 y 与世界 y **同轴**（at(x,y,z) 的
	// 布局 (x*H+y)*D+z 只是内存排布，不改变坐标轴语义）。地形 = 噪声地表
	// 以下全部实心 → 丘陵。地表中心约在世界体素 y=48（相机/天降物同一坐标系）。
	static void GeneratePlate3(BaseGrid3D **grid, int x, int y, int z, void *data)
	{
		BaseGrid3D *g = *grid;
		const int edge = PixelBlockEdgeSize3D;
		(void)data;
		for (int ix = 0; ix < edge; ++ix)
			for (int iy = 0; iy < edge; ++iy)
				for (int iz = 0; iz < edge; ++iz)
				{
					const float wx = (x * edge + ix) * 0.05f;
					const float wz = (z * edge + iz) * 0.05f;
					const float wyAbs = (float)(y * edge + iy);
					float val = ValueNoise3(wx, wz, 3.7f); // 二维地表噪声
					val = val * 0.5f + 0.5f;               // [0,1]
					// 地表高度 = 48 ± 8·噪声；其下方全部实心
					const float surface = 48.0f + 16.0f * (val - 0.5f);
					const bool solid = (wyAbs <= surface);
					GridBlock3D &b = g->at(ix, iy, iz);
					b.Entity = solid;
					b.Collision = solid;
					b.mass = 1.0f;
					b.Healthpoint = 24;
				}
	}

	static void DeletePlate3(BaseGrid3D **grid, void *data)
	{
		BaseGrid3D *g = *grid;
		const int edge = PixelBlockEdgeSize3D;
		(void)data;
		for (int ix = 0; ix < edge; ++ix)
			for (int iy = 0; iy < edge; ++iy)
				for (int iz = 0; iz < edge; ++iz)
				{
					g->at(ix, iy, iz).Entity = false;
					g->at(ix, iy, iz).Collision = false;
				}
	}

	void PhysicsDemo3D22(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);

		// 6×6×6 个 16³ 板块滚动窗口（体素世界 96³）。观察者初点 = 地表中心 (48,48,48)。
		MapDynamic3D *map = new MapDynamic3D(6, 6, 6);
		map->SetCallback(GeneratePlate3, nullptr, DeletePlate3, nullptr);
		map->SetPos(48.0f, 48.0f, 48.0f);
		map->ALLUpData(48.0f, 48.0f, 48.0f);
		(*myPhysicsWorld)->SetMapFormwork(map);

		// 天降物：落在丘陵上方（世界坐标 = 体素坐标，动态地图 centrality 恒 0）。
		// 地表在 y≈40~56，天降物放在 y=64~76，落差不大不至于超速。
		for (int i = 0; i < 20; ++i)
		{
			(*myPhysicsWorld)->AddObject(MakeSolidShape3D(
			    Vec3_(48.0f + Random(-12.0f, 12.0f), Random(64.0f, 76.0f), 48.0f + Random(-12.0f, 12.0f)),
			    2, 2, 2));
		}
		for (int i = 0; i < 12; ++i)
		{
			PhysicsCircle3D *ball = new PhysicsCircle3D(
			    Vec3_(48.0f + Random(-12.0f, 12.0f), Random(62.0f, 74.0f), 48.0f + Random(-12.0f, 12.0f)),
			    Random(0.5f, 1.5f), 1.0f, 0.3f);
			(*myPhysicsWorld)->AddObject(ball);
		}

		// 从斜上方俯视地表中心
		ApplyCameraView3D(mCamera, CameraView3D{Vec3_(26.0f, 66.0f, 26.0f), 45.0f, -30.0f});
	}

	// ─────────────────────────────────────────────────────────────────────
	// Demo 23 液体模拟（PBF）：池水 + 空中水团砸落 + 一块"船"与一块铁
	// ─────────────────────────────────────────────────────────────────────

	void PhysicsDemo3D23(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsWorld3D(Vec3_(0.0f, -9.8f, 0.0f), false);
		(*myPhysicsWorld)->SetMapFormwork(BuildBoxMap3D(32, 20));

		// AddGrid 由 spacing 反推粒子半径 r = spacing/1.6 并同步 h/ρ0（空池时才自动同步）
		PhysicsLiquid3D *liquid = new PhysicsLiquid3D(*myPhysicsWorld);
		(*myPhysicsWorld)->SetLiquid(liquid);

		// ── 关键几何：PBF 只抗压不抗张，摊在大平面上的水会铺成 1~2 层的
		// 薄膜、托不起任何船（薄膜深度 < 船吃水，λ 钳零后无围压）。要演示
		// 浮沉，必须给水一个**有侧壁的深井**让体积堆出深度：
		// 井 = 世界 x,z ∈ [-3.5, 3.5]、y ∈ [-15, -7] 的空腔，四周用体素 shape
		// 砌墙（kinematic 钉死当静态井壁），水直接倒满井。
		auto addWall = [&](Vec3_ pos, int w, int h, int d)
		{
			PhysicsShape3D *wall = MakeSolidShape3D(pos, w, h, d);
			(*myPhysicsWorld)->AddObject(wall);
			(*myPhysicsWorld)->GetKinematic().SetIsKinematic(wall, true); // 静态井壁
		};
		addWall(Vec3_(-4.0f, -11.0f, 0.0f), 1, 8, 8); // 西墙
		addWall(Vec3_(4.0f, -11.0f, 0.0f), 1, 8, 8);  // 东墙
		addWall(Vec3_(0.0f, -11.0f, -4.0f), 8, 8, 1); // 北墙
		addWall(Vec3_(0.0f, -11.0f, 4.0f), 8, 8, 1);  // 南墙

		// 井内注水：8×8×8 spacing 0.4 → 512 粒，井截面 6.6² → 水深约 4.4
		//（水面平衡位 y≈-10.7）。
		// 【测试发现的引擎边界，如实记录】不要用"高空水团砸进井"演示溅射：
		// 下坠 ~9 格时 CFL 子步位移 ≈0.25 已与墙厚 0.5 同量级，粒子会**隧穿过
		// 1 格刚体壁**（位置级边界钳制只对地图地形做、不对动态刚体做——与 2D
		// 参考实现同源的边界）。溅射版实测 448 粒中 213 粒越壁逃逸。
		liquid->AddGrid(Vec3_(0.0f, -12.3f, 0.0f), 8, 8, 8, 0.4f);

		// "船"：2×1×2 = 4 体素 × 0.12 = 质量 0.48；同体积水 ≈ 0.4³·⁰⁴·… 远更重
		// → 深井中应浮在水面（浮力由压力梯度自然涌现，无经验浮力参数）
		(*myPhysicsWorld)->AddObject(MakeSolidShape3D(Vec3_(-1.0f, -10.6f, 0.0f), 2, 1, 2, 0.12f, 0.5f));
		// "铁"：1 体素 × 40 → 密度 40 超过水的等效密度（每粒子质量 1 /
		// spacing³=0.064 ≈ 15.6），应穿过水面沉到井底。
		// （数值直觉陷阱：质量 8 时铁比"水"还轻会浮起来——浅膜场景里它
		//  沉底只是因为膜托不住任何东西，深井里密度比较才是真判据）
		(*myPhysicsWorld)->AddObject(MakeSolidShape3D(Vec3_(1.0f, -8.0f, 0.0f), 1, 1, 1, 40.0f, 0.4f));

		ApplyCameraView3D(mCamera, MakeOrbitView3D(Vec3_(0.0f, -11.0f, 0.0f), 28.0f));
	}
}
