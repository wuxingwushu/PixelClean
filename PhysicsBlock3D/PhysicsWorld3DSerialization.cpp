/**
 * @file   PhysicsWorld3DSerialization.cpp
 * @brief  PhysicsWorld3D 的 JSON 序列化 / 反序列化（3D 版）
 * @details 由 2D `PhysicsWorld::JsonSerialization`（PhysicsWorld.cpp:1713）
 *          与 `PhysicsWorld::JsonContrarySerialization`（:1887）逐段改写。
 *
 * 【3D 相对 2D 的差异】
 *  1. `Vec2_` → `Vec3_`，`SerializationVec2`/`ContrarySerializationVec2`
 *     → `SerializationVec3`/`ContrarySerializationVec3`；
 *     `MapStatic` → `MapStatic3D`，`GridSearch` → `GridSearch3D`。
 *  2. `GridWindSize` 是 `glm::uvec3`（2D 是 `glm::uvec2`），无符号类型用
 *     `SerializationVecN` 往返会出现 float→unsigned 的隐式转换，这里显式
 *     按 x/y/z 三个整数键读写。
 *  3. **`CollideGroupVector`（裁决器列表）不再序列化**。理由：裁决器是每帧由
 *     广相重新生成的**瞬态**对象（`PhysicsEmulator` 第 ③ 阶段 + `HandleCollideGroup`），
 *     存档时它们携带的只有"热启动冲量"这一帧间缓存；载入后第一帧就会重新
 *     建立全部碰撞对，只损失一帧的接触冲量继承。2D 之所以要序列化它，是为了
 *     让内存池分配出的裁决器下标在存档往返后保持一致——3D 无此必要，且可
 *     避免 12 个裁决器类都要实现 `JsonContrarySerialization` 的连带负担。
 *     故 3D 只写"稳态"数据（地图、物体、关节、连接、组装体）。
 *  4. 组装体走 3D 新增的 `ChildDescriptor` / `ResolveChildren` 机制（2D 是
 *     `children[i]["type"]/["childIndex"]` 内联在 `PhysicsAssembly` 里）。
 *
 * 依赖：`PhysicsBlock_Serialization3D` 必须为 1（见 BaseSerialization3D.hpp）。
 */

#include "PhysicsWorld3D.hpp"
#include "MapStatic3D.hpp"
#include "MapDynamic3D.hpp"

#if PhysicsBlock_Serialization3D

namespace PhysicsBlock3D
{

	// =========================================================================
	// 构造：从 JSON 重建世界
	// =========================================================================
	PhysicsWorld3D::PhysicsWorld3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	    : WindBool(data.contains("WindBool") ? data["WindBool"].get<bool>() : false)
	{
		JsonContrarySerialization(data);
	}

	// =========================================================================
	// 序列化
	// =========================================================================
	void PhysicsWorld3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		data["WindBool"] = WindBool;
		SerializationVec3(data, Wind);
		SerializationVec3(data, GravityAcceleration);

		// ── 网格风尺寸（无符号，显式按整数键读写） ──
		data["GridWindSize.x"] = static_cast<unsigned int>(GridWindSize.x);
		data["GridWindSize.y"] = static_cast<unsigned int>(GridWindSize.y);
		data["GridWindSize.z"] = static_cast<unsigned int>(GridWindSize.z);

		if (WindBool && GridWind != nullptr)
		{
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["GridWind"];
			dataArray = dataArray.array();
			const size_t total = static_cast<size_t>(GridWindSize.x) *
			                     static_cast<size_t>(GridWindSize.y) *
			                     static_cast<size_t>(GridWindSize.z);
			for (size_t i = 0; i < total; ++i)
			{
				dataArray[i]["x"] = GridWind[i].x;
				dataArray[i]["y"] = GridWind[i].y;
				dataArray[i]["z"] = GridWind[i].z;
			}
		}

		// ── 地图（3D 只序列化静态地图；动态地图由玩家数据自行重建） ──
		if (wMapFormwork)
		{
			switch (wMapFormwork->FMGetType())
			{
			case PhysicsObjectEnum::_MapStatic:
				data["wMapFormwork"]["Type"] = PhysicsObjectEnum::_MapStatic;
				static_cast<MapStatic3D *>(wMapFormwork)->JsonSerialization(data["wMapFormwork"]);
				break;

			default:
				break;
			}
		}

		// ── 五类物理物体 ──
		if (!PhysicsShapeS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsShapeS"];
			dataArray = dataArray.array();
			for (auto *j : PhysicsShapeS)
			{
				j->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}
		if (!PhysicsParticleS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsParticleS"];
			dataArray = dataArray.array();
			for (auto *j : PhysicsParticleS)
			{
				j->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}
		if (!PhysicsCircleS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsCircleS"];
			dataArray = dataArray.array();
			for (auto *j : PhysicsCircleS)
			{
				j->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}
		if (!PhysicsLineS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsLineS"];
			dataArray = dataArray.array();
			for (auto *j : PhysicsLineS)
			{
				j->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}

		// ── 组装体（先算子对象索引，再序列化自身） ──
		if (!PhysicsAssemblyS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsAssemblyS"];
			dataArray = dataArray.array();
			for (auto *ass : PhysicsAssemblyS)
			{
				ass->BuildChildDescriptors(
				    [this](PhysicsFormwork3D *child) -> unsigned int
				    { return GetPtrIndex(child); });
				ass->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}

		// ── 关节 ──
		if (!PhysicsJointS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsJointS"];
			dataArray = dataArray.array();
			for (auto *j : PhysicsJointS)
			{
				dataArray[dataIndex]["body1"] = GetPtrIndex(j->body1);
				dataArray[dataIndex]["body1Type"] = j->body1->PFGetType();
				dataArray[dataIndex]["body2"] = GetPtrIndex(j->body2);
				dataArray[dataIndex]["body2Type"] = j->body2->PFGetType();
				j->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}

		// ── 连接（绳/弹簧/杠杆/橡胶） ──
		if (!BaseJunctionS.empty())
		{
			unsigned int dataIndex = 0;
			nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["BaseJunctionS"];
			dataArray = dataArray.array();
			for (auto *j : BaseJunctionS)
			{
				dataArray[dataIndex]["ObjectType"] = j->ObjectType();
				switch (j->ObjectType())
				{
				case CordObjectType3D::JunctionAA3D:
				{
					auto *js = static_cast<PhysicsJunctionSS3D *>(j);
					dataArray[dataIndex]["body1"] = GetPtrIndex(js->mParticle1);
					dataArray[dataIndex]["body1Type"] = js->mParticle1->PFGetType();
					dataArray[dataIndex]["body2"] = GetPtrIndex(js->mParticle2);
					dataArray[dataIndex]["body2Type"] = js->mParticle2->PFGetType();
					break;
				}
				case CordObjectType3D::JunctionA3D:
				{
					auto *js = static_cast<PhysicsJunctionS3D *>(j);
					dataArray[dataIndex]["body1"] = GetPtrIndex(js->mParticle);
					dataArray[dataIndex]["body1Type"] = js->mParticle->PFGetType();
					break;
				}
				case CordObjectType3D::JunctionP3D:
				{
					auto *jp = static_cast<PhysicsJunctionP3D *>(j);
					dataArray[dataIndex]["body1"] = GetPtrIndex(jp->mParticle);
					dataArray[dataIndex]["body1Type"] = jp->mParticle->PFGetType();
					break;
				}
				case CordObjectType3D::JunctionPP3D:
				{
					auto *jpp = static_cast<PhysicsJunctionPP3D *>(j);
					dataArray[dataIndex]["body1"] = GetPtrIndex(jpp->mParticle1);
					dataArray[dataIndex]["body1Type"] = jpp->mParticle1->PFGetType();
					dataArray[dataIndex]["body2"] = GetPtrIndex(jpp->mParticle2);
					dataArray[dataIndex]["body2Type"] = jpp->mParticle2->PFGetType();
					break;
				}

				default:
					break;
				}
				j->JsonSerialization(dataArray[dataIndex]);
				++dataIndex;
			}
		}

		// CollideGroupVector（瞬态裁决器）按设计不序列化，见文件头说明。
	}

	// =========================================================================
	// 反序列化
	// =========================================================================
	void PhysicsWorld3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		ContrarySerializationVec3(data, Wind);
		ContrarySerializationVec3(data, GravityAcceleration);

		if (data.contains("GridWindSize.x"))
		{
			GridWindSize.x = data["GridWindSize.x"].get<unsigned int>();
			GridWindSize.y = data["GridWindSize.y"].get<unsigned int>();
			GridWindSize.z = data["GridWindSize.z"].get<unsigned int>();
		}

		if (WindBool && data.contains("GridWind"))
		{
			const size_t total = static_cast<size_t>(GridWindSize.x) *
			                     static_cast<size_t>(GridWindSize.y) *
			                     static_cast<size_t>(GridWindSize.z);
			GridWind = new Vec3_[total == 0 ? 1 : total];
			for (size_t i = 0; i < total; ++i)
			{
				GridWind[i].x = data["GridWind"][i]["x"].get<FLOAT_>();
				GridWind[i].y = data["GridWind"][i]["y"].get<FLOAT_>();
				GridWind[i].z = data["GridWind"][i]["z"].get<FLOAT_>();
			}
		}

		// ── 地图 ──
		if (data.contains("wMapFormwork") && data["wMapFormwork"].contains("Type"))
		{
			switch (static_cast<PhysicsObjectEnum>(data["wMapFormwork"]["Type"].get<int>()))
			{
			case PhysicsObjectEnum::_MapStatic:
				wMapFormwork = new MapStatic3D(data["wMapFormwork"]);
				break;

			default:
				break;
			}
		}

		// 网格范围按三个轴的最大值取（2D 只有两轴）
		mGridSearch.SetMapRange(std::max({static_cast<unsigned int>(GridWindSize.x),
		                                  static_cast<unsigned int>(GridWindSize.y),
		                                  static_cast<unsigned int>(GridWindSize.z)}));
		mGridCenter = mGridSearch.GetGridCenter();

		// ── 五类物理物体（顺序必须与序列化一致，索引才对得上） ──
		if (data.contains("PhysicsShapeS"))
		{
			for (size_t i = 0; i < data["PhysicsShapeS"].size(); ++i)
			{
				AddObject(new PhysicsShape3D(data["PhysicsShapeS"][i]));
			}
		}
		if (data.contains("PhysicsParticleS"))
		{
			for (size_t i = 0; i < data["PhysicsParticleS"].size(); ++i)
			{
				AddObject(new PhysicsParticle3D(data["PhysicsParticleS"][i]));
			}
		}
		if (data.contains("PhysicsCircleS"))
		{
			for (size_t i = 0; i < data["PhysicsCircleS"].size(); ++i)
			{
				AddObject(new PhysicsCircle3D(data["PhysicsCircleS"][i]));
			}
		}
		if (data.contains("PhysicsLineS"))
		{
			for (size_t i = 0; i < data["PhysicsLineS"].size(); ++i)
			{
				AddObject(new PhysicsLine3D(data["PhysicsLineS"][i]));
			}
		}

		// ── 组装体（依赖上面的物体已就位） ──
		if (data.contains("PhysicsAssemblyS"))
		{
			for (size_t i = 0; i < data["PhysicsAssemblyS"].size(); ++i)
			{
				PhysicsAssembly3D *ass = new PhysicsAssembly3D(data["PhysicsAssemblyS"][i]);
				ass->ResolveChildren(
				    [this](PhysicsObjectEnum type, unsigned int idx) -> PhysicsFormwork3D *
				    { return static_cast<PhysicsFormwork3D *>(GetIndexPtr(type, idx)); });
				ass->SetWorld(this);
				PhysicsAssemblyS.push_back(ass);
			}
		}

		// ── 关节 ──
		if (data.contains("PhysicsJointS"))
		{
			for (size_t i = 0; i < data["PhysicsJointS"].size(); ++i)
			{
				PhysicsJoint3D *joint = new PhysicsJoint3D(data["PhysicsJointS"][i]);
				joint->body1 = static_cast<PhysicsAngle3D *>(
				    GetIndexPtr(static_cast<PhysicsObjectEnum>(data["PhysicsJointS"][i]["body1Type"].get<int>()),
				                data["PhysicsJointS"][i]["body1"].get<unsigned int>()));
				joint->body2 = static_cast<PhysicsAngle3D *>(
				    GetIndexPtr(static_cast<PhysicsObjectEnum>(data["PhysicsJointS"][i]["body2Type"].get<int>()),
				                data["PhysicsJointS"][i]["body2"].get<unsigned int>()));
				if (joint->body1 && joint->body2)
				{
					AddObject(joint);
				}
				else
				{
					delete joint;
				}
			}
		}

		// ── 连接 ──
		if (data.contains("BaseJunctionS"))
		{
			for (size_t i = 0; i < data["BaseJunctionS"].size(); ++i)
			{
				const auto &jd = data["BaseJunctionS"][i];
				const auto objType = static_cast<CordObjectType3D>(jd["ObjectType"].get<int>());
				switch (objType)
				{
				case CordObjectType3D::JunctionAA3D:
				{
					auto *js = new PhysicsJunctionSS3D(jd);
					js->mParticle1 = static_cast<PhysicsAngle3D *>(
					    GetIndexPtr(static_cast<PhysicsObjectEnum>(jd["body1Type"].get<int>()),
					                jd["body1"].get<unsigned int>()));
					js->mParticle2 = static_cast<PhysicsAngle3D *>(
					    GetIndexPtr(static_cast<PhysicsObjectEnum>(jd["body2Type"].get<int>()),
					                jd["body2"].get<unsigned int>()));
					if (js->mParticle1 && js->mParticle2)
						AddObject(js);
					else
						delete js;
					break;
				}
				case CordObjectType3D::JunctionA3D:
				{
					auto *js = new PhysicsJunctionS3D(jd);
					js->mParticle = static_cast<PhysicsAngle3D *>(
					    GetIndexPtr(static_cast<PhysicsObjectEnum>(jd["body1Type"].get<int>()),
					                jd["body1"].get<unsigned int>()));
					if (js->mParticle)
						AddObject(js);
					else
						delete js;
					break;
				}
				case CordObjectType3D::JunctionP3D:
				{
					auto *jp = new PhysicsJunctionP3D(jd);
					jp->mParticle = static_cast<PhysicsParticle3D *>(
					    GetIndexPtr(static_cast<PhysicsObjectEnum>(jd["body1Type"].get<int>()),
					                jd["body1"].get<unsigned int>()));
					if (jp->mParticle)
						AddObject(jp);
					else
						delete jp;
					break;
				}
				case CordObjectType3D::JunctionPP3D:
				{
					auto *jpp = new PhysicsJunctionPP3D(jd);
					jpp->mParticle1 = static_cast<PhysicsParticle3D *>(
					    GetIndexPtr(static_cast<PhysicsObjectEnum>(jd["body1Type"].get<int>()),
					                jd["body1"].get<unsigned int>()));
					jpp->mParticle2 = static_cast<PhysicsParticle3D *>(
					    GetIndexPtr(static_cast<PhysicsObjectEnum>(jd["body2Type"].get<int>()),
					                jd["body2"].get<unsigned int>()));
					if (jpp->mParticle1 && jpp->mParticle2)
						AddObject(jpp);
					else
						delete jpp;
					break;
				}

				default:
					break;
				}
			}
		}
	}

	// =========================================================================
	// 指针 ↔ 索引 映射
	// =========================================================================
	unsigned int PhysicsWorld3D::GetPtrIndex(PhysicsFormwork3D *ptr)
	{
		if (ptr == nullptr)
		{
			return UINT_MAX;
		}
		// 与序列化顺序严格一致：Shape → Particle → Circle → Line
#define PhysicsVectorIndex3D(Vector, Ptr)          \
	for (size_t i = 0; i < (Vector).size(); ++i)   \
	{                                              \
		if ((Vector)[i] == (Ptr))                  \
		{                                          \
			return static_cast<unsigned int>(i);   \
		}                                          \
	}

		PhysicsVectorIndex3D(PhysicsShapeS, ptr);
		PhysicsVectorIndex3D(PhysicsParticleS, ptr);
		PhysicsVectorIndex3D(PhysicsCircleS, ptr);
		PhysicsVectorIndex3D(PhysicsLineS, ptr);
#undef PhysicsVectorIndex3D
		return UINT_MAX;
	}

	void *PhysicsWorld3D::GetIndexPtr(PhysicsObjectEnum Enum, unsigned int index)
	{
		switch (Enum)
		{
		case PhysicsObjectEnum::shape:
			return (index < PhysicsShapeS.size()) ? PhysicsShapeS[index] : nullptr;
		case PhysicsObjectEnum::particle:
			return (index < PhysicsParticleS.size()) ? PhysicsParticleS[index] : nullptr;
		case PhysicsObjectEnum::circle:
			return (index < PhysicsCircleS.size()) ? PhysicsCircleS[index] : nullptr;
		case PhysicsObjectEnum::line:
			return (index < PhysicsLineS.size()) ? PhysicsLineS[index] : nullptr;

		default:
			return nullptr;
		}
	}

} // namespace PhysicsBlock3D

#endif // PhysicsBlock_Serialization3D
