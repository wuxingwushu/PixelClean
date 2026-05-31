#pragma once
#include "PhysicsBlockTypes.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace PhysicsBlock
{
    class GridSearch;

    /**
     * @brief 触发器条目配置
     * @details 存储一个触发器的完整配置信息，包括检测范围、
     * 层级过滤和事件回调绑定。
     */
    struct TriggerConfig
    {
        /**
         * @brief 触发器检测区域
         * @details 轴对齐包围盒（AABB），定义触发器的检测范围。
         * 只有位置在此范围内的物体才会触发回调。
         */
        Bounds TriggerBounds;

        /**
         * @brief 可检测的碰撞层级掩码
         * @details 只有层级与此掩码有交集的物体才会被检测。
         * 默认为 LayerMaskAll（检测所有层级）。
         */
        LayerMask TriggerLayers = LayerMaskAll;

        /**
         * @brief Enter 事件回调函数
         * @details 物体首次进入触发器区域时调用。
         * 回调参数为进入触发器的物体指针。
         */
        TriggerCallback OnTriggerEnter = nullptr;

        /**
         * @brief Stay 事件回调函数
         * @details 物体持续停留在触发器区域内时每帧调用。
         * 回调参数为停留在触发器内的物体指针。
         */
        TriggerCallback OnTriggerStay = nullptr;

        /**
         * @brief Exit 事件回调函数
         * @details 物体离开触发器区域时调用。
         * 回调参数为离开触发器的物体指针。
         */
        TriggerCallback OnTriggerExit = nullptr;
    };

    /**
     * @brief 触发器系统管理类
     * @details 管理所有触发器的配置与事件分发。
     *
     * 典型使用流程：
     * 1. 调用 AddTriggerListener() 为指定对象注册触发回调
     * 2. 调用 SetTriggerBounds() 配置检测区域
     * 3. 调用 SetTriggerLayers() 配置层级过滤
     * 4. 在物理更新循环中调用 ProcessTriggers() 处理事件分发
     *
     * 触发器与碰撞回调的区别：
     * - 触发器不产生物理响应（不改变速度和位置），仅检测空间重叠
     * - 碰撞回调伴随真实的物理冲量交换
     * - 同一对象可以同时拥有触发器和碰撞回调
     *
     * @note 触发器事件在碰撞响应完成后执行，确保物理状态已更新完毕。
     * @warning 回调函数中不应修改触发器的配置或删除物理对象。
     */
    class PhysicsTrigger
    {
    public:
        /**
         * @brief 默认构造函数
         */
        PhysicsTrigger() = default;

        /**
         * @brief 析构函数
         */
        ~PhysicsTrigger() = default;

        /**
         * @brief 为指定对象注册触发器事件回调
         * @param object 目标物理对象指针，不可为空
         * @param type 要注册的触发器事件类型（Enter / Stay / Exit）
         * @param callback 事件回调函数。Enter 在物体进入区域时触发，
         *                 Stay 在每帧停留时触发，Exit 在离开区域时触发。
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         * @exception ArgumentNullException 当 callback 为空时抛出
         *
         * @code
         * trigger.AddTriggerListener(myObj, TriggerEventType::Enter,
         *     [](PhysicsFormwork* other) {
         *         std::cout << "Object entered trigger zone!" << std::endl;
         *     });
         * @endcode
         */
        void AddTriggerListener(PhysicsFormwork *object, TriggerEventType type, TriggerCallback callback);

        /**
         * @brief 移除指定对象的指定类型触发器回调
         * @param object 目标物理对象指针，不可为空
         * @param type 要移除的触发器事件类型
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        void RemoveTriggerListener(PhysicsFormwork *object, TriggerEventType type);

        /**
         * @brief 移除指定对象的所有触发器回调和配置
         * @param object 目标物理对象指针，不可为空
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @note 当物理对象从世界中移除前应调用此方法。
         */
        void RemoveAllTriggerListeners(PhysicsFormwork *object);

        /**
         * @brief 设置触发器的检测区域
         * @param object 目标物理对象指针，不可为空
         * @param bounds 轴对齐包围盒（AABB）定义检测区域。
         *               center 为中心位置（世界坐标），
         *               halfSize 为从中心到各边的半尺寸。
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @code
         * // 在 (100, 200) 处创建一个 50x50 的触发区
         * trigger.SetTriggerBounds(myObj, Bounds({100, 200}, {25, 25}));
         * @endcode
         */
        void SetTriggerBounds(PhysicsFormwork *object, const Bounds &bounds);

        /**
         * @brief 获取触发器的当前检测区域
         * @param object 目标物理对象指针，不可为空
         * @return 当前设置的检测区域边界
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        Bounds GetTriggerBounds(PhysicsFormwork *object) const;

        /**
         * @brief 设置触发器可检测的层级掩码
         * @param object 目标物理对象指针，不可为空
         * @param layers 层级掩码。只有与此掩码有交集层级的物体才会触发事件。
         *               默认值为 LayerMaskAll（检测所有层级）。
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @code
         * // 只检测第 0 层和第 2 层的物体
         * trigger.SetTriggerLayers(myObj, (1 << 0) | (1 << 2));
         * @endcode
         */
        void SetTriggerLayers(PhysicsFormwork *object, LayerMask layers);

        /**
         * @brief 获取触发器的当前层级掩码
         * @param object 目标物理对象指针，不可为空
         * @return 当前设置的层级掩码
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        LayerMask GetTriggerLayers(PhysicsFormwork *object) const;

        /**
         * @brief 处理一帧的触发器事件分发
         * @param gridSearch 网格搜索实例，用于高效查询触发器区域内的物体
         * @details 遍历所有已注册触发器的检测区域，通过 GridSearch
         * 空间索引获取区域附近的物体列表，再进行精确的 AABB 重叠检测，
         * 触发 Enter / Stay / Exit 事件。
         *
         * 执行顺序：
         * 1. 对每个已注册的触发器，通过 GridSearch 查询 Bounds 范围内的物体
         * 2. 对查询结果逐一进行精确的 AABB 包含检测
         * 3. 与上一帧状态对比，触发 Enter / Stay / Exit 回调
         *
         * @note 应在碰撞响应完成后调用，确保对象位置已更新。
         */
        void ProcessTriggers(GridSearch &gridSearch);

        /**
         * @brief 清理所有已注册的触发器和回调
         * @details 释放所有内部存储的触发器配置数据和状态。
         */
        void Clear();

    private:
        /**
         * @brief 获取或创建指定对象的触发器配置
         * @param object 目标对象
         * @return TriggerConfig 的引用
         */
        TriggerConfig &GetOrCreateConfig(PhysicsFormwork *object);

        /**
         * @brief 获取指定对象的触发器配置（只读版本）
         * @param object 目标对象
         * @return TriggerConfig 的常量引用
         */
        const TriggerConfig &GetConfig(PhysicsFormwork *object) const;

        std::unordered_map<PhysicsFormwork *, TriggerConfig> mConfigs;
        std::unordered_map<PhysicsFormwork *, std::unordered_set<PhysicsFormwork *>> mActiveOverlaps;
        mutable std::mutex mMutex;
    };

}
