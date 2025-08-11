#include "PhysicsBaseCollide.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <cmath>

namespace PhysicsBlock
{

    bool CollideAABB(PhysicsShape *ShapeA, PhysicsShape *ShapeB)
    {
        FLOAT_ R = ShapeA->CollisionR + ShapeB->CollisionR;
        if (abs(ShapeA->pos.x - ShapeB->pos.x) > R)
        {
            return false;
        }
        if (abs(ShapeA->pos.y - ShapeB->pos.y) > R)
        {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsShape *Shape, PhysicsCircle *Circle)
    {
        FLOAT_ R = Shape->CollisionR + Circle->radius;
        if (abs(Shape->pos.x - Circle->pos.x) > R)
        {
            return false;
        }
        if (abs(Shape->pos.y - Circle->pos.y) > R)
        {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsShape *Shape, PhysicsParticle *Particle)
    {
        if (abs(Shape->pos.x - Particle->pos.x) > Shape->CollisionR)
        {
            return false;
        }
        if (abs(Shape->pos.y - Particle->pos.y) > Shape->CollisionR)
        {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsCircle *CircleA, PhysicsCircle *CircleB)
    {
        FLOAT_ R = CircleA->radius + CircleB->radius;
        if (abs(CircleA->pos.x - CircleB->pos.x) > R)
        {
            return false;
        }
        if (abs(CircleA->pos.y - CircleB->pos.y) > R)
        {
            return false;
        }
        return true;
    }

    bool CollideAABB(PhysicsCircle *Circle, PhysicsParticle *Particle)
    {
        if (abs(Circle->pos.x - Particle->pos.x) > Circle->radius)
        {
            return false;
        }
        if (abs(Circle->pos.y - Particle->pos.y) > Circle->radius)
        {
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
        for (auto d : Outline)
        {
            d.pos -= LMapStatic->centrality;
            d.pos = d.pos - Circle->pos;
            L = ModulusLength(d.pos);
            if ((Circle->radius * Circle->radius) > L)
            {
                L = SQRT_(L);
                contacts->separation = L - Circle->radius; // 碰撞距离
                // 地图临时为 0.2
                contacts->friction = SQRT_(d.F * Circle->friction);
                contacts->normal = d.pos / L;                                         // （反向作用力法向量）地形不会旋转
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
            contacts->separation = L - Circle->radius; // 碰撞距离
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
            contacts->separation = L - R; // 碰撞距离
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
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
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
                contacts->separation = L - Circle->radius; // 碰撞距离
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

    FLOAT_ corssProduct(const Vec2_ &A, const Vec2_ &B, const Vec2_ &C)
    {
        return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    }

    bool onSegment(const Vec2_ &P, const Vec2_ &A, const Vec2_ &B)
    {
        if ((P.x < std::min(A.x, B.x)) || P.x > std::max(A.x, B.x) ||
            (P.y < std::min(A.y, B.y)) || P.y > std::max(A.y, B.y))
        {
            return false;
        }
        return std::abs(corssProduct(A, B, P)) < 1e-9;
    }

    bool segmentIntersection(const Vec2_ &A, const Vec2_ &B, const Vec2_ &C, const Vec2_ &D, Vec2_ &P)
    {
        // 快速排斥实验（检测投影重叠）
        if (std::max(A.x, B.x) < std::min(C.x, D.x) ||
            std::min(A.x, B.x) > std::max(C.x, D.x) ||
            std::max(A.y, B.y) < std::min(C.y, D.y) ||
            std::min(A.y, B.y) > std::max(C.y, D.y))
        {
            return false;
        }

        // 计算四个叉积值
        FLOAT_ d1 = corssProduct(A, B, C); // AB x AC
        FLOAT_ d2 = corssProduct(A, B, D); // AB x AD
        FLOAT_ d3 = corssProduct(C, D, A); // CD x CA
        FLOAT_ d4 = corssProduct(C, D, B); // CD x CB

        // 规范相交
        if ((d1 * d2 < 0) && (d3 * d4 < 0))
        {
            FLOAT_ denominator = (B.x - A.x) * (D.y - C.y) - (B.y - A.y) * (D.x - C.x);
            if (std::abs(denominator) < 1e-9)
            {
                return false;
            }

            FLOAT_ t = ((C.x - A.x) * (D.y - C.y) - (C.y - A.y) * (D.x - C.x)) / denominator;
            P.x = A.x + t * (B.x - A.x);
            P.y = A.y + t * (B.y - A.y);
            return true;
        }

        // 非规范相交（端点相交或部分相交）
        if (onSegment(C, A, B))
        {
            P = C;
            return true;
        }
        if (onSegment(D, A, B))
        {
            P = D;
            return true;
        }
        if (onSegment(A, C, D))
        {
            P = A;
            return true;
        }
        if (onSegment(B, C, D))
        {
            P = B;
            return true;
        }

        return false;
    }

    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsShape *Shape)
    {
        unsigned int ContactSize = 0;
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        Vec2_ AB = {end.x - begin.x, end.y - begin.y};
        Vec2_ AP = {Shape->OldPos.x - begin.x, Shape->OldPos.y - begin.y};

        FLOAT_ dotProduct = Dot(AP, AB);
        FLOAT_ len = ModulusLength(AB);
        FLOAT_ t = dotProduct / len;

        Vec2_ normal = Shape->OldPos - (begin + (AB * t));
        normal = normal / Modulus(normal);

        AngleMat Mat(Shape->angle);
        Vec2_ Drop, DropPos; // 骨骼点
        for (size_t i = 0; i < Shape->OutlineSize; ++i)
        {
            Drop = Shape->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
            DropPos = Shape->pos + Drop;
            if (segmentIntersection(begin, end, Shape->OldPos, DropPos, contacts->position))
            {
                contacts->normal = normal;
                contacts->separation = -Modulus(contacts->position - DropPos); // 碰撞距离差
                contacts->friction = SQRT_(Shape->FrictionSet[i] * Line->friction);
                contacts->w_side = ContactSize; // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        CollisionInfoD info;
        if (Shape->GetCollision(begin))
        {
            info = Shape->PsBresenhamDetection(Line->OldPos, begin);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - begin.y;
                }
                else
                {
                    contacts->separation = info.pos.x - begin.x;
                }
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        if (Shape->GetCollision(end))
        {
            info = Shape->PsBresenhamDetection(Line->OldPos, end);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - end.y;
                }
                else
                {
                    contacts->separation = info.pos.x - end.x;
                }
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsCircle *Circle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        Vec2_ AB = {end.x - begin.x, end.y - begin.y};
        Vec2_ AP = {Circle->pos.x - begin.x, Circle->pos.y - begin.y};

        FLOAT_ dotProduct = Dot(AP, AB);
        FLOAT_ len = ModulusLength(AB);
        FLOAT_ t = dotProduct / len;

        if ((t >= 0) && (t <= 1.0))
        {
            contacts->position = begin + (AB * t);
            FLOAT_ R = Modulus(contacts->position - Circle->pos);
            if (Circle->radius > R)
            {
                contacts->normal = (Circle->pos - contacts->position) / R;
                contacts->separation = R - Circle->radius;
                contacts->friction = SQRT_(Line->friction * Circle->friction);
                contacts->w_side = 0;
                return 1;
            }
        }

        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsParticle *Particle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        if (segmentIntersection(begin, end, Particle->OldPos, Particle->pos, contacts->position))
        {
            Vec2_ AB = {end.x - begin.x, end.y - begin.y};
            Vec2_ AP = {Particle->OldPos.x - begin.x, Particle->OldPos.y - begin.y};

            FLOAT_ dotProduct = Dot(AP, AB);
            FLOAT_ len = ModulusLength(AB);
            FLOAT_ t = dotProduct / len;

            contacts->normal = Particle->OldPos - (begin + (AB * t));

            contacts->normal = contacts->normal / Modulus(contacts->normal);
            contacts->separation = -Modulus(contacts->position - Particle->pos); // 碰撞距离差
            contacts->friction = SQRT_(Particle->friction * Line->friction);
            contacts->w_side = 0; // 边索引 ID
            return 1;
        }

        return 0;
    }

    unsigned int Collide(Contact *contacts, PhysicsLine *Line, MapFormwork *Map)
    {
        unsigned int ContactSize = 0;

        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 2;
        std::vector<MapOutline> Outline = LMapStatic->GetLightweightOutline(ToInt(Line->pos.x - Line->radius) - KD, ToInt(Line->pos.y - Line->radius) - KD, ToInt(Line->pos.x + Line->radius) + KD, ToInt(Line->pos.y + Line->radius) + KD);
        FLOAT_ L;
        Vec2_ beginDian;
        for (auto d : Outline)
        {
            d.pos -= LMapStatic->centrality;
            beginDian = d.pos - (d.face * FLOAT_(0.5));
            if (segmentIntersection(begin, end, d.pos, beginDian, contacts->position))
            {
                Vec2_ AB = {end.x - begin.x, end.y - begin.y};
                Vec2_ AP = {beginDian.x - begin.x, beginDian.y - begin.y};

                FLOAT_ dotProduct = Dot(AP, AB);
                FLOAT_ len = ModulusLength(AB);
                FLOAT_ t = dotProduct / len;

                contacts->normal = beginDian - (begin + (AB * t));

                contacts->normal = contacts->normal / Modulus(contacts->normal);
                contacts->separation = -Modulus(contacts->position - d.pos); // 碰撞距离差
                contacts->friction = SQRT_(d.F * Line->friction);
                contacts->w_side = ContactSize; // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        CollisionInfoD info;
        if (Map->FMGetCollide(begin))
        {
            info = Map->FMBresenhamDetection(Line->OldPos, begin);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - begin.y;
                }
                else
                {
                    contacts->separation = info.pos.x - begin.x;
                }
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }
        if (Map->FMGetCollide(end))
        {
            info = Map->FMBresenhamDetection(Line->OldPos, end);
            if (info.Collision)
            {
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - end.y;
                }
                else
                {
                    contacts->separation = info.pos.x - end.x;
                }
                contacts->separation = -abs(contacts->separation); // 碰撞距离差
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = info.pos;                                      // 碰撞点的位置
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2)); // （反向作用力法向量）地形不会旋转
                contacts->w_side = ContactSize;                                     // 边索引 ID
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

}
