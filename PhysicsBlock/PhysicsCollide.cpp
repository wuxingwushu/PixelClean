#include "PhysicsCollide.hpp"
#include "BaseCalculate.hpp"
#include <cmath>

namespace PhysicsBlock
{

    int Collide(Contact *contacts, PhysicsShape *A, PhysicsShape *B)
    {
        int ContactSize = 0;
        glm::dvec2 Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < A->OutlineSize; ++i)
        {
            Drop = A->OutlineSet[i] - A->CentreMass;
            Drop = vec2angle(Drop, A->angle);
            DropPos = A->pos + Drop;
            CollisionInfoD info = B->PsBresenhamDetection(A->OldPos, DropPos);
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
                contacts->separation = -abs(contacts->separation);                                        // 碰撞距离差
                contacts->position = info.pos;                                                            // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, (info.Direction * (3.14159265359 / 2)) + B->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                                           // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        return ContactSize;
    }

    int Collide(Contact *contacts, PhysicsShape *A, PhysicsParticle *B)
    {
        if (A->DropCollision(B->pos).Collision)
        {
            CollisionInfoD info = A->PsBresenhamDetection(B->OldPos, B->pos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - B->pos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - B->pos.x;
                }
                contacts->separation = -abs(contacts->separation);                                        // 碰撞距离差
                contacts->position = info.pos;                                                            // 碰撞点的位置
                contacts->normal = -vec2angle({-1, 0}, (info.Direction * (3.14159265359 / 2)) + A->angle); // （反向作用力法向量）地形不会旋转
                contacts->w_side = 0;                                                                     // 边索引 ID
                // 一直馅在碰撞体，无法更新旧位置（旧位置不可以在碰撞体内）
                B->OldPos = info.pos + (contacts->normal * 0.1);
                return 1; // 有碰撞返回碰撞位置
            }
        }
        return 0;
    }

    int Collide(Contact *contacts, PhysicsShape *A, MapFormwork *B)
    {
        int ContactSize = 0;
        glm::dvec2 Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < A->OutlineSize; ++i)
        {
            Drop = A->OutlineSet[i] - A->CentreMass;
            Drop = vec2angle(Drop, A->angle);
            DropPos = A->pos + Drop;
            CollisionInfoD info = B->FMBresenhamDetection(A->OldPos, DropPos);
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
                contacts->separation = -abs(contacts->separation);                           // 碰撞距离差
                contacts->position = info.pos;                                               // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (3.14159265359 / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                              // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        return ContactSize;
    }

    int Collide(Contact *contacts, PhysicsParticle *A, MapFormwork *B)
    {
        if (B->FMGetCollide(A->pos))
        {
            CollisionInfoD info = B->FMBresenhamDetection(A->OldPos, A->pos);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - A->pos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - A->pos.x;
                }
                contacts->separation = -abs(contacts->separation);                           // 碰撞距离差
                contacts->position = info.pos;                                               // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (3.14159265359 / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = 0;                                                        // 边索引 ID
                // 一直馅在碰撞体，无法更新旧位置（旧位置不可以在碰撞体内）
                A->OldPos = info.pos - (contacts->normal * 0.1);
                return 1;
            }
        }
        return 0;
    }

}
