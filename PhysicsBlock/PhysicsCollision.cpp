#include "PhysicsCollision.hpp"
#include "PhysicsWorld.hpp"

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
            if (Ait == mBindings.end())
            {
                return;
            }
            auto Bit = mBindings.find((PhysicsFormwork *) (arbiterKey->mOriginalObject2));
            if (Bit == mBindings.end())
            {
                return;
            }

            if (Bit->second.Layers & Ait->second.Layers) {

                if (Ait->second.OnEnter)
                {   
                    Ait->second.OnEnter(Ait->first, Bit->first, arbiterKey);
                }
                
                if (Bit->second.OnEnter)
                {
                    Bit->second.OnEnter(Ait->first, Bit->first, arbiterKey);
                }

                if ((Ait->second.OnStay != nullptr) || Bit->second.OnStay != nullptr) {
                    CollisionArbiter stayArbiter = {Ait->second, Bit->second, arbiterKey};
                    mCollisionArbiterStayS.push_back(stayArbiter);
                }
            }
            return;
        }

        PhysicsFormwork *physObj = (PhysicsFormwork *)(arbiterKey->mOriginalObject1);
        MapFormwork *mapObj = (MapFormwork *)(arbiterKey->mOriginalObject2);

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

    void PhysicsCollision::Clear()
    {
        mBindings.clear();
        mCollisionArbiterStayS.clear();
        mMapBindings.clear();
        mMapCollisionArbiterStayS.clear();
    }

}