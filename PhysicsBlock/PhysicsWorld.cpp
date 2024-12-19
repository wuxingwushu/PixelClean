#include "PhysicsWorld.hpp"
#include <algorithm>
#include "BaseCalculate.hpp"
#include <map>
#include <iostream>

namespace PhysicsBlock
{

    PhysicsWorld::PhysicsWorld(glm::dvec2 gravityAcceleration, const bool wind) : GravityAcceleration(gravityAcceleration), WindBool(wind)
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
    }

    void PhysicsWorld::PhysicsProcess(PhysicsShape *a, PhysicsShape *b)
    {
        glm::dvec2 ForceCollisionDrop{0, 0}; // 累加碰撞点的位置
        unsigned int DropSize = 0;           // 碰撞点的数量
        glm::dvec2 OutlineDrop;              // 某个骨骼点的坐标
        CollisionInfoI info;                 // 碰撞信息
        for (size_t k = 0; k < b->OutlineSize; ++k)
        {
            OutlineDrop = vec2angle(b->OutlineSet[k] - b->CentreMass, b->angle) + b->pos;
            info = a->DropCollision(OutlineDrop);
            if (info.Collision)
            {
                ForceCollisionDrop += OutlineDrop;
                ++DropSize;
            }
        }
        for (size_t k = 0; k < a->OutlineSize; ++k)
        {
            OutlineDrop = vec2angle(a->OutlineSet[k] - a->CentreMass, a->angle) + a->pos;
            info = b->DropCollision(OutlineDrop);
            if (info.Collision)
            {
                ForceCollisionDrop += OutlineDrop;
                ++DropSize;
            }
        }
        if (DropSize > 0)
        {
            ForceCollisionDrop /= DropSize;
            a->AddForce(ForceCollisionDrop, b->force);
        }
    }

    void PhysicsWorld::PhysicsProcess(PhysicsParticle *a, PhysicsShape *b)
    {
        if (b->DropCollision(a->pos).Collision)
        {
            a->AddForce(b->force);
        }
    }

    void PhysicsWorld::PhysicsProcess(PhysicsShape *a, PhysicsParticle *b)
    {
        if (a->DropCollision(b->pos).Collision)
        {
            a->AddForce(b->pos, b->force);
        }
    }

#define EnergyConservationOptimum 1
    void PhysicsWorld::EnergyConservation(PhysicsParticle *a, PhysicsParticle *b)
    {
        glm::dvec2 Distance = a->pos - b->pos;
        double DistanceAngle = EdgeVecToCosAngleFloat(Distance);         // 获得相撞的角度
        double Angle = DistanceAngle - EdgeVecToCosAngleFloat(a->speed); // A相差角度
        double Long = Modulus(a->speed);                                 // A速度大小
        double Va = Long * cos(Angle);                                   // A对象撞向B的速度
        a->speed.y = Long * sin(Angle);                                  // 剩下的速度不参与碰撞（摩擦？）
        Angle = DistanceAngle - EdgeVecToCosAngleFloat(b->speed);        // B相差角度
        Long = Modulus(b->speed);                                        // B速度大小
        double Vb = Long * cos(Angle);                                   // B对象撞向A的速度
        b->speed.y = Long * sin(Angle);                                  // 剩下的速度不参与碰撞（摩擦？）
#if EnergyConservationOptimum == 0
        double MomentumA = a->mass * Va;
        double MomentumB = b->mass * Vb;
        double KineticEnergy = MomentumA * Va + MomentumB * Vb; // 动能
        double Momentum = MomentumA + MomentumB;                // 动量

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        double A = a->mass + (pow((-a->mass), 2) / b->mass);
        double B = ((-a->mass) * Momentum * (-2)) / b->mass;
        double C = (-KineticEnergy) + (pow(Momentum, 2) / b->mass);

        double discriminant = B * B - (4 * A * C);
        if (discriminant >= 0)
        {
            discriminant = sqrt(discriminant);
            a->speed.x = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // B速度前 + B速度后    将两个结果相加，方程将会变成这样
            a->speed.x = -(Va + a->speed.x);                                    // 其中一个结果是Va之前的速度,（不知道为什么要一个负号， 理论上应该是 减Va 为什么变成 加Va, 结果还是相反的）
            // 计算另外一个值
            b->speed.x = (Momentum - (a->speed.x * a->mass)) / b->mass;
            Distance = AngleFloatToAngleVec(DistanceAngle);
            a->speed = vec2angle(a->speed, Distance);
            b->speed = vec2angle(b->speed, Distance);
        }
        else
        {
            // 复数解
            std::cout << "[PhysicsWorld]EnergyConservation: Error" << std::endl;
        }
#else
        double Momentum = a->mass * Va + b->mass * Vb;      // 动量
        a->speed.x = (Momentum * -2) / (b->mass + a->mass); // B速度前 + B速度后    将两个结果相加，方程将会变成这样
        a->speed.x = -(Va + a->speed.x);                    // 其中一个结果是Va之前的速度,（不知道为什么要一个负号， 理论上应该是 减Va。 为什么变成 加Va, 结果还是相反的）
        // 计算另外一个值
        b->speed.x = (Momentum - (a->speed.x * a->mass)) / b->mass;
        Distance = AngleFloatToAngleVec(DistanceAngle);
        a->speed = vec2angle(a->speed, Distance);
        b->speed = vec2angle(b->speed, Distance);
#endif
    }

    void PhysicsWorld::EnergyConservation(PhysicsShape *a, PhysicsShape *b, glm::dvec2 CollisionDrop, double Vertical)
    {
        glm::dvec2 SpeedD = b->speed - a->speed;                      // 速度差
        glm::dvec2 PosD = a->pos - CollisionDrop;                     // 碰撞点指向A物体的距离
        double SpeedAngle = EdgeVecToCosAngleFloat(SpeedD);           // 速度的角度
        double SpeedA = Modulus(SpeedD) * cos(SpeedAngle - Vertical); // 垂直法向量的速度
        double CBAngle = EdgeVecToCosAngleFloat(PosD);                // 碰撞点 指向 A 的角度
        /*************移动*************/
        double SpeedVal = SpeedA * cos(CBAngle - Vertical); // 被移动的速度
        double Momentum = SpeedVal * b->mass;               // 动量
        double KineticEnergy = Momentum * b->mass;          // 动能

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        double A = b->mass + (pow(b->mass, 2) / a->mass);
        double B = (Momentum * (-b->mass) * -2) / a->mass;
        double C = (-KineticEnergy) + (pow(Momentum, 2) / a->mass);

        double discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        double Bs = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // B速度前 + B速度后
        Bs = -(SpeedVal + Bs);                                             // B物体的速度 变化量（被改变，大小，没有调整方向）
        double As = (Momentum - (Bs * b->mass)) / a->mass;                 // A物体的速度 变化量（被改变，大小，没有调整方向）

        a->speed += vec2angle({As, 0}, CBAngle);
        b->speed += vec2angle({Bs - SpeedVal, 0}, CBAngle);


        double NL = b->mass * pow(Modulus(b->speed), 2);

        
        /*************转动*************/
        double AngleVal = SpeedA * sin(CBAngle - Vertical);
        b->speed += vec2angle({AngleVal, 0}, EdgeVecToCosAngleFloat(b->speed));
        //b->speed += vec2angle({sqrt((Bs - SpeedVal) * (Bs - SpeedVal) + AngleVal * AngleVal), 0}, CBAngle);// 将这速度转换为角速度
        double HornKineticEnergy = AngleVal * AngleVal * b->mass;       // 角动能 = 动能 （能量守恒定律）
        double AngleSpeed = sqrt(HornKineticEnergy / b->MomentInertia); // B角速度
        double HornMomentum = AngleSpeed * b->MomentInertia;            // 角动量

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        A = b->MomentInertia + (pow(b->MomentInertia, 2) / a->MomentInertia);
        B = (HornMomentum * (-b->MomentInertia) * -2) / a->MomentInertia;
        C = (-HornKineticEnergy) + (pow(HornMomentum, 2) / a->MomentInertia);

        discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        double Bas = ((-B + discriminant) + (-B - discriminant)) / (2 * A);        // B角速度前 + B角速度后
        Bas = -(AngleSpeed + Bas);                                                 // B物体的角速度 变化量
        double Aas = (HornMomentum - (Bas * b->MomentInertia)) / a->MomentInertia; // A物体的角速度 变化量

        a->angleSpeed -= Aas;
        b->angleSpeed += Bas;
    }

    void PhysicsWorld::PhysicsEmulator(double time)
    {
        PhysicsShape PhysicsShape1({0, 0}, {2, 2});
        for (size_t i = 0; i < (PhysicsShape1.width * PhysicsShape1.height); i++)
        {
            PhysicsShape1.at(i).Collision = true;
            PhysicsShape1.at(i).Entity = true;
            PhysicsShape1.at(i).mass = 1;
        }
        PhysicsShape1.UpdateInfo();
        PhysicsShape PhysicsShape2({1.5, 2}, {2, 2});
        for (size_t i = 0; i < (PhysicsShape2.width * PhysicsShape2.height); i++)
        {
            PhysicsShape2.at(i).Collision = true;
            PhysicsShape2.at(i).Entity = true;
            PhysicsShape2.at(i).mass = 1;
        }
        PhysicsShape2.UpdateInfo();
        PhysicsShape2.speed = {0, -1};
        double D = PhysicsShape1.mass * pow(Modulus(PhysicsShape1.speed), 2) + PhysicsShape2.mass * pow(Modulus(PhysicsShape2.speed), 2);
        D += PhysicsShape1.MomentInertia * pow(PhysicsShape1.angleSpeed, 2) + PhysicsShape2.MomentInertia * pow(PhysicsShape2.angleSpeed, 2);
        EnergyConservation(&PhysicsShape1, &PhysicsShape2, {0.75, 1}, (-3.1415926 / 2));
        D = 0;
        D = PhysicsShape1.mass * pow(Modulus(PhysicsShape1.speed), 2) + PhysicsShape2.mass * pow(Modulus(PhysicsShape2.speed), 2);
        D += PhysicsShape1.MomentInertia * pow(PhysicsShape1.angleSpeed, 2) + PhysicsShape2.MomentInertia * pow(PhysicsShape2.angleSpeed, 2);

        // 根据 Y 轴重力加速度 正负进行排序
        if (GravityAcceleration.y < 0)
        {
            std::sort(PhysicsFormworkS.begin(), PhysicsFormworkS.end(),
                      [](PhysicsFormwork *a, PhysicsFormwork *b)
                      {
                          return a->PFGetPos().y > b->PFGetPos().y;
                      });
        }
        else if (GravityAcceleration.y > 0)
        {
            std::sort(PhysicsFormworkS.begin(), PhysicsFormworkS.end(),
                      [](PhysicsFormwork *a, PhysicsFormwork *b)
                      {
                          return a->PFGetPos().y < b->PFGetPos().y;
                      });
        }

        if (PhysicsFormworkS.size() > 1)
        {
            PhysicsFormwork *JS; // 被解算受力的对象
            PhysicsFormwork *DX; // 和谁受力解算
            int j;               // for循环变量
            // 开始逐一解算 Y 轴受力
            for (size_t i = 1; i < PhysicsFormworkS.size(); ++i)
            {
                JS = PhysicsFormworkS[i]; // 获取被解算对象
                // 向上遍历对象
                for (j = i - 1; j >= 0; --j)
                {
                    DX = PhysicsFormworkS[j];
                    // 两个对象的距离差
                    double JL = Modulus(DX->PFGetPos() - JS->PFGetPos());
                    if (JL > PhysicsCollisionMaxRadius)
                    {
                        break; // 超过最大检测范围跳出
                    }
                    // 两个对象碰撞圆(外轮廓)相交
                    if (JL < (DX->PFGetCollisionR() + JS->PFGetCollisionR()))
                    {
                        if (DX->PFGetType() == JS->PFGetType())
                        {
                            if (DX->PFGetType() == PhysicsObjectEnum::shape)
                                PhysicsProcess((PhysicsShape *)JS, (PhysicsShape *)DX);
                        }
                        if (JS->PFGetType() == PhysicsObjectEnum::shape)
                        {
                            if (DX->PFGetType() == PhysicsObjectEnum::particle)
                                PhysicsProcess((PhysicsShape *)JS, (PhysicsParticle *)DX);
                        }
                        else if (JS->PFGetType() == PhysicsObjectEnum::particle)
                        {
                            if (DX->PFGetType() == PhysicsObjectEnum::shape)
                                PhysicsProcess((PhysicsParticle *)JS, (PhysicsShape *)DX);
                        }
                    }
                }
            }
        }
    }

    void PhysicsWorld::SetMapFormwork(MapFormwork *MapFormwork_)
    {
        wMapFormwork = MapFormwork_;
        if (!WindBool)
        {
            return;
        }
        GridWindSize = wMapFormwork->FMGetMapSize();
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
        GridWind = new glm::dvec2[GridWindSize.x * GridWindSize.y];
        for (size_t i = 0; i < (GridWindSize.x * GridWindSize.y); i++)
        {
            GridWind[i] = glm::dvec2{0};
        }
    }

    PhysicsFormwork *PhysicsWorld::Get(glm::dvec2 pos)
    {
        for (auto i : PhysicsFormworkS)
        {
            switch (i->PFGetType())
            {
            case PhysicsObjectEnum::shape:
                if ((Modulus(((PhysicsParticle *)i)->pos - pos) < i->PFGetCollisionR()) && ((PhysicsShape *)i)->DropCollision(pos).Collision)
                {
                    return i;
                }
                break;
            case PhysicsObjectEnum::particle:
                if (Modulus(((PhysicsParticle *)i)->pos - pos) < 0.25) // 点击位置距离点位置小于 0.25， 就判断选择 点
                {
                    return i;
                }
                break;

            default:
                break;
            }
        }
        return nullptr;
    }

}
