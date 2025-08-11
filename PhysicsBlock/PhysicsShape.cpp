#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    PhysicsShape::PhysicsShape(Vec2_ Pos, glm::ivec2 Size) : PhysicsAngle(Pos), BaseOutline(Size.x, Size.y)
    {
    }

    PhysicsShape::~PhysicsShape()
    {
    }

    CollisionInfoI PhysicsShape::DropCollision(Vec2_ Pos)
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

    void PhysicsShape::AddTorque(FLOAT_ ArmForce, FLOAT_ Force)
    {
        torque += ArmForce * Force; // 累加扭矩
    }

    void PhysicsShape::UpdateInfo()
    {
        Vec2_ UsedCentreMass = CentreMass; // 旧质心
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
                    CentreMass += (Vec2_{x, y} * at(i).mass);
                }
            }
        }
        CentreShape /= Size;
        invMass = 1.0 / mass;
        if (invMass == 0)
        {
            mass = DBL_MAX;
            CentreMass = CentreShape;
        }
        else
        {
            CentreMass /= mass;
        }
        CentreShape += Vec2_{0.5, 0.5}; // 移动到格子的中心
        CentreMass += Vec2_{0.5, 0.5};  // 移动到格子的中心

        // 因为位置是指 质心 在世界坐标的位置，质心改了，位置也需要进行偏移
        pos += vec2angle(CentreMass - UsedCentreMass, angle);
        OldPos = pos;

        MomentInertia = 0; // 清空转动惯量
        Vec2_ lpos;        // 存储临时位置
        FLOAT_ Lmass;      // 用于存储切割出的小正方形的质量
        for (FLOAT_ x = 0; x < width; ++x)
        {
            for (FLOAT_ y = 0; y < height; ++y)
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
        if (invMomentInertia == 0)
        {
            MomentInertia = DBL_MAX;
        }
    }

    void PhysicsShape::UpdateCollisionR()
    {
        CollisionR = 0;
        FLOAT_ R;
        for (size_t i = 0; i < OutlineSize; i++)
        {
            R = ModulusLength(OutlineSet[i]);
            if (CollisionR < R)
            {
                CollisionR = R;
            }
        }
        CollisionR = SQRT_(CollisionR);
    }

    void PhysicsShape::UpdateAll()
    {
        if (mass == FLOAT_MAX)
        {
            invMass = 0;
            MomentInertia = FLOAT_MAX;
            invMomentInertia = 0;
            //Vec2_ UsedCentreMass = CentreMass; // 旧质心
            CentreMass = {width / 2 + 0.5, height / 2 + 0.5};
            CentreShape = CentreMass;
            //pos += vec2angle(CentreMass - UsedCentreMass, angle);
            OldPos = pos;
        }
        else
        {
            UpdateInfo();
        }
        UpdateOutline(CentreMass);
        UpdateCollisionR();
    }

    void PhysicsShape::ApproachDrop(Vec2_ drop)
    {
        Vec2_ OutlineDrop = vec2angle({CollisionR, 0}, EdgeVecToCosAngleFloat(drop - pos));
        CollisionInfoD info = BresenhamDetection(CentreMass + OutlineDrop, CentreMass - OutlineDrop);
        pos += drop - (info.pos + pos);
    }

    CollisionInfoD PhysicsShape::RayCollide(Vec2_ Pos, FLOAT_ Angle)
    {
        Vec2_ drop = vec2angle({CollisionR, 0}, Angle);
        return PsBresenhamDetection(Pos - drop, Pos + drop);
    }

    CollisionInfoI PhysicsShape::PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        return BresenhamDetection(start, end);
    }

    CollisionInfoD PhysicsShape::PsBresenhamDetection(Vec2_ start, Vec2_ end)
    {
        // 偏移中心位置，对其网格坐标系
        start -= pos;
        end -= pos;

        AngleMat Mat(angle);
        start = Mat.Anticlockwise(start);
        end = Mat.Anticlockwise(end);

        // 裁剪线段 让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start + CentreMass, end + CentreMass, width, height);
        if (data.Focus)
        {
            // 线段碰撞检测
            CollisionInfoD info = BresenhamDetection(data.start, data.end);
            if (info.Collision)
            {
                // 返回物理坐标系
                info.pos = Mat.Rotary(info.pos - CentreMass) + pos;
                return info;
            }
        }
        return {false};
    }

}