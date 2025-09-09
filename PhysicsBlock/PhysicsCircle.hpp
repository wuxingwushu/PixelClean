#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    class PhysicsCircle : public PhysicsAngle
    {
#if PhysicsBlock_Serialization
    public:
        PhysicsCircle(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsAngle(data) {};
#endif
    public:
        PhysicsCircle(Vec2_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction = 0.2);
        ~PhysicsCircle();


        /*=========PhysicsFormwork=========*/

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::circle; }
        
    };
    
}
