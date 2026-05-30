#pragma once
#include "PhysicsBlockTypes.hpp"
#include "BaseArbiter.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <mutex>

namespace PhysicsBlock
{

    struct CollisionBinding
    {
        LayerMask Layers = LayerMaskAll;
        int Priority = 50;//默认优先级
        CollisionCallback OnEnter = nullptr; // 进入碰撞回调
        CollisionCallback OnStay = nullptr; // 持续碰撞回调
        CollisionCallback OnExit = nullptr; // 退出碰撞回调
    };

    struct CollisionArbiter
    {
        CollisionBinding A_CollisionBinding;
        CollisionBinding B_CollisionBinding;
        BaseArbiter *arbiter = nullptr;
    };

    struct MapCollisionBinding
    {
        LayerMask Layers = LayerMaskAll;
        int Priority = 50;
        MapCollisionCallback OnEnter = nullptr;
        MapCollisionCallback OnStay = nullptr;
        MapCollisionCallback OnExit = nullptr;
    };

    struct MapCollisionArbiter
    {
        MapCollisionBinding MapBinding;
        BaseArbiter *arbiter = nullptr;
    };

    struct PairKey
    {
        PhysicsFormwork *a;
        PhysicsFormwork *b;

        PairKey(PhysicsFormwork *x, PhysicsFormwork *y)
        {
            if (reinterpret_cast<uintptr_t>(x) > reinterpret_cast<uintptr_t>(y))
            {
                a = x; b = y;
            }
            else
            {
                a = y; b = x;
            }
        }

        bool operator==(const PairKey &other) const
        {
            return a == other.a && b == other.b;
        }
    };

    struct PairKeyHash
    {
        std::size_t operator()(const PairKey &key) const
        {
            auto h1 = std::hash<void *>{}(key.a);
            auto h2 = std::hash<void *>{}(key.b);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    /**
     * @brief 碰撞回调管理类
     * @details 管理 PhysicsBlock 层面的碰撞事件回调注册与分发。
     * 通过绑定 PhysicsFormwork 对象与 CollisionBinding 来建立
     * 碰撞事件的监听关系。
     *
     * 工作机制：
     * - 在物理模拟完成后的回调阶段，遍历 PhysicsWorld::CollideGroupVector，
     *   对每个活跃的碰撞对检查双方是否绑定了回调。
     * - 通过与上一帧的碰撞状态对比，决定触发 Enter / Stay / Exit 事件。
     * - 支持层级过滤（LayerMask）和优先级排序。
     *
     * @note 该类本身不包含物理模拟逻辑，仅做事件分发。
     * @warning 回调函数中不应执行耗时操作或修改同一碰撞对的结构。
     */
    class PhysicsCollision
    {
    public:
        PhysicsCollision() = default;
        ~PhysicsCollision() = default;

        // 设置碰撞层级
        void SetCollisionLayers(PhysicsFormwork *object, LayerMask layers);
        LayerMask GetCollisionLayers(PhysicsFormwork *object) const;

        // 设置碰撞优先级
        void SetCollisionPriority(PhysicsFormwork *object, int priority);
        int GetCollisionPriority(PhysicsFormwork *object) const;

        // 添加碰撞监听
        void AddCollisionEnterListener(PhysicsFormwork *object, CollisionCallback callback);
        void AddCollisionStayListener(PhysicsFormwork *object, CollisionCallback callback);
        void AddCollisionExitListener(PhysicsFormwork *object, CollisionCallback callback);

        // 移除所有碰撞监听
        void RemoveAllCollisionListeners(PhysicsFormwork *object);
        // 移除指定对象的碰撞绑定
        void RemoveCollisionBinding(PhysicsFormwork *object);

        // 地图碰撞监听
        void AddMapCollisionEnterListener(MapFormwork *object, MapCollisionCallback callback);
        void AddMapCollisionStayListener(MapFormwork *object, MapCollisionCallback callback);
        void AddMapCollisionExitListener(MapFormwork *object, MapCollisionCallback callback);
        void RemoveAllMapCollisionListeners(MapFormwork *object);
        void RemoveMapCollisionBinding(MapFormwork *object);

        // 新增碰撞对
        void AddCollisionPair(BaseArbiter *arbiterKey);
        // 处理持续碰撞事件
        void ProcessCollisions();
        // 移除碰撞对
        void RemoveCollisionPair(BaseArbiter *arbiterKey);

        

        // 清除所有碰撞绑定
        void Clear();

    private:
        CollisionBinding &GetOrCreateBinding(PhysicsFormwork *object)
        {
            return mBindings[object];
        }

        const CollisionBinding &GetBinding(PhysicsFormwork *object) const
        {
            auto it = mBindings.find(object);
            if (it != mBindings.end())
                return it->second;
            static const CollisionBinding defaultBinding{};
            return defaultBinding;
        }

        MapCollisionBinding &GetOrCreateMapBinding(MapFormwork *object)
        {
            return mMapBindings[object];
        }

        const MapCollisionBinding &GetMapBinding(MapFormwork *object) const
        {
            auto it = mMapBindings.find(object);
            if (it != mMapBindings.end())
                return it->second;
            static const MapCollisionBinding defaultBinding{};
            return defaultBinding;
        }

        std::unordered_map<PhysicsFormwork *, CollisionBinding> mBindings;
        std::unordered_map<MapFormwork *, MapCollisionBinding> mMapBindings;

        std::vector<CollisionArbiter> mCollisionArbiterStayS;
        std::vector<MapCollisionArbiter> mMapCollisionArbiterStayS;
    };

}
