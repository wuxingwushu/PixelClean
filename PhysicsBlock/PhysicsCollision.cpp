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
        struct DeferredEvent
        {
            CollisionCallback callback;
            CollisionData data;
        };

        std::vector<DeferredEvent> deferredEvents;

        {
            std::lock_guard<std::mutex> lock(mMutex);

            std::unordered_set<PairKey, PairKeyHash> currentPairs;
            currentPairs.reserve(collideGroupVector.size());

            struct CallbackEntry
            {
                BaseArbiter *arbiter;
                CollisionBinding *binding;
            };
            std::vector<CallbackEntry> enterEntries;
            std::vector<CallbackEntry> stayEntries;
            std::vector<CallbackEntry> exitEntries;

            enterEntries.reserve(collideGroupVector.size() * 2);
            stayEntries.reserve(collideGroupVector.size() * 2);

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

                PairKey pairKey(obj1, obj2);
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
                        enterEntries.push_back({arbiter, bind1});
                    }
                    if (bind2 && bind2->OnEnter)
                    {
                        enterEntries.push_back({arbiter, bind2});
                    }
                }

                if (bind1 && bind1->OnStay)
                {
                    stayEntries.push_back({arbiter, bind1});
                }
                if (bind2 && bind2->OnStay)
                {
                    stayEntries.push_back({arbiter, bind2});
                }
            }

            for (const auto &oldPair : mActivePairs)
            {
                if (currentPairs.find(oldPair) == currentPairs.end())
                {
                    PhysicsFormwork *obj1 = oldPair.a;
                    PhysicsFormwork *obj2 = oldPair.b;

                    auto it1 = mBindings.find(obj1);
                    auto it2 = mBindings.find(obj2);

                    if (it1 != mBindings.end() && it1->second.OnExit)
                    {
                        exitEntries.push_back({nullptr, &it1->second});
                    }
                    if (it2 != mBindings.end() && it2->second.OnExit)
                    {
                        exitEntries.push_back({nullptr, &it2->second});
                    }
                }
            }

            auto sortByPriority = [](const CallbackEntry &a, const CallbackEntry &b)
            {
                return a.binding->Priority > b.binding->Priority;
            };

            std::sort(enterEntries.begin(), enterEntries.end(), sortByPriority);
            std::sort(stayEntries.begin(), stayEntries.end(), sortByPriority);
            std::sort(exitEntries.begin(), exitEntries.end(), sortByPriority);

            deferredEvents.reserve(enterEntries.size() + stayEntries.size() + exitEntries.size());

            for (auto &entry : exitEntries)
            {
                if (entry.binding && entry.binding->OnExit)
                {
                    CollisionData data;
                    if (entry.arbiter != nullptr)
                    {
                        PhysicsFormwork *self = static_cast<PhysicsFormwork *>(entry.arbiter->key.object1);
                        data = BuildCollisionData(entry.arbiter, 0, self, nullptr);
                    }
                    deferredEvents.push_back({entry.binding->OnExit, data});
                }
            }

            for (auto &entry : enterEntries)
            {
                if (entry.binding && entry.binding->OnEnter)
                {
                    for (int ci = 0; ci < entry.arbiter->numContacts; ++ci)
                    {
                        PhysicsFormwork *self = static_cast<PhysicsFormwork *>(entry.arbiter->key.object1);
                        PhysicsFormwork *other = static_cast<PhysicsFormwork *>(entry.arbiter->key.object2);
                        CollisionData data = BuildCollisionData(entry.arbiter, ci, self, other);
                        deferredEvents.push_back({entry.binding->OnEnter, data});
                    }
                }
            }

            for (auto &entry : stayEntries)
            {
                if (entry.binding && entry.binding->OnStay)
                {
                    for (int ci = 0; ci < entry.arbiter->numContacts; ++ci)
                    {
                        PhysicsFormwork *self = static_cast<PhysicsFormwork *>(entry.arbiter->key.object1);
                        PhysicsFormwork *other = static_cast<PhysicsFormwork *>(entry.arbiter->key.object2);
                        CollisionData data = BuildCollisionData(entry.arbiter, ci, self, other);
                        deferredEvents.push_back({entry.binding->OnStay, data});
                    }
                }
            }

            mActivePairs = std::move(currentPairs);
        }

        for (auto &deferred : deferredEvents)
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

}