#include "PhysicsTrigger.hpp"
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    void PhysicsTrigger::AddTriggerListener(PhysicsFormwork *object, TriggerEventType type, TriggerCallback callback)
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
        TriggerConfig &config = GetOrCreateConfig(object);

        switch (type)
        {
        case TriggerEventType::Enter:
            config.OnTriggerEnter = std::move(callback);
            break;
        case TriggerEventType::Stay:
            config.OnTriggerStay = std::move(callback);
            break;
        case TriggerEventType::Exit:
            config.OnTriggerExit = std::move(callback);
            break;
        }
    }

    void PhysicsTrigger::RemoveTriggerListener(PhysicsFormwork *object, TriggerEventType type)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mConfigs.find(object);
        if (it == mConfigs.end())
        {
            return;
        }

        switch (type)
        {
        case TriggerEventType::Enter:
            it->second.OnTriggerEnter = nullptr;
            break;
        case TriggerEventType::Stay:
            it->second.OnTriggerStay = nullptr;
            break;
        case TriggerEventType::Exit:
            it->second.OnTriggerExit = nullptr;
            break;
        }
    }

    void PhysicsTrigger::RemoveAllTriggerListeners(PhysicsFormwork *object)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        mConfigs.erase(object);
        mActiveOverlaps.erase(object);
    }

    void PhysicsTrigger::SetTriggerBounds(PhysicsFormwork *object, const Bounds &bounds)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateConfig(object).TriggerBounds = bounds;
    }

    Bounds PhysicsTrigger::GetTriggerBounds(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        return GetConfig(object).TriggerBounds;
    }

    void PhysicsTrigger::SetTriggerLayers(PhysicsFormwork *object, LayerMask layers)
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateConfig(object).TriggerLayers = layers;
    }

    LayerMask PhysicsTrigger::GetTriggerLayers(PhysicsFormwork *object) const
    {
        if (object == nullptr)
        {
            throw ArgumentNullException("object");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        return GetConfig(object).TriggerLayers;
    }

    void PhysicsTrigger::ProcessTriggers(const std::vector<PhysicsFormwork *> &allObjects)
    {
        struct DeferredTriggerEvent
        {
            TriggerCallback callback;
            PhysicsFormwork *targetObject;
        };
        std::vector<DeferredTriggerEvent> deferredEvents;

        {
            std::lock_guard<std::mutex> lock(mMutex);

            struct TriggerEvent
            {
                PhysicsFormwork *triggerOwner;
                PhysicsFormwork *targetObject;
                TriggerEventType eventType;
            };
            std::vector<TriggerEvent> events;

            for (auto &[triggerObj, config] : mConfigs)
            {
                if (triggerObj == nullptr)
                {
                    continue;
                }

                auto &activeOverlaps = mActiveOverlaps[triggerObj];
                std::unordered_set<PhysicsFormwork *> currentOverlaps;

                for (auto *obj : allObjects)
                {
                    if (obj == nullptr || obj == triggerObj)
                    {
                        continue;
                    }

                    Vec2_ objPos = obj->PFGetPos();

                    if (!config.TriggerBounds.Contains(objPos))
                    {
                        continue;
                    }

                    currentOverlaps.insert(obj);

                    bool wasOverlapping = (activeOverlaps.find(obj) != activeOverlaps.end());

                    if (!wasOverlapping)
                    {
                        if (config.OnTriggerEnter)
                        {
                            events.push_back({triggerObj, obj, TriggerEventType::Enter});
                        }
                    }

                    if (config.OnTriggerStay)
                    {
                        events.push_back({triggerObj, obj, TriggerEventType::Stay});
                    }
                }

                for (auto *obj : activeOverlaps)
                {
                    if (currentOverlaps.find(obj) == currentOverlaps.end())
                    {
                        if (config.OnTriggerExit)
                        {
                            events.push_back({triggerObj, obj, TriggerEventType::Exit});
                        }
                    }
                }

                activeOverlaps = std::move(currentOverlaps);
            }

            for (const auto &event : events)
            {
                auto it = mConfigs.find(event.triggerOwner);
                if (it == mConfigs.end())
                {
                    continue;
                }

                TriggerCallback callback = nullptr;
                switch (event.eventType)
                {
                case TriggerEventType::Enter:
                    callback = it->second.OnTriggerEnter;
                    break;
                case TriggerEventType::Stay:
                    callback = it->second.OnTriggerStay;
                    break;
                case TriggerEventType::Exit:
                    callback = it->second.OnTriggerExit;
                    break;
                }

                if (callback)
                {
                    deferredEvents.push_back({callback, event.targetObject});
                }
            }
        }

        for (auto &deferred : deferredEvents)
        {
            deferred.callback(deferred.targetObject);
        }
    }

    void PhysicsTrigger::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mConfigs.clear();
        mActiveOverlaps.clear();
    }

    TriggerConfig &PhysicsTrigger::GetOrCreateConfig(PhysicsFormwork *object)
    {
        auto it = mConfigs.find(object);
        if (it == mConfigs.end())
        {
            it = mConfigs.emplace(object, TriggerConfig{}).first;
        }
        return it->second;
    }

    const TriggerConfig &PhysicsTrigger::GetConfig(PhysicsFormwork *object) const
    {
        static TriggerConfig defaultConfig;
        auto it = mConfigs.find(object);
        if (it == mConfigs.end())
        {
            return defaultConfig;
        }
        return it->second;
    }

}
