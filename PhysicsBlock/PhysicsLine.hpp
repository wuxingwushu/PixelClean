#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    class PhysicsLine : public PhysicsAngle
    {
#if PhysicsBlock_Serialization
    public:
        PhysicsLine(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsAngle(data) {};
#endif
    public:
        PhysicsLine(Vec2_ begin_, Vec2_ end_, FLOAT_ mass_, FLOAT_ friction_ = 0.2);
        ~PhysicsLine(){};

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::line; }
    };

}
