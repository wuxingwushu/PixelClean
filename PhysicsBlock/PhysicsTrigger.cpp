#include "PhysicsTrigger.hpp"
#include "PhysicsFormwork.hpp"
#include "GridSearch.hpp"

namespace PhysicsBlock
{

    TriggerHandle PhysicsTrigger::CreateTrigger()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        TriggerHandle handle = mNextHandle++;
        mConfigs.emplace(handle, TriggerConfig{});
        return handle;
    }

    void PhysicsTrigger::RemoveTrigger(TriggerHandle handle)
    {
        if (handle == InvalidTriggerHandle)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        mConfigs.erase(handle);
        mActiveOverlaps.erase(handle);
    }

    void PhysicsTrigger::AddTriggerListener(TriggerHandle handle, TriggerEventType type, TriggerCallback callback)
    {
        if (handle == InvalidTriggerHandle)
        {
            return;
        }
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        TriggerConfig &config = GetOrCreateConfig(handle);

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

    void PhysicsTrigger::RemoveTriggerListener(TriggerHandle handle, TriggerEventType type)
    {
        if (handle == InvalidTriggerHandle)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mConfigs.find(handle);
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

    void PhysicsTrigger::SetTriggerBounds(TriggerHandle handle, const Bounds &bounds)
    {
        if (handle == InvalidTriggerHandle)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateConfig(handle).TriggerBounds = bounds;
    }

    Bounds PhysicsTrigger::GetTriggerBounds(TriggerHandle handle) const
    {
        if (handle == InvalidTriggerHandle)
        {
            return Bounds{};
        }

        std::lock_guard<std::mutex> lock(mMutex);
        return GetConfig(handle).TriggerBounds;
    }

    void PhysicsTrigger::SetTriggerLayers(TriggerHandle handle, LayerMask layers)
    {
        if (handle == InvalidTriggerHandle)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateConfig(handle).TriggerLayers = layers;
    }

    LayerMask PhysicsTrigger::GetTriggerLayers(TriggerHandle handle) const
    {
        if (handle == InvalidTriggerHandle)
        {
            return LayerMaskAll;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        return GetConfig(handle).TriggerLayers;
    }

    void PhysicsTrigger::ProcessTriggers(GridSearch &gridSearch)
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
                TriggerHandle triggerHandle;
                TriggerConfig *config;
                PhysicsFormwork *targetObject;
                TriggerEventType eventType;
            };
            std::vector<TriggerEvent> events;

            std::vector<PhysicsFormwork *> nearbyObjects;

            for (auto &[handle, config] : mConfigs)
            {
                auto &activeOverlaps = mActiveOverlaps[handle];
                std::unordered_set<PhysicsFormwork *> currentOverlaps;

                Vec2_ minPos = config.TriggerBounds.Center - config.TriggerBounds.HalfSize;
                Vec2_ maxPos = config.TriggerBounds.Center + config.TriggerBounds.HalfSize;
                gridSearch.Get(minPos, maxPos, nearbyObjects);

                for (auto *obj : nearbyObjects)
                {
                    if (obj == nullptr)
                    {
                        continue;
                    }

                    if (!config.TriggerBounds.Contains(obj->PFGetPos()))
                    {
                        continue;
                    }

                    currentOverlaps.insert(obj);

                    bool wasOverlapping = (activeOverlaps.find(obj) != activeOverlaps.end());

                    if (!wasOverlapping)
                    {
                        if (config.OnTriggerEnter)
                        {
                            events.push_back({handle, &config, obj, TriggerEventType::Enter});
                        }
                    }

                    if (config.OnTriggerStay)
                    {
                        events.push_back({handle, &config, obj, TriggerEventType::Stay});
                    }
                }

                for (auto *obj : activeOverlaps)
                {
                    if (currentOverlaps.find(obj) == currentOverlaps.end())
                    {
                        if (config.OnTriggerExit)
                        {
                            events.push_back({handle, &config, obj, TriggerEventType::Exit});
                        }
                    }
                }

                activeOverlaps = std::move(currentOverlaps);
            }

            for (const auto &event : events)
            {
                if (event.config == nullptr)
                {
                    continue;
                }

                TriggerCallback callback = nullptr;
                switch (event.eventType)
                {
                case TriggerEventType::Enter:
                    callback = event.config->OnTriggerEnter;
                    break;
                case TriggerEventType::Stay:
                    callback = event.config->OnTriggerStay;
                    break;
                case TriggerEventType::Exit:
                    callback = event.config->OnTriggerExit;
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

    TriggerConfig &PhysicsTrigger::GetOrCreateConfig(TriggerHandle handle)
    {
        auto it = mConfigs.find(handle);
        if (it == mConfigs.end())
        {
            it = mConfigs.emplace(handle, TriggerConfig{}).first;
        }
        return it->second;
    }

    const TriggerConfig &PhysicsTrigger::GetConfig(TriggerHandle handle) const
    {
        static TriggerConfig defaultConfig;
        auto it = mConfigs.find(handle);
        if (it == mConfigs.end())
        {
            return defaultConfig;
        }
        return it->second;
    }

}