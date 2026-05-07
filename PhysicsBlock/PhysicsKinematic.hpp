#pragma once
#include "PhysicsBlockTypes.hpp"
#include <unordered_map>
#include <mutex>

namespace PhysicsBlock
{

    /**
     * @brief 运动学运动状态
     * @details 存储一个运动学物体的目标移动/旋转数据，
     * 用于在一段时间内平滑过渡到目标位置/角度。
     */
    struct KinematicMotion
    {
        /**
         * @brief 是否正在执行移动动画
         */
        bool IsMoving = false;

        /**
         * @brief 移动起始位置（世界坐标）
         */
        Vec2_ MoveStartPos{0.0f, 0.0f};

        /**
         * @brief 移动目标位置（世界坐标）
         */
        Vec2_ MoveTargetPos{0.0f, 0.0f};

        /**
         * @brief 移动总持续时间（秒）
         */
        FLOAT_ MoveDuration = 0.0f;

        /**
         * @brief 移动已用时间（秒）
         */
        FLOAT_ MoveElapsed = 0.0f;

        /**
         * @brief 是否正在执行旋转动画
         */
        bool IsRotating = false;

        /**
         * @brief 旋转起始角度（弧度）
         */
        FLOAT_ RotateStartAngle = 0.0f;

        /**
         * @brief 旋转目标角度（弧度）
         */
        FLOAT_ RotateTargetAngle = 0.0f;

        /**
         * @brief 旋转总持续时间（秒）
         */
        FLOAT_ RotateDuration = 0.0f;

        /**
         * @brief 旋转已用时间（秒）
         */
        FLOAT_ RotateElapsed = 0.0f;
    };

    /**
     * @brief 运动学物体状态数据
     * @details 存储物体的运动学属性配置，包括 IsKinematic 标志
     * 和用于模式切换平滑过渡的缓存数据。
     */
    struct KinematicData
    {
        /**
         * @brief 是否为运动学物体
         * @details 为 true 时物体由外部代码驱动，不受重力/碰撞力影响；
         * 为 false 时物体由物理引擎全权控制。
         */
        bool IsKinematic = false;

        /**
         * @brief 模式切换前保存的速度（用于恢复动态模式）
         */
        Vec2_ SavedSpeed{0.0f, 0.0f};

        /**
         * @brief 模式切换前保存的角速度（用于恢复动态模式）
         */
        FLOAT_ SavedAngleSpeed = 0.0f;

        /**
         * @brief 运动学运动数据（移动/旋转动画）
         */
        KinematicMotion Motion;
    };

    /**
     * @brief 运动学物体属性管理类
     * @details 管理物理对象的运动学属性切换与运动控制。
     *
     * 运动学物体（Kinematic Body）的特性：
     * - IsKinematic == true 时：
     *   - 不受重力影响（重力加速度被跳过）
     *   - 不受碰撞冲量影响（invMass 被临时设为 0）
     *   - 位置/角度完全由 MoveTo() / RotateTo() 或外部代码控制
     *   - 仍然参与碰撞检测，可向动态物体施加碰撞响应
     * - IsKinematic == false 时：
     *   - 恢复为正常动态物体行为
     *   - 保留模式切换前的速度实现平滑过渡
     *
     * 平滑过渡机制：
     * - 切换到运动学模式时：保存当前速度并清零物理速度
     * - 切换回动态模式时：恢复之前保存的速度，避免速度突变
     * - MoveTo/RotateTo 在指定时间内使用线性插值平滑移动
     *
     * @warning 运动学模式切换后应立即调用 SynchronizePhysicsState()
     * 以确保物理引擎内部状态一致。
     */
    class PhysicsKinematic
    {
    public:
        /**
         * @brief 默认构造函数
         */
        PhysicsKinematic() = default;

        /**
         * @brief 析构函数
         */
        ~PhysicsKinematic() = default;

        /**
         * @brief 设置物体是否为运动学模式
         * @param object 目标物理对象指针，不可为空
         * @param kinematic true 为运动学模式，false 为动态模式
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @note 调用此方法后会自动触发物理状态同步。
         * 运动学模式切换时的速度平滑过渡由内部自动处理。
         *
         * @code
         * // 将对象设为运动学物体并移动到新位置
         * kinematic.SetIsKinematic(myObj, true);
         * kinematic.MoveTo(myObj, {500, 300}, 2.0f);
         * @endcode
         */
        void SetIsKinematic(PhysicsFormwork *object, bool kinematic);

        /**
         * @brief 获取物体当前是否为运动学模式
         * @param object 目标物理对象指针，不可为空
         * @return true 表示运动学模式，false 表示动态模式
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        bool GetIsKinematic(PhysicsFormwork *object) const;

        /**
         * @brief 将运动学物体平滑移动到目标位置
         * @param object 目标物理对象指针（必须是运动学物体），不可为空
         * @param position 目标世界坐标位置
         * @param duration 移动持续时间（秒），必须大于 0
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         * @exception ArgumentOutOfRangeException 当 duration <= 0 时抛出
         * @exception PhysicsException 当物体不是运动学模式时抛出
         *
         * @details 在 duration 秒内使用线性插值（Lerp）将物体从
         * 当前位置平滑移动到目标位置。
         *
         * @code
         * // 2 秒内移动到 (100, 200)
         * kinematic.MoveTo(kinematicObj, {100.0f, 200.0f}, 2.0f);
         * @endcode
         */
        void MoveTo(PhysicsFormwork *object, const Vec2_ &position, FLOAT_ duration);

        /**
         * @brief 将运动学物体平滑旋转到目标角度
         * @param object 目标物理对象指针（必须是运动学物体），不可为空
         * @param rotation 目标角度（弧度）
         * @param duration 旋转持续时间（秒），必须大于 0
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         * @exception ArgumentOutOfRangeException 当 duration <= 0 时抛出
         * @exception PhysicsException 当物体不是运动学模式时抛出
         *
         * @details 在 duration 秒内使用线性插值（Lerp）将物体从
         * 当前角度平滑旋转到目标角度。
         *
         * @code
         * // 1.5 秒内旋转到 90 度（π/2 弧度）
         * kinematic.RotateTo(kinematicObj, 1.5708f, 1.5f);
         * @endcode
         */
        void RotateTo(PhysicsFormwork *object, FLOAT_ rotation, FLOAT_ duration);

        /**
         * @brief 同步运动学物体的物理引擎状态
         * @param object 目标物理对象指针，不可为空
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         *
         * @details 确保 IsKinematic 属性变更后物理引擎的
         * 质量倒数、速度等内部状态正确更新。
         * 通常在 SetIsKinematic 后自动调用，也可手动调用。
         */
        void SynchronizePhysicsState(PhysicsFormwork *object);

        /**
         * @brief 更新所有运动学物体的位置/旋转动画
         * @param deltaTime 时间步长（秒），必须大于 0
         * @exception ArgumentOutOfRangeException 当 deltaTime <= 0 时抛出
         *
         * @details 每帧调用一次，在物理更新前执行。
         * 更新所有处于 MoveTo/RotateTo 动画中的运动学物体位置和角度。
         * 动画完成后自动停止。
         *
         * @note 应在 PhysicsWorld::PhysicsEmulator 之前调用，
         * 使运动学物体的新位置能参与碰撞检测。
         */
        void UpdateKinematicMotion(FLOAT_ deltaTime);

        /**
         * @brief 移除指定物体的所有运动学属性
         * @param object 目标物理对象指针，不可为空
         * @exception ArgumentNullException 当 object 为 nullptr 时抛出
         */
        void RemoveKinematicData(PhysicsFormwork *object);

        /**
         * @brief 清理所有运动学数据
         */
        void Clear();

    private:
        /**
         * @brief 获取或创建物体的运动学数据
         * @param object 目标对象
         * @return KinematicData 引用
         */
        KinematicData &GetOrCreateData(PhysicsFormwork *object);

        /**
         * @brief 获取物体的运动学数据（只读）
         * @param object 目标对象
         * @return KinematicData 常量引用
         */
        const KinematicData &GetData(PhysicsFormwork *object) const;

        void SynchronizePhysicsStateImpl(PhysicsFormwork *object);

        /**
         * @brief 线性插值辅助函数
         */
        static FLOAT_ Lerp(FLOAT_ a, FLOAT_ b, FLOAT_ t)
        {
            return a + (b - a) * t;
        }

        std::unordered_map<PhysicsFormwork *, KinematicData> mKinematicData;
        mutable std::mutex mMutex;
    };

}
