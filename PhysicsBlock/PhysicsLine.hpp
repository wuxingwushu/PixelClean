#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理线段类
     * @details 表示一个具有物理属性的线段，继承自PhysicsAngle
     *          用于模拟线条状物体的物理行为 */
    class PhysicsLine : public PhysicsAngle
    {
#if PhysicsBlock_Serialization
    public:
        /** @brief 从JSON反序列化构造 */
        PhysicsLine(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsAngle(data) { type = PhysicsObjectEnum::line; };
#endif
    public:
        /** @brief 构造物理线段
         * @param begin_ 线段起点
         * @param end_ 线段终点
         * @param mass_ 质量
         * @param friction_ 摩擦系数，默认为0.2 */
        PhysicsLine(Vec2_ begin_, Vec2_ end_, FLOAT_ mass_, FLOAT_ friction_ = 0.2);
        /** @brief 析构函数 */
        ~PhysicsLine(){};

    };

}
