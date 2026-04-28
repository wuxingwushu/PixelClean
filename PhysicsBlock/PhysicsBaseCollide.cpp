#include "PhysicsBaseCollide.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <cmath>

namespace PhysicsBlock
{

    /**
     * @brief   AABB 粗糙碰撞检测（网格形状 vs 网格形状）
     * @details 使用轴向包围盒进行快速碰撞预检测
     *          通过比较两个物体中心在 X 和 Y 方向上的距离是否小于半径之和
     * @param   ShapeA 第一个网格形状对象
     * @param   ShapeB 第二个网格形状对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsShape *ShapeA, PhysicsShape *ShapeB)
    {
        FLOAT_ R = ShapeA->radius + ShapeB->radius;
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

    /**
     * @brief   AABB 粗糙碰撞检测（网格形状 vs 圆形）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Shape 网格形状对象
     * @param   Circle 圆形对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsShape *Shape, PhysicsCircle *Circle)
    {
        FLOAT_ R = Shape->radius + Circle->radius;
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

    /**
     * @brief   AABB 粗糙碰撞检测（网格形状 vs 点）
     * @details 使用轴向包围盒进行快速碰撞预检测
     *          点没有半径，只需判断是否在形状的包围盒内
     * @param   Shape 网格形状对象
     * @param   Particle 点对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsShape *Shape, PhysicsParticle *Particle)
    {
        if (abs(Shape->pos.x - Particle->pos.x) > Shape->radius)
        {
            return false;
        }
        if (abs(Shape->pos.y - Particle->pos.y) > Shape->radius)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief   AABB 粗糙碰撞检测（圆形 vs 圆形）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   CircleA 第一个圆形对象
     * @param   CircleB 第二个圆形对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
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

    /**
     * @brief   AABB 粗糙碰撞检测（圆形 vs 点）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Circle 圆形对象
     * @param   Particle 点对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
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

    /**
     * @brief   AABB 粗糙碰撞检测（线 vs 网格形状）
     * @details 使用轴向包围盒进行快速碰撞预检测
     *          线对象需要考虑其旋转后的包围盒
     * @param   Line 线对象
     * @param   Shape 网格形状对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsLine *Line, PhysicsShape *Shape)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        if (abs(Shape->pos.x - Line->pos.x) > (Shape->radius + abs(pR.x)))
        {
            return false;
        }
        if (abs(Shape->pos.y - Line->pos.y) > (Shape->radius + abs(pR.y)))
        {
            return false;
        }
        return true;
    }

    /**
     * @brief   AABB 粗糙碰撞检测（线 vs 圆形）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Line 线对象
     * @param   Circle 圆形对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsLine *Line, PhysicsCircle *Circle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        if (abs(Circle->pos.x - Line->pos.x) > (Circle->radius + abs(pR.x)))
        {
            return false;
        }
        if (abs(Circle->pos.y - Line->pos.y) > (Circle->radius + abs(pR.y)))
        {
            return false;
        }
        return true;
    }

    /**
     * @brief   精确碰撞检测（网格形状 vs 网格形状）
     * @details 通过检测两个形状的轮廓点之间的碰撞来确定碰撞信息
     *          使用 Bresenham 算法检测线段与形状的交点
     * @param   contacts 碰撞信息输出数组
     * @param   ShapeA 第一个网格形状对象
     * @param   ShapeB 第二个网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsShape *ShapeA, PhysicsShape *ShapeB)
    {
        AngleMat Mat(ShapeA->angle);
        int ContactSize = 0;
        Vec2_ Drop, DropPos;

        // 检测 ShapeA 的轮廓点与 ShapeB 的碰撞
        for (size_t i = 0; i < ShapeA->OutlineSize; ++i)
        {
            Drop = ShapeA->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
            DropPos = ShapeA->pos + Drop;
            CollisionInfoD info = ShapeB->PsBresenhamDetection(ShapeA->OldPos, DropPos);
            if (info.Collision)
            {
                // 计算分离距离
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - DropPos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - DropPos.x;
                }
                contacts->separation = -abs(contacts->separation);
                // 计算摩擦系数（几何平均）
                contacts->friction = SQRT_(info.Friction * ShapeA->FrictionSet[i]);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeB->angle);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测 ShapeB 的轮廓点与 ShapeA 的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * ShapeB->FrictionSet[i]);
                contacts->position = info.pos;
                contacts->normal = -vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeA->angle);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }
        return ContactSize;
    }

    /**
     * @brief   精确碰撞检测（网格形状 vs 点）
     * @details 检测点是否与网格形状发生碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Shape 网格形状对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsShape *Shape, PhysicsParticle *Particle)
    {
        // 首先检查点是否在形状内
        if (Shape->DropCollision(Particle->pos).Collision)
        {
            CollisionInfoD info;
            // 使用反向射线检测碰撞点（避免点在形状内部时无法检测）
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Particle->friction);
                contacts->position = info.pos;
                contacts->normal = -vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + Shape->angle);
                contacts->w_side = 0;
                return 1;
            }
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（网格形状 vs 地形）
     * @details 检测网格形状与地形之间的碰撞
     *          包括形状轮廓点与地形的碰撞，以及地形轮廓点与形状的碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Shape 网格形状对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsShape *Shape, MapFormwork *Map)
    {
        AngleMat Mat(Shape->angle);
        int ContactSize = 0;
        Vec2_ Drop, DropPos;

        // 检测形状轮廓点与地形的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Shape->FrictionSet[i]);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测地形轮廓点与形状的碰撞（临时处理方法）
        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 1;
        std::vector<MapOutline> Outline = LMapStatic->GetLightweightOutline(
            ToInt(Shape->pos.x - Shape->radius) - KD,
            ToInt(Shape->pos.y - Shape->radius) - KD,
            ToInt(Shape->pos.x + Shape->radius) + KD,
            ToInt(Shape->pos.y + Shape->radius) + KD);

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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Outline[i].F);
                contacts->position = info.pos;
                contacts->normal = -vec2angle({-1, 0}, info.Direction * (M_PI / 2) + Shape->angle);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

    /**
     * @brief   精确碰撞检测（点 vs 地形）
     * @details 检测点与地形之间的碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Particle 点对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsParticle *Particle, MapFormwork *Map)
    {
        // 首先检查点是否在地形碰撞区域内
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Particle->friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = 0;
                // 处理点卡在地形内部的情况
                Particle->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
                Particle->OldPosUpDataBool = false;
                return 1;
            }
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 地形）
     * @details 检测圆形与地形之间的碰撞
     *          包括圆形四个方向的采样点与地形的碰撞，以及地形轮廓点与圆形的碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, MapFormwork *Map)
    {
        int ContactSize = 0;

        // 定义圆形四个方向的采样点
        std::vector<Vec2_> Cd{
            Circle->pos + Vec2_{Circle->radius, 0},
            Circle->pos + Vec2_{0, Circle->radius},
            Circle->pos + Vec2_{-Circle->radius, 0},
            Circle->pos + Vec2_{0, -Circle->radius}};

        // 检测采样点与地形的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Circle->friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测地形轮廓点与圆形的碰撞
        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 2;
        std::vector<MapOutline> Outline = LMapStatic->GetLightweightOutline(
            ToInt(Circle->pos.x - Circle->radius) - KD,
            ToInt(Circle->pos.y - Circle->radius) - KD,
            ToInt(Circle->pos.x + Circle->radius) + KD,
            ToInt(Circle->pos.y + Circle->radius) + KD);

        FLOAT_ L;
        for (auto d : Outline)
        {
            d.pos -= LMapStatic->centrality;
            d.pos = d.pos - Circle->pos;
            L = ModulusLength(d.pos);
            if ((Circle->radius * Circle->radius) > L)
            {
                L = SQRT_(L);
                contacts->separation = L - Circle->radius;
                contacts->friction = SQRT_(d.F * Circle->friction);
                contacts->normal = d.pos / L;
                contacts->position = contacts->normal * Circle->radius + Circle->pos;
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }
        return ContactSize;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 点）
     * @details 使用距离检测判断点是否在圆形内部
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, PhysicsParticle *Particle)
    {
        Vec2_ dp = Particle->pos - Circle->pos;
        FLOAT_ L = ModulusLength(dp);
        FLOAT_ R2 = Circle->radius * Circle->radius;
        if (R2 > L)
        {
            L = SQRT_(L);
            contacts->separation = L - Circle->radius;
            contacts->friction = SQRT_(Particle->friction * Circle->friction);
            contacts->normal = dp / L;
            contacts->position = contacts->normal * Circle->radius + Circle->pos;
            contacts->w_side = 0;
            return 1;
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 圆形）
     * @details 使用距离检测判断两个圆形是否相交
     * @param   contacts 碰撞信息输出数组
     * @param   CircleA 第一个圆形对象
     * @param   CircleB 第二个圆形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *CircleA, PhysicsCircle *CircleB)
    {
        Vec2_ dp = CircleB->pos - CircleA->pos;
        FLOAT_ L = ModulusLength(dp);
        const FLOAT_ R = CircleA->radius + CircleB->radius;
        const FLOAT_ R2 = R * R;
        if (R2 > L)
        {
            L = SQRT_(L);
            contacts->separation = L - R;
            contacts->friction = SQRT_(CircleB->friction * CircleA->friction);
            contacts->normal = dp / L;
            contacts->position = contacts->normal * CircleA->radius + CircleA->pos;
            contacts->w_side = 0;
            return 1;
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 网格形状）
     * @details 检测圆形与网格形状之间的碰撞
     *          包括圆形旋转后的采样点与形状的碰撞，以及形状轮廓点与圆形的碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Shape 网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, PhysicsShape *Shape)
    {
        int ContactSize = 0;
        AngleMat Mat(Shape->angle);

        // 计算旋转后的圆形采样方向
        Vec2_ Rx = Mat.Rotary({Circle->radius, 0});
        Vec2_ Ry = Mat.Rotary({0, Circle->radius});

        std::vector<Vec2_> Cd{
            Circle->pos + Rx,
            Circle->pos + Ry,
            Circle->pos - Rx,
            Circle->pos - Ry};

        // 检测采样点与形状的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(Circle->friction * info.Friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2) + Shape->angle);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测形状轮廓点与圆形的碰撞
        FLOAT_ L;
        Vec2_ dp;
        FLOAT_ R2 = Circle->radius * Circle->radius;
        for (size_t i = 0; i < Shape->OutlineSize; i++)
        {
            dp = Shape->pos + Mat.Rotary(Shape->OutlineSet[i]) - Circle->pos;
            L = ModulusLength(dp);
            if (R2 > L)
            {
                L = SQRT_(L);
                contacts->separation = L - Circle->radius;
                contacts->friction = SQRT_(Circle->friction * Shape->FrictionSet[i]);
                contacts->normal = dp / L;
                contacts->position = contacts->normal * Circle->radius + Circle->pos;
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

    /**
     * @brief   计算叉积
     * @details 计算向量 AB 和 AC 的叉积
     * @param   A 点 A
     * @param   B 点 B
     * @param   C 点 C
     * @return  叉积值
     */
    FLOAT_ corssProduct(const Vec2_ &A, const Vec2_ &B, const Vec2_ &C)
    {
        return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    }

    /**
     * @brief   判断点是否在线段上
     * @details 检查点 P 是否在线段 AB 上
     * @param   P 待检测点
     * @param   A 线段起点
     * @param   B 线段终点
     * @return  如果点在线段上返回 true，否则返回 false
     */
    bool onSegment(const Vec2_ &P, const Vec2_ &A, const Vec2_ &B)
    {
        if ((P.x < std::min(A.x, B.x)) || P.x > std::max(A.x, B.x) ||
            (P.y < std::min(A.y, B.y)) || P.y > std::max(A.y, B.y))
        {
            return false;
        }
        return std::abs(corssProduct(A, B, P)) < 1e-9;
    }

    /**
     * @brief   线段相交检测
     * @details 使用跨立实验检测两条线段是否相交
     * @param   A 第一条线段起点
     * @param   B 第一条线段终点
     * @param   C 第二条线段起点
     * @param   D 第二条线段终点
     * @param   P 输出交点
     * @return  如果相交返回 true，否则返回 false
     */
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
        FLOAT_ d1 = corssProduct(A, B, C);
        FLOAT_ d2 = corssProduct(A, B, D);
        FLOAT_ d3 = corssProduct(C, D, A);
        FLOAT_ d4 = corssProduct(C, D, B);

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

    /**
     * @brief   精确碰撞检测（线 vs 网格形状）
     * @details 检测线对象与网格形状之间的碰撞
     *          包括形状轮廓点与线段的碰撞，以及线段端点与形状的碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Shape 网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsShape *Shape)
    {
        unsigned int ContactSize = 0;
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        // 计算线段的法向量
        Vec2_ AB = {end.x - begin.x, end.y - begin.y};
        Vec2_ AP = {Shape->OldPos.x - begin.x, Shape->OldPos.y - begin.y};
        FLOAT_ dotProduct = Dot(AP, AB);
        FLOAT_ len = ModulusLength(AB);
        FLOAT_ t = dotProduct / len;
        Vec2_ normal = Shape->OldPos - (begin + (AB * t));
        normal = normal / Modulus(normal);

        // 检测形状轮廓点与线段的碰撞
        AngleMat Mat(Shape->angle);
        Vec2_ Drop, DropPos;
        for (size_t i = 0; i < Shape->OutlineSize; ++i)
        {
            Drop = Shape->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
            DropPos = Shape->pos + Drop;
            if (segmentIntersection(begin, end, Shape->OldPos, DropPos, contacts->position))
            {
                contacts->normal = normal;
                contacts->separation = -Modulus(contacts->position - DropPos);
                contacts->friction = SQRT_(Shape->FrictionSet[i] * Line->friction);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测线对象与形状的碰撞（处理线穿过形状的情况）
        CollisionInfoD info;
        info = Shape->PsBresenhamDetection(Line->OldPos, Line->pos);
        if (info.Collision)
        {
            contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
            Line->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
            Line->OldPosUpDataBool = false;
        }

        // 检测线段起点与形状的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测线段终点与形状的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

    /**
     * @brief   精确碰撞检测（线 vs 圆）
     * @details 检测线对象与圆形之间的碰撞
     *          使用点到线段的最近点距离判断碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Circle 圆形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsCircle *Circle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        // 计算圆心到线段的最近点
        contacts->position = DropUptoLineShortesIntersect(begin, end, Circle->pos);

        // 计算距离
        FLOAT_ R = Modulus(contacts->position - Circle->pos);
        if (Circle->radius > R)
        {
            contacts->normal = (Circle->pos - contacts->position) / R;
            contacts->separation = R - Circle->radius;
            contacts->friction = SQRT_(Line->friction * Circle->friction);
            contacts->w_side = 0;
            return 1;
        }

        return 0;
    }

    /**
     * @brief   精确碰撞检测（线 vs 点）
     * @details 检测线对象与点之间的碰撞
     *          使用线段相交检测判断点的运动轨迹是否与线相交
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsParticle *Particle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        if (segmentIntersection(begin, end, Particle->OldPos, Particle->pos, contacts->position))
        {
            // 计算法向量
            Vec2_ AB = {end.x - begin.x, end.y - begin.y};
            Vec2_ AP = {Particle->OldPos.x - begin.x, Particle->OldPos.y - begin.y};
            FLOAT_ dotProduct = Dot(AP, AB);
            FLOAT_ len = ModulusLength(AB);
            FLOAT_ t = dotProduct / len;

            contacts->normal = Particle->OldPos - (begin + (AB * t));
            contacts->normal = contacts->normal / Modulus(contacts->normal);
            contacts->separation = -Modulus(contacts->position - Particle->pos);
            contacts->friction = SQRT_(Particle->friction * Line->friction);
            contacts->w_side = 0;

            // 处理点卡在线内部的情况
            Particle->OldPos = contacts->position - (contacts->normal * FLOAT_(0.1));
            Particle->OldPosUpDataBool = false;
            return 1;
        }

        return 0;
    }

    /**
     * @brief   精确碰撞检测（线 vs 地形）
     * @details 检测线对象与地形之间的碰撞
     *          包括地形轮廓边与线段的碰撞，以及线段端点与地形的碰撞
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, MapFormwork *Map)
    {
        unsigned int ContactSize = 0;

        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        Vec2_ AB = {end.x - begin.x, end.y - begin.y};
        FLOAT_ len = ModulusLength(AB);

        // 检测地形轮廓边与线段的碰撞
        MapStatic *LMapStatic = (MapStatic *)Map;
        int KD = 2;
        std::vector<MapOutline> Outline = LMapStatic->GetLightweightOutline(
            ToInt(Line->pos.x - Line->radius) - KD,
            ToInt(Line->pos.y - Line->radius) - KD,
            ToInt(Line->pos.x + Line->radius) + KD,
            ToInt(Line->pos.y + Line->radius) + KD);

        FLOAT_ L;
        Vec2_ beginDian;
        for (auto d : Outline)
        {
            d.pos -= LMapStatic->centrality;
            beginDian = d.pos - (d.face * FLOAT_(0.5));
            if (segmentIntersection(begin, end, d.pos, beginDian, contacts->position))
            {
                Vec2_ AP = {beginDian.x - begin.x, beginDian.y - begin.y};
                FLOAT_ dotProduct = Dot(AP, AB);
                FLOAT_ t = dotProduct / len;

                contacts->normal = beginDian - (begin + (AB * t));
                contacts->normal = contacts->normal / Modulus(contacts->normal);
                contacts->separation = -Modulus(contacts->position - d.pos);
                contacts->friction = SQRT_(d.F * Line->friction);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测线对象与地形的碰撞（处理线穿过地形的情况）
        CollisionInfoD info;
        info = Map->FMBresenhamDetection(Line->OldPos, Line->pos);
        if (info.Collision)
        {
            contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
            Line->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
            Line->OldPosUpDataBool = false;
        }

        // 检测线段起点与地形的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = begin;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // 检测线段终点与地形的碰撞
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Line->friction);
                contacts->position = end;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        return ContactSize;
    }

}
