#include "PhysicsBaseCollide.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <cmath>

namespace PhysicsBlock
{

    bool CollideAABB(PhysicsShape *ShapeA, PhysicsShape *ShapeB)
    {
        FLOAT_ R = ShapeA->CollisionR + ShapeB->CollisionR;
        if (abs(ShapeA->pos.x - ShapeB->pos.x) > R) {
            return false;
        }
        if (abs(ShapeA->pos.y - ShapeB->pos.y) > R) {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsShape* Shape, PhysicsCircle* Circle)
    {
        FLOAT_ R = Shape->CollisionR + Circle->radius;
        if (abs(Shape->pos.x - Circle->pos.x) > R) {
            return false;
        }
        if (abs(Shape->pos.y - Circle->pos.y) > R) {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsShape *Shape, PhysicsParticle *Particle)
    {
        if (abs(Shape->pos.x - Particle->pos.x) > Shape->CollisionR) {
            return false;
        }
        if (abs(Shape->pos.y - Particle->pos.y) > Shape->CollisionR) {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsCircle* CircleA, PhysicsCircle* CircleB)
    {
        FLOAT_ R = CircleA->radius + CircleB->radius;
        if (abs(CircleA->pos.x - CircleB->pos.x) > R) {
            return false;
        }
        if (abs(CircleA->pos.y - CircleB->pos.y) > R) {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsCircle* Circle, PhysicsParticle* Particle)
    {
        if (abs(Circle->pos.x - Particle->pos.x) > Circle->radius) {
            return false;
        }
        if (abs(Circle->pos.y - Particle->pos.y) > Circle->radius) {
            return false;
        }
        return true;
    }

    unsigned int Collide(Contact *contacts, PhysicsShape *ShapeA, PhysicsShape *ShapeB)
    {
        AngleMat Mat(ShapeA->angle);
        int ContactSize = 0;
        Vec2_ Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < ShapeA->OutlineSize; ++i)
        {
            Drop = ShapeA->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
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
                contacts->friction = SQRT_(info.Friction * ShapeA->FrictionSet[i]);
                contacts->position = info.pos;                                                        // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeB->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                       // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        Mat.SetAngle(ShapeB->angle);
        for (size_t i = 0; i < ShapeB->OutlineSize; ++i)
        {
            Drop = ShapeB->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
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
                contacts->separation = -abs(contacts->separation);                                     // 碰撞距离差
                contacts->friction = SQRT_(info.Friction * ShapeB->FrictionSet[i]);
                contacts->position = info.pos;                                                         // 碰撞点的位置
                contacts->normal = -vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeA->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                        // 边索引 ID
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
                contacts->friction = SQRT_(info.Friction * Particle->friction);
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
        AngleMat Mat(Shape->angle);

        int ContactSize = 0;
        Vec2_ Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < Shape->OutlineSize; ++i)
        {
            Drop = Shape->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
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
                contacts->friction = SQRT_(info.Friction * Shape->FrictionSet[i]);
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
        std::vector<MapOutline> Outline = LMapStatic->GetLightweightOutline(ToInt(Shape->pos.x - Shape->CollisionR) - KD, ToInt(Shape->pos.y - Shape->CollisionR) - KD, ToInt(Shape->pos.x + Shape->CollisionR) + KD, ToInt(Shape->pos.y + Shape->CollisionR) + KD);
        for (size_t i = 0; i < Outline.size(); ++i)
        {
            Drop = Outline[i].pos - LMapStatic->centrality;
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
                // 地图临时为 0.2
                contacts->friction = SQRT_(info.Friction * Outline[i].F);
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
                contacts->friction = SQRT_(info.Friction * Particle->friction);
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = 0;                                               // 边索引 ID
                // 一直馅在碰撞体，无法更新旧位置（旧位置不可以在碰撞体内）
                Particle->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
                return 1;
            }
        }
        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, MapFormwork *Map)
    {
        int ContactSize = 0;

        std::vector<Vec2_> Cd{
            Circle->pos + Vec2_{Circle->radius, 0},
            Circle->pos + Vec2_{0, Circle->radius},
            Circle->pos + Vec2_{-Circle->radius, 0},
            Circle->pos + Vec2_{0, -Circle->radius}};

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
                contacts->friction = SQRT_(info.Friction * Circle->friction);
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 2;
        std::vector<MapOutline> Outline = LMapStatic->GetLightweightOutline(ToInt(Circle->pos.x - Circle->radius) - KD, ToInt(Circle->pos.y - Circle->radius) - KD, ToInt(Circle->pos.x + Circle->radius) + KD, ToInt(Circle->pos.y + Circle->radius) + KD);
        FLOAT_ L;
        FLOAT_ angle;
        for (auto d : Outline)
        {
            d.pos -= LMapStatic->centrality;
            d.pos = d.pos - Circle->pos;
            L = ModulusLength(d.pos);
            if ((Circle->radius * Circle->radius) > L)
            {
                L = SQRT_(L);
                contacts->separation = L - Circle->radius;                            // 碰撞距离
                // 地图临时为 0.2
                contacts->friction = SQRT_(d.F * Circle->friction);
                contacts->normal = d.pos / L;                                             // （反向作用力法向量）地形不会旋转
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
        Vec2_ dp = Particle->pos - Circle->pos;
        FLOAT_ L = ModulusLength(dp);
        FLOAT_ R2 = Circle->radius * Circle->radius;
        if (R2 > L)
        {
            L = SQRT_(L);
            contacts->separation = L - Circle->radius;                            // 碰撞距离
            contacts->friction = SQRT_(Particle->friction * Circle->friction);
            contacts->normal = dp / L;                                            // （反向作用力法向量）地形不会旋转
            contacts->position = contacts->normal * Circle->radius + Circle->pos; // 碰撞点的位置
            contacts->w_side = 0;                                                 // 边索引 ID
            return 1;
        }
        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsCircle *CircleA, PhysicsCircle *CircleB)
    {
        Vec2_ dp = CircleB->pos - CircleA->pos;
        FLOAT_ L = ModulusLength(dp);
        const FLOAT_ R = CircleA->radius + CircleB->radius;
        const FLOAT_ R2 = R * R;
        if (R2 > L)
        {
            L = SQRT_(L);
            contacts->separation = L - R;         // 碰撞距离
            contacts->friction = SQRT_(CircleB->friction * CircleA->friction);
            contacts->normal = dp / L;                                              // （反向作用力法向量）地形不会旋转
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

        Vec2_ Rx = Mat.Rotary({Circle->radius, 0});
        Vec2_ Ry = Mat.Rotary({0, Circle->radius});
        std::vector<Vec2_> Cd{
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
                contacts->friction = SQRT_(Circle->friction * info.Friction);
                contacts->position = info.pos;                                                     // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2) + Shape->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                    // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        FLOAT_ L;
        FLOAT_ angle;
        Vec2_ dp;
        FLOAT_ R2 = Circle->radius * Circle->radius;
        for (size_t i = 0; i < Shape->OutlineSize; i++)
        {
            dp = Shape->pos + Mat.Rotary(Shape->OutlineSet[i]) - Circle->pos;
            L = ModulusLength(dp);
            if (R2 > L)
            {
                L = SQRT_(L);
                contacts->separation = L - Circle->radius;                            // 碰撞距离
                // 临时为 0.2
                contacts->friction = SQRT_(Circle->friction * Shape->FrictionSet[i]);
                contacts->normal = dp / L;                                            // （反向作用力法向量）地形不会旋转
                contacts->position = contacts->normal * Circle->radius + Circle->pos; // 碰撞点的位置
                contacts->w_side = ContactSize;                                       // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

}
