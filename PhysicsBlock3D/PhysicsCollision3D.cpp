#include "PhysicsCollision3D.hpp"
#include "PhysicsParticle3D.hpp"
#include "MapFormwork3D.hpp"

namespace PhysicsBlock3D
{

    void PhysicsCollision3D::SetCollisionLayers(PhysicsFormwork3D *object, LayerMask layers)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        GetOrCreateBinding(object).Layers = layers;
    }

    LayerMask PhysicsCollision3D::GetCollisionLayers(PhysicsFormwork3D *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        return GetBinding(object).Layers;
    }

    void PhysicsCollision3D::SetCollisionPriority(PhysicsFormwork3D *object, int priority)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (priority < 0 || priority > 100)
        {
            throw ArgumentOutOfRangeException("priority",
                "Priority must be between 0 and 100. Received: " + std::to_string(priority));
        }
        GetOrCreateBinding(object).Priority = priority;
    }

    int PhysicsCollision3D::GetCollisionPriority(PhysicsFormwork3D *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        return GetBinding(object).Priority;
    }

    void PhysicsCollision3D::AddCollisionEnterListener(PhysicsFormwork3D *object, CollisionCallback callback)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }

        GetOrCreateBinding(object).OnEnter = std::move(callback);
    }

    void PhysicsCollision3D::AddCollisionStayListener(PhysicsFormwork3D *object, CollisionCallback callback)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        GetOrCreateBinding(object).OnStay = std::move(callback);
    }

    void PhysicsCollision3D::AddCollisionExitListener(PhysicsFormwork3D *object, CollisionCallback callback)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        GetOrCreateBinding(object).OnExit = std::move(callback);
    }

    void PhysicsCollision3D::RemoveAllCollisionListeners(PhysicsFormwork3D *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mBindings.erase(object);
    }

    void PhysicsCollision3D::RemoveCollisionBinding(PhysicsFormwork3D *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        auto it = mBindings.find(object);
        if (it != mBindings.end())
        {
            it->second.Layers = LayerMaskAll;
            it->second.Priority = 50;
        }
    }

    void PhysicsCollision3D::AddMapCollisionEnterListener(MapFormwork3D *object, MapCollisionCallback callback)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        GetOrCreateMapBinding(object).OnEnter = std::move(callback);
    }

    void PhysicsCollision3D::AddMapCollisionStayListener(MapFormwork3D *object, MapCollisionCallback callback)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        GetOrCreateMapBinding(object).OnStay = std::move(callback);
    }

    void PhysicsCollision3D::AddMapCollisionExitListener(MapFormwork3D *object, MapCollisionCallback callback)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        GetOrCreateMapBinding(object).OnExit = std::move(callback);
    }

    void PhysicsCollision3D::RemoveAllMapCollisionListeners(MapFormwork3D *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mMapBindings.erase(object);
    }

    void PhysicsCollision3D::RemoveMapCollisionBinding(MapFormwork3D *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        auto it = mMapBindings.find(object);
        if (it != mMapBindings.end())
        {
            it->second.Layers = LayerMaskAll;
            it->second.Priority = 50;
        }
    }

    void PhysicsCollision3D::AddCollisionPair(BaseArbiter3D *arbiterKey)
    {
        if (arbiterKey == nullptr)
        {
            return;
        }

        if (arbiterKey->mArbiterType >= PhysicsArbiterType3D::ArbiterSS3D)
        {
            PhysicsFormwork3D *objA = (PhysicsFormwork3D *)(arbiterKey->mOriginalObject1);
            PhysicsFormwork3D *objB = (PhysicsFormwork3D *)(arbiterKey->mOriginalObject2);

            auto Ait = mBindings.find(objA);
            auto Bit = mBindings.find(objB);

            bool A_bound = (Ait != mBindings.end());
            bool B_bound = (Bit != mBindings.end());

            if (!A_bound && !B_bound) return;   // 双方都没有绑定，跳过

            // 层级掩码检查：只有双方都有绑定时才做交集检查；单方有绑定则放行
            bool layerOk = true;
            if (A_bound && B_bound)
            {
                layerOk = (Bit->second.Layers & Ait->second.Layers) != 0;
            }
            if (!layerOk) return;

            if (A_bound && Ait->second.OnEnter)
            {
                // 对象 A 的 OnEnter：参数 (objA, objB, arbiter)
                Ait->second.OnEnter(objA, objB, arbiterKey);
            }
            if (B_bound && Bit->second.OnEnter)
            {
                // 对象 B 的 OnEnter：参数 (objA, objB, arbiter)
                // 修复 2D 缺陷：2D 此处误用 Ait->first，当只有 B 有绑定时 Ait == end()，
                // 对其取 ->first 属未定义行为。3D 直接使用已解析好的 objA。
                Bit->second.OnEnter(objA, objB, arbiterKey);
            }

            // 单方有绑定时也创建 stay 记录（用于 ProcessCollisions 持续回调）
            bool A_stay = A_bound && (Ait->second.OnStay != nullptr);
            bool B_stay = B_bound && (Bit->second.OnStay != nullptr);
            if (A_stay || B_stay)
            {
                CollisionArbiter3D stayArbiter;
                stayArbiter.arbiter = arbiterKey;
                if (A_bound)
                    stayArbiter.A_CollisionBinding = Ait->second;
                if (B_bound)
                    stayArbiter.B_CollisionBinding = Bit->second;
                mCollisionArbiterStayS.push_back(stayArbiter);
            }
            return;
        }

        PhysicsFormwork3D *physObj = (PhysicsFormwork3D *)(arbiterKey->mOriginalObject1);
        MapFormwork3D *mapObj = (MapFormwork3D *)(arbiterKey->mOriginalObject2);

        // 对于粒子-地图碰撞，触发粒子地形碰撞回调（统一由 PhysicsCollision3D 管理）
        // 从 contacts[0] 构造 BulletHitInfo3D，让回调层拿到精确碰撞位置和法向量
        if (arbiterKey->mArbiterType == PhysicsArbiterType3D::ArbiterP3D && arbiterKey->numContacts > 0)
        {
            PhysicsParticle3D *particle = (PhysicsParticle3D *)physObj;

            BulletHitInfo3D hitInfo;
            hitInfo.WorldPos = arbiterKey->contacts[0].position;
            hitInfo.Normal = arbiterKey->contacts[0].normal;
            hitInfo.Friction = arbiterKey->contacts[0].friction;
            hitInfo.Separation = arbiterKey->contacts[0].separation;

            // 从轴向法向量反推 DDA 碰撞面方向
            hitInfo.Direction = DirectionFromNormal(arbiterKey->contacts[0].normal);

            // GridPos = WorldPos + centrality：世界坐标 → 地图网格坐标（三维）
            Vec3_ cen = mapObj->FMGetCentrality();
            hitInfo.GridPos = IVec3_(
                (int)(hitInfo.WorldPos.x + cen.x),
                (int)(hitInfo.WorldPos.y + cen.y),
                (int)(hitInfo.WorldPos.z + cen.z));

            this->OnTerrainHit(particle, hitInfo);
        }

        auto Mit = mMapBindings.find(mapObj);
        if (Mit == mMapBindings.end())
        {
            return;
        }

        if (Mit->second.OnEnter)
        {
            Mit->second.OnEnter(physObj, mapObj, arbiterKey);
        }

        if (Mit->second.OnStay != nullptr)
        {
            MapCollisionArbiter3D stayArbiter = {Mit->second, arbiterKey};
            mMapCollisionArbiterStayS.push_back(stayArbiter);
        }
    }

    void PhysicsCollision3D::ProcessCollisions()
    {
        for (auto &arbiter : mCollisionArbiterStayS)
        {
            PhysicsFormwork3D *objA = (PhysicsFormwork3D *)(arbiter.arbiter->mOriginalObject1);
            PhysicsFormwork3D *objB = (PhysicsFormwork3D *)(arbiter.arbiter->mOriginalObject2);

            if (arbiter.A_CollisionBinding.OnStay)
            {
                arbiter.A_CollisionBinding.OnStay(objA, objB, arbiter.arbiter);
            }
            if (arbiter.B_CollisionBinding.OnStay)
            {
                arbiter.B_CollisionBinding.OnStay(objA, objB, arbiter.arbiter);
            }
        }

        for (auto &mapArbiter : mMapCollisionArbiterStayS)
        {
            if (mapArbiter.MapBinding.OnStay)
            {
                mapArbiter.MapBinding.OnStay(
                    (PhysicsFormwork3D *)(mapArbiter.arbiter->mOriginalObject1),
                    (MapFormwork3D *)(mapArbiter.arbiter->mOriginalObject2),
                    mapArbiter.arbiter);
            }
        }
    }

    void PhysicsCollision3D::RemoveCollisionPair(BaseArbiter3D *arbiterKey)
    {
        if (arbiterKey == nullptr)
        {
            return;
        }

        for (unsigned int i = 0; i < mCollisionArbiterStayS.size(); ++i)
        {
            if (mCollisionArbiterStayS[i].arbiter == arbiterKey)
            {
                PhysicsFormwork3D *objA = (PhysicsFormwork3D *)(arbiterKey->mOriginalObject1);
                PhysicsFormwork3D *objB = (PhysicsFormwork3D *)(arbiterKey->mOriginalObject2);

                if (mCollisionArbiterStayS[i].A_CollisionBinding.OnExit)
                {
                    mCollisionArbiterStayS[i].A_CollisionBinding.OnExit(objA, objB, arbiterKey);
                }
                if (mCollisionArbiterStayS[i].B_CollisionBinding.OnExit)
                {
                    mCollisionArbiterStayS[i].B_CollisionBinding.OnExit(objA, objB, arbiterKey);
                }

                mCollisionArbiterStayS[i] = mCollisionArbiterStayS[mCollisionArbiterStayS.size() - 1];
                mCollisionArbiterStayS.pop_back();
                break;
            }
        }

        for (unsigned int i = 0; i < mMapCollisionArbiterStayS.size(); ++i)
        {
            if (mMapCollisionArbiterStayS[i].arbiter == arbiterKey)
            {
                if (mMapCollisionArbiterStayS[i].MapBinding.OnExit)
                {
                    mMapCollisionArbiterStayS[i].MapBinding.OnExit(
                        (PhysicsFormwork3D *)(arbiterKey->mOriginalObject1),
                        (MapFormwork3D *)(arbiterKey->mOriginalObject2),
                        arbiterKey);
                }

                mMapCollisionArbiterStayS[i] = mMapCollisionArbiterStayS[mMapCollisionArbiterStayS.size() - 1];
                mMapCollisionArbiterStayS.pop_back();
                break;
            }
        }
    }

    // ========== 地形碰撞命中回调实现 ==========

    void PhysicsCollision3D::AddTerrainHitListener(PhysicsParticle3D *particle, TerrainHitCallback callback, void *userData)
    {
        if (particle == nullptr)
        {
            throw ArgumentNullException("particle");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        mTerrainHitBindings[particle].OnHit = std::move(callback);
        mTerrainHitBindings[particle].userData = userData;
    }

    void PhysicsCollision3D::RemoveTerrainHitListener(PhysicsParticle3D *particle)
    {
        if (particle == nullptr)
        {
            throw ArgumentNullException("particle");
        }
        mTerrainHitBindings.erase(particle);
    }

    void PhysicsCollision3D::RemoveAllTerrainHitListeners()
    {
        mTerrainHitBindings.clear();
    }

    void PhysicsCollision3D::OnTerrainHit(PhysicsParticle3D *particle, const BulletHitInfo3D &hitInfo)
    {
        auto it = mTerrainHitBindings.find(particle);
        if (it != mTerrainHitBindings.end() && it->second.OnHit)
        {
            it->second.OnHit(hitInfo, particle, it->second.userData);
        }
    }

    // ========== 地图碰撞状态变化回调实现 ==========

    void PhysicsCollision3D::AddMapCollisionChangeListener(MapFormwork3D *map, MapCollisionChangeCallback callback, void *userData)
    {
        if (map == nullptr)
        {
            throw ArgumentNullException("map");
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }
        mMapCollisionChangeBindings[map].OnChange = std::move(callback);
        mMapCollisionChangeBindings[map].userData = userData;
    }

    void PhysicsCollision3D::RemoveMapCollisionChangeListener(MapFormwork3D *map)
    {
        if (map == nullptr)
        {
            throw ArgumentNullException("map");
        }
        mMapCollisionChangeBindings.erase(map);
    }

    void PhysicsCollision3D::RemoveAllMapCollisionChangeListeners()
    {
        mMapCollisionChangeBindings.clear();
    }

    void PhysicsCollision3D::OnMapCollisionChanged(MapFormwork3D *map, IVec3_ pos, bool newState)
    {
        auto it = mMapCollisionChangeBindings.find(map);
        if (it != mMapCollisionChangeBindings.end() && it->second.OnChange)
        {
            it->second.OnChange(pos, newState, it->second.userData);
        }
    }

    void PhysicsCollision3D::Clear()
    {
        mBindings.clear();
        mCollisionArbiterStayS.clear();
        mMapBindings.clear();
        mMapCollisionArbiterStayS.clear();
        mTerrainHitBindings.clear();
        mMapCollisionChangeBindings.clear();
    }

}
