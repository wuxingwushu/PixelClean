#pragma once
#include "PhysicsBaseArbiter3D.hpp"
#include "PhysicsShape3D.hpp"
#include "PhysicsParticle3D.hpp"
#include "PhysicsCircle3D.hpp"
#include "PhysicsLine3D.hpp"
#include "MapFormwork3D.hpp"

namespace PhysicsBlock3D
{

	// =========================================================================
	// 具体裁决器（12 个）
	// =========================================================================
	//
	// 由 2D `PhysicsArbiter.hpp/.cpp`（155/103 行）改写。2D 的 12 个类全是
	// 「继承对应 BaseArbiter 子类 + 构造里设 mArbiterType + 实现 ComputeCollide()」
	// 的极薄壳，ComputeCollide 是两行模板：
	//     numContacts = Collide(contacts, (具体类型*)object1, (具体类型*)object2);
	//     for (i < numContacts) contacts[i].w_side = i;
	// 3D 完全沿用该模式，仅把 Collide 换成 3D 重载。
	//
	// 【类名后缀含义（与 2D 一致）】
	//   S = Shape, P = Particle, C = Circle, L = Line
	//   两个字母 = 两类物体相碰；单个字母 = 该类物体 vs 地图。
	//
	// 【指针重排约定】
	//   2D 的 SS / CC 构造会按指针大小重排两对象
	//   （`Object1 > Object2 ? Object1 : Object2`），以保证 ArbiterKey 与
	//   Collide 的 (object1, object2) 顺序一致。3D 沿用同一约定。
	//
	// 【3D 对 w_side 的语义升级】
	//   2D 的 `w_side = i` 只是接触点下标（用于跨帧继承冲量）。
	//   3D 的窄相已在 PhysicsBaseCollide3D 内用几何特征哈希生成了稳定的
	//   FeatureId，故这里**不再覆盖** w_side，保留几何特征 ID 以获得
	//   跨帧稳定的冲量继承。仅当窄相未设置时才回退为下标。

	class PhysicsArbiterSS3D : public PhysicsBaseArbiterAA3D
	{
	public:
		PhysicsArbiterSS3D(PhysicsShape3D *Object1, PhysicsShape3D *Object2)
		    : PhysicsBaseArbiterAA3D(Object1 > Object2 ? (PhysicsAngle3D *)Object1 : (PhysicsAngle3D *)Object2,
		                             Object1 > Object2 ? (PhysicsAngle3D *)Object2 : (PhysicsAngle3D *)Object1)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterSS3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterSP3D : public PhysicsBaseArbiterAD3D
	{
	public:
		PhysicsArbiterSP3D(PhysicsShape3D *Object1, PhysicsParticle3D *Object2)
		    : PhysicsBaseArbiterAD3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterSP3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterS3D : public PhysicsBaseArbiterA3D
	{
	public:
		PhysicsArbiterS3D(PhysicsShape3D *Object1, MapFormwork3D *Object2)
		    : PhysicsBaseArbiterA3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterS3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterP3D : public PhysicsBaseArbiterD3D
	{
	public:
		PhysicsArbiterP3D(PhysicsParticle3D *Object1, MapFormwork3D *Object2)
		    : PhysicsBaseArbiterD3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterP3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterC3D : public PhysicsBaseArbiterA3D
	{
	public:
		PhysicsArbiterC3D(PhysicsCircle3D *Object1, MapFormwork3D *Object2)
		    : PhysicsBaseArbiterA3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterC3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterCS3D : public PhysicsBaseArbiterAA3D
	{
	public:
		PhysicsArbiterCS3D(PhysicsCircle3D *Object1, PhysicsShape3D *Object2)
		    : PhysicsBaseArbiterAA3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterCS3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterCP3D : public PhysicsBaseArbiterAD3D
	{
	public:
		PhysicsArbiterCP3D(PhysicsCircle3D *Object1, PhysicsParticle3D *Object2)
		    : PhysicsBaseArbiterAD3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterCP3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterCC3D : public PhysicsBaseArbiterAA3D
	{
	public:
		PhysicsArbiterCC3D(PhysicsCircle3D *Object1, PhysicsCircle3D *Object2)
		    : PhysicsBaseArbiterAA3D(Object1 > Object2 ? (PhysicsAngle3D *)Object1 : (PhysicsAngle3D *)Object2,
		                             Object1 > Object2 ? (PhysicsAngle3D *)Object2 : (PhysicsAngle3D *)Object1)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterCC3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterLC3D : public PhysicsBaseArbiterAA3D
	{
	public:
		PhysicsArbiterLC3D(PhysicsLine3D *Object1, PhysicsCircle3D *Object2)
		    : PhysicsBaseArbiterAA3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterLC3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterLP3D : public PhysicsBaseArbiterAD3D
	{
	public:
		PhysicsArbiterLP3D(PhysicsLine3D *Object1, PhysicsParticle3D *Object2)
		    : PhysicsBaseArbiterAD3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterLP3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterLS3D : public PhysicsBaseArbiterAA3D
	{
	public:
		PhysicsArbiterLS3D(PhysicsLine3D *Object1, PhysicsShape3D *Object2)
		    : PhysicsBaseArbiterAA3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterLS3D;
		}
		void ComputeCollide() override;
	};

	class PhysicsArbiterL3D : public PhysicsBaseArbiterA3D
	{
	public:
		PhysicsArbiterL3D(PhysicsLine3D *Object1, MapFormwork3D *Object2)
		    : PhysicsBaseArbiterA3D(Object1, Object2)
		{
			mArbiterType = PhysicsArbiterType3D::ArbiterL3D;
		}
		void ComputeCollide() override;
	};

}
