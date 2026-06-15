#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock 
{
    // 动能守恒算法版本选择
    // 0：原始版本（展开一元二次方程系数并求解判别式）
    // 1：优化版本（利用 -B/A 简化公式，约去质量/转动惯量平方项，减少计算量）
    #define EnergyConservationOptimum 1

    /**
     * @brief 粒子间动能守恒碰撞（质点-质点理想弹性碰撞）
     * @details 将碰撞问题简化为一维对心弹性碰撞：先将两粒子的速度分解到两粒子质心连线方向（碰撞法线方向），
     *          在该方向上应用动量和动能守恒定律，求解两粒子的碰撞后法向速度，再与各自切向速度（不参与碰撞）
     *          合成最终速度。该算法假定碰撞为完全弹性碰撞（恢复系数 e=1），且不产生角速度。
     * @param a 粒子A（碰撞双方之一）
     * @param b 粒子B（碰撞双方之一）
     * @warning 理想碰撞，只考虑平动速度变化，不产生角速度（可理解为光滑球体相撞） */
    void EnergyConservation(PhysicsParticle *a, PhysicsParticle *b)
    {
        Vec2_ Distance = a->pos - b->pos;
        FLOAT_ DistanceAngle = EdgeVecToCosAngleFloat(Distance);         // 碰撞方向角（从B指向A的连线方向）
        FLOAT_ Angle = DistanceAngle - EdgeVecToCosAngleFloat(a->speed); // A速度方向与碰撞方向的夹角
        FLOAT_ Long = Modulus(a->speed);                                 // A速度的模长
        FLOAT_ Va = Long * cos(Angle);                                   // A沿碰撞方向的速度分量（法向速度）
        a->speed.y = Long * sin(Angle);                                  // A垂直于碰撞方向的速度分量（切向速度，不参与对心碰撞）
        Angle = DistanceAngle - EdgeVecToCosAngleFloat(b->speed);        // B速度方向与碰撞方向的夹角
        Long = Modulus(b->speed);                                        // B速度的模长
        FLOAT_ Vb = Long * cos(Angle);                                   // B沿碰撞方向的速度分量（法向速度）
        b->speed.y = Long * sin(Angle);                                  // B垂直于碰撞方向的速度分量（切向速度，不参与对心碰撞）
#if EnergyConservationOptimum == 0
        FLOAT_ MomentumA = a->mass * Va;
        FLOAT_ MomentumB = b->mass * Vb;
        FLOAT_ KineticEnergy = MomentumA * Va + MomentumB * Vb; // 碰撞前系统总动能（沿碰撞方向）
        FLOAT_ Momentum = MomentumA + MomentumB;                // 碰撞前系统总动量（沿碰撞方向）

        // 联立动量守恒与动能守恒方程，消元后得到关于 A 碰撞后法向速度 vA' 的一元二次方程：A·vA'² + B·vA' + C = 0
        FLOAT_ A = a->mass + (pow((-a->mass), 2) / b->mass);
        FLOAT_ B = ((-a->mass) * Momentum * (-2)) / b->mass;
        FLOAT_ C = (-KineticEnergy) + (pow(Momentum, 2) / b->mass);

        FLOAT_ discriminant = B * B - (4 * A * C);
        if (discriminant >= 0)
        {
            discriminant = sqrt(discriminant);
            a->speed.x = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // 二次方程两解之和 = -B/A，即 vA_before + vA_after
            a->speed.x = -(Va + a->speed.x);                                    // 从两解之和中消去碰撞前速度 Va，得到碰撞后 A 的法向速度 vA_after
            // 根据动量守恒反推 B 碰撞后的法向速度
            b->speed.x = (Momentum - (a->speed.x * a->mass)) / b->mass;
            Distance = AngleFloatToAngleVec(DistanceAngle);
            a->speed = vec2angle(a->speed, Distance); // 将碰撞后速度旋转回世界坐标系（原先是一维标量，现恢复为二维向量）
            b->speed = vec2angle(b->speed, Distance);
        }
        else
        {
            // 判别式小于零，无实数解（数值误差导致的不可能碰撞状态）
            std::cout << "[PhysicsWorld]EnergyConservation: Error" << std::endl;
        }
#else
        FLOAT_ Momentum = a->mass * Va + b->mass * Vb;      // 碰撞前系统总动量（沿碰撞方向）
        // 优化：利用完全弹性碰撞的封闭解，跳过一元二次方程展开，直接使用简化公式 -B/A
        a->speed.x = (Momentum * -2) / (b->mass + a->mass); // 两解之和 -B/A = vA_before + vA_after（化简后消去了质量平方项）
        a->speed.x = -(Va + a->speed.x);                    // 从两解之和中消去碰撞前速度 Va，得到碰撞后 A 的法向速度 vA_after
        // 根据动量守恒反推 B 碰撞后的法向速度
        b->speed.x = (Momentum - (a->speed.x * a->mass)) / b->mass;
        Distance = AngleFloatToAngleVec(DistanceAngle);
        a->speed = vec2angle(a->speed, Distance);
        b->speed = vec2angle(b->speed, Distance);
#endif
    }

    /**
     * @brief 形状间动能守恒碰撞（刚体-刚体弹性碰撞，含角速度）
     * @details 根据碰撞点位置和碰撞法向量，将两物体的相对速度分解为碰撞点的法向分量和切向分量。
     *          法向分量产生平动速度变化（法向冲量），切向分量产生角速度变化（摩擦力/切向冲量）。
     *          分别在平动和转动两个维度上，应用动量和动能守恒定律联立求解。
     *          碰撞过程分两步处理：先计算物体 A 的速度/角速度变化，再根据反作用力计算物体 B 的变化。
     * @param a 物体A（刚体，具备质量和转动惯量）
     * @param b 物体B（刚体，具备质量和转动惯量）
     * @param CollisionDrop 碰撞点在世界坐标系中的位置
     * @param Vertical 碰撞面法向量角度（指向物体A内部，即碰撞边的向内垂直法向量）
     * @warning 假定碰撞点位于两物体质心连线上，此时动能和角动能才同时守恒 */
    void EnergyConservation(PhysicsShape *a, PhysicsShape *b, Vec2_ CollisionDrop, FLOAT_ Vertical)
    {
        Vec2_ SpeedD = b->speed - a->speed;     // B相对于A的速度差（相对速度）
        Vec2_ PosArmB = CollisionDrop - b->pos; // B质心指向碰撞点的力臂向量
        Vec2_ PosArmA = a->pos - CollisionDrop; // 碰撞点指向A质心的力臂向量

        DecompositionForce dfB = CalculateDecompositionForce(PosArmB, SpeedD);        // 将速度差分解到B力臂方向的平行和垂直分量
        DecompositionForce dfP = CalculateDecompositionForce(Vertical, dfB.Parallel); // 将B力臂方向的速度分量再分解到碰撞法向量方向
        DecompositionForce dfA = CalculateDecompositionForce(PosArmA, dfP.Parallel);  // 将法向量方向的速度分量分解到A力臂方向的平行和垂直分量

#if EnergyConservationOptimum == 0
        /*------------ A ------------*/
        /*************移动*************/
        FLOAT_ SpeedVal = Modulus(dfA.Parallel);   // A力臂方向的法向碰撞速度大小
        FLOAT_ Momentum = SpeedVal * b->mass;      // 碰撞前系统法向动量（以B质量为参考）
        FLOAT_ KineticEnergy = Momentum * b->mass; // 碰撞前系统法向动能（以B质量为参考）

        // 联立动量守恒与动能守恒方程，消元后得到关于 B 碰撞后法向速度的一元二次方程：A·vB'² + B·vB' + C = 0
        FLOAT_ A = b->mass + (pow(b->mass, 2) / a->mass);
        FLOAT_ B = (Momentum * (-b->mass) * -2) / a->mass;
        FLOAT_ C = (-KineticEnergy) + (pow(Momentum, 2) / a->mass);

        FLOAT_ discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        FLOAT_ Bs = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // 二次方程两解之和 = -B/A，即 vB_before + vB_after
        Bs = -(SpeedVal + Bs);                                             // B物体法向速度变化量（消去碰撞前速度分量）
        FLOAT_ As = (Momentum - (Bs * b->mass)) / a->mass;                 // A物体法向速度变化量（由动量守恒推导）
        a->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfA.Parallel));

        /*************转动*************/
        FLOAT_ AngleVal = Modulus(dfA.Vartical);                        // A力臂方向的切向碰撞速度大小
        FLOAT_ HornKineticEnergy = AngleVal * AngleVal * b->mass;       // 切向动能（用于角速度的能量，以B质量为参考）
        FLOAT_ AngleSpeed = sqrt(HornKineticEnergy / b->MomentInertia); // 碰撞前等效角速度（以B转动惯量为参考）
        FLOAT_ HornMomentum = AngleSpeed * b->MomentInertia;            // 碰撞前等效角动量（以B转动惯量为参考）

        // 联立角动量守恒与转动动能守恒方程，得到关于 B 碰撞后角速度的一元二次方程：A·ωB'² + B·ωB' + C = 0
        A = b->MomentInertia + (pow(b->MomentInertia, 2) / a->MomentInertia);
        B = (HornMomentum * (-b->MomentInertia) * -2) / a->MomentInertia;
        C = (-HornKineticEnergy) + (pow(HornMomentum, 2) / a->MomentInertia);

        discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        FLOAT_ Bas = ((-B + discriminant) + (-B - discriminant)) / (2 * A);        // 二次方程两解之和 = -B/A，即 ωB_before + ωB_after
        Bas = -(AngleSpeed + Bas);                                                 // B物体角速度变化量（消去碰撞前角速度分量）
        FLOAT_ Aas = (HornMomentum - (Bas * b->MomentInertia)) / a->MomentInertia; // A物体角速度变化量（由角动量守恒推导）
        a->angleSpeed += Aas;

        /*------------ B ------------*/
        /*************移动*************/
        dfB = CalculateDecompositionForce(-PosArmB, -dfP.Parallel);
        SpeedVal = Modulus(dfB.Parallel);   // B力臂方向的法向碰撞速度大小（反作用力方向）
        Momentum = SpeedVal * a->mass;      // 碰撞前系统法向动量（以A质量为参考）
        KineticEnergy = Momentum * a->mass; // 碰撞前系统法向动能（以A质量为参考）

        // 联立动量守恒与动能守恒方程，消元后得到关于 A 碰撞后法向速度的一元二次方程
        A = a->mass + (pow(a->mass, 2) / b->mass);
        B = (Momentum * (-a->mass) * -2) / b->mass;
        C = (-KineticEnergy) + (pow(Momentum, 2) / b->mass);

        discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        Bs = ((-B + discriminant) + (-B - discriminant)) / (2 * A); // 二次方程两解之和 = -B/A，即 vA_before + vA_after
        Bs = -(SpeedVal + Bs);                                      // A物体法向速度变化量（消去碰撞前速度分量）
        As = (Momentum - (Bs * a->mass)) / b->mass;                 // B物体法向速度变化量（由动量守恒推导）
        b->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfB.Parallel));

        /*************转动*************/
        AngleVal = Modulus(dfB.Vartical);                        // B力臂方向的切向碰撞速度大小（反作用力方向）
        HornKineticEnergy = AngleVal * AngleVal * a->mass;       // 切向动能（用于角速度的能量，以A质量为参考）
        AngleSpeed = sqrt(HornKineticEnergy / a->MomentInertia); // 碰撞前等效角速度（以A转动惯量为参考）
        HornMomentum = AngleSpeed * a->MomentInertia;            // 碰撞前等效角动量（以A转动惯量为参考）

        // 联立角动量守恒与转动动能守恒方程，得到关于 A 碰撞后角速度的一元二次方程
        A = a->MomentInertia + (pow(a->MomentInertia, 2) / b->MomentInertia);
        B = (HornMomentum * (-a->MomentInertia) * -2) / b->MomentInertia;
        C = (-HornKineticEnergy) + (pow(HornMomentum, 2) / b->MomentInertia);

        discriminant = B * B - (4 * A * C);
        discriminant = sqrt(discriminant);
        Bas = ((-B + discriminant) + (-B - discriminant)) / (2 * A);        // 二次方程两解之和 = -B/A，即 ωA_before + ωA_after
        Bas = -(AngleSpeed + Bas);                                          // A物体角速度变化量（消去碰撞前角速度分量）
        Aas = (HornMomentum - (Bas * a->MomentInertia)) / b->MomentInertia; // B物体角速度变化量（由角动量守恒推导）
        b->angleSpeed += Aas;
#else
        /*------------ A ------------*/
        /*************移动*************/
        FLOAT_ SpeedVal = Modulus(dfA.Parallel); // A力臂方向的法向碰撞速度大小

        // 优化：一元二次方程系数中的质量平方项可在 -B/A 中约去，直接使用简化形式
        FLOAT_ A = (b->mass + a->mass);
        FLOAT_ B = (SpeedVal * b->mass * 2);
        FLOAT_ Bs = (-B) / A;                              // 两解之和 -B/A = vB_before + vB_after（简化后）
        Bs = -(SpeedVal + Bs);                             // B物体法向速度变化量（消去碰撞前速度分量）
        FLOAT_ As = ((SpeedVal - Bs) * b->mass) / a->mass; // A物体法向速度变化量（由动量守恒推导）
        a->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfA.Parallel));

        /*************转动*************/
        FLOAT_ AngleVal = Modulus(dfA.Vartical);                        // A力臂方向的切向碰撞速度大小
        FLOAT_ HornKineticEnergy = AngleVal * AngleVal * b->mass;       // 切向动能（用于角速度的能量，以B质量为参考）
        FLOAT_ AngleSpeed = sqrt(HornKineticEnergy / b->MomentInertia); // 碰撞前等效角速度（以B转动惯量为参考）
        FLOAT_ HornMomentum = AngleSpeed * b->MomentInertia;            // 碰撞前等效角动量（以B转动惯量为参考）

        // 优化：转动惯量平方项在 -B/A 中可约去，HornMomentum*2 为简化后的 B 系数
        A = b->MomentInertia + a->MomentInertia;
        B = HornMomentum * 2;
        FLOAT_ Bas = (-B) / A;                                                     // 两解之和 -B/A = ωB_before + ωB_after（简化后）
        Bas = -(AngleSpeed + Bas);                                                 // B物体角速度变化量（消去碰撞前角速度分量）
        FLOAT_ Aas = (HornMomentum - (Bas * b->MomentInertia)) / a->MomentInertia; // A物体角速度变化量（由角动量守恒推导）
        a->angleSpeed += Aas;

        /*------------ B ------------*/
        /*************移动*************/
        dfB = CalculateDecompositionForce(-PosArmB, -dfP.Parallel);
        SpeedVal = Modulus(dfB.Parallel); // B力臂方向的法向碰撞速度大小（反作用力方向）

        // 优化：质量平方项在 -B/A 中可约去，直接使用简化系数
        A = (a->mass + b->mass);
        B = (SpeedVal * a->mass * 2);
        Bs = (-B) / A;                              // 两解之和 -B/A = vA_before + vA_after（简化后）
        Bs = -(SpeedVal + Bs);                      // A物体法向速度变化量（消去碰撞前速度分量）
        As = ((SpeedVal - Bs) * a->mass) / b->mass; // B物体法向速度变化量（由动量守恒推导）
        b->speed += vec2angle({As, 0}, EdgeVecToCosAngleFloat(dfB.Parallel));

        /*************转动*************/
        AngleVal = Modulus(dfB.Vartical);                        // B力臂方向的切向碰撞速度大小（反作用力方向）
        HornKineticEnergy = AngleVal * AngleVal * a->mass;       // 切向动能（用于角速度的能量，以A质量为参考）
        AngleSpeed = sqrt(HornKineticEnergy / a->MomentInertia); // 碰撞前等效角速度（以A转动惯量为参考）
        HornMomentum = AngleSpeed * a->MomentInertia;            // 碰撞前等效角动量（以A转动惯量为参考）

        // 优化：转动惯量平方项在 -B/A 中可约去
        A = b->MomentInertia + a->MomentInertia;
        B = HornMomentum * 2;
        Bas = (-B) / A;                                                     // 两解之和 -B/A = ωA_before + ωA_after（简化后）
        Bas = -(AngleSpeed + Bas);                                          // A物体角速度变化量（消去碰撞前角速度分量）
        Aas = (HornMomentum - (Bas * a->MomentInertia)) / b->MomentInertia; // B物体角速度变化量（由角动量守恒推导）
        b->angleSpeed += Aas;
#endif
    }
}