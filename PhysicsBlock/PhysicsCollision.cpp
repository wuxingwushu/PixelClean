#include "PhysicsCollision.hpp"
#include "PhysicsWorld.hpp"
#include <sstream>

namespace PhysicsBlock
{

    void PhysicsCollision::SetCollisionLayers(PhysicsFormwork *object, LayerMask layers)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateBinding(object).Layers = layers;
    }

    LayerMask PhysicsCollision::GetCollisionLayers(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
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
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateBinding(object).Priority = priority;
    }

    int PhysicsCollision::GetCollisionPriority(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
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
        std::lock_guard<std::mutex> lock(mMutex);
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
        std::lock_guard<std::mutex> lock(mMutex);
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
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateBinding(object).OnExit = std::move(callback);
    }

    void PhysicsCollision::RemoveAllCollisionListeners(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        mBindings.erase(object);
    }

    void PhysicsCollision::RemoveCollisionBinding(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mBindings.find(object);
        if (it != mBindings.end())
        {
            it->second.Layers = LayerMaskAll;
            it->second.Priority = 50;
        }
    }

    void PhysicsCollision::ProcessCollisions(const std::vector<BaseArbiter *> &collideGroupVector)
    {
        struct DeferredEnter
        {
            CollisionCallback callback;
            CollisionData data;
        };
        struct DeferredStay
        {
            CollisionCallback callback;
            CollisionData data;
        };
        struct DeferredExit
        {
            CollisionCallback callback;
            CollisionData data;
        };

        std::vector<DeferredEnter> enterQueue;
        std::vector<DeferredStay> stayQueue;
        std::vector<DeferredExit> exitQueue;

        {
            std::lock_guard<std::mutex> lock(mMutex);

            std::unordered_set<std::string> currentPairs;
            std::vector<std::pair<BaseArbiter *, CollisionBinding *>> enterCallbacks;
            std::vector<std::pair<BaseArbiter *, CollisionBinding *>> stayCallbacks;
            std::vector<std::pair<BaseArbiter *, CollisionBinding *>> exitCallbacks;

            for (const auto &arbiter : collideGroupVector)
            {
                if (arbiter->numContacts <= 0)
                {
                    continue;
                }

                PhysicsFormwork *obj1 = static_cast<PhysicsFormwork *>(arbiter->key.object1);
                PhysicsFormwork *obj2 = static_cast<PhysicsFormwork *>(arbiter->key.object2);

                if (obj1 == nullptr || obj2 == nullptr)
                {
                    continue;
                }

                if (!LayersOverlap(obj1, obj2))
                {
                    continue;
                }

                std::string pairKey = MakePairKey(obj1, obj2);
                currentPairs.insert(pairKey);

                bool isNew = (mActivePairs.find(pairKey) == mActivePairs.end());

                auto it1 = mBindings.find(obj1);
                auto it2 = mBindings.find(obj2);

                CollisionBinding *bind1 = (it1 != mBindings.end()) ? &it1->second : nullptr;
                CollisionBinding *bind2 = (it2 != mBindings.end()) ? &it2->second : nullptr;

                if (isNew)
                {
                    if (bind1 && bind1->OnEnter)
                    {
                        enterCallbacks.push_back({arbiter, bind1});
                    }
                    if (bind2 && bind2->OnEnter)
                    {
                        enterCallbacks.push_back({arbiter, bind2});
                    }
                }

                if (bind1 && bind1->OnStay)
                {
                    stayCallbacks.push_back({arbiter, bind1});
                }
                if (bind2 && bind2->OnStay)
                {
                    stayCallbacks.push_back({arbiter, bind2});
                }
            }

            for (const auto &oldPair : mActivePairs)
            {
                if (currentPairs.find(oldPair) == currentPairs.end())
                {
                    size_t sepPos = oldPair.find('|');
                    if (sepPos == std::string::npos)
                    {
                        continue;
                    }

                    void *ptr1 = reinterpret_cast<void *>(std::stoull(oldPair.substr(0, sepPos)));
                    void *ptr2 = reinterpret_cast<void *>(std::stoull(oldPair.substr(sepPos + 1)));

                    PhysicsFormwork *obj1 = static_cast<PhysicsFormwork *>(ptr1);
                    PhysicsFormwork *obj2 = static_cast<PhysicsFormwork *>(ptr2);

                    auto it1 = mBindings.find(obj1);
                    auto it2 = mBindings.find(obj2);

                    if (it1 != mBindings.end() && it1->second.OnExit)
                    {
                        CollisionData data;
                        data.Self = obj1;
                        data.Other = obj2;
                        exitCallbacks.push_back({nullptr, &it1->second});
                    }
                    if (it2 != mBindings.end() && it2->second.OnExit)
                    {
                        CollisionData data;
                        data.Self = obj2;
                        data.Other = obj1;
                        exitCallbacks.push_back({nullptr, &it2->second});
                    }
                }
            }

            auto sortByPriority = [](const std::pair<BaseArbiter *, CollisionBinding *> &a,
                                      const std::pair<BaseArbiter *, CollisionBinding *> &b)
            {
                return a.second->Priority > b.second->Priority;
            };

            std::sort(enterCallbacks.begin(), enterCallbacks.end(), sortByPriority);
            std::sort(stayCallbacks.begin(), stayCallbacks.end(), sortByPriority);
            std::sort(exitCallbacks.begin(), exitCallbacks.end(), sortByPriority);

            for (auto &[arbiter, binding] : exitCallbacks)
            {
                if (binding && binding->OnExit)
                {
                    CollisionData data;
                    if (arbiter != nullptr)
                    {
                        PhysicsFormwork *self = static_cast<PhysicsFormwork *>(arbiter->key.object1);
                        data = BuildCollisionData(arbiter, 0, self, nullptr);
                    }
                    exitQueue.push_back({binding->OnExit, data});
                }
            }

            for (auto &[arbiter, binding] : enterCallbacks)
            {
                if (binding && binding->OnEnter)
                {
                    for (int ci = 0; ci < arbiter->numContacts; ++ci)
                    {
                        PhysicsFormwork *self = static_cast<PhysicsFormwork *>(arbiter->key.object1);
                        PhysicsFormwork *other = static_cast<PhysicsFormwork *>(arbiter->key.object2);
                        CollisionData data = BuildCollisionData(arbiter, ci, self, other);
                        enterQueue.push_back({binding->OnEnter, data});
                    }
                }
            }

            for (auto &[arbiter, binding] : stayCallbacks)
            {
                if (binding && binding->OnStay)
                {
                    for (int ci = 0; ci < arbiter->numContacts; ++ci)
                    {
                        PhysicsFormwork *self = static_cast<PhysicsFormwork *>(arbiter->key.object1);
                        PhysicsFormwork *other = static_cast<PhysicsFormwork *>(arbiter->key.object2);
                        CollisionData data = BuildCollisionData(arbiter, ci, self, other);
                        stayQueue.push_back({binding->OnStay, data});
                    }
                }
            }

            mActivePairs = std::move(currentPairs);
        }

        for (auto &deferred : exitQueue)
        {
            deferred.callback(deferred.data);
        }
        for (auto &deferred : enterQueue)
        {
            deferred.callback(deferred.data);
        }
        for (auto &deferred : stayQueue)
        {
            deferred.callback(deferred.data);
        }
    }

    void PhysicsCollision::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBindings.clear();
        mActivePairs.clear();
    }

    CollisionBinding &PhysicsCollision::GetOrCreateBinding(PhysicsFormwork *object)
    {
        auto it = mBindings.find(object);
        if (it == mBindings.end())
        {
            it = mBindings.emplace(object, CollisionBinding{}).first;
            it->second.Layers = LayerMaskDefault;
        }
        return it->second;
    }

    const CollisionBinding &PhysicsCollision::GetBinding(PhysicsFormwork *object) const
    {
        static CollisionBinding defaultBinding;
        auto it = mBindings.find(object);
        if (it == mBindings.end())
        {
            return defaultBinding;
        }
        return it->second;
    }

    bool PhysicsCollision::LayersOverlap(PhysicsFormwork *a, PhysicsFormwork *b) const
    {
        LayerMask layersA = LayerMaskAll;
        LayerMask layersB = LayerMaskAll;

        auto itA = mBindings.find(a);
        if (itA != mBindings.end())
        {
            layersA = itA->second.Layers;
        }

        auto itB = mBindings.find(b);
        if (itB != mBindings.end())
        {
            layersB = itB->second.Layers;
        }

        return (layersA & layersB) != 0;
    }

    CollisionData PhysicsCollision::BuildCollisionData(const BaseArbiter *arbiter, int contactIndex,
                                                        PhysicsFormwork *self, PhysicsFormwork *other) const
    {
        CollisionData data;
        if (arbiter != nullptr && contactIndex >= 0 && contactIndex < arbiter->numContacts)
        {
            const Contact &contact = arbiter->contacts[contactIndex];
            data.ContactPoint = contact.position;
            data.Normal = contact.normal;
            data.PenetrationDepth = (contact.separation < 0) ? -contact.separation : 0.0f;
            data.NormalImpulse = contact.Pn;
            data.TangentImpulse = contact.Pt;
            data.Friction = contact.friction;
        }

        data.Self = static_cast<PhysicsFormwork *>(self);
        data.Other = static_cast<PhysicsFormwork *>(other);

        if (self && other)
        {
            data.RelativeVelocity = self->PFSpeed() - other->PFSpeed();
        }

        return data;
    }

    std::string PhysicsCollision::MakePairKey(PhysicsFormwork *a, PhysicsFormwork *b) const
    {
        void *pa = static_cast<void *>(a);
        void *pb = static_cast<void *>(b);

        if (pa > pb)
        {
            std::swap(pa, pb);
        }

        std::ostringstream oss;
        oss << reinterpret_cast<uintptr_t>(pa) << '|' << reinterpret_cast<uintptr_t>(pb);
        return oss.str();
    }

}
