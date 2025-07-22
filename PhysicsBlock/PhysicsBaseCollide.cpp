#include "PhysicsBaseCollide.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <cmath>

namespace PhysicsBlock
{

    unsigned int Collide(Contact *contacts, PhysicsShape *ShapeA, PhysicsShape *ShapeB)
    {
        int ContactSize = 0;
        glm::dvec2 Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < ShapeA->OutlineSize; ++i)
        {
            Drop = ShapeA->OutlineSet[i];
            Drop = vec2angle(Drop, ShapeA->angle);
            DropPos = ShapeA->pos + Drop;
            CollisionInfoD info = ShapeB->PsBresenhamDetection(ShapeA->OldPos, DropPos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - DropPos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - DropPos.x;
                }
                contacts->separation = -abs(contacts->separation);                                    // 碰撞距离差
                contacts->position = info.pos;                                                        // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeB->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                       // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        for (size_t i = 0; i < ShapeB->OutlineSize; ++i)
        {
            Drop = ShapeB->OutlineSet[i];
            Drop = vec2angle(Drop, ShapeB->angle);
            DropPos = ShapeB->pos + Drop;
            CollisionInfoD info = ShapeA->PsBresenhamDetection(ShapeB->OldPos, DropPos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - DropPos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - DropPos.x;
                }
                contacts->separation = -abs(contacts->separation);                                    // 碰撞距离差
                contacts->position = info.pos;                                                        // 碰撞点的位置
                contacts->normal = -vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeA->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                       // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        return ContactSize;
    }

    unsigned int Collide(Contact *contacts, PhysicsShape *Shape, PhysicsParticle *Particle)
    {
        if (Shape->DropCollision(Particle->pos).Collision)
        {
            CollisionInfoD info;
            // 如果 当前位置 和 旧位置 是一样的话 会导致无法碰撞的问题
            info = Shape->PsBresenhamDetection(Particle->pos - (Shape->pos - Particle->pos), Particle->pos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - Particle->pos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - Particle->pos.x;
                }
                contacts->separation = -abs(contacts->separation);                                    // 碰撞距离差
                contacts->position = info.pos;                                                        // 碰撞点的位置
                contacts->normal = -vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + Shape->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = 0;                                                                 // 边索引 ID
                // 一直馅在碰撞体，无法更新旧位置（旧位置不可以在碰撞体内）
                // B->OldPos = info.pos + (contacts->normal * 0.1);
                return 1; // 有碰撞返回碰撞位置
            }
        }
        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsShape *Shape, MapFormwork *Map)
    {
        int ContactSize = 0;
        glm::dvec2 Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < Shape->OutlineSize; ++i)
        {
            Drop = Shape->OutlineSet[i];
            Drop = vec2angle(Drop, Shape->angle);
            DropPos = Shape->pos + Drop;
            CollisionInfoD info = Map->FMBresenhamDetection(Shape->OldPos, DropPos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - DropPos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - DropPos.x;
                }
                contacts->separation = -abs(contacts->separation);                  // 碰撞距离差
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        /**************************************/ // 临时处理方法（待日后优化）
        // 计算附近地形的碰撞点
        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 1;
        std::vector<glm::dvec2> Outline = LMapStatic->GetLightweightOutline(ToInt(Shape->pos.x - Shape->CollisionR) - KD, ToInt(Shape->pos.y - Shape->CollisionR) - KD, ToInt(Shape->pos.x + Shape->CollisionR) + KD, ToInt(Shape->pos.y + Shape->CollisionR) + KD);
        for (size_t i = 0; i < Outline.size(); ++i)
        {
            Drop = Outline[i] - LMapStatic->centrality;
            if (!Shape->DropCollision(Drop).Collision)
            {
                continue;
            }
            CollisionInfoD info = Shape->PsBresenhamDetection(Drop - (Shape->pos - Drop), Drop);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - Drop.y;
                }
                else
                {
                    contacts->separation = info.pos.x - Drop.x;
                }
                contacts->separation = -abs(contacts->separation);                                  // 碰撞距离差
                contacts->position = info.pos;                                                      // 碰撞点的位置
                contacts->normal = -vec2angle({-1, 0}, info.Direction * (M_PI / 2) + Shape->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

    unsigned int Collide(Contact *contacts, PhysicsParticle *Particle, MapFormwork *Map)
    {
        if (Map->FMGetCollide(Particle->pos))
        {
            CollisionInfoD info = Map->FMBresenhamDetection(Particle->OldPos, Particle->pos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - Particle->pos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - Particle->pos.x;
                }
                contacts->separation = -abs(contacts->separation);                  // 碰撞距离差
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = 0;                                               // 边索引 ID
                // 一直馅在碰撞体，无法更新旧位置（旧位置不可以在碰撞体内）
                Particle->OldPos = info.pos - (contacts->normal * 0.1);
                return 1;
            }
        }
        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, MapFormwork *Map)
    {
        int ContactSize = 0;

        std::vector<glm::dvec2> Cd{
            Circle->pos + glm::dvec2{Circle->radius, 0},
            Circle->pos + glm::dvec2{0, Circle->radius},
            Circle->pos + glm::dvec2{-Circle->radius, 0},
            Circle->pos + glm::dvec2{0, -Circle->radius}};

        for (auto d : Cd)
        {
            CollisionInfoD info = Map->FMBresenhamDetection(Circle->OldPos, d);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - d.y;
                }
                else
                {
                    contacts->separation = info.pos.x - d.x;
                }
                contacts->separation = -abs(contacts->separation);                  // 碰撞距离差
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 2;
        std::vector<glm::dvec2> Outline = LMapStatic->GetLightweightOutline(ToInt(Circle->pos.x - Circle->radius) - KD, ToInt(Circle->pos.y - Circle->radius) - KD, ToInt(Circle->pos.x + Circle->radius) + KD, ToInt(Circle->pos.y + Circle->radius) + KD);
        double L;
        double angle;
        for (auto d : Outline)
        {
            d -= LMapStatic->centrality;
            L = Modulus(d - Circle->pos);
            if (Circle->radius > L)
            {
                contacts->separation = L - Circle->radius;                            // 碰撞距离
                angle = EdgeVecToCosAngleFloat(d - Circle->pos);                      // 计算出角度
                contacts->normal = vec2angle({1, 0}, angle);                          // （反向作用力法向量）地形不会旋转
                contacts->position = contacts->normal * Circle->radius + Circle->pos; // 碰撞点的位置
                contacts->w_side = ContactSize;                                       // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        return ContactSize;
    }

    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, PhysicsParticle *Particle)
    {
        glm::dvec2 dp = Particle->pos - Circle->pos;
        double L = Modulus(dp);
        if (Circle->radius > L)
        {
            contacts->separation = L - Circle->radius;                            // 碰撞距离
            double angle = EdgeVecToCosAngleFloat(dp);                            // 计算出角度
            contacts->normal = vec2angle({1, 0}, angle);                          // （反向作用力法向量）地形不会旋转
            contacts->position = contacts->normal * Circle->radius + Circle->pos; // 碰撞点的位置
            contacts->w_side = 0;                                                 // 边索引 ID
            return 1;
        }
        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsCircle *CircleA, PhysicsCircle *CircleB)
    {
        glm::dvec2 dp = CircleB->pos - CircleA->pos;
        double L = Modulus(dp);
        if ((CircleA->radius + CircleB->radius) > L)
        {
            contacts->separation = L - CircleB->radius;                             // 碰撞距离
            double angle = EdgeVecToCosAngleFloat(dp);                              // 计算出角度
            contacts->normal = vec2angle({1, 0}, angle);                            // （反向作用力法向量）地形不会旋转
            contacts->position = contacts->normal * CircleA->radius + CircleA->pos; // 碰撞点的位置
            contacts->w_side = 0;                                                   // 边索引 ID
            return 1;
        }
        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, PhysicsShape *Shape)
    {
        int ContactSize = 0;

        AngleMat Mat(Shape->angle);

        glm::dvec2 Rx = Mat.Rotary({Circle->radius, 0});
        glm::dvec2 Ry = Mat.Rotary({0, Circle->radius});
        std::vector<glm::dvec2> Cd{
            Circle->pos + Rx,
            Circle->pos + Ry,
            Circle->pos - Rx,
            Circle->pos - Ry};

        for (auto d : Cd)
        {
            CollisionInfoD info = Shape->PsBresenhamDetection(Circle->OldPos, d);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - d.y;
                }
                else
                {
                    contacts->separation = info.pos.x - d.x;
                }
                contacts->separation = -abs(contacts->separation);                                 // 碰撞距离差
                contacts->position = info.pos;                                                     // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2) + Shape->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                    // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        double L;
        double angle;
        glm::dvec2 dp;
        for (size_t i = 0; i < Shape->OutlineSize; i++)
        {
            dp = Shape->pos + Mat.Rotary(Shape->OutlineSet[i]) - Circle->pos;
            L = Modulus(dp);
            if (Circle->radius > L)
            {
                contacts->separation = L - Circle->radius;                            // 碰撞距离
                angle = EdgeVecToCosAngleFloat(dp);                                   // 计算出角度
                contacts->normal = vec2angle({1, 0}, angle);                          // （反向作用力法向量）地形不会旋转
                contacts->position = contacts->normal * Circle->radius + Circle->pos; // 碰撞点的位置
                contacts->w_side = ContactSize;                                       // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

}
