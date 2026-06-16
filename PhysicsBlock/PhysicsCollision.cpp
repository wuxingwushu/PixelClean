#include "PhysicsCollision.hpp"
#include "PhysicsWorld.hpp"
#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    void PhysicsCollision::SetCollisionLayers(PhysicsFormwork *object, LayerMask layers)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        GetOrCreateBinding(object).Layers = layers;
    }

    LayerMask PhysicsCollision::GetCollisionLayers(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        return GetBinding(object).Layers;
    }

    void PhysicsCollision::SetCollisionPriority(PhysicsFormwork *object, int priority)
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

    int PhysicsCollision::GetCollisionPriority(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        return GetBinding(object).Priority;
    }

    void PhysicsCollision::AddCollisionEnterListener(PhysicsFormwork *object, CollisionCallback callback)
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

    void PhysicsCollision::AddCollisionStayListener(PhysicsFormwork *object, CollisionCallback callback)
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

    void PhysicsCollision::AddCollisionExitListener(PhysicsFormwork *object, CollisionCallback callback)
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

    void PhysicsCollision::RemoveAllCollisionListeners(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mBindings.erase(object);
    }

    void PhysicsCollision::RemoveCollisionBinding(PhysicsFormwork *object)
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

    void PhysicsCollision::AddMapCollisionEnterListener(MapFormwork *object, MapCollisionCallback callback)
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

    void PhysicsCollision::AddMapCollisionStayListener(MapFormwork *object, MapCollisionCallback callback)
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

    void PhysicsCollision::AddMapCollisionExitListener(MapFormwork *object, MapCollisionCallback callback)
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

    void PhysicsCollision::RemoveAllMapCollisionListeners(MapFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        mMapBindings.erase(object);
    }

    void PhysicsCollision::RemoveMapCollisionBinding(MapFormwork *object)
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

    void PhysicsCollision::AddCollisionPair(BaseArbiter *arbiterKey)
    {
        if (arbiterKey->mArbiterType >= PhysicsArbiterType::ArbiterSS)
        {
            auto Ait = mBindings.find((PhysicsFormwork *) (arbiterKey->mOriginalObject1));
            auto Bit = mBindings.find((PhysicsFormwork *) (arbiterKey->mOriginalObject2));

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
                Ait->second.OnEnter(Ait->first,
                    (const PhysicsFormwork *)(arbiterKey->mOriginalObject2),
                    arbiterKey);
            }
            if (B_bound && Bit->second.OnEnter)
            {
                // 对象 B 的 OnEnter：参数 (objA, objB, arbiter)
                Bit->second.OnEnter(Ait->first,
                    (const PhysicsFormwork *)(arbiterKey->mOriginalObject2),
                    arbiterKey);
            }

            // 单方有绑定时也创建 stay 记录（用于 ProcessCollisions 持续回调）
            bool A_stay = A_bound && (Ait->second.OnStay != nullptr);
            bool B_stay = B_bound && (Bit->second.OnStay != nullptr);
            if (A_stay || B_stay)
            {
                CollisionArbiter stayArbiter;
                stayArbiter.arbiter = arbiterKey;
                if (A_bound)
                    stayArbiter.A_CollisionBinding = Ait->second;
                if (B_bound)
                    stayArbiter.B_CollisionBinding = Bit->second;
                mCollisionArbiterStayS.push_back(stayArbiter);
            }
            return;
        }

        PhysicsFormwork *physObj = (PhysicsFormwork *)(arbiterKey->mOriginalObject1);
        MapFormwork *mapObj = (MapFormwork *)(arbiterKey->mOriginalObject2);

        // 对于粒子-地图碰撞，触发粒子地形碰撞回调（统一由 PhysicsCollision 管理）
        // 从 contacts[0] 构造 BulletHitInfo，让回调层拿到精确碰撞位置和法向量
        if (arbiterKey->mArbiterType == PhysicsArbiterType::ArbiterP && arbiterKey->numContacts > 0) {
            PhysicsParticle* particle = (PhysicsParticle*)physObj;

            BulletHitInfo hitInfo;
            hitInfo.WorldPos   = arbiterKey->contacts[0].position;
            hitInfo.Normal     = arbiterKey->contacts[0].normal;
            hitInfo.Friction   = arbiterKey->contacts[0].friction;
            hitInfo.Separation = arbiterKey->contacts[0].separation;

            // 从轴对齐法向量反推 Bresenham 碰撞边方向
            hitInfo.Direction = DirectionFromNormal(arbiterKey->contacts[0].normal);

            // GridPos = WorldPos + centrality：世界坐标 → 地图网格坐标
            Vec2_ cen = mapObj->FMGetCentrality();
            hitInfo.GridPos = glm::ivec2(
                (int)(hitInfo.WorldPos.x + cen.x),
                (int)(hitInfo.WorldPos.y + cen.y)
            );

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

        if (Mit->second.OnStay != nullptr) {
            MapCollisionArbiter stayArbiter = {Mit->second, arbiterKey};
            mMapCollisionArbiterStayS.push_back(stayArbiter);
        }
    }

    void PhysicsCollision::ProcessCollisions()
    {
        for (auto &arbiter : mCollisionArbiterStayS)
        {
            if (arbiter.A_CollisionBinding.OnStay)
            {
                arbiter.A_CollisionBinding.OnStay((PhysicsFormwork *) (arbiter.arbiter->mOriginalObject1), (PhysicsFormwork *) (arbiter.arbiter->mOriginalObject2), arbiter.arbiter);
            }
            if (arbiter.B_CollisionBinding.OnStay)
            {
                arbiter.B_CollisionBinding.OnStay((PhysicsFormwork *) (arbiter.arbiter->mOriginalObject1), (PhysicsFormwork *) (arbiter.arbiter->mOriginalObject2), arbiter.arbiter);
            }
        }

        for (auto &mapArbiter : mMapCollisionArbiterStayS)
        {
            if (mapArbiter.MapBinding.OnStay)
            {
                mapArbiter.MapBinding.OnStay((PhysicsFormwork *) (mapArbiter.arbiter->mOriginalObject1), (MapFormwork *) (mapArbiter.arbiter->mOriginalObject2), mapArbiter.arbiter);
            }
        }
    }

    // 移除碰撞对
    void PhysicsCollision::RemoveCollisionPair(BaseArbiter *arbiterKey)
    {
        for (unsigned int i = 0; i < mCollisionArbiterStayS.size(); ++i)
        {
            if (mCollisionArbiterStayS[i].arbiter == arbiterKey)
            {
                if (mCollisionArbiterStayS[i].A_CollisionBinding.OnExit)
                {
                    mCollisionArbiterStayS[i].A_CollisionBinding.OnExit((PhysicsFormwork *) (arbiterKey->mOriginalObject1), (PhysicsFormwork *) (arbiterKey->mOriginalObject2), arbiterKey);
                }
                if (mCollisionArbiterStayS[i].B_CollisionBinding.OnExit)
                {
                    mCollisionArbiterStayS[i].B_CollisionBinding.OnExit((PhysicsFormwork *) (arbiterKey->mOriginalObject1), (PhysicsFormwork *) (arbiterKey->mOriginalObject2), arbiterKey);
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
                    mMapCollisionArbiterStayS[i].MapBinding.OnExit((PhysicsFormwork *) (arbiterKey->mOriginalObject1), (MapFormwork *) (arbiterKey->mOriginalObject2), arbiterKey);
                }

                mMapCollisionArbiterStayS[i] = mMapCollisionArbiterStayS[mMapCollisionArbiterStayS.size() - 1];
                mMapCollisionArbiterStayS.pop_back();
                break;
            }
        }
    }

    // ========== 地形碰撞命中回调实现 ==========

    void PhysicsCollision::AddTerrainHitListener(PhysicsParticle *particle, TerrainHitCallback callback, void* userData)
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

    void PhysicsCollision::RemoveTerrainHitListener(PhysicsParticle *particle)
    {
        if (particle == nullptr)
        {
            throw ArgumentNullException("particle");
        }
        mTerrainHitBindings.erase(particle);
    }

    void PhysicsCollision::RemoveAllTerrainHitListeners()
    {
        mTerrainHitBindings.clear();
    }

    void PhysicsCollision::OnTerrainHit(PhysicsParticle *particle, const BulletHitInfo& hitInfo)
    {
        auto it = mTerrainHitBindings.find(particle);
        if (it != mTerrainHitBindings.end() && it->second.OnHit)
        {
            it->second.OnHit(hitInfo, particle, it->second.userData);
        }
    }

    // ========== 地图碰撞状态变化回调实现 ==========

    void PhysicsCollision::AddMapCollisionChangeListener(MapFormwork *map, MapCollisionChangeCallback callback, void* userData)
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

    void PhysicsCollision::RemoveMapCollisionChangeListener(MapFormwork *map)
    {
        if (map == nullptr)
        {
            throw ArgumentNullException("map");
        }
        mMapCollisionChangeBindings.erase(map);
    }

    void PhysicsCollision::RemoveAllMapCollisionChangeListeners()
    {
        mMapCollisionChangeBindings.clear();
    }

    void PhysicsCollision::OnMapCollisionChanged(MapFormwork *map, glm::ivec2 pos, bool newState)
    {
        auto it = mMapCollisionChangeBindings.find(map);
        if (it != mMapCollisionChangeBindings.end() && it->second.OnChange)
        {
            it->second.OnChange(pos, newState, it->second.userData);
        }
    }

    void PhysicsCollision::Clear()
    {
        mBindings.clear();
        mCollisionArbiterStayS.clear();
        mMapBindings.clear();
        mMapCollisionArbiterStayS.clear();
        mTerrainHitBindings.clear();
        mMapCollisionChangeBindings.clear();
    }

}