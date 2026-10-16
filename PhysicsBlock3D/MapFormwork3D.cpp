#include "MapFormwork3D.hpp"
#include "BaseCalculate3D.hpp"

namespace PhysicsBlock3D
{

	bool MapFormwork3D::FMGetCollide(IVec3_ pos)
	{
		return FMGetGridBlock(pos).Collision;
	}

	bool MapFormwork3D::FMGetCollide(Vec3_ pos)
	{
		// 关键：先加 centrality 再取整（2D MapStatic.cpp:14-22 的约定）
		return FMGetCollide(ToInt(pos + FMGetCentrality()));
	}

	CollisionInfoI3D MapFormwork3D::FMDDADetection(IVec3_ start, IVec3_ end)
	{
		// 体素坐标转世界坐标（加 centrality）后走浮点 DDA，保证与 FMGetCollide 一致
		const Vec3_ offset = FMGetCentrality();
		const CollisionInfoD3D info = FMDDADetection(Vec3_(start) + Vec3_(0.5f) + offset,
		                                             Vec3_(end) + Vec3_(0.5f) + offset);
		CollisionInfoI3D result;
		result.Collision = info.Collision;
		result.pos = ToInt(info.pos - offset);
		result.Friction = info.Friction;
		return result;
	}

	CollisionInfoI3D MapFormwork3D::FMSafeDDADetection(IVec3_ start, IVec3_ end)
	{
		const UVec3_ size = FMGetMapSize();
		const int maxX = static_cast<int>(size.x) - 1;
		const int maxY = static_cast<int>(size.y) - 1;
		const int maxZ = static_cast<int>(size.z) - 1;

		CollisionInfoI3D result;
		result.Collision = false;
		result.pos = start;
		result.Friction = 0.2f;

		// 起点越界直接判否（2D FMSafeBresenhamDetection 的语义）
		if (start.x < 0 || start.y < 0 || start.z < 0 ||
		    start.x > maxX || start.y > maxY || start.z > maxZ)
		{
			return result;
		}

		// 终点夹到 [0, max] 体素范围内（整数夹取，避免 float/double 重载歧义）
		const auto ClampAxis = [](int v, int hi) -> int {
			if (v < 0) return 0;
			if (v > hi) return hi;
			return v;
		};

		const IVec3_ clampedEnd(ClampAxis(end.x, maxX),
		                        ClampAxis(end.y, maxY),
		                        ClampAxis(end.z, maxZ));

		return FMDDADetection(start, clampedEnd);
	}

}
