#include "PhysicsFaceMesh3D.hpp"
#include "MapFormwork3D.hpp"
#include "MapDynamic3D.hpp"
#include "PhysicsWorld3D.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PhysicsBlock3D
{

	namespace
	{
		/**
		 * @brief 6 个面的 4 个角点偏移，顺序 = 从面外侧看**顺时针**
		 * @details 与 `GameMods/BlockWorld/BlockWorld.cpp` 的
		 *          `BuildBlockFaceVertices` 六个分支同序（例如 +Z 面是
		 *          (0,0,1) → (0,1,1) → (1,1,1) → (1,0,1)）。
		 */
		const int kFaceCorner[6][4][3] = {
			{{0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}, // 0: +Z
			{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, // 1: -Z
			{{0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}}, // 2: -X
			{{1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 0, 0}}, // 3: +X
			{{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}}, // 4: -Y
			{{1, 1, 1}, {0, 1, 1}, {0, 1, 0}, {1, 1, 0}}, // 5: +Y
		};

		/** @brief 6 个面的法线（与 kFaceCorner 同序） */
		const float kFaceNormal[6][3] = {
			{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0},
		};

		/** @brief 6 个面的邻格偏移（判据用：该方向邻格非实体才画这个面） */
		const int kFaceNeighbor[6][3] = {
			{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0},
		};
	} // namespace

	void EmitVoxelFaces3D(int gx, int gy, int gz,
	                      const glm::dvec3 &origin, const Quat_ &rotation, const Vec3_ &centreMass,
	                      const glm::vec4 &color,
	                      const std::function<bool(int, int, int)> &IsEntity,
	                      std::vector<FaceVertex3D> &out)
	{
		const glm::vec3 originF = glm::vec3(origin);

		for (int f = 0; f < 6; ++f)
		{
			// 邻格是实体 → 该面被挡住，不画（与线框的判据完全一致）
			if (IsEntity(gx + kFaceNeighbor[f][0], gy + kFaceNeighbor[f][1], gz + kFaceNeighbor[f][2]))
				continue;

			const Vec3_ normal = QuatRotate(
			    rotation, Vec3_((FLOAT_)kFaceNormal[f][0], (FLOAT_)kFaceNormal[f][1], (FLOAT_)kFaceNormal[f][2]));

			FaceVertex3D v[4];
			for (int c = 0; c < 4; ++c)
			{
				const int i = kFaceCorner[f][c][0];
				const int j = kFaceCorner[f][c][1];
				const int k = kFaceCorner[f][c][2];

				// 角点局部坐标 = 格索引 + 角偏移，再减质心、旋转、平移到世界
				const Vec3_ local = Vec3_((FLOAT_)(gx + i), (FLOAT_)(gy + j), (FLOAT_)(gz + k)) - centreMass;
				v[c].Pos = originF + QuatRotate(rotation, local);
				v[c].Color = color;
				v[c].Normal = normal;
			}

			// 一个四边形拆两个三角形，保持四边形的绕序
			out.push_back(v[0]);
			out.push_back(v[1]);
			out.push_back(v[2]);
			out.push_back(v[0]);
			out.push_back(v[2]);
			out.push_back(v[3]);
		}
	}

	void BuildShapeFaces3D(PhysicsShape3D *obj, const glm::vec4 &color, std::vector<FaceVertex3D> &out)
	{
		if (obj == nullptr)
			return;

		// 邻格实体判定（带越界保护：越界视为非实体，从而画出外表面）
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
						EmitVoxelFaces3D((int)x, (int)y, (int)z, origin, obj->rotation,
						                 obj->CentreMass, color, IsEntity, out);
					}
				}
			}
		}
	}

	void BuildCircleFaces3D(PhysicsCircle3D *obj, const glm::vec4 &color, std::vector<FaceVertex3D> &out,
	                        int segments, int rings)
	{
		if (obj == nullptr || obj->radius <= 0.0f)
			return;
		if (segments < 3)
			segments = 3;
		if (rings < 2)
			rings = 2;

		const Vec3_ c = obj->pos;
		const FLOAT_ r = obj->radius;

		// 单位球面上的点：φ=0 在 +Y 极，φ=π 在 -Y 极
		auto SpherePoint = [&](int i, int j) -> Vec3_ {
			const FLOAT_ theta = (FLOAT_)(2.0 * M_PI * (double)i / (double)segments);
			const FLOAT_ phi = (FLOAT_)(M_PI * (double)j / (double)rings);
			const FLOAT_ sp = sinf(phi);
			return Vec3_(sp * cosf(theta), cosf(phi), sp * sinf(theta));
		};

		// 单位方向 → 顶点（法线就是该方向，球心在 c、半径 r）
		auto Push = [&](const Vec3_ &n) {
			FaceVertex3D v;
			v.Pos = c + r * n;
			v.Color = color;
			v.Normal = n;
			out.push_back(v);
		};

		for (int j = 0; j < rings; ++j)
		{
			for (int i = 0; i < segments; ++i)
			{
				// 四边形 A(θ,φ) B(θ+1,φ) C(θ+1,φ+1) D(θ,φ+1)
				// 从球外看顺时针的绕序是 A→D→C→A、A→C→B→A（见文件头绕序说明）
				const Vec3_ A = SpherePoint(i, j);
				const Vec3_ B = SpherePoint(i + 1, j);
				const Vec3_ C = SpherePoint(i + 1, j + 1);
				const Vec3_ D = SpherePoint(i, j + 1);

				Push(A);
				Push(D);
				Push(C);
				Push(A);
				Push(C);
				Push(B);
			}
		}
	}

	void BuildMapFaces3D(PhysicsWorld3D *world, const glm::vec4 &color, std::vector<FaceVertex3D> &out)
	{
		if (world == nullptr)
			return;
		MapFormwork3D *map = world->GetMapFormwork();
		if (map == nullptr)
			return;

		const UVec3_ mapSize = map->FMGetMapSize();
		if (mapSize.x == 0 || mapSize.y == 0 || mapSize.z == 0)
			return;

		const Quat_ noRotation(1.0f, 0.0f, 0.0f, 0.0f);
		const Vec3_ noCentre(0.0f);

		if (map->FMGetType() == PhysicsObjectEnum::_MapDynamic)
		{
			MapDynamic3D *dynMap = (MapDynamic3D *)(map);

			// 动态地图：板块位置是"负的世界坐标起点"，格子直接用世界坐标
			const int startX = -(int)(dynMap->GetPlatePos().x);
			const int startY = -(int)(dynMap->GetPlatePos().y);
			const int startZ = -(int)(dynMap->GetPlatePos().z);
			const int endX = startX + (int)mapSize.x;
			const int endY = startY + (int)mapSize.y;
			const int endZ = startZ + (int)mapSize.z;

			auto IsEntity = [&](int x, int y, int z) -> bool {
				if (x < startX || y < startY || z < startZ || x >= endX || y >= endY || z >= endZ)
					return false;
				return map->FMGetGridBlock(IVec3_{x, y, z}).Collision;
			};

			for (int x = startX; x < endX; ++x)
			{
				for (int y = startY; y < endY; ++y)
				{
					for (int z = startZ; z < endZ; ++z)
					{
						if (IsEntity(x, y, z))
						{
							EmitVoxelFaces3D(x, y, z, glm::dvec3(0.0), noRotation,
							                 noCentre, color, IsEntity, out);
						}
					}
				}
			}
		}
		else
		{
			const Vec3_ mapCentrality = map->FMGetCentrality();
			const int endX = (int)mapSize.x;
			const int endY = (int)mapSize.y;
			const int endZ = (int)mapSize.z;

			auto IsEntity = [&](int x, int y, int z) -> bool {
				if (x < 0 || y < 0 || z < 0 || x >= endX || y >= endY || z >= endZ)
					return false;
				return map->FMGetGridBlock(IVec3_{x, y, z}).Collision;
			};

			// 静态地图世界坐标 = (x,y,z) - 地图中心（与碰撞用的 FMGetCentrality 对应）
			const glm::dvec3 origin = glm::dvec3(-mapCentrality);

			for (int x = 0; x < endX; ++x)
			{
				for (int y = 0; y < endY; ++y)
				{
					for (int z = 0; z < endZ; ++z)
					{
						if (IsEntity(x, y, z))
						{
							EmitVoxelFaces3D(x, y, z, origin, noRotation,
							                 noCentre, color, IsEntity, out);
						}
					}
				}
			}
		}
	}

} // namespace PhysicsBlock3D
