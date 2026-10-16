#pragma once
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "PhysicsShape3D.hpp"
#include "PhysicsCircle3D.hpp"

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * 面渲染的 CPU 端几何生成（3D 物理）
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 与 `PhysicsAuxiliaryVision3D.cpp` 的 `DrawCellOutline3D` 一一对应：
 * 同一个"邻格非实体才画该面"的判据，只是把输出从线段换成三角形。
 *
 * 绕序约定（必须与 BlockWorld 一致，否则背面剔除会把面全剔掉）：
 * 从面的**外侧**看过去，顶点顺序为**顺时针**——这与
 * `PipelineMods::BlockWorldMods` 的 `frontFace = VK_FRONT_FACE_CLOCKWISE`
 * 配套。物体会整体乘一个旋转四元数（正规旋转不改变绕序），所以旋转后的
 * 物体依然正确。
 *
 * 本文件只生成 CPU 顶点（不含 Vulkan），便于单独对照测试。
 */

namespace PhysicsBlock3D
{

	/**
	 * @brief 面渲染顶点
	 * @details 内存布局必须与 BlockWorld 的 `BlockVertex` 完全一致
	 *          （位置 vec3 + 颜色 vec4 + 法线 vec3 = 40 字节），因为面渲染
	 *          直接复用 `PipelineMods::BlockWorldMods` 的顶点输入描述
	 *          （location 0/1/2 偏移 0/12/28）。
	 */
	struct FaceVertex3D
	{
		glm::vec3 Pos;
		glm::vec4 Color;
		glm::vec3 Normal;
	};

	/**
	 * @brief 输出一个体素格子的所有"外露面"
	 * @param gx,gy,gz   格子在**所在网格**里的索引（不是世界坐标）
	 * @param origin     网格原点的世界坐标（物体 = `obj->pos`；静态地图 = `-地图中心`；
	 *                   动态地图 = 0）
	 * @param rotation   物体朝向（地图传单位四元数）
	 * @param centreMass 质心（局部坐标，绕它旋转；地图传 (0,0,0)）
	 * @param color      面颜色
	 * @param IsEntity   邻格实体判据（越界应返回 false：越界即"外面"，该面要画）
	 * @param out        顶点输出（追加）
	 * @details 世界坐标 = origin + Rotate(rotation, (gx+i, gy+j, gz+k) - centreMass)，
	 *          与物理引擎的 `PhysicsShape3D`（`PhysicsShape3D.cpp` 里
	 *          `pos + QuatRotate(rotation, 体素 - CentreMass)`）以及地图的
	 *          `FMGetCollide(pos + FMGetCentrality())` 保持一致。
	 */
	void EmitVoxelFaces3D(int gx, int gy, int gz,
	                      const glm::dvec3 &origin, const Quat_ &rotation, const Vec3_ &centreMass,
	                      const glm::vec4 &color,
	                      const std::function<bool(int, int, int)> &IsEntity,
	                      std::vector<FaceVertex3D> &out);

	/**
	 * @brief 体素网格物体（PhysicsShape3D）的面
	 */
	void BuildShapeFaces3D(PhysicsShape3D *obj, const glm::vec4 &color, std::vector<FaceVertex3D> &out);

	/**
	 * @brief 球（PhysicsCircle3D）的面：UV 球面网格
	 * @param segments 经线分段（绕 Y 一圈）
	 * @param rings    纬线分段（从 +Y 极到 -Y 极）
	 * @details 球用 `radius` 表达，几何上各向同性，故不乘 `rotation`。
	 */
	void BuildCircleFaces3D(PhysicsCircle3D *obj, const glm::vec4 &color, std::vector<FaceVertex3D> &out,
	                        int segments = 24, int rings = 12);

	/**
	 * @brief 地形（地图样板）的面
	 * @details 与 `RenderMapOutline3D` 同一套遍历与判据：
	 *          静态地图 `原格(0..size) - 地图中心`；动态地图直接用世界坐标（`-板块位置` 起算）。
	 *          只画"外露面"，所以实心地形只是一个外壳，内部不产生顶点。
	 */
	void BuildMapFaces3D(class PhysicsWorld3D *world, const glm::vec4 &color, std::vector<FaceVertex3D> &out);

} // namespace PhysicsBlock3D
