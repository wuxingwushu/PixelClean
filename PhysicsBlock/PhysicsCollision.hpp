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
    // 前向声明（完整定义由 .cpp 包含）
    class PhysicsParticle;

    /**
     * @brief 物体碰撞绑定配置
     * @details 存储单个物理对象（PhysicsFormwork）的碰撞回调设置，
     * 包括层级掩码、优先级和 Enter/Stay/Exit 三种事件的回调函数。
     */
    struct CollisionBinding
    {
        LayerMask Layers = LayerMaskAll;       ///< 碰撞层级掩码，只有层级交集不为空时才触发回调
        int Priority = 50;                     ///< 碰撞优先级（0–100），默认 50
        CollisionCallback OnEnter = nullptr;   ///< 碰撞开始回调
        CollisionCallback OnStay = nullptr;    ///< 碰撞持续回调
        CollisionCallback OnExit = nullptr;    ///< 碰撞结束回调
    };

    /**
     * @brief 物体间碰撞对及其回调绑定
     * @details 关联一个 BaseArbiter（碰撞对）与双方各自的 CollisionBinding，
     * 用于在 ProcessCollisions 阶段批量触发 OnStay 回调。
     */
    struct CollisionArbiter
    {
        CollisionBinding A_CollisionBinding;  ///< 物体 A 的碰撞绑定
        CollisionBinding B_CollisionBinding;  ///< 物体 B 的碰撞绑定
        BaseArbiter *arbiter = nullptr;       ///< 碰撞对指针
    };

    /**
     * @brief 地图碰撞绑定配置
     * @details 存储单个地图对象（MapFormwork）的碰撞回调设置。
     */
    struct MapCollisionBinding
    {
        LayerMask Layers = LayerMaskAll;               ///< 碰撞层级掩码
        int Priority = 50;                             ///< 碰撞优先级（0–100）
        MapCollisionCallback OnEnter = nullptr;        ///< 物体进入地图碰撞回调
        MapCollisionCallback OnStay = nullptr;         ///< 物体持续在地图碰撞回调
        MapCollisionCallback OnExit = nullptr;         ///< 物体离开地图碰撞回调
    };

    /**
     * @brief 地图碰撞对及其回调绑定
     * @details 关联一个 BaseArbiter 与对应地图的 MapCollisionBinding。
     */
    struct MapCollisionArbiter
    {
        MapCollisionBinding MapBinding;  ///< 地图的碰撞绑定
        BaseArbiter *arbiter = nullptr;  ///< 碰撞对指针
    };

    /**
     * @brief 无序配对键
     * @details 将两个 PhysicsFormwork 指针编码为无序对，
     * 保证 (a, b) 和 (b, a) 产生相同的键值。
     * 用于 std::unordered_map / std::unordered_set 中作为键。
     */
    struct PairKey
    {
        PhysicsFormwork *a;  ///< 按指针地址排序后的大者
        PhysicsFormwork *b;  ///< 按指针地址排序后的小者

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

    /**
     * @brief PairKey 的哈希函数
     * @details 使用 boost::hash_combine 风格的组合哈希，
     * 保证同一无序对的哈希值一致。
     */
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
     * 通过将 PhysicsFormwork 对象绑定 CollisionBinding 来建立
     * 碰撞事件的监听关系。
     *
     * 工作机制：
     * - PhysicsWorld 在检测到新的碰撞对时调用 AddCollisionPair()，
     *   触发 OnEnter 回调，并将有 OnStay 的碰撞对加入持续列表。
     * - 每帧调用 ProcessCollisions() 触发所有持续碰撞对的 OnStay 回调。
     * - PhysicsWorld 在碰撞对解除时调用 RemoveCollisionPair()，
     *   触发 OnExit 回调。
     * - 支持层级过滤（LayerMask）和优先级排序。
     *
     * @note 该类本身不包含物理模拟逻辑，仅做事件分发。
     * @note Enter/Exit 的触发时机由 PhysicsWorld 控制（新增/移除 arbiter），
     * 而非本类内部自行对比帧间状态。
     * @warning 回调函数中不应执行耗时操作或修改同一碰撞对的结构。
     */
    class PhysicsCollision
    {
    public:
        PhysicsCollision() = default;
        ~PhysicsCollision() = default;

        /// @brief 设置对象的碰撞层级掩码（仅与层级有交集的碰撞对才会触发回调）
        void SetCollisionLayers(PhysicsFormwork *object, LayerMask layers);
        /// @brief 获取对象的碰撞层级掩码
        LayerMask GetCollisionLayers(PhysicsFormwork *object) const;

        /// @brief 设置对象的碰撞回调优先级（0–100，越高越优先）
        void SetCollisionPriority(PhysicsFormwork *object, int priority);
        /// @brief 获取对象的碰撞回调优先级
        int GetCollisionPriority(PhysicsFormwork *object) const;

        /// @brief 注册碰撞开始（Enter）回调，当新的碰撞对产生时触发
        void AddCollisionEnterListener(PhysicsFormwork *object, CollisionCallback callback);
        /// @brief 注册碰撞持续（Stay）回调，每帧在已存在的碰撞对上触发
        void AddCollisionStayListener(PhysicsFormwork *object, CollisionCallback callback);
        /// @brief 注册碰撞结束（Exit）回调，当碰撞对解除时触发
        void AddCollisionExitListener(PhysicsFormwork *object, CollisionCallback callback);

        /// @brief 移除指定对象的所有碰撞监听回调
        void RemoveAllCollisionListeners(PhysicsFormwork *object);
        /// @brief 重置指定对象的碰撞绑定为默认值（层掩码=All，优先级=50），不删除条目
        void RemoveCollisionBinding(PhysicsFormwork *object);

        /// @brief 注册地图碰撞开始（Enter）回调
        void AddMapCollisionEnterListener(MapFormwork *object, MapCollisionCallback callback);
        /// @brief 注册地图碰撞持续（Stay）回调
        void AddMapCollisionStayListener(MapFormwork *object, MapCollisionCallback callback);
        /// @brief 注册地图碰撞结束（Exit）回调
        void AddMapCollisionExitListener(MapFormwork *object, MapCollisionCallback callback);
        /// @brief 移除指定地图的所有碰撞监听回调
        void RemoveAllMapCollisionListeners(MapFormwork *object);
        /// @brief 重置指定地图的碰撞绑定为默认值，不删除条目
        void RemoveMapCollisionBinding(MapFormwork *object);

        // ========== 粒子-地形命中回调 ==========
        /// @brief 注册粒子地形碰撞命中回调（当粒子击中地图时触发）
        void AddTerrainHitListener(PhysicsParticle *particle, TerrainHitCallback callback, void* userData = nullptr);
        /// @brief 移除指定粒子的地形碰撞命中回调
        void RemoveTerrainHitListener(PhysicsParticle *particle);
        /// @brief 移除所有粒子的地形碰撞命中回调
        void RemoveAllTerrainHitListeners();
        /// @brief 内部触发地形碰撞命中回调
        void OnTerrainHit(PhysicsParticle *particle, const BulletHitInfo& hitInfo);

        // ========== 地图碰撞状态变化回调 ==========
        /// @brief 注册地图碰撞状态变化回调（当 SafeSetCollision 改变网格碰撞状态时触发）
        void AddMapCollisionChangeListener(MapFormwork *map, MapCollisionChangeCallback callback, void* userData = nullptr);
        /// @brief 移除指定地图的碰撞状态变化回调
        void RemoveMapCollisionChangeListener(MapFormwork *map);
        /// @brief 移除所有地图的碰撞状态变化回调
        void RemoveAllMapCollisionChangeListeners();
        /// @brief 内部触发地图碰撞状态变化回调
        void OnMapCollisionChanged(MapFormwork *map, glm::ivec2 pos, bool newState);

        /// @brief 新的碰撞对产生时由 PhysicsWorld 调用，触发 OnEnter 并登记 OnStay
        void AddCollisionPair(BaseArbiter *arbiterKey);
        /// @brief 每帧由 PhysicsWorld 调用，触发所有持续碰撞对的 OnStay 回调
        void ProcessCollisions();
        /// @brief 碰撞对解除时由 PhysicsWorld 调用，触发 OnExit 并从持续列表中移除
        void RemoveCollisionPair(BaseArbiter *arbiterKey);

        /// @brief 清除所有碰撞绑定和持续碰撞列表
        void Clear();

    private:
        /// @brief 获取或创建对象的碰撞绑定（非 const）
        CollisionBinding &GetOrCreateBinding(PhysicsFormwork *object)
        {
            return mBindings[object];
        }

        /// @brief 获取对象的碰撞绑定（const，不存在时返回静态默认值）
        const CollisionBinding &GetBinding(PhysicsFormwork *object) const
        {
            auto it = mBindings.find(object);
            if (it != mBindings.end())
                return it->second;
            static const CollisionBinding defaultBinding{};
            return defaultBinding;
        }

        /// @brief 获取或创建地图的碰撞绑定（非 const）
        MapCollisionBinding &GetOrCreateMapBinding(MapFormwork *object)
        {
            return mMapBindings[object];
        }

        /// @brief 获取地图的碰撞绑定（const，不存在时返回静态默认值）
        const MapCollisionBinding &GetMapBinding(MapFormwork *object) const
        {
            auto it = mMapBindings.find(object);
            if (it != mMapBindings.end())
                return it->second;
            static const MapCollisionBinding defaultBinding{};
            return defaultBinding;
        }

        std::unordered_map<PhysicsFormwork *, CollisionBinding> mBindings;          ///< 物体碰撞绑定表
        std::unordered_map<MapFormwork *, MapCollisionBinding> mMapBindings;        ///< 地图碰撞绑定表

        std::vector<CollisionArbiter> mCollisionArbiterStayS;      ///< 物体间持续碰撞列表（每帧触发 OnStay）
        std::vector<MapCollisionArbiter> mMapCollisionArbiterStayS; ///< 地图持续碰撞列表（每帧触发 OnStay）

        // ========== 粒子-地形命中回调绑定 ==========
        struct TerrainHitBinding
        {
            TerrainHitCallback OnHit = nullptr;
            void* userData = nullptr;
        };
        std::unordered_map<PhysicsParticle *, TerrainHitBinding> mTerrainHitBindings;

        // ========== 地图碰撞状态变化回调绑定 ==========
        struct MapCollisionChangeBinding
        {
            MapCollisionChangeCallback OnChange = nullptr;
            void* userData = nullptr;
        };
        std::unordered_map<MapFormwork *, MapCollisionChangeBinding> mMapCollisionChangeBindings;
    };

}
