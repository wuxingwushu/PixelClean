#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock 
{
    #define EnergyConservationOptimum 1

    /**
     * @brief 动能守恒定律
     * @param a 物体A
     * @param b 物体B
     * @warning 理想碰撞，力都转换为速度，没有转换为角速度(可以理解为球体相撞) */
    void EnergyConservation(PhysicsParticle *a, PhysicsParticle *b)
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

    /**
     * @brief 形状碰撞动能守恒尝试
     * @param a 被撞物体A
     * @param b 物体B
     * @param CollisionDrop 碰撞点
     * @param Vertical 法向量角度(碰撞边的垂直法向量， 向内)
     * @warning 碰撞点在两质心线段上的，（动能 和 角动能 才守恒） */
    void EnergyConservation(PhysicsShape *a, PhysicsShape *b, glm::dvec2 CollisionDrop, double Vertical)
    {
        glm::dvec2 SpeedD = b->speed - a->speed;     // 速度差
        glm::dvec2 PosArmB = CollisionDrop - b->pos; // B物体 指向 碰撞点 的距离
        glm::dvec2 PosArmA = a->pos - CollisionDrop; // 碰撞点 指向 A物体 的距离

        DecompositionForce dfB = CalculateDecompositionForce(PosArmB, SpeedD);        // 速度差 到 碰撞点 的分速度
        DecompositionForce dfP = CalculateDecompositionForce(Vertical, dfB.Parallel); // 碰撞点 跟 法向量 的分速度
        DecompositionForce dfA = CalculateDecompositionForce(PosArmA, dfP.Parallel);  // 边垂直力 到 A物体 的分速度

#if EnergyConservationOptimum == 0
        /*------------ A ------------*/
        /*************移动*************/
        double SpeedVal = Modulus(dfA.Parallel);   // 被移动的速度
        double Momentum = SpeedVal * b->mass;      // 动量
        double KineticEnergy = Momentum * b->mass; // 动能

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        double A = b->mass + (pow(b->mass, 2) / a->mass);
        double B = (Momentum * (-b->mass) * -2) / a->mass;
        double C = (-KineticEnergy) + (pow(Momentum, 2) / a->mass);

        double discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        double Bs = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // B速度前 + B速度后
        Bs = -(SpeedVal + Bs);                                             // B物体的速度 变化量（被改变，大小，没有调整方向）
        double As = (Momentum - (Bs * b->mass)) / a->mass;                 // A物体的速度 变化量（被改变，大小，没有调整方向）
        a->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfA.Parallel));

        /*************转动*************/
        double AngleVal = Modulus(dfA.Vartical);                        // 被转动的速度
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
        a->angleSpeed += Aas;

        /*------------ B ------------*/
        /*************移动*************/
        dfB = CalculateDecompositionForce(-PosArmB, -dfP.Parallel);
        SpeedVal = Modulus(dfB.Parallel);   // 被移动的速度
        Momentum = SpeedVal * a->mass;      // 动量
        KineticEnergy = Momentum * a->mass; // 动能

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        A = a->mass + (pow(a->mass, 2) / b->mass);
        B = (Momentum * (-a->mass) * -2) / b->mass;
        C = (-KineticEnergy) + (pow(Momentum, 2) / b->mass);

        discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        Bs = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // B速度前 + B速度后
        Bs = -(SpeedVal + Bs);                                      // B物体的速度 变化量（被改变，大小，没有调整方向）
        As = (Momentum - (Bs * a->mass)) / b->mass;                 // A物体的速度 变化量（被改变，大小，没有调整方向）
        b->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfB.Parallel));

        /*************转动*************/
        AngleVal = Modulus(dfB.Vartical);                        // 被转动的速度
        HornKineticEnergy = AngleVal * AngleVal * a->mass;       // 角动能 = 动能 （能量守恒定律）
        AngleSpeed = sqrt(HornKineticEnergy / a->MomentInertia); // B角速度
        HornMomentum = AngleSpeed * a->MomentInertia;            // 角动量

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        A = a->MomentInertia + (pow(a->MomentInertia, 2) / b->MomentInertia);
        B = (HornMomentum * (-a->MomentInertia) * -2) / b->MomentInertia;
        C = (-HornKineticEnergy) + (pow(HornMomentum, 2) / b->MomentInertia);

        discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        Bas = ((-B + discriminant) + (-B - discriminant)) / (2 * A);        // B角速度前 + B角速度后
        Bas = -(AngleSpeed + Bas);                                          // B物体的角速度 变化量
        Aas = (HornMomentum - (Bas * a->MomentInertia)) / b->MomentInertia; // A物体的角速度 变化量
        b->angleSpeed += Aas;
#else
        /*------------ A ------------*/
        /*************移动*************/
        double SpeedVal = Modulus(dfA.Parallel); // 被移动的速度

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        double A = (b->mass + a->mass);
        double B = (SpeedVal * b->mass * 2);
        double Bs = (-B) / A;                              // B速度前 + B速度后
        Bs = -(SpeedVal + Bs);                             // B物体的速度 变化量（被改变，大小，没有调整方向）
        double As = ((SpeedVal - Bs) * b->mass) / a->mass; // A物体的速度 变化量（被改变，大小，没有调整方向）
        a->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfA.Parallel));

        /*************转动*************/
        double AngleVal = Modulus(dfA.Vartical);                        // 被转动的速度
        double HornKineticEnergy = AngleVal * AngleVal * b->mass;       // 角动能 = 动能 （能量守恒定律）
        double AngleSpeed = sqrt(HornKineticEnergy / b->MomentInertia); // B角速度
        double HornMomentum = AngleSpeed * b->MomentInertia;            // 角动量

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        A = b->MomentInertia + a->MomentInertia;
        B = HornMomentum * 2;
        double Bas = (-B) / A;                                                     // B角速度前 + B角速度后
        Bas = -(AngleSpeed + Bas);                                                 // B物体的角速度 变化量
        double Aas = (HornMomentum - (Bas * b->MomentInertia)) / a->MomentInertia; // A物体的角速度 变化量
        a->angleSpeed += Aas;

        /*------------ B ------------*/
        /*************移动*************/
        dfB = CalculateDecompositionForce(-PosArmB, -dfP.Parallel);
        SpeedVal = Modulus(dfB.Parallel); // 被移动的速度

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        A = (a->mass + b->mass);
        B = (SpeedVal * a->mass * 2);
        Bs = (-B) / A;                              // B速度前 + B速度后
        Bs = -(SpeedVal + Bs);                      // B物体的速度 变化量（被改变，大小，没有调整方向）
        As = ((SpeedVal - Bs) * a->mass) / b->mass; // A物体的速度 变化量（被改变，大小，没有调整方向）
        b->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfB.Parallel));

        /*************转动*************/
        AngleVal = Modulus(dfB.Vartical);                        // 被转动的速度
        HornKineticEnergy = AngleVal * AngleVal * a->mass;       // 角动能 = 动能 （能量守恒定律）
        AngleSpeed = sqrt(HornKineticEnergy / a->MomentInertia); // B角速度
        HornMomentum = AngleSpeed * a->MomentInertia;            // 角动量

        // 一元二次方程 的 ABC，：0 = AX^2 + BX + C
        A = b->MomentInertia + a->MomentInertia;
        B = HornMomentum * 2;
        Bas = (-B) / A;                                                     // B角速度前 + B角速度后
        Bas = -(AngleSpeed + Bas);                                          // B物体的角速度 变化量
        Aas = (HornMomentum - (Bas * a->MomentInertia)) / b->MomentInertia; // A物体的角速度 变化量
        b->angleSpeed += Aas;
#endif
    }
}