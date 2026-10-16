#include "PhysicsTrigger3D.hpp"
#include "GridSearch3D.hpp"
#include "PhysicsFormwork3D.hpp"
#include "PhysicsParticle3D.hpp"
#include "BaseCalculate3D.hpp"
#include <cmath>
#include <algorithm>

namespace PhysicsBlock3D
{

    TriggerHandle3D PhysicsTrigger3D::CreateTrigger()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const TriggerHandle3D handle = mNextHandle++;
        mConfigs.emplace(handle, TriggerConfig3D{});
        return handle;
    }

    void PhysicsTrigger3D::RemoveTrigger(TriggerHandle3D handle)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mConfigs.erase(handle);
        mActiveOverlaps.erase(handle);
    }

    void PhysicsTrigger3D::AddTriggerListener(TriggerHandle3D handle, TriggerEventType type, TriggerCallback callback)
    {
        if (!callback)
        {
            throw ArgumentNullException("callback");
        }

        std::lock_guard<std::mutex> lock(mMutex);
        TriggerConfig3D &config = GetOrCreateConfig(handle);

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

    void PhysicsTrigger3D::RemoveTriggerListener(TriggerHandle3D handle, TriggerEventType type)
    {
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

    void PhysicsTrigger3D::SetTriggerBounds(TriggerHandle3D handle, const Bounds3D &bounds)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateConfig(handle).TriggerBounds = bounds;
    }

    Bounds3D PhysicsTrigger3D::GetTriggerBounds(TriggerHandle3D handle) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return GetConfig(handle).TriggerBounds;
    }

    void PhysicsTrigger3D::SetTriggerLayers(TriggerHandle3D handle, LayerMask layers)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        GetOrCreateConfig(handle).TriggerLayers = layers;
    }

    LayerMask PhysicsTrigger3D::GetTriggerLayers(TriggerHandle3D handle) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return GetConfig(handle).TriggerLayers;
    }

    void PhysicsTrigger3D::SetLayerProvider(std::function<LayerMask(const PhysicsFormwork3D *)> provider)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mLayerProvider = std::move(provider);
    }

    void PhysicsTrigger3D::ProcessTriggers(GridSearch3D &gridSearch)
    {
        // 事件收集：锁内只收集，锁外统一回调（避免回调中再次调用本类造成死锁）
        struct TriggerEvent3D
        {
            TriggerHandle3D handle;
            PhysicsFormwork3D *object;
            TriggerCallback callback;
        };

        std::vector<TriggerEvent3D> events;

        {
            std::lock_guard<std::mutex> lock(mMutex);

            std::vector<PhysicsFormwork3D *> nearbyObjects;

            for (auto &[handle, config] : mConfigs)
            {
                // 修复 2D 缺陷：2D 的 nearbyObjects 声明在循环之外且 Get() 只追加
                // 不清空，后续触发器会看到前面触发器的查询结果（误报）。
                nearbyObjects.clear();

                const Vec3_ minPos = config.TriggerBounds.Center - config.TriggerBounds.HalfSize;
                const Vec3_ maxPos = config.TriggerBounds.Center + config.TriggerBounds.HalfSize;

                gridSearch.Get(minPos, maxPos, nearbyObjects);

                std::unordered_set<PhysicsFormwork3D *> currentOverlaps;

                for (PhysicsFormwork3D *obj : nearbyObjects)
                {
                    if (obj == nullptr)
                    {
                        continue;
                    }

                    // 液体粒子不参与触发
                    if (static_cast<PhysicsParticle3D *>(obj)->IsLiquidParticle)
                    {
                        continue;
                    }

                    // 层级过滤（2D 中 TriggerLayers 是死字段，3D 通过可选钩子真正生效）
                    if (mLayerProvider)
                    {
                        const LayerMask objLayers = mLayerProvider(obj);
                        if ((objLayers & config.TriggerLayers) == 0)
                        {
                            continue;
                        }
                    }

                    // 精确检测：点包含 ∨ 球体与盒重叠
                    // （2D 只有点包含，对半径较大的物体容易漏报）
                    const Vec3_ objPos = obj->PFGetPos();
                    bool overlap = config.TriggerBounds.Contains(objPos);

                    if (!overlap)
                    {
                        const FLOAT_ r = obj->PFGetCollisionR();
                        if (r > 0.0f)
                        {
                            const Vec3_ closest = config.TriggerBounds.ClosestPoint(objPos);
                            const Vec3_ diff = objPos - closest;
                            overlap = ModulusLength(diff) <= (r * r);
                        }
                    }

                    if (overlap)
                    {
                        currentOverlaps.insert(obj);
                    }
                }

                auto &activeOverlaps = mActiveOverlaps[handle];

                // Enter：本帧有 ∧ 上帧无
                for (PhysicsFormwork3D *obj : currentOverlaps)
                {
                    if (activeOverlaps.find(obj) == activeOverlaps.end())
                    {
                        if (config.OnTriggerEnter)
                        {
                            events.push_back({handle, obj, config.OnTriggerEnter});
                        }
                    }
                }

                // Stay：本帧有（每帧无条件发）
                for (PhysicsFormwork3D *obj : currentOverlaps)
                {
                    if (config.OnTriggerStay)
                    {
                        events.push_back({handle, obj, config.OnTriggerStay});
                    }
                }

                // Exit：上帧有 ∧ 本帧无
                for (PhysicsFormwork3D *obj : activeOverlaps)
                {
                    if (currentOverlaps.find(obj) == currentOverlaps.end())
                    {
                        if (config.OnTriggerExit)
                        {
                            events.push_back({handle, obj, config.OnTriggerExit});
                        }
                    }
                }

                activeOverlaps = std::move(currentOverlaps);
            }
        }

        // 锁外统一回调
        for (const TriggerEvent3D &ev : events)
        {
            if (ev.callback)
            {
                ev.callback(ev.object);
            }
        }
    }

    void PhysicsTrigger3D::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mConfigs.clear();
        mActiveOverlaps.clear();
        mNextHandle = InvalidTriggerHandle3D + 1;
    }

    TriggerConfig3D &PhysicsTrigger3D::GetOrCreateConfig(TriggerHandle3D handle)
    {
        auto it = mConfigs.find(handle);
        if (it == mConfigs.end())
        {
            it = mConfigs.emplace(handle, TriggerConfig3D{}).first;
        }
        return it->second;
    }

    const TriggerConfig3D &PhysicsTrigger3D::GetConfig(TriggerHandle3D handle) const
    {
        static TriggerConfig3D defaultConfig;
        auto it = mConfigs.find(handle);
        if (it == mConfigs.end())
        {
            return defaultConfig;
        }
        return it->second;
    }

}
