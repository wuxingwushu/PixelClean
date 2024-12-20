#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    PhysicsShape::PhysicsShape(glm::dvec2 Pos, glm::ivec2 Size) : PhysicsParticle(Pos), BaseOutline(Size.x, Size.y)
    {
    }

    PhysicsShape::~PhysicsShape()
    {
    }

    CollisionInfoI PhysicsShape::DropCollision(glm::dvec2 Pos)
    {
        Pos -= pos;
        Pos = vec2angle(Pos, -angle);
        CollisionInfoI info;
        info.pos = ToInt(Pos + CentreMass);
        if (Pos.x == width)
        {
            info.pos.x = width - 1;
        }
        if (Pos.y == height)
        {
            info.pos.y = height - 1;
        }
        info.Collision = GetCollision(info.pos);
        return info;
    }

    void PhysicsShape::AddForce(glm::dvec2 Pos, glm::dvec2 Force)
    {
        Pos -= pos;                                                    // 质心指向受力点的力矩
        double Langle = EdgeVecToCosAngleFloat(Pos);                   // 力臂角度
        double disparity = Langle - EdgeVecToCosAngleFloat(Force);     // 力矩角度
        double ForceDouble = Modulus(Force);                           // 力大小
        double F = ForceDouble * cos(disparity);                       // 垂直力臂的力大小
        PhysicsParticle::AddForce({F * cos(Langle), F * sin(Langle)}); // 转为世界力矩 // 位置移动受到力矩
        AddTorque(Modulus(Pos), ForceDouble * sin(disparity));         // 旋转受到扭矩
    }

    void PhysicsShape::AddTorque(double ArmForce, double Force)
    {
        torque += ArmForce * Force; // 累加扭矩
    }

    void PhysicsShape::UpdateInfo()
    {
        mass = 0;
        unsigned int Size = 0;    // 实体格子数
        unsigned int i;           // 存储网格索引
        CentreMass = {0.0, 0.0};  // 清空位置
        CentreShape = {0.0, 0.0}; // 清空位置
        for (int x = 0; x < width; ++x)
        {
            for (int y = 0; y < height; ++y)
            {
                i = x * width + y;
                // 不为空
                if (at(i).type & GridBlockType::Entity)
                {
                    mass += at(i).mass;
                    ++Size;
                    CentreShape += glm::ivec2{x, y};
                    CentreMass += (glm::dvec2{x, y} * at(i).mass);
                }
            }
        }
        CentreShape /= Size;
        CentreMass /= mass;
        CentreShape += glm::dvec2{0.5, 0.5}; // 移动到格子的中心
        CentreMass += glm::dvec2{0.5, 0.5};  // 移动到格子的中心

        MomentInertia = 0; // 清空转动惯量
        glm::dvec2 lpos;   // 存储临时位置
        double Lmass;      // 用于存储切割出的小正方形的质量
        for (double x = 0; x < width; ++x)
        {
            for (double y = 0; y < height; ++y)
            {
                // 不存在跳过
                if ((at(i).Entity) == 0)
                {
                    continue;
                }
                // 获取格子切分的小格子质量
                Lmass = at(x, y).mass / (MomentInertiaSamplingSize * MomentInertiaSamplingSize);
                for (int cx = 0; cx < MomentInertiaSamplingSize; ++cx)
                {
                    for (int cy = 0; cy < MomentInertiaSamplingSize; ++cy)
                    {
                        // 得到采样点的位置
                        lpos = {x + ((1.0 / MomentInertiaSamplingSize) * cx + (0.5 / MomentInertiaSamplingSize)), y + ((1.0 / MomentInertiaSamplingSize) * cy + (0.5 / MomentInertiaSamplingSize))};
                        // 获取位置偏移量（方向，距离）
                        lpos -= CentreMass;
                        // 累加转动惯量
                        MomentInertia += Lmass * (lpos.x * lpos.x + lpos.y * lpos.y);
                    }
                }
            }
        }
    }

    void PhysicsShape::PhysicsEmulator(double time, glm::dvec2 Ga)
    {
        PhysicsParticle::PhysicsEmulator(time, Ga);           // 位置的物理演算
        double AddAngleSpeed = torque / MomentInertia * time; // 角速度的增加量
        angle += (angleSpeed + (AddAngleSpeed / 2)) * time;    // 角度
        angleSpeed += AddAngleSpeed;                          // 角速度
        torque = 0;                                           // 清空扭矩
    }

    glm::dvec2 PhysicsShape::PhysicsPlayact(double time, glm::dvec2 Ga)
    {
        return PhysicsParticle::PhysicsPlayact(time, Ga); // 位置的物理演戏
    }

}