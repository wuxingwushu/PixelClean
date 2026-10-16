#pragma once
#include "BaseDefine3D.h"
#include "BaseStruct3D.hpp"
#include <functional>
#include "BaseSerialization3D.hpp"

namespace PhysicsBlock3D
{
	/**
	 * @brief 碰撞点结构体（3D）
	 * @details 存储两个物体碰撞时的详细信息
	 *
	 * @note 由 2D 版 Contact 改写。**主要维度变化**：
	 *       ① `position`/`normal`/`r1`/`r2` 由 Vec2_ 变 Vec3_；
	 *       ② 单一 `Pt` 切向冲量 → **两个正交切向分量 `Pt1`/`Pt2`**，
	 *          因为 3D 摩擦是一个**圆盘约束**（F ≤ μN 在切平面内），
	 *          而非 2D 的 1D 区间钳制；
	 *       ③ 新增 `tangent1`/`tangent2` 正交切向基（由 PreStep 用
	 *          `BuildTangentBasis` 生成并缓存，避免 ApplyImpulse 重算）；
	 *       ④ `w_side` 语义由"小矩形边的分离标识"泛化为
	 *          **特征 ID（feature id）**，用于跨帧匹配接触点、继承累积冲量。
	 */
	struct Contact3D
	{
		/**
		 * @brief 默认构造函数
		 * @details 初始化所有冲量累积量为 0
		 */
		Contact3D() : Pn(0.0f), Pt1(0.0f), Pt2(0.0f) {}

		// ========== Collide 阶段数据 ==========
		Vec3_ position;           ///< 碰撞点位置（世界坐标）
		Vec3_ normal;             ///< 最佳分离轴的法向量（从物体A指向物体B）
		FLOAT_ separation;        ///< 最小分离距离（负值表示重叠）
		FLOAT_ friction = 0.2;    ///< 两个物体间的摩擦系数
		unsigned char w_side = 0; ///< 特征 ID（用于跨帧匹配接触点、继承冲量）

		// ========== PreStep 阶段数据 ==========
		FLOAT_ massNormal;   ///< 法向有效质量
		FLOAT_ massTangent1; ///< 切向1 有效质量
		FLOAT_ massTangent2; ///< 切向2 有效质量
		FLOAT_ bias;         ///< 物体位置修正值（用于位置约束）

		// ========== ApplyImpulse 阶段数据 ==========
		Vec3_ r1;             ///< A物体质心指向碰撞点的向量
		Vec3_ r2;             ///< B物体质心指向碰撞点的向量
		Vec3_ tangent1;       ///< 正交切向基 #1（单位向量，与 normal 正交）
		Vec3_ tangent2;       ///< 正交切向基 #2（= normal × tangent1）
		FLOAT_ Pn = 0;        ///< 累积的法向冲量
		FLOAT_ Pt1 = 0;       ///< 累积的切向冲量 #1
		FLOAT_ Pt2 = 0;       ///< 累积的切向冲量 #2
	};

	/**
	 * @brief 仲裁器键结构体（3D）
	 * @details 用于唯一标识一对碰撞物体，内部保证 object1 >= object2
	 *
	 * @note 与维度无关，逻辑与 2D 版完全一致。修正了 2D 注释中
	 *       "保证 object1 <= object2" 与实现（大指针在 object1）不符的问题。
	 */
	struct ArbiterKey3D
	{
		/**
		 * @brief 构造函数
		 * @param Object1 第一个物体指针
		 * @param Object2 第二个物体指针
		 * @details 自动将较大的指针赋值给 object1（内部保证 object1 >= object2）
		 */
		ArbiterKey3D(void *Object1, void *Object2)
		{
			if (Object1 > Object2)
			{
				object1 = Object1;
				object2 = Object2;
			}
			else
			{
				object1 = Object2;
				object2 = Object1;
			}
		}

		void *object1; ///< 碰撞的第一个对象（指针值较大）
		void *object2; ///< 碰撞的第二个对象（指针值较小）
		char PoolID;   ///< 对象池 ID

		/**
		 * @brief 小于运算符重载
		 * @param a1 要比较的 ArbiterKey3D
		 * @return 如果 this < a1 返回 true
		 */
		inline bool operator<(const ArbiterKey3D &a1) const
		{
			if (a1.object1 < object1)
				return true;

			if (a1.object1 == object1 && a1.object2 < object2)
				return true;

			return false;
		}

		/**
		 * @brief 等于运算符重载
		 * @param a1 要比较的 ArbiterKey3D
		 * @return 如果两个键相等返回 true
		 */
		inline bool operator==(const ArbiterKey3D &a1) const
		{
			return (a1.object1 == object1) && (a1.object2 == object2);
		}
	};

	/**
	 * @brief ArbiterKey3D 的哈希函数
	 * @details 用于在 unordered_map 中作为键时计算哈希值
	 */
	struct ArbiterKey3DHash
	{
		/**
		 * @brief 哈希运算符重载
		 * @param key 要计算哈希的 ArbiterKey3D
		 * @return 哈希值
		 * @details 使用 hash_combine 技术组合两个对象指针的哈希值
		 */
		inline std::size_t operator()(const ArbiterKey3D &key) const
		{
			auto hash_combine = [](std::size_t a, std::size_t b)
			{
				return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
			};

			std::size_t h1 = std::hash<void *>{}(key.object1);
			std::size_t h2 = std::hash<void *>{}(key.object2);
			return hash_combine(h1, h2);
		}
	};

	/**
	 * @brief 裁决器类型枚举（3D）
	 * @details 与 2D 版成员完全一致，仅重命名以区分
	 */
	enum PhysicsArbiterType3D
	{
		ArbiterS3D,
		ArbiterP3D,
		ArbiterC3D,
		ArbiterL3D,
		ArbiterSS3D,
		ArbiterSP3D,
		ArbiterCS3D,
		ArbiterCP3D,
		ArbiterCC3D,
		ArbiterLS3D,
		ArbiterLC3D,
		ArbiterLP3D,
	};

	class BaseArbiter3D; // 前向声明：CollideOutput3D / CollideGroupEntry3D 需要它

	/** @brief 内存池线程槽上限（与 PhysicsWorld3D.hpp 的 kMaxPoolThreads3D 一致） */
	constexpr unsigned kMaxPoolThreads3D = 9;
	/** @brief 主线程使用的内存池槽（最后一个） */
	constexpr unsigned kMainThreadPoolIndex3D = kMaxPoolThreads3D - 1;

	/**
	 * @brief 单线程的碰撞检测输出缓冲（3D）
	 * @details 多线程碰撞检测时每个工作线程写自己的缓冲，主线程在
	 *          ResolveCollideGroup 阶段统一合并，避免加锁。
	 *          `mThreadIndex` 同时用于选择该线程专属的裁决器内存池。
	 *
	 * @note 结构与 2D 版 CollideOutput 完全同构，仅类型换成 3D 版。
	 */
	struct CollideOutput3D
	{
		/** @brief 该输出所属的工作线程编号（同时是内存池下标） */
		unsigned char mThreadIndex = (unsigned char)kMainThreadPoolIndex3D;
		/** @brief 本帧新登记的裁决器（所有权移交全局表） */
		std::vector<BaseArbiter3D *> newGroup;
		/** @brief 本帧待删除的裁决器键 */
		std::vector<ArbiterKey3D> deleteGroup;

		/** @brief 清空两个队列（保留 mThreadIndex） */
		void clear()
		{
			newGroup.clear();
			deleteGroup.clear();
		}
	};

	/**
	 * @brief 碰撞对表项（3D）
	 * @details 把裁决器指针与其在 CollideGroupVector 中的下标绑在一起，
	 *          使删除时可以 O(1) swap-and-pop 并修正被移动元素的下标。
	 */
	struct CollideGroupEntry3D
	{
		/** @brief 该碰撞对的裁决器 */
		BaseArbiter3D *arbiter = nullptr;
		/** @brief 该裁决器在 CollideGroupVector 中的下标 */
		int vectorIndex = -1;
	};

	/**
	 * @brief 基础物理裁决器类（3D）
	 * @details 负责处理两个物理对象之间的碰撞响应
	 *          是所有具体裁决器的基类，定义了碰撞处理的接口
	 *
	 * @note 与 2D 版相比，**接口签名完全不变**（仍是 Update/PreStep/ApplyImpulse），
	 *       只是 Contact 换成了 3D 版本。这个类的抽象层次与维度无关，
	 *       真正需要重写的是四个派生求解器里的有效质量与摩擦求解数学。
	 */
	class BaseArbiter3D SerializationInherit3D_
	{
#if PhysicsBlock_Serialization3D
	public:
		/**
		 * @brief 将碰撞信息序列化为 JSON
		 * @param data 输出的 JSON 对象
		 */
		virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
		{
			auto &dataContacts = data["contacts"];
			dataContacts = dataContacts.array();
			for (size_t i = 0; i < numContacts; ++i)
			{
				dataContacts[i]["position"]["x"] = contacts[i].position.x;
				dataContacts[i]["position"]["y"] = contacts[i].position.y;
				dataContacts[i]["position"]["z"] = contacts[i].position.z;
				dataContacts[i]["normal"]["x"] = contacts[i].normal.x;
				dataContacts[i]["normal"]["y"] = contacts[i].normal.y;
				dataContacts[i]["normal"]["z"] = contacts[i].normal.z;
				dataContacts[i]["separation"] = contacts[i].separation;
				dataContacts[i]["friction"] = contacts[i].friction;
				dataContacts[i]["w_side"] = contacts[i].w_side;
				dataContacts[i]["Pn"] = contacts[i].Pn;
				dataContacts[i]["Pt1"] = contacts[i].Pt1;
				dataContacts[i]["Pt2"] = contacts[i].Pt2;
			}
		}

		/**
		 * @brief 从 JSON 反序列化碰撞信息
		 * @param data 输入的 JSON 对象
		 */
		virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
		{
			numContacts = data["contacts"].size();
			auto dataContacts = data["contacts"];
			for (size_t i = 0; i < numContacts; ++i)
			{
				contacts[i].position = Vec3_(dataContacts[i]["position"]["x"].get<FLOAT_>(),
				                             dataContacts[i]["position"]["y"].get<FLOAT_>(),
				                             dataContacts[i]["position"]["z"].get<FLOAT_>());
				contacts[i].normal = Vec3_(dataContacts[i]["normal"]["x"].get<FLOAT_>(),
				                           dataContacts[i]["normal"]["y"].get<FLOAT_>(),
				                           dataContacts[i]["normal"]["z"].get<FLOAT_>());
				contacts[i].separation = dataContacts[i]["separation"];
				contacts[i].friction = dataContacts[i]["friction"];
				contacts[i].w_side = dataContacts[i]["w_side"];
				contacts[i].Pn = dataContacts[i]["Pn"];
				contacts[i].Pt1 = dataContacts[i]["Pt1"];
				contacts[i].Pt2 = dataContacts[i]["Pt2"];
			}
		}

		/**
		 * @brief 获取裁决器类型
		 * @return 裁决器类型标识
		 */
		PhysicsArbiterType3D GetArbiterType() { return mArbiterType; }
#endif

	public:
		/// 碰撞点集合，最多支持 PhysicsContactMaxSize3D 个碰撞点
		Contact3D contacts[PhysicsContactMaxSize3D];
		int numContacts = 0; ///< 当前有效碰撞点数量

		ArbiterKey3D key; ///< 两个碰撞对象的唯一标识符

		void *mOriginalObject1; ///< 原始顺序的第一个碰撞对象指针
		void *mOriginalObject2; ///< 原始顺序的第二个碰撞对象指针
		unsigned char mAllocThreadIndex = 0; ///< 分配此裁决器的线程池索引
		PhysicsArbiterType3D mArbiterType = PhysicsArbiterType3D::ArbiterS3D; ///< 裁决器具体类型

	public:
		/**
		 * @brief 构造函数
		 * @param Object1 第一个碰撞对象
		 * @param Object2 第二个碰撞对象
		 */
		BaseArbiter3D(void *Object1, void *Object2) : key(Object1, Object2),
		                                              mOriginalObject1(Object1), mOriginalObject2(Object2){};

		/**
		 * @brief 析构函数
		 */
		virtual ~BaseArbiter3D(){};

		/**
		 * @brief 计算两个物体的碰撞点
		 */
		virtual void ComputeCollide() = 0;

		/**
		 * @brief 更新碰撞信息
		 * @param NewContacts 新的碰撞点数组
		 * @param numNewContacts 新碰撞点数量
		 */
		virtual void Update(Contact3D *NewContacts, int numNewContacts) = 0;

		/**
		 * @brief 预处理（带时间步长）
		 * @param inv_dt 时间步长的倒数
		 */
		virtual void PreStep(FLOAT_ inv_dt) = 0;

		/**
		 * @brief 预处理（无参数版本）
		 */
		virtual void PreStep() = 0;

		/**
		 * @brief 应用冲量
		 */
		virtual void ApplyImpulse() = 0;
	};

}
