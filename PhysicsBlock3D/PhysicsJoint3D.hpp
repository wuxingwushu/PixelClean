#pragma once
#include "PhysicsShape3D.hpp"

namespace PhysicsBlock3D
{

    /**
     * @brief 物理关节类（3D）
     * @details 用于连接两个 PhysicsAngle3D 物体的刚性关节，保持两物体间的相对位置关系。
     *          3D 版相对 2D 版的关键变化：
     *            - 位置约束由 2 DOF（x/y）变为 3 DOF（x/y/z），`Mat2_ M` → `Mat3_ M`；
     *            - 旋转由标量 `angle` 变为四元数 `rotation`，角速度由标量变 `Vec3_`；
     *            - 有效质量矩阵 K 由 2D 的 `invI*(|r|²E - r⊗r)` 推广为
     *              `-Skew(r) * WorldInvInertia() * Skew(r)`（各向异性惯量张量可用）；
     *            - 所有"用 invI 乘世界系向量"处必须走 `WorldInvInertia()`，
     *              因为 3D 惯量张量随朝向变化（2D 的标量 Izz 与朝向无关）。
     *          支持序列化功能 */
    class PhysicsJoint3D SerializationInherit3D_
    {
#if PhysicsBlock_Serialization3D
    public:
        SerializationVirtualFunction3D;
        /** @brief 从JSON反序列化构造 */
        PhysicsJoint3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
#endif
    private:
        Mat3_ M;                            // 迭代矩阵，力方向
        Vec3_ localAnchor1, localAnchor2;   // 物体局部坐标系下的锚点
        Vec3_ P;                            // 累计冲量
        Vec3_ bias;                         // 位置偏移修正量
        FLOAT_ biasFactor;                  // 偏移矫正因子
        FLOAT_ softness;                    // 柔软度（影响关节的刚性）
    public:
        Vec3_ r1, r2;                  // 世界坐标系下的锚点位置
        PhysicsAngle3D *body1, *body2; // 连接的两个物体
    public:
        /** @brief 默认构造函数 */
        PhysicsJoint3D(/* args */);
        /** @brief 析构函数 */
        ~PhysicsJoint3D();

        /** @brief 设置关节连接
         *  @param Body1 第一个物体
         *  @param Body2 第二个物体
         *  @param anchor 锚点位置（世界坐标） */
        void Set(PhysicsAngle3D *Body1, PhysicsAngle3D *Body2, const Vec3_ anchor);

        /** @brief 预处理阶段
         *  @param inv_dt 时间步长的倒数 */
        void PreStep(FLOAT_ inv_dt);

        /** @brief 应用冲量阶段 */
        void ApplyImpulse();
    };

}
