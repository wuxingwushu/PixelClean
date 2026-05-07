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

    /**
     * @brief 碰撞回调优先级条目
     * @details 存储一个对象的碰撞层级掩码、优先级和回调函数绑定。
     * 优先级越高（0-100）的物体在碰撞处理时优先被响应。
     */
    struct CollisionBinding
    {
        /**
         * @brief 碰撞层级掩码
         * @details 定义此对象可以与哪些层级的物体发生碰撞
         */
        LayerMask Layers = LayerMaskAll;

        /**
         * @brief 碰撞响应优先级（0-100）
         * @details 值越高越优先响应。用于在多个碰撞回调中控制执行顺序。
         */
        int Priority = 50;

        /**
         * @brief Enter 事件回调函数
         * @details 当此对象与目标首次发生碰撞时调用
         */
        CollisionCallback OnEnter = nullptr;

        /**
         * @brief Stay 事件回调函数
         * @details 当此对象与目标持续碰撞时每帧调用
         */
        CollisionCallback OnStay = nullptr;

        /**
         * @brief Exit 事件回调函数
         * @details 当此对象与目标的碰撞结束时调用
         */
        CollisionCallback OnExit = nullptr;
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
        /**
         * @brief 默认构造函数
         */
        PhysicsCollision() = default;

        /**
         * @brief 析构函数
         */
        ~PhysicsCollision() = default;

        /**
         * @brief 为指定物理对象设置碰撞层级掩码
         * @param object 目标物理对象指针，不可为空
         * @param layers 碰撞层级掩码。只有层级交集非空的物体间才会触发回调。
         *               默认值为 LayerMaskAll，即与所有层级均可碰撞。
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @code
         * // 设置对象只与第 1 层和第 3 层碰撞
         * collision.SetCollisionLayers(myObject, (1 << 1) | (1 << 3));
         * @endcode
         */
        void SetCollisionLayers(PhysicsFormwork *object, LayerMask layers);

        /**
         * @brief 获取指定物理对象的碰撞层级掩码
         * @param object 目标物理对象指针，不可为空
         * @return 当前设置的碰撞层级掩码
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        LayerMask GetCollisionLayers(PhysicsFormwork *object) const;

        /**
         * @brief 为指定物理对象设置碰撞响应优先级
         * @param object 目标物理对象指针，不可为空
         * @param priority 响应优先级，范围 [0, 100]。值越高越优先被回调。
         *                  默认值为 50。
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         * @exception ArgumentOutOfRangeException 当 priority 不在 [0, 100] 范围内时抛出
         *
         * @note 优先级仅在同一个碰撞对的两个对象间比较。
         */
        void SetCollisionPriority(PhysicsFormwork *object, int priority);

        /**
         * @brief 获取指定物理对象的碰撞响应优先级
         * @param object 目标物理对象指针，不可为空
         * @return 当前设置的优先级值
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        int GetCollisionPriority(PhysicsFormwork *object) const;

        /**
         * @brief 注册碰撞 Enter 事件回调
         * @param object 目标物理对象指针，不可为空
         * @param callback 碰撞 Enter 回调函数，当此对象与其他物体首次碰撞时触发
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @code
         * collision.AddCollisionEnterListener(myObj, [](const CollisionData& data) {
         *     std::cout << "Collision Enter at: " << data.ContactPoint.x << std::endl;
         * });
         * @endcode
         */
        void AddCollisionEnterListener(PhysicsFormwork *object, CollisionCallback callback);

        /**
         * @brief 注册碰撞 Stay 事件回调
         * @param object 目标物理对象指针，不可为空
         * @param callback 碰撞 Stay 回调函数，每帧碰撞持续时触发
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        void AddCollisionStayListener(PhysicsFormwork *object, CollisionCallback callback);

        /**
         * @brief 注册碰撞 Exit 事件回调
         * @param object 目标物理对象指针，不可为空
         * @param callback 碰撞 Exit 回调函数，碰撞结束时触发
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        void AddCollisionExitListener(PhysicsFormwork *object, CollisionCallback callback);

        /**
         * @brief 移除指定对象的所有碰撞回调绑定
         * @param object 目标物理对象指针，不可为空
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @note 当物理对象从世界中移除前应调用此方法，
         * 以避免悬空指针导致的未定义行为。
         */
        void RemoveAllCollisionListeners(PhysicsFormwork *object);

        /**
         * @brief 移除指定对象的碰撞层级绑定（不清除回调函数）
         * @param object 目标物理对象指针，不可为空
         */
        void RemoveCollisionBinding(PhysicsFormwork *object);

        /**
         * @brief 处理一帧的碰撞事件分发
         * @param collideGroupVector 当前帧活跃的碰撞对数组
         * @details 该方法应每帧物理模拟结束后被调用一次。
         * 比较当前活跃碰撞对与上一帧状态的差异，触发对应回调。
         *
         * 执行顺序：
         * 1. 收集当前帧所有活跃碰撞对
         * 2. 与上一帧状态对比，找出 Enter / Exit / Stay 事件
         * 3. 构造 CollisionData 并调用绑定的回调函数
         *
         * @note 线程安全：调用此方法期间不应同时修改绑定表。
         */
        void ProcessCollisions(const std::vector<BaseArbiter *> &collideGroupVector);

        /**
         * @brief 清理所有已注册的回调和绑定
         * @details 释放所有内部存储的绑定数据。
         * 通常在 PhysicsWorld 销毁前调用。
         */
        void Clear();

    private:
        /**
         * @brief 获取或创建指定对象的碰撞绑定
         * @param object 目标对象
         * @return 绑定引用
         */
        CollisionBinding &GetOrCreateBinding(PhysicsFormwork *object);

        /**
         * @brief 获取指定对象的绑定（只读）
         * @param object 目标对象
         * @return 绑定常量引用
         */
        const CollisionBinding &GetBinding(PhysicsFormwork *object) const;

        /**
         * @brief 检查两个对象的层级掩码是否有交集
         * @param a 对象 A
         * @param b 对象 B
         * @return 如果有交集可碰撞则返回 true
         */
        bool LayersOverlap(PhysicsFormwork *a, PhysicsFormwork *b) const;

        /**
         * @brief 将碰撞点数据填充为 CollisionData
         * @param arbiter 碰撞裁决器
         * @param contactIndex 碰撞点索引
         * @param self 自身对象
         * @param other 对方对象
         * @return 填充好的 CollisionData
         */
        CollisionData BuildCollisionData(const BaseArbiter *arbiter, int contactIndex,
                                          PhysicsFormwork *self, PhysicsFormwork *other) const;

        /**
         * @brief 生成碰撞对的字符串标识键
         */
        std::string MakePairKey(PhysicsFormwork *a, PhysicsFormwork *b) const;

        std::unordered_map<PhysicsFormwork *, CollisionBinding> mBindings;
        std::unordered_set<std::string> mActivePairs;
        mutable std::mutex mMutex;
    };

}
