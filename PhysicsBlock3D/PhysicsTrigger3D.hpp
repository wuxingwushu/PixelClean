#pragma once
#include "PhysicsBlockTypes3D.hpp"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <functional>

namespace PhysicsBlock3D
{
    class GridSearch3D;

    /**
     * @brief 触发器句柄 */
    using TriggerHandle3D = uint64_t;
    constexpr TriggerHandle3D InvalidTriggerHandle3D = 0;

    /**
     * @brief 触发器条目配置（3D） */
    struct TriggerConfig3D
    {
        /** @brief 触发器检测区域（三维 AABB） */
        Bounds3D TriggerBounds;

        /** @brief 可检测的碰撞层级掩码 */
        LayerMask TriggerLayers = LayerMaskAll;

        TriggerCallback OnTriggerEnter = nullptr;
        TriggerCallback OnTriggerStay = nullptr;
        TriggerCallback OnTriggerExit = nullptr;
    };

    /**
     * @brief 触发器系统管理类（3D）
     * @details 语义与 2D 版一致。3D 版的实质变化：
     *            - `Bounds` → `Bounds3D`（补 Z 轴）；
     *            - `GridSearch` → `GridSearch3D`；
     *            - 修复 2D 的两个既有缺陷：
     *              ① `TriggerLayers` 在 2D 中完整读写但 `ProcessTriggers` 从未使用
     *                 → 3D 通过可选的 `SetLayerProvider()` 真正生效；
     *              ② `nearbyObjects` 在 2D 中声明于触发器循环之外且 `Get()` 只
     *                 追加不清空，导致后续触发器会看到前面触发器的查询结果
     *                 （误报）→ 3D 每个触发器前清空。
     */
    class PhysicsTrigger3D
    {
    public:
        PhysicsTrigger3D() = default;
        ~PhysicsTrigger3D() = default;

        /** @brief 创建一个新的触发器并返回唯一句柄 */
        TriggerHandle3D CreateTrigger();

        /** @brief 移除指定触发器及其所有配置 */
        void RemoveTrigger(TriggerHandle3D handle);

        /** @brief 为指定触发器注册事件回调
         *  @exception ArgumentNullException 当 callback 为空时抛出 */
        void AddTriggerListener(TriggerHandle3D handle, TriggerEventType type, TriggerCallback callback);

        /** @brief 移除指定触发器的指定类型回调 */
        void RemoveTriggerListener(TriggerHandle3D handle, TriggerEventType type);

        /** @brief 设置触发器的检测区域 */
        void SetTriggerBounds(TriggerHandle3D handle, const Bounds3D &bounds);

        /** @brief 获取触发器的当前检测区域 */
        Bounds3D GetTriggerBounds(TriggerHandle3D handle) const;

        /** @brief 设置触发器可检测的层级掩码 */
        void SetTriggerLayers(TriggerHandle3D handle, LayerMask layers);

        /** @brief 获取触发器的当前层级掩码 */
        LayerMask GetTriggerLayers(TriggerHandle3D handle) const;

        /**
         * @brief 设置层级查询器，使 `TriggerLayers` 真正生效
         * @param provider 输入物体指针返回其层级掩码；未设置时层级过滤被跳过
         * @details 2D 版 `TriggerLayers` 是死字段（读写但从不参与判定）。
         *          3D 通过这个可选钩子把层级过滤接上：未提供时保持 2D 行为
         *          （全部命中），提供了则只对层级有交集的物体触发事件。 */
        void SetLayerProvider(std::function<LayerMask(const PhysicsFormwork3D *)> provider);

        /**
         * @brief 处理一帧的触发器事件分发
         * @param gridSearch 网格搜索实例
         * @details 粗筛用 AABB 查询，精确检测用「点包含 ∨ 球体与盒重叠」
         *          （2D 只有点包含，对半径较大的物体容易漏报）。
         *          锁内收集事件、锁外统一回调，避免重入死锁（沿用 2D 做法）。 */
        void ProcessTriggers(GridSearch3D &gridSearch);

        /** @brief 清理所有已注册的触发器和回调 */
        void Clear();

        /** @brief 获取所有触发器配置（只读） */
        const std::unordered_map<TriggerHandle3D, TriggerConfig3D> &GetConfigs() const { return mConfigs; }

    private:
        TriggerConfig3D &GetOrCreateConfig(TriggerHandle3D handle);
        const TriggerConfig3D &GetConfig(TriggerHandle3D handle) const;

        std::unordered_map<TriggerHandle3D, TriggerConfig3D> mConfigs;
        std::unordered_map<TriggerHandle3D, std::unordered_set<PhysicsFormwork3D *>> mActiveOverlaps;
        std::function<LayerMask(const PhysicsFormwork3D *)> mLayerProvider;
        TriggerHandle3D mNextHandle = InvalidTriggerHandle3D + 1;
        mutable std::mutex mMutex;
    };

}
