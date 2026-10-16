#include "PhysicsArbiter3D.hpp"
#include "PhysicsBaseCollide3D.hpp"

namespace PhysicsBlock3D
{

	// 说明：窄相 PhysicsBaseCollide3D 已为每个接触点写入几何特征 ID
	//（w_side = FeatureId(体素, 面索引)），PhysicsBaseArbiter3D 的
	// WarmStartByFeature 依赖它跨帧继承累积冲量。因此这里**不再**把
	// w_side 重编号为数组下标（2D 的 `w_side = i` 做法会破坏特征匹配）。
	// =========================================================================
	// 物体 vs 物体
	// =========================================================================

	void PhysicsArbiterSS3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsShape3D *>(object1),
		            static_cast<PhysicsShape3D *>(object2)));
	}

	void PhysicsArbiterSP3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsShape3D *>(object1),
		            static_cast<PhysicsParticle3D *>(object2)));
	}

	void PhysicsArbiterCS3D::ComputeCollide()
	{
		// object1 = Circle, object2 = Shape（窄相签名顺序为 Circle, Shape）
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsCircle3D *>(object1),
		            static_cast<PhysicsShape3D *>(object2)));
	}

	void PhysicsArbiterCP3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsCircle3D *>(object1),
		            static_cast<PhysicsParticle3D *>(object2)));
	}

	void PhysicsArbiterCC3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsCircle3D *>(object1),
		            static_cast<PhysicsCircle3D *>(object2)));
	}

	void PhysicsArbiterLC3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsLine3D *>(object1),
		            static_cast<PhysicsCircle3D *>(object2)));
	}

	void PhysicsArbiterLP3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsLine3D *>(object1),
		            static_cast<PhysicsParticle3D *>(object2)));
	}

	void PhysicsArbiterLS3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsLine3D *>(object1),
		            static_cast<PhysicsShape3D *>(object2)));
	}

	// =========================================================================
	// 物体 vs 地图
	// =========================================================================

	void PhysicsArbiterS3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsShape3D *>(object1),
		            static_cast<MapFormwork3D *>(object2)));
	}

	void PhysicsArbiterP3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsParticle3D *>(object1),
		            static_cast<MapFormwork3D *>(object2)));
	}

	void PhysicsArbiterC3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsCircle3D *>(object1),
		            static_cast<MapFormwork3D *>(object2)));
	}

	void PhysicsArbiterL3D::ComputeCollide()
	{
		numContacts = static_cast<int>(
		    Collide(contacts, static_cast<PhysicsLine3D *>(object1),
		            static_cast<MapFormwork3D *>(object2)));
	}

}
