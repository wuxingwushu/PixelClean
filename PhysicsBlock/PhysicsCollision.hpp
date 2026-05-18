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
        CollisionCallback OnEnter = nullptr;
        CollisionCallback OnStay = nullptr;
        CollisionCallback OnExit = nullptr;
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

        void SetCollisionLayers(PhysicsFormwork *object, LayerMask layers);
        LayerMask GetCollisionLayers(PhysicsFormwork *object) const;
        void SetCollisionPriority(PhysicsFormwork *object, int priority);
        int GetCollisionPriority(PhysicsFormwork *object) const;
        void AddCollisionEnterListener(PhysicsFormwork *object, CollisionCallback callback);
        void AddCollisionStayListener(PhysicsFormwork *object, CollisionCallback callback);
        void AddCollisionExitListener(PhysicsFormwork *object, CollisionCallback callback);
        void RemoveAllCollisionListeners(PhysicsFormwork *object);
        void RemoveCollisionBinding(PhysicsFormwork *object);
        void ProcessCollisions(const std::vector<BaseArbiter *> &collideGroupVector);
        void Clear();

    private:
        CollisionBinding &GetOrCreateBinding(PhysicsFormwork *object);
        const CollisionBinding &GetBinding(PhysicsFormwork *object) const;
        bool LayersOverlap(PhysicsFormwork *a, PhysicsFormwork *b) const;
        CollisionData BuildCollisionData(const BaseArbiter *arbiter, int contactIndex,
                                          PhysicsFormwork *self, PhysicsFormwork *other) const;

        std::unordered_map<PhysicsFormwork *, CollisionBinding> mBindings;
        std::unordered_set<PairKey, PairKeyHash> mActivePairs;
        mutable std::mutex mMutex;
    };

}
