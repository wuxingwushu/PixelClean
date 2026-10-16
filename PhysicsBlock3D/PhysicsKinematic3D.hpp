#pragma once
#include "PhysicsBlockTypes3D.hpp"
#include "BaseStruct3D.hpp"
#include <unordered_map>
#include <mutex>

namespace PhysicsBlock3D
{

    enum class KinematicMoveMode3D
    {
        Once,
        PingPong
    };

    /**
     * @brief 运动学运动状态（3D）
     * @details 存储一个运动学物体的目标移动/旋转数据。
     *          3D 相对 2D 的关键变化：
     *            - 位置由 `Vec2_` 变 `Vec3_`（多 Z 轴）；
     *            - 旋转由标量角度变**四元数**，插值走 `glm::slerp` 而非线性 lerp
     *              —— 2D 线性插值角度的做法在 3D 会导致非匀速旋转且需处理绕远路，
     *              slerp 天然走最短弧且角速度均匀。
     */
    struct KinematicMotion3D
    {
        /** @brief 是否正在执行移动动画 */
        bool IsMoving = false;

        /** @brief 移动起始位置（世界坐标） */
        Vec3_ MoveStartPos{0.0f};

        /** @brief 移动目标位置（世界坐标） */
        Vec3_ MoveTargetPos{0.0f};

        /** @brief 移动总持续时间（秒） */
        FLOAT_ MoveDuration = 0.0f;

        /** @brief 移动已用时间（秒） */
        FLOAT_ MoveElapsed = 0.0f;

        KinematicMoveMode3D MoveMode = KinematicMoveMode3D::Once;

        /** @brief 是否正在执行旋转动画 */
        bool IsRotating = false;

        /** @brief 旋转起始朝向（四元数） */
        Quat_ RotateStartRotation{1.0f, 0.0f, 0.0f, 0.0f};

        /** @brief 旋转目标朝向（四元数） */
        Quat_ RotateTargetRotation{1.0f, 0.0f, 0.0f, 0.0f};

        /** @brief 旋转总持续时间（秒） */
        FLOAT_ RotateDuration = 0.0f;

        /** @brief 旋转已用时间（秒） */
        FLOAT_ RotateElapsed = 0.0f;

        KinematicMoveMode3D RotateMode = KinematicMoveMode3D::Once;
    };

    /**
     * @brief 运动学物体状态数据（3D） */
    struct KinematicData3D
    {
        /** @brief 是否为运动学物体 */
        bool IsKinematic = false;

        /** @brief 模式切换前保存的速度 */
        Vec3_ SavedSpeed{0.0f};

        /** @brief 模式切换前保存的角速度（3D 为向量） */
        Vec3_ SavedAngleSpeed{0.0f};

        /** @brief 运动学运动数据 */
        KinematicMotion3D Motion;
    };

    /**
     * @brief 运动学物体属性管理类（3D）
     * @details 语义与 2D 版一致：运动学物体不受重力/冲量影响、位置朝向由外部驱动，
     *          但仍参与碰撞检测。3D 版的实质变化只在数据类型（Vec3_ / Quat_ /
     *          Mat3_）与旋转插值方式（slerp）。
     *
     * @warning 运动学模式切换后应立即调用 SynchronizePhysicsState3D()
     * 以确保物理引擎内部状态一致。
     */
    class PhysicsKinematic3D
    {
    public:
        PhysicsKinematic3D() = default;
        ~PhysicsKinematic3D() = default;

        /** @brief 设置物体是否为运动学模式
         *  @exception ArgumentNullException 当 object 为 nullptr 时抛出 */
        void SetIsKinematic(PhysicsFormwork3D *object, bool kinematic);

        /** @brief 获取物体当前是否为运动学模式
         *  @exception ArgumentNullException 当 object 为 nullptr 时抛出 */
        bool GetIsKinematic(PhysicsFormwork3D *object) const;

        /** @brief 将运动学物体平滑移动到目标位置（三维线性插值）
         *  @exception ArgumentNullException / ArgumentOutOfRangeException / PhysicsException */
        void MoveTo(PhysicsFormwork3D *object, const Vec3_ &position, FLOAT_ duration);

        /** @brief 将运动学物体平滑旋转到目标朝向（四元数 slerp）
         *  @param object 目标物理对象指针（必须是运动学物体），不可为空
         *  @param rotation 目标朝向四元数（内部归一化）
         *  @param duration 旋转持续时间（秒），必须大于 0
         *  @exception ArgumentNullException / ArgumentOutOfRangeException / PhysicsException
         *  @details 2D 传标量角度且用线性插值；3D 传四元数并用 slerp，
         *           自动走最短弧、角速度均匀，无需 2D 那样手动处理绕远路。 */
        void RotateTo(PhysicsFormwork3D *object, const Quat_ &rotation, FLOAT_ duration);

        void SetMoveMode(PhysicsFormwork3D *object, KinematicMoveMode3D mode);
        void SetRotateMode(PhysicsFormwork3D *object, KinematicMoveMode3D mode);

        /** @brief 同步运动学物体的物理引擎状态 */
        void SynchronizePhysicsState(PhysicsFormwork3D *object);

        /** @brief 更新所有运动学物体的位置/旋转动画
         *  @param deltaTime 时间步长（秒），必须大于 0
         *  @exception ArgumentOutOfRangeException 当 deltaTime <= 0 时抛出 */
        void UpdateKinematicMotion(FLOAT_ deltaTime);

        /** @brief 移除指定物体的所有运动学属性 */
        void RemoveKinematicData(PhysicsFormwork3D *object);

        /** @brief 清理所有运动学数据 */
        void Clear();

    private:
        KinematicData3D &GetOrCreateData(PhysicsFormwork3D *object);
        const KinematicData3D &GetData(PhysicsFormwork3D *object) const;
        void SynchronizePhysicsStateImpl(PhysicsFormwork3D *object);

        /** @brief 三维线性插值辅助函数 */
        static Vec3_ Lerp(const Vec3_ &a, const Vec3_ &b, FLOAT_ t)
        {
            return a + (b - a) * t;
        }

        std::unordered_map<PhysicsFormwork3D *, KinematicData3D> mKinematicData;
        mutable std::mutex mMutex;
    };

}
