#include "PhysicsBaseCollide.hpp"
#include "BaseCalculate.hpp"
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
     * @details 双向轮廓点遍历检测：先遍历 ShapeA 的轮廓点用 Bresenham 射线检测是否穿透了 ShapeB，
     *          再遍历 ShapeB 的轮廓点用 Bresenham 射线检测是否穿透了 ShapeA。
     *          碰撞法线方向由 Bresenham 检测到的 Direction 确定，摩擦系数取两者几何平均。
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

        // ===== 第一轮：ShapeA 轮廓点 → ShapeB 的 Bresenham 射线检测 =====
        // 将 ShapeA 的每个轮廓点从其旧位置到当前位置做射线，检测是否与 ShapeB 的网格相交
        for (size_t i = 0; i < ShapeA->OutlineSize && ContactSize < PhysicsContactMaxSize; ++i)
        {
            Drop = ShapeA->OutlineSet[i];                // 获取局部坐标下的轮廓点
            Drop = Mat.Rotary(Drop);                     // 旋转变换到世界方向
            DropPos = ShapeA->pos + Drop;                // 平移到世界坐标
            CollisionInfoD info = ShapeB->PsBresenhamDetection(ShapeA->OldPos + Mat.Rotary(ShapeA->MaxOutlineCentreMass[i]), DropPos);
            if (info.Collision)
            {
                // 根据碰撞面的方向轴（X轴或Y轴）计算分离深度
                if (info.Direction & 0x1)
                {
                    contacts->separation = info.pos.y - DropPos.y;
                }
                else
                {
                    contacts->separation = info.pos.x - DropPos.x;
                }
                contacts->separation = -abs(contacts->separation);   // 分离距离取负表示穿透深度
                // 摩擦系数取两者几何平均
                contacts->friction = SQRT_(info.Friction * ShapeA->FrictionSet[i]);
                contacts->position = info.pos;
                // 法线方向由碰撞面的轴向(Direction * 90°)叠加 ShapeB 的旋转角得到
                contacts->normal = vec2angle({-1, 0}, (info.Direction * (M_PI / 2)) + ShapeB->angle);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // ===== 第二轮：ShapeB 轮廓点 → ShapeA 的 Bresenham 射线检测 =====
        // 对称地检测 ShapeB 的轮廓点是否穿透了 ShapeA，法线取反方向
        Mat.SetAngle(ShapeB->angle);
        for (size_t i = 0; i < ShapeB->OutlineSize && ContactSize < PhysicsContactMaxSize; ++i)
        {
            Drop = ShapeB->OutlineSet[i];
            Drop = Mat.Rotary(Drop);
            DropPos = ShapeB->pos + Drop;
            CollisionInfoD info = ShapeA->PsBresenhamDetection(ShapeB->OldPos + Mat.Rotary(ShapeB->MaxOutlineCentreMass[i]), DropPos);
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
                // 法线取反方向（从 ShapeA 指向 ShapeB）
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
     * @details 两步检测法：先通过包含性测试判断点是否在形状内部，
     *          若在内部则用 Bresenham 射线从旧位置到当前位置检测穿透面，
     *          获得碰撞法线和穿透深度。检测到后将点沿法线方向推出以避免卡住。
     * @param   contacts 碰撞信息输出数组
     * @param   Shape 网格形状对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsShape *Shape, PhysicsParticle *Particle)
    {
        // 第一步：包含性测试——检查点当前是否在形状内部
        if (Shape->DropCollision(Particle->pos).Collision)
        {
            CollisionInfoD info;
            // 第二步：Bresenham 射线检测——从旧位置向当前位置发射射线，找出穿透面
            info = Shape->PsBresenhamDetection(Particle->OldPos, Particle->pos);
            if (info.Collision)
            {
                // 根据碰撞方向轴计算分离深度
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
                // 将旧位置沿法线方向微调推出，防止粒子卡在形状内部
                Particle->OldPos = info.pos + (contacts->normal * FLOAT_(0.1));
                Particle->OldPosUpDataBool = false;
                return 1;
            }
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（网格形状 vs 地形）
     * @details 双向碰撞检测：先遍历形状的旋转轮廓点，用 Bresenham 射线检测是否穿透了地形网格；
     *          再获取形状包围盒范围内的地形轮廓点，反向检测地形轮廓点是否穿透了形状。
     *          这种双向检测确保无论谁先穿透谁都能正确检测到碰撞。
     * @param   contacts 碰撞信息输出数组
     * @param   Shape 网格形状对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsShape *Shape, MapFormwork *Map)
    {
        if (!Map) return 0;

        AngleMat Mat(Shape->angle);
        int ContactSize = 0;
        Vec2_ Drop, DropPos;

        // ===== 第一轮：形状轮廓点 → 地形的 Bresenham 射线检测 =====
        // 对形状的每个轮廓点，从其旧位置到当前位置做射线，检测是否与地形网格相交
        for (size_t i = 0; i < Shape->OutlineSize; ++i)
        {
            Drop = Shape->OutlineSet[i];                              // 局部坐标轮廓点
            Drop = Mat.Rotary(Drop);                                  // 施加旋转
            DropPos = Shape->pos + Drop;                              // 转换到世界坐标
            CollisionInfoD info = Map->FMBresenhamDetection(Shape->OldPos + Mat.Rotary(Shape->MaxOutlineCentreMass[i]), DropPos);
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
                // 地形无旋转，法线直接由 Direction 轴向确定
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // ===== 第二轮：地形轮廓点 → 形状的 Bresenham 射线检测 =====
        // 获取形状包围盒范围内的地形轻量轮廓（减少检测量），反向检测地形的棱角是否穿透形状
        int KD = 1;
        Vec2_ mapCentrality = Map->FMGetCentrality();
        std::vector<MapOutline> Outline = Map->FMGetLightweightOutline(
            ToInt(Shape->pos.x - Shape->radius) - KD,
            ToInt(Shape->pos.y - Shape->radius) - KD,
            ToInt(Shape->pos.x + Shape->radius) + KD,
            ToInt(Shape->pos.y + Shape->radius) + KD);

        for (size_t i = 0; i < Outline.size(); ++i)
        {
            Drop = Outline[i].pos - mapCentrality;                    // 地形轮廓点转换到世界坐标
            if (!Shape->DropCollision(Drop).Collision)                // 快速过滤：跳过不在形状内的点
            {
                continue;
            }
            // 反向射线：从轮廓点外侧向内侧发射
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
                // 法线取反（从形状指向地形外）
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
     * @details 先通过地形碰撞查询判断点是否在地形实体区域内，
     *          若在区域内则用 Bresenham 射线从旧位置到当前位置检测穿透面，
     *          获得碰撞法线后将点沿法线方向推出以避免卡在地形内部。
     * @param   contacts 碰撞信息输出数组
     * @param   Particle 点对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsParticle *Particle, MapFormwork *Map)
    {
        if (!Map) return 0;

        // 第一步：检查点是否在地形碰撞区域内
        // 注意：必须用 Vec2_ 重载（FMGetCollide(Vec2_) 内部会加 centrality 把世界坐标转为网格坐标）。
        // 旧的 ToInt(Particle->pos) 走 glm::ivec2 重载，不加 centrality，
        // 导致世界坐标为负（地图左下半）时碰撞检测完全失效。
        if (Map->FMGetCollide(Particle->pos))
        {
            // 第二步：Bresenham 射线检测——从旧位置向当前位置发射射线，找出穿透面
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
                // 将旧位置沿法线方向微调推出，防止点卡在地形内部
                Particle->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
                Particle->OldPosUpDataBool = false;
                return 1;
            }
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 地形）
     * @details 双向检测：先在圆的上、右、下、左四个方向取采样点，用 Bresenham 射线检测是否穿透了地形；
     *          再获取圆形包围盒范围内的地形轮廓点，用点-圆距离判断地形棱角是否进入圆内。
     *          这种采样点 + 轮廓反向检测的组合覆盖了圆形与地形格子的所有接触形态。
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, MapFormwork *Map)
    {
        if (!Map) return 0;

        int ContactSize = 0;

        // ===== 第一轮：圆形四个方向采样点 → 地形的 Bresenham 射线检测 =====
        // 在圆的上(+x)、右(+y)、下(-x)、左(-y) 四个方向边界取采样点，检测其运动轨迹是否穿过地形
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(info.Friction * Circle->friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // ===== 第二轮：地形轮廓点 → 圆形的距离检测 =====
        // 获取圆形包围盒范围内的地形轻量轮廓，判断每个轮廓点到圆心的距离是否小于半径
        int KD = 2;
        Vec2_ mapCentrality = Map->FMGetCentrality();
        std::vector<MapOutline> Outline = Map->FMGetLightweightOutline(
            ToInt(Circle->pos.x - Circle->radius) - KD,
            ToInt(Circle->pos.y - Circle->radius) - KD,
            ToInt(Circle->pos.x + Circle->radius) + KD,
            ToInt(Circle->pos.y + Circle->radius) + KD);

        FLOAT_ L;
        for (auto d : Outline)
        {
            d.pos -= mapCentrality;                                  // 地形轮廓点转换到世界坐标
            d.pos = d.pos - Circle->pos;                             // 转为相对于圆心的向量
            L = ModulusLength(d.pos);                                // 计算距离平方
            if ((Circle->radius * Circle->radius) > L)               // 判断是否在圆内
            {
                L = SQRT_(L);                                        // 开方得实际距离
                contacts->separation = L - Circle->radius;           // 穿透深度（负值）
                contacts->friction = SQRT_(d.F * Circle->friction);
                contacts->normal = d.pos / L;                        // 法线 = 圆心指向轮廓点
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
     * @details 基于点-圆距离的简单碰撞检测：计算点到圆心的距离平方，
     *          与半径平方比较。若在圆内则计算穿透深度和指向圆外的法线。
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, PhysicsParticle *Particle)
    {
        Vec2_ dp = Particle->pos - Circle->pos;                      // 圆心指向点的向量
        FLOAT_ L = ModulusLength(dp);                                // 距离平方
        FLOAT_ R2 = Circle->radius * Circle->radius;
        if (R2 > L)                                                  // 点在圆内
        {
            L = SQRT_(L);                                            // 实际距离
            contacts->separation = L - Circle->radius;               // 穿透深度
            contacts->friction = SQRT_(Particle->friction * Circle->friction);
            contacts->normal = dp / L;                               // 法线方向 = 圆心指向外
            contacts->position = contacts->normal * Circle->radius + Circle->pos;
            contacts->w_side = 0;
            return 1;
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 圆形）
     * @details 基于圆心距的碰撞检测：计算两个圆心之间的距离平方，
     *          与半径之和的平方比较。若相交则计算穿透深度和分离法线。
     * @param   contacts 碰撞信息输出数组
     * @param   CircleA 第一个圆形对象
     * @param   CircleB 第二个圆形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *CircleA, PhysicsCircle *CircleB)
    {
        Vec2_ dp = CircleB->pos - CircleA->pos;                      // A圆心指向B圆心的向量
        FLOAT_ L = ModulusLength(dp);                                // 圆心距平方
        const FLOAT_ R = CircleA->radius + CircleB->radius;          // 半径之和
        const FLOAT_ R2 = R * R;
        if (R2 > L)                                                  // 两圆相交
        {
            L = SQRT_(L);                                            // 实际圆心距
            contacts->separation = L - R;                            // 穿透深度（负值）
            contacts->friction = SQRT_(CircleB->friction * CircleA->friction);
            contacts->normal = dp / L;                               // 法线 = A指向B
            contacts->position = contacts->normal * CircleA->radius + CircleA->pos;
            contacts->w_side = 0;
            return 1;
        }
        return 0;
    }

    /**
     * @brief   精确碰撞检测（圆形 vs 网格形状）
     * @details 双向检测：先根据形状的旋转角度计算圆形的四个采样方向，用 Bresenham 射线检测采样点是否穿透了形状；
     *          再遍历形状的轮廓点，用点-圆距离判断形状轮廓是否进入圆内。
     *          采样方向依赖于形状角度，确保采样点与形状的旋转姿态对齐。
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Shape 网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsCircle *Circle, PhysicsShape *Shape)
    {
        int ContactSize = 0;
        AngleMat Mat(Shape->angle);

        // ===== 第一轮：圆形采样点 → 形状的 Bresenham 射线检测 =====
        // 用形状的旋转矩阵来计算圆形四个方向的采样向量，使得采样方向与形状姿态匹配
        Vec2_ Rx = Mat.Rotary({Circle->radius, 0});                  // 旋转后的X方向
        Vec2_ Ry = Mat.Rotary({0, Circle->radius});                  // 旋转后的Y方向

        std::vector<Vec2_> Cd{
            Circle->pos + Rx,                                        // 按形状旋转方向的四个采样点
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
                contacts->separation = -abs(contacts->separation);
                contacts->friction = SQRT_(Circle->friction * info.Friction);
                contacts->position = info.pos;
                contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2) + Shape->angle);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // ===== 第二轮：形状轮廓点 → 圆形的距离检测 =====
        // 遍历形状的旋转后轮廓点，判断每个点到圆心的距离是否小于半径
        FLOAT_ L;
        Vec2_ dp;
        FLOAT_ R2 = Circle->radius * Circle->radius;
        for (size_t i = 0; i < Shape->OutlineSize; i++)
        {
            dp = Shape->pos + Mat.Rotary(Shape->OutlineSet[i]) - Circle->pos;  // 圆心指向轮廓点
            L = ModulusLength(dp);                                             // 距离平方
            if (R2 > L)                                                        // 轮廓点在圆内
            {
                L = SQRT_(L);
                contacts->separation = L - Circle->radius;                     // 穿透深度
                contacts->friction = SQRT_(Circle->friction * Shape->FrictionSet[i]);
                contacts->normal = dp / L;                                     // 法线 = 圆心指向外
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
     * @details 多阶段碰撞检测，覆盖线与形状的各种接触形态：
     *          1. 遍历形状轮廓点，用线段相交检测判断轮廓边是否穿过线；
     *          2. 用 Bresenham 射线检测线中心是否穿透形状（处理线整体陷入形状的情况）；
     *          3. 分别检测线的起点和终点是否穿透形状。
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Shape 网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsShape *Shape)
    {
        unsigned int ContactSize = 0;
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;           // 计算线段的世界坐标端点

        // 预计算线段的法向量（从旧位置投影得到的垂线方向）
        Vec2_ AB = {end.x - begin.x, end.y - begin.y};
        Vec2_ AP = {Shape->OldPos.x - begin.x, Shape->OldPos.y - begin.y};
        FLOAT_ dotProduct = Dot(AP, AB);
        FLOAT_ len = ModulusLength(AB);
        FLOAT_ t = dotProduct / len;                                  // 投影参数
        Vec2_ normal = Shape->OldPos - (begin + (AB * t));            // 旧位置到线段的垂直向量
        normal = normal / Modulus(normal);

        // ===== 第一阶段：形状轮廓边 → 线段的相交检测 =====
        // 遍历形状的每个轮廓点，检测轮廓边（旧位置→当前位置）是否与线段相交
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

        // ===== 第二阶段：线中心 → 形状的 Bresenham 检测（处理线整体穿透形状） =====
        CollisionInfoD info;
        info = Shape->PsBresenhamDetection(Line->OldPos, Line->pos);
        if (info.Collision)
        {
            contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
            // 将旧位置沿法线方向推出，避免卡在形状内部
            Line->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
            Line->OldPosUpDataBool = false;
        }

        // ===== 第三阶段：线段起点 → 形状的 Bresenham 检测 =====
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

        // ===== 第四阶段：线段终点 → 形状的 Bresenham 检测 =====
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
     * @details 基于点到线段最近点的距离检测：计算圆心到线段的最短距离点，
     *          若该距离小于圆半径则发生碰撞。法线由圆心指向最近点方向确定。
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Circle 圆形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsCircle *Circle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;            // 世界坐标端点

        // 求圆心到线段的最短距离点（垂足或端点）
        contacts->position = DropUptoLineShortesIntersect(begin, end, Circle->pos);

        // 判断最短距离是否小于圆半径
        FLOAT_ R = Modulus(contacts->position - Circle->pos);
        if (Circle->radius > R)
        {
            contacts->normal = (Circle->pos - contacts->position) / R; // 法线 = 最近点指向圆心
            contacts->separation = R - Circle->radius;                 // 穿透深度
            contacts->friction = SQRT_(Line->friction * Circle->friction);
            contacts->w_side = 0;
            return 1;
        }

        return 0;
    }

    /**
     * @brief   精确碰撞检测（线 vs 点）
     * @details 使用线段相交检测判断点的运动轨迹（旧位置→当前位置）是否穿过线。
     *          若相交则计算穿透点、法线（点到线段的垂线方向）和穿透深度，
     *          然后将点的旧位置沿法线方向推出以避免卡在线内部。
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, PhysicsParticle *Particle)
    {
        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;

        // 线段相交检测：点的运动轨迹是否穿过了线
        if (segmentIntersection(begin, end, Particle->OldPos, Particle->pos, contacts->position))
        {
            // 计算法向量：旧位置到线段的垂直投影方向
            Vec2_ AB = {end.x - begin.x, end.y - begin.y};
            Vec2_ AP = {Particle->OldPos.x - begin.x, Particle->OldPos.y - begin.y};
            FLOAT_ dotProduct = Dot(AP, AB);
            FLOAT_ len = ModulusLength(AB);
            FLOAT_ t = dotProduct / len;                                // 投影参数

            contacts->normal = Particle->OldPos - (begin + (AB * t));   // 垂线向量
            contacts->normal = contacts->normal / Modulus(contacts->normal);
            contacts->separation = -Modulus(contacts->position - Particle->pos);
            contacts->friction = SQRT_(Particle->friction * Line->friction);
            contacts->w_side = 0;

            // 将旧位置沿法线推出，防止卡在线内部
            Particle->OldPos = contacts->position - (contacts->normal * FLOAT_(0.1));
            Particle->OldPosUpDataBool = false;
            return 1;
        }

        return 0;
    }

    /**
     * @brief   精确碰撞检测（线 vs 地形）
     * @details 多阶段碰撞检测，覆盖线与地形的各种接触形态：
     *          1. 获取地形包围盒内的轻量轮廓，用地形轮廓边与线段做相交检测；
     *          2. 用 Bresenham 射线检测线中心是否穿透地形（处理线整体陷入的情况）；
     *          3. 分别检测线段的起点和终点是否穿透地形。
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact *contacts, PhysicsLine *Line, MapFormwork *Map)
    {
        if (!Map) return 0;

        unsigned int ContactSize = 0;

        Vec2_ pR = vec2angle({Line->radius, 0}, Line->angle);
        Vec2_ begin = Line->pos + pR, end = Line->pos - pR;           // 世界坐标端点

        // 预计算线段方向向量和长度，用于法线投影计算
        Vec2_ AB = {end.x - begin.x, end.y - begin.y};
        FLOAT_ len = ModulusLength(AB);

        // ===== 第一阶段：地形轮廓边 → 线段的相交检测 =====
        // 获取线段包围盒范围内的地形轻量轮廓，用地形的轮廓边与线段做相交测试
        int KD = 2;
        Vec2_ mapCentrality = Map->FMGetCentrality();
        std::vector<MapOutline> Outline = Map->FMGetLightweightOutline(
            ToInt(Line->pos.x - Line->radius) - KD,
            ToInt(Line->pos.y - Line->radius) - KD,
            ToInt(Line->pos.x + Line->radius) + KD,
            ToInt(Line->pos.y + Line->radius) + KD);

        FLOAT_ L;
        Vec2_ beginDian;
        for (auto d : Outline)
        {
            d.pos -= mapCentrality;                                   // 转换到世界坐标
            beginDian = d.pos - (d.face * FLOAT_(0.5));               // 地形轮廓边的另一端点
            if (segmentIntersection(begin, end, d.pos, beginDian, contacts->position))
            {
                // 计算法向量：从地形边端点到线段的垂线方向
                Vec2_ AP = {beginDian.x - begin.x, beginDian.y - begin.y};
                FLOAT_ dotProduct = Dot(AP, AB);
                FLOAT_ t = dotProduct / len;                          // 投影参数

                contacts->normal = beginDian - (begin + (AB * t));    // 垂线向量
                contacts->normal = contacts->normal / Modulus(contacts->normal);
                contacts->separation = -Modulus(contacts->position - d.pos);
                contacts->friction = SQRT_(d.F * Line->friction);
                contacts->w_side = ContactSize;
                ++contacts;
                ++ContactSize;
            }
        }

        // ===== 第二阶段：线中心 → 地形的 Bresenham 检测（处理线整体穿透地形） =====
        CollisionInfoD info;
        info = Map->FMBresenhamDetection(Line->OldPos, Line->pos);
        if (info.Collision)
        {
            contacts->normal = vec2angle({-1, 0}, info.Direction * (M_PI / 2));
            // 将旧位置沿法线推出，避免卡在地形内部
            Line->OldPos = info.pos - (contacts->normal * FLOAT_(0.1));
            Line->OldPosUpDataBool = false;
        }

        // ===== 第三阶段：线段起点 → 地形的 Bresenham 检测 =====
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

        // ===== 第四阶段：线段终点 → 地形的 Bresenham 检测 =====
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
