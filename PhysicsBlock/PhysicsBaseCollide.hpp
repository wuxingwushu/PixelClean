#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsCircle.hpp"
#include "PhysicsLine.hpp"
#include "MapFormwork.hpp"
#include "BaseArbiter.hpp"

namespace PhysicsBlock
{
    /**
     * @brief   AABB 粗糙碰撞检测（网格形状 vs 网格形状）
     * @details 使用轴向包围盒进行快速碰撞预检测，判断两个物体是否可能发生碰撞
     * @param   ShapeA 第一个网格形状对象
     * @param   ShapeB 第二个网格形状对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsShape* ShapeA, PhysicsShape* ShapeB);
    
    /**
     * @brief   AABB 粗糙碰撞检测（网格形状 vs 点）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Shape 网格形状对象
     * @param   Particle 点对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsShape* Shape, PhysicsParticle* Particle);

    /**
     * @brief   AABB 粗糙碰撞检测（网格形状 vs 圆形）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Shape 网格形状对象
     * @param   Circle 圆形对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsShape* Shape, PhysicsCircle* Circle);

    /**
     * @brief   AABB 粗糙碰撞检测（圆形 vs 圆形）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   CircleA 第一个圆形对象
     * @param   CircleB 第二个圆形对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsCircle* CircleA, PhysicsCircle* CircleB);

    /**
     * @brief   AABB 粗糙碰撞检测（圆形 vs 点）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Circle 圆形对象
     * @param   Particle 点对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsCircle* Circle, PhysicsParticle* Particle);

    /**
     * @brief   AABB 粗糙碰撞检测（线 vs 网格形状）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Line 线对象
     * @param   Shape 网格形状对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsLine* Line, PhysicsShape* Shape);

    /**
     * @brief   AABB 粗糙碰撞检测（线 vs 圆形）
     * @details 使用轴向包围盒进行快速碰撞预检测
     * @param   Line 线对象
     * @param   Circle 圆形对象
     * @return  如果可能碰撞返回 true，否则返回 false
     */
    bool CollideAABB(PhysicsLine* Line, PhysicsCircle* Circle);

    /**
     * @brief   精确碰撞检测（网格形状 vs 网格形状）
     * @param   contacts 碰撞信息输出数组
     * @param   ShapeA 第一个网格形状对象
     * @param   ShapeB 第二个网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsShape* ShapeA, PhysicsShape* ShapeB);

    /**
     * @brief   精确碰撞检测（网格形状 vs 点）
     * @param   contacts 碰撞信息输出数组
     * @param   Shape 网格形状对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsShape* Shape, PhysicsParticle* Particle);

    /**
     * @brief   精确碰撞检测（网格形状 vs 地形）
     * @param   contacts 碰撞信息输出数组
     * @param   Shape 网格形状对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsShape* Shape, MapFormwork* Map);

    /**
     * @brief   精确碰撞检测（点 vs 地形）
     * @param   contacts 碰撞信息输出数组
     * @param   Particle 点对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsParticle* Particle, MapFormwork* Map);

    /**
     * @brief   精确碰撞检测（圆形 vs 地形）
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsCircle* Circle, MapFormwork* Map);

    /**
     * @brief   精确碰撞检测（圆形 vs 点）
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsCircle* Circle, PhysicsParticle* Particle);

    /**
     * @brief   精确碰撞检测（圆形 vs 圆形）
     * @param   contacts 碰撞信息输出数组
     * @param   CircleA 第一个圆形对象
     * @param   CircleB 第二个圆形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsCircle* CircleA, PhysicsCircle* CircleB);

    /**
     * @brief   精确碰撞检测（圆形 vs 网格形状）
     * @param   contacts 碰撞信息输出数组
     * @param   Circle 圆形对象
     * @param   Shape 网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsCircle* Circle, PhysicsShape* Shape);

    /**
     * @brief   精确碰撞检测（线 vs 网格形状）
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Shape 网格形状对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsLine* Line, PhysicsShape* Shape);

    /**
     * @brief   精确碰撞检测（线 vs 圆）
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Circle 圆形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsLine* Line, PhysicsCircle* Circle);

    /**
     * @brief   精确碰撞检测（线 vs 点）
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Particle 点对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsLine* Line, PhysicsParticle* Particle);

    /**
     * @brief   精确碰撞检测（线 vs 地形）
     * @param   contacts 碰撞信息输出数组
     * @param   Line 线对象
     * @param   Map 地形对象
     * @return  碰撞点数量
     */
    unsigned int Collide(Contact* contacts, PhysicsLine* Line, MapFormwork* Map);

}
