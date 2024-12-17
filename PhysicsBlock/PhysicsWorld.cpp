#include "PhysicsWorld.hpp"
#include <algorithm>
#include "BaseCalculate.hpp"
#include <map>
#include <iostream>

namespace PhysicsBlock
{

    PhysicsWorld::PhysicsWorld(glm::dvec2 gravityAcceleration, const bool wind) : GravityAcceleration(gravityAcceleration), WindBool(wind)
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
    }

    void PhysicsWorld::PhysicsEmulator(double time)
    {
        // 根据 Y 轴重力加速度 正负进行排序
        if (GravityAcceleration.y < 0)
        {
            std::sort(PhysicsShapeS.begin(), PhysicsShapeS.end(),
                      [](PhysicsShape *a, PhysicsShape *b)
                      {
                          return a->pos.y > b->pos.y;
                      });
            std::sort(PhysicsParticleS.begin(), PhysicsParticleS.end(),
                      [](PhysicsParticle *a, PhysicsParticle *b)
                      {
                          return a->pos.y > b->pos.y;
                      });
        }
        else if (GravityAcceleration.y > 0)
        {
            std::sort(PhysicsShapeS.begin(), PhysicsShapeS.end(),
                      [](PhysicsShape *a, PhysicsShape *b)
                      {
                          return a->pos.y < b->pos.y;
                      });
            std::sort(PhysicsParticleS.begin(), PhysicsParticleS.end(),
                      [](PhysicsParticle *a, PhysicsParticle *b)
                      {
                          return a->pos.y < b->pos.y;
                      });
        }

        std::map<PhysicsShape*, std::vector<PhysicsShape*>> PressureSource;// 压力源

        if (PhysicsShapeS.size() > 1)
        {
            PhysicsShape *JS_PhysicsParticle; // 被解算受力的对象
            PhysicsShape *DX_PhysicsParticle; // 和谁受力解算
            // 开始逐一解算 Y 轴受力
            for (size_t i = 1; i < PhysicsShapeS.size(); ++i)
            {
                JS_PhysicsParticle = PhysicsShapeS[i]; // 获取被解算对象
                // 向上遍历对象
                for (int j = i - 1; j >= 0; --j)
                {
                    DX_PhysicsParticle = PhysicsShapeS[j];
                    double JL = DX_PhysicsParticle->pos.y - JS_PhysicsParticle->pos.y;
                    if (JL > PhysicsCollisionMaxRadius)
                    {
                        break; // 超过最大检测范围跳出
                    }
                    if (JL < (DX_PhysicsParticle->CollisionR + JS_PhysicsParticle->CollisionR))
                    {
                        glm::dvec2 ForceCollisionDrop{0, 0}; // 累加碰撞点的位置
                        unsigned int DropSize = 0;           // 碰撞点的数量
                        for (size_t k = 0; k < DX_PhysicsParticle->OutlineSize; ++k)
                        {
                            glm::dvec2 OutlineDrop = vec2angle(DX_PhysicsParticle->OutlineSet[k] - DX_PhysicsParticle->CentreMass, DX_PhysicsParticle->angle);
                            OutlineDrop += DX_PhysicsParticle->pos;
                            CollisionInfoI info = JS_PhysicsParticle->DropCollision(OutlineDrop);
                            if (info.Collision)
                            {
                                ForceCollisionDrop += OutlineDrop;
                                ++DropSize;
                            }
                        }
                        for (size_t k = 0; k < JS_PhysicsParticle->OutlineSize; ++k)
                        {
                            glm::dvec2 OutlineDrop = vec2angle(JS_PhysicsParticle->OutlineSet[k] - JS_PhysicsParticle->CentreMass, JS_PhysicsParticle->angle);
                            OutlineDrop += JS_PhysicsParticle->pos;
                            CollisionInfoI info = DX_PhysicsParticle->DropCollision(OutlineDrop);
                            if (info.Collision)
                            {
                                ForceCollisionDrop += OutlineDrop;
                                ++DropSize;
                            }
                        }
                        if (DropSize > 0)
                        {
                            PressureSource[JS_PhysicsParticle].push_back(DX_PhysicsParticle);
                            ForceCollisionDrop /= DropSize;
                            JS_PhysicsParticle->AddForce(ForceCollisionDrop, DX_PhysicsParticle->force);
                        }
                    }
                }
            }
        }
        
        // 谁碰撞到不可移动的就按照压力链，一路反馈上去
        PhysicsShape *PhysicsParticlePtr = PhysicsShapeS.back();
        while (PressureSource[PhysicsParticlePtr].size() > 0)
        {
            std::cout << PressureSource[PhysicsParticlePtr][0]->force.y << std::endl;
            PhysicsParticlePtr = PressureSource[PhysicsParticlePtr][0];
        }
        

        if (PhysicsParticleS.size() > 1)
        {
        }
    }

    void PhysicsWorld::SetMapFormwork(MapFormwork *MapFormwork_)
    {
        wMapFormwork = MapFormwork_;
        if (!WindBool)
        {
            return;
        }
        GridWindSize = wMapFormwork->FMGetMapSize();
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
        GridWind = new glm::dvec2[GridWindSize.x * GridWindSize.y];
        for (size_t i = 0; i < (GridWindSize.x * GridWindSize.y); i++)
        {
            GridWind[i] = glm::dvec2{0};
        }
    }

}
