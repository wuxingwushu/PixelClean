#pragma once
#include "PhysicsShape.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理关节类
     * @details 用于连接两个PhysicsAngle物体的刚性关节，保持两物体间的相对位置关系
     *          支持序列化功能 */
    class PhysicsJoint SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        SerializationVirtualFunction;
        /** @brief 从JSON反序列化构造 */
        PhysicsJoint(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
#endif
    private:
        Mat2_ M;                          // 迭代矩阵，力方向
        Vec2_ localAnchor1, localAnchor2; // 物体局部坐标系下的锚点
        Vec2_ P;                          // 累计冲量
        Vec2_ bias;                       // 位置偏移修正量
        FLOAT_ biasFactor;                // 偏移矫正因子
        FLOAT_ softness;                  // 柔软度（影响关节的刚性）
    public:
        Vec2_ r1, r2;                // 世界坐标系下的锚点位置
        PhysicsAngle *body1, *body2; // 连接的两个物体
    public:
        /** @brief 默认构造函数 */
        PhysicsJoint(/* args */);
        /** @brief 析构函数 */
        ~PhysicsJoint();

        /** @brief 设置关节连接
         *  @param Body1 第一个物体
         *  @param Body2 第二个物体
         *  @param anchor 锚点位置（世界坐标） */
        void Set(PhysicsAngle *Body1, PhysicsAngle *Body2, const Vec2_ anchor);

        /** @brief 预处理阶段
         *  @param inv_dt 时间步长的倒数 */
        void PreStep(FLOAT_ inv_dt);

        /** @brief 应用冲量阶段 */
        void ApplyImpulse();
    };

}