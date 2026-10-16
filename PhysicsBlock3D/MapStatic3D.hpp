#pragma once
#include "MapFormwork3D.hpp"
#include "BaseGrid3D.hpp"
#include "BaseCalculate3D.hpp"
#include <vector>

namespace PhysicsBlock3D
{

	/**
	 * @brief 静态三维地图类
	 * @details 由 2D `MapStatic`（181 行）改写。继承 BaseGrid3D（体素数据）
	 *          与 MapFormwork3D（地图接口），表示静态三维地形。
	 *
	 * 维度映射：
	 *  - 构造 `(Width, Height)` → `(Width, Height, Depth)`
	 *  - `centrality` `Vec2_` → `Vec3_`
	 *  - `FMGetMapSize()` `glm::uvec2` → `UVec3_`
	 *  - 三个轮廓函数 `(x,y,w,h)` → `(x,y,z,w,h,d)`
	 *  - `FMBresenhamDetection` → `FMDDADetection`（3D DDA）
	 *  - `SetFriction(int,int)` → `SetFriction(int,int,int)`
	 *
	 * 轮廓语义变更（关键）：2D 的 `MapOutline.face` 是「2×2 角点方向」
	 * （分量 ±1），`pos` 是角点坐标；3D 的 `face` 改为**外向面法线**，
	 * `pos` 是**面中心**（= 体素中心 + 0.5·法线）。两者在窄相里
	 * `pos - face*0.5` 都恰好回到体素中心，语义一致。
	 */
	class MapStatic3D : public BaseGrid3D, public MapFormwork3D
	{
#if PhysicsBlock_Serialization3D
	public:
		virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
		{
			BaseGrid3D::JsonSerialization(data);
			SerializationVec3(data, centrality);
		}

		virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
		{
			BaseGrid3D::JsonContrarySerialization(data);
			ContrarySerializationVec3(data, centrality);
		}

		MapStatic3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
		    : BaseGrid3D(data)
		{
			type = PhysicsObjectEnum::_MapStatic;
			ContrarySerializationVec3(data, centrality);
		}
#endif
	public:
		/**
		 * @brief 地图质心（世界坐标原点对应的体素坐标）
		 * @details 世界坐标 = 体素坐标 + centrality
		 */
		Vec3_ centrality{0.0f};

	public:
		/**
		 * @brief 构造函数
		 * @param Width 地图宽度（体素）
		 * @param Height 地图高度（体素）
		 * @param Depth 地图深度（体素）
		 */
		MapStatic3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth)
		    : BaseGrid3D(Width, Height, Depth)
		{
			type = PhysicsObjectEnum::_MapStatic;
		}

		~MapStatic3D() {}

		/**
		 * @brief 设置地图质心（必须在地图范围内，否则忽略）
		 */
		void SetCentrality(Vec3_ Centrality);

		/**
		 * @brief 提取区域轻量轮廓
		 */
		void GetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                           std::vector<MapOutline3D> &Outline);

		/**
		 * @brief 提取区域完整轮廓
		 */
		void GetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                std::vector<MapOutline3D> &Outline);

		/**
		 * @brief 提取区域最小轮廓（只留暴露面）
		 */
		void GetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                   std::vector<MapOutline3D> &Outline);

		/*=========MapFormwork3D=========*/

		virtual UVec3_ FMGetMapSize() override { return UVec3_(width, height, depth); }

		/**
		 * @brief 查询体素碰撞（体素坐标，越界返回 false）
		 */
		virtual bool FMGetCollide(IVec3_ start) override
		{
			if (start.x < 0 || start.y < 0 || start.z < 0 ||
			    start.x >= (int)width || start.y >= (int)height || start.z >= (int)depth)
			{
				return false;
			}
			return at(start).Collision;
		}

		/**
		 * @brief 查询体素碰撞（世界坐标，内部加 centrality）
		 */
		virtual bool FMGetCollide(Vec3_ start) override
		{
			return FMGetCollide(ToInt(start + centrality));
		}

		virtual GridBlock3D &FMGetGridBlock(IVec3_ start) override { return at(start); }

		/**
		 * @brief 三维 DDA（体素坐标）
		 */
		virtual CollisionInfoI3D FMDDADetection(IVec3_ start, IVec3_ end) override
		{
			return DDADetection(start, end);
		}

		/**
		 * @brief 三维 DDA（世界坐标，内部加 centrality，结果减回）
		 */
		virtual CollisionInfoD3D FMDDADetection(Vec3_ start, Vec3_ end) override;

		virtual CollisionInfoI3D FMSafeDDADetection(IVec3_ start, IVec3_ end) override;

		virtual Vec3_ FMGetCentrality() override { return centrality; }

		virtual void FMGetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                             std::vector<MapOutline3D> &Outline) override
		{
			GetMinOutline(x_, y_, z_, w_, h_, d_, Outline);
		}

		virtual void FMGetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                                     std::vector<MapOutline3D> &Outline) override
		{
			GetLightweightOutline(x_, y_, z_, w_, h_, d_, Outline);
		}

		virtual void FMGetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                          std::vector<MapOutline3D> &Outline) override
		{
			GetOutline(x_, y_, z_, w_, h_, d_, Outline);
		}

		virtual bool SafeSetCollision(IVec3_ pos, bool state) override;
		virtual void SetFriction(int x, int y, int z, FLOAT_ friction) override;
		virtual FLOAT_ GetFriction(int x, int y, int z) override;
	};

}
