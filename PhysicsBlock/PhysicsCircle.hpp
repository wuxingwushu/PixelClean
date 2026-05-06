#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理圆形类
     * @details 继承自 PhysicsAngle，表示一个圆形的物理对象
     * 包含位置、半径、质量、摩擦因数等属性
     */
    class PhysicsCircle : public PhysicsAngle
    {
#if PhysicsBlock_Serialization
    public:
        /**
         * @brief 从 JSON 数据构造物理圆形对象
         * @param data JSON 数据
         */
        PhysicsCircle(const nlohmann::json_abi_v3_12_0::basic_json<>& data) : PhysicsAngle(data) { type = PhysicsObjectEnum::circle; };
#endif
    public:
        /**
         * @brief 构造函数
         * @param Pos 圆形的位置
         * @param Radius 圆形的半径
         * @param Mass 圆形的质量
         * @param Friction 摩擦因数，默认为0.2
         * @details 初始化物理圆形对象，设置位置、半径、质量和摩擦因数
         * 并计算转动惯量
         */
        PhysicsCircle(Vec2_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction = 0.2);

        /**
         * @brief 析构函数
         */
        ~PhysicsCircle();

        /*=========PhysicsFormwork=========*/

    };

}