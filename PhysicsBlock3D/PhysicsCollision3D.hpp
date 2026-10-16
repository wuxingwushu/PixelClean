#pragma once
#include "PhysicsBlockTypes3D.hpp"
#include "BaseArbiter3D.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cstdint>

namespace PhysicsBlock3D
{
    // 前向声明（完整定义由 .cpp 包含）
    class PhysicsParticle3D;

    /**
     * @brief 物体碰撞绑定配置（3D） */
    struct CollisionBinding3D
    {
        LayerMask Layers = LayerMaskAll;         ///< 碰撞层级掩码，只有层级交集不为空时才触发回调
        int Priority = 50;                       ///< 碰撞优先级（0–100），默认 50
        CollisionCallback OnEnter = nullptr;     ///< 碰撞开始回调
        CollisionCallback OnStay = nullptr;      ///< 碰撞持续回调
        CollisionCallback OnExit = nullptr;      ///< 碰撞结束回调
    };

    /**
     * @brief 物体间碰撞对及其回调绑定（3D） */
    struct CollisionArbiter3D
    {
        CollisionBinding3D A_CollisionBinding;   ///< 物体 A 的碰撞绑定
        CollisionBinding3D B_CollisionBinding;   ///< 物体 B 的碰撞绑定
        BaseArbiter3D *arbiter = nullptr;        ///< 碰撞对指针
    };

    /**
     * @brief 地图碰撞绑定配置（3D） */
    struct MapCollisionBinding3D
    {
        LayerMask Layers = LayerMaskAll;               ///< 碰撞层级掩码
        int Priority = 50;                             ///< 碰撞优先级（0–100）
        MapCollisionCallback OnEnter = nullptr;        ///< 物体进入地图碰撞回调
        MapCollisionCallback OnStay = nullptr;         ///< 物体持续在地图碰撞回调
        MapCollisionCallback OnExit = nullptr;         ///< 物体离开地图碰撞回调
    };

    /**
     * @brief 地图碰撞对及其回调绑定（3D） */
    struct MapCollisionArbiter3D
    {
        MapCollisionBinding3D MapBinding;        ///< 地图的碰撞绑定
        BaseArbiter3D *arbiter = nullptr;        ///< 碰撞对指针
    };

    /**
     * @brief 无序配对键（3D）
     * @details 将两个 PhysicsFormwork3D 指针编码为无序对，
     *          保证 (a, b) 和 (b, a) 产生相同的键值。
     *          与 2D 版语义完全一致（纯指针身份比较，维度无关）。 */
    struct PairKey3D
    {
        PhysicsFormwork3D *a;  ///< 按指针地址排序后的大者
        PhysicsFormwork3D *b;  ///< 按指针地址排序后的小者

        PairKey3D(PhysicsFormwork3D *x, PhysicsFormwork3D *y)
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

        bool operator==(const PairKey3D &other) const
        {
            return a == other.a && b == other.b;
        }
    };

    /**
     * @brief PairKey3D 的哈希函数 */
    struct PairKey3DHash
    {
        std::size_t operator()(const PairKey3D &key) const
        {
            auto h1 = std::hash<void *>{}(key.a);
            auto h2 = std::hash<void *>{}(key.b);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    /**
     * @brief 碰撞回调管理类（3D）
     * @details 语义与 2D 版一致，整类几乎与维度无关（只做事件分发，不含模拟逻辑）。
     *          3D 版修复 2D 的一处既有缺陷：
     *          `AddCollisionPair` 中 B 侧 OnEnter 误用 `Ait->first`（当只有 B 有绑定
     *          时 `Ait == end()`，对其取 `->first` 属 UB）→ 3D 改为正确传入双方指针。
     */
    class PhysicsCollision3D
    {
    public:
        PhysicsCollision3D() = default;
        ~PhysicsCollision3D() = default;

        /// @brief 设置对象的碰撞层级掩码（仅与层级有交集的碰撞对才会触发回调）
        void SetCollisionLayers(PhysicsFormwork3D *object, LayerMask layers);
        /// @brief 获取对象的碰撞层级掩码
        LayerMask GetCollisionLayers(PhysicsFormwork3D *object) const;

        /// @brief 设置对象的碰撞回调优先级（0–100，越高越优先）
        void SetCollisionPriority(PhysicsFormwork3D *object, int priority);
        /// @brief 获取对象的碰撞回调优先级
        int GetCollisionPriority(PhysicsFormwork3D *object) const;

        /// @brief 注册碰撞开始（Enter）回调，当新的碰撞对产生时触发
        void AddCollisionEnterListener(PhysicsFormwork3D *object, CollisionCallback callback);
        /// @brief 注册碰撞持续（Stay）回调，每帧在已存在的碰撞对上触发
        void AddCollisionStayListener(PhysicsFormwork3D *object, CollisionCallback callback);
        /// @brief 注册碰撞结束（Exit）回调，当碰撞对解除时触发
        void AddCollisionExitListener(PhysicsFormwork3D *object, CollisionCallback callback);

        /// @brief 移除指定对象的所有碰撞监听回调
        void RemoveAllCollisionListeners(PhysicsFormwork3D *object);
        /// @brief 重置指定对象的碰撞绑定为默认值（层掩码=All，优先级=50），不删除条目
        void RemoveCollisionBinding(PhysicsFormwork3D *object);

        /// @brief 注册地图碰撞开始（Enter）回调
        void AddMapCollisionEnterListener(MapFormwork3D *object, MapCollisionCallback callback);
        /// @brief 注册地图碰撞持续（Stay）回调
        void AddMapCollisionStayListener(MapFormwork3D *object, MapCollisionCallback callback);
        /// @brief 注册地图碰撞结束（Exit）回调
        void AddMapCollisionExitListener(MapFormwork3D *object, MapCollisionCallback callback);
        /// @brief 移除指定地图的所有碰撞监听回调
        void RemoveAllMapCollisionListeners(MapFormwork3D *object);
        /// @brief 重置指定地图的碰撞绑定为默认值，不删除条目
        void RemoveMapCollisionBinding(MapFormwork3D *object);

        // ========== 粒子-地形命中回调 ==========
        /// @brief 注册粒子地形碰撞命中回调（当粒子击中地图时触发）
        void AddTerrainHitListener(PhysicsParticle3D *particle, TerrainHitCallback callback, void *userData = nullptr);
        /// @brief 移除指定粒子的地形碰撞命中回调
        void RemoveTerrainHitListener(PhysicsParticle3D *particle);
        /// @brief 移除所有粒子的地形碰撞命中回调
        void RemoveAllTerrainHitListeners();
        /// @brief 内部触发地形碰撞命中回调
        void OnTerrainHit(PhysicsParticle3D *particle, const BulletHitInfo3D &hitInfo);

        // ========== 地图碰撞状态变化回调 ==========
        /// @brief 注册地图碰撞状态变化回调（当 SafeSetCollision 改变网格碰撞状态时触发）
        void AddMapCollisionChangeListener(MapFormwork3D *map, MapCollisionChangeCallback callback, void *userData = nullptr);
        /// @brief 移除指定地图的碰撞状态变化回调
        void RemoveMapCollisionChangeListener(MapFormwork3D *map);
        /// @brief 移除所有地图的碰撞状态变化回调
        void RemoveAllMapCollisionChangeListeners();
        /// @brief 内部触发地图碰撞状态变化回调
        void OnMapCollisionChanged(MapFormwork3D *map, IVec3_ pos, bool newState);

        /// @brief 新的碰撞对产生时由 PhysicsWorld3D 调用，触发 OnEnter 并登记 OnStay
        void AddCollisionPair(BaseArbiter3D *arbiterKey);
        /// @brief 每帧由 PhysicsWorld3D 调用，触发所有持续碰撞对的 OnStay 回调
        void ProcessCollisions();
        /// @brief 碰撞对解除时由 PhysicsWorld3D 调用，触发 OnExit 并从持续列表中移除
        void RemoveCollisionPair(BaseArbiter3D *arbiterKey);

        /// @brief 清除所有碰撞绑定和持续碰撞列表
        void Clear();

    private:
        CollisionBinding3D &GetOrCreateBinding(PhysicsFormwork3D *object)
        {
            return mBindings[object];
        }

        const CollisionBinding3D &GetBinding(PhysicsFormwork3D *object) const
        {
            auto it = mBindings.find(object);
            if (it != mBindings.end())
                return it->second;
            static const CollisionBinding3D defaultBinding{};
            return defaultBinding;
        }

        MapCollisionBinding3D &GetOrCreateMapBinding(MapFormwork3D *object)
        {
            return mMapBindings[object];
        }

        const MapCollisionBinding3D &GetMapBinding(MapFormwork3D *object) const
        {
            auto it = mMapBindings.find(object);
            if (it != mMapBindings.end())
                return it->second;
            static const MapCollisionBinding3D defaultBinding{};
            return defaultBinding;
        }

        std::unordered_map<PhysicsFormwork3D *, CollisionBinding3D> mBindings;    ///< 物体碰撞绑定表
        std::unordered_map<MapFormwork3D *, MapCollisionBinding3D> mMapBindings;  ///< 地图碰撞绑定表

        std::vector<CollisionArbiter3D> mCollisionArbiterStayS;         ///< 物体间持续碰撞列表
        std::vector<MapCollisionArbiter3D> mMapCollisionArbiterStayS;   ///< 地图持续碰撞列表

        struct TerrainHitBinding3D
        {
            TerrainHitCallback OnHit = nullptr;
            void *userData = nullptr;
        };
        std::unordered_map<PhysicsParticle3D *, TerrainHitBinding3D> mTerrainHitBindings;

        struct MapCollisionChangeBinding3D
        {
            MapCollisionChangeCallback OnChange = nullptr;
            void *userData = nullptr;
        };
        std::unordered_map<MapFormwork3D *, MapCollisionChangeBinding3D> mMapCollisionChangeBindings;
    };

}
