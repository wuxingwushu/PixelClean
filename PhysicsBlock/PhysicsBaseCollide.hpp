#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsCircle.hpp"
#include "MapFormwork.hpp"
#include "BaseArbiter.hpp"

namespace PhysicsBlock
{
    // 网格形状 和 网格形状 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsShape* ShapeA, PhysicsShape* ShapeB);

    // 网格形状 和 点 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsShape* Shape, PhysicsParticle* Particle);

    // 网格形状 和 地形 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsShape* Shape, MapFormwork* Map);

    // 点 和 地形 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsParticle* Particle, MapFormwork* Map);

    // 圆形 和 地形 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsCircle* Circle, MapFormwork* Map);

    // 圆形 和 点 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsCircle* Circle, PhysicsParticle* Particle);

    // 圆形 和 圆形 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsCircle* CircleA, PhysicsCircle* CircleB);

    // 圆形 和 网格形状 的碰撞检测
    unsigned int Collide(Contact* contacts, PhysicsCircle* Circle, PhysicsShape* Shape);
}
