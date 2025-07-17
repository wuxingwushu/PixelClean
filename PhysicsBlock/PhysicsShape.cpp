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
        AddTorque(Modulus(Pos), ForceDouble * -sin(disparity));         // 旋转受到扭矩
    }

    void PhysicsShape::AddTorque(double ArmForce, double Force)
    {
        torque += ArmForce * Force; // 累加扭矩
    }

    void PhysicsShape::UpdateInfo()
    {
        glm::dvec2 UsedCentreMass = CentreMass;//旧质心
        mass = 0;
        unsigned int Size = 0;    // 实体格子数
        unsigned int i;           // 存储网格索引
        CentreMass = {0.0, 0.0};  // 清空位置
        CentreShape = {0.0, 0.0}; // 清空位置
        for (int x = 0; x < width; ++x)
        {
            for (int y = 0; y < height; ++y)
            {
                i = x * height + y;
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
        invMass = 1.0 / mass;
        if (invMass == 0) {
            mass = DBL_MAX;
            CentreMass = CentreShape;
        }else {
            CentreMass /= mass;
        }
        CentreShape += glm::dvec2{0.5, 0.5}; // 移动到格子的中心
        CentreMass += glm::dvec2{0.5, 0.5};  // 移动到格子的中心

        // 因为位置是指 质心 在世界坐标的位置，质心改了，位置也需要进行偏移
        pos += vec2angle(CentreMass - UsedCentreMass, angle);
        OldPos = pos;

        MomentInertia = 0; // 清空转动惯量
        glm::dvec2 lpos;   // 存储临时位置
        double Lmass;      // 用于存储切割出的小正方形的质量
        for (double x = 0; x < width; ++x)
        {
            for (double y = 0; y < height; ++y)
            {
                // 不存在跳过
                i = x * height + y;
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
        invMomentInertia = 1.0 / MomentInertia;
        if (invMomentInertia == 0) {
            MomentInertia = DBL_MAX;
        }
    }

    void PhysicsShape::UpdateCollisionR() {
        CollisionR = 0;
        double R;
        for (size_t i = 0; i < OutlineSize; i++)
        {
            R = ModulusLength(OutlineSet[i]);
            if (CollisionR < R) {
                CollisionR = R;
            }
        }
        CollisionR = sqrt(CollisionR);
    }

    void PhysicsShape::UpdateAll() {
        UpdateInfo();
        UpdateOutline(CentreMass);
        UpdateCollisionR();
    }

    void PhysicsShape::ApproachDrop(glm::dvec2 drop){
        glm::dvec2 OutlineDrop = vec2angle({CollisionR, 0}, EdgeVecToCosAngleFloat(drop - pos));
        CollisionInfoD info = BresenhamDetection(CentreMass + OutlineDrop, CentreMass - OutlineDrop);
        pos += drop - (info.pos + pos);
    }

    CollisionInfoD PhysicsShape::RayCollide(glm::dvec2 Pos, double Angle){
        glm::dvec2 drop = vec2angle({CollisionR, 0}, Angle);
        return PsBresenhamDetection(Pos - drop, Pos + drop);
    }

    CollisionInfoI PhysicsShape::PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end){
        return BresenhamDetection(start, end);
    }

    CollisionInfoD PhysicsShape::PsBresenhamDetection(glm::dvec2 start, glm::dvec2 end){
        // 偏移中心位置，对其网格坐标系
        start -= pos;
        end -= pos;

        AngleMat Mat(-angle);
        start = Mat.Rotary(start);
        end = Mat.Rotary(end);
        
        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start + CentreMass, end + CentreMass, width, height);
        if (data.Focus)
        {
            // 线段碰撞检测
            CollisionInfoD info = BresenhamDetection(data.start, data.end);
            if (info.Collision)
            {
                // 返回物理坐标系
                info.pos = Mat.Anticlockwise(info.pos - CentreMass) + pos;
                return info;
            }
        }
        return {false};
    }

    void PhysicsShape::PhysicsSpeed(double time, glm::dvec2 Ga)
    {
        PhysicsParticle::PhysicsSpeed(time, Ga);
        angleSpeed += time * invMomentInertia * torque;
        torque = 0;
    }

    void PhysicsShape::PhysicsPos(double time, glm::dvec2 Ga)
    {
        PhysicsParticle::PhysicsPos(time, Ga);
        angle += time * angleSpeed;
    }

}