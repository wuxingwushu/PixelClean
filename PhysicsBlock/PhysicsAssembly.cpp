#include "PhysicsAssembly.hpp"
#include "PhysicsWorld.hpp"
#include <iostream>

namespace PhysicsBlock
{

    PhysicsAssembly::PhysicsAssembly()
    {
    }

    PhysicsAssembly::~PhysicsAssembly()
    {
        if (mWorld)
        {
            RemoveFromWorld(mWorld);
        }
        for (auto child : mChildren)
        {
            std::cout << "[PhysicsAssembly] 直接删除孤立子对象: " << child << std::endl;
            delete child;
        }
        mChildren.clear();
        std::cout << "[PhysicsAssembly] 组装体销毁完成, 地址: " << this << std::endl;
    }

    void PhysicsAssembly::Add(PhysicsFormwork *child)
    {
        child->assembly = this;
        mChildren.push_back(child);
    }

    void PhysicsAssembly::RemoveChild(PhysicsFormwork *child)
    {
        for (size_t i = 0; i < mChildren.size(); ++i)
        {
            if (mChildren[i] == child)
            {
                child->assembly = nullptr;
                mChildren[i] = mChildren.back();
                mChildren.pop_back();
                return;
            }
        }
    }

    void PhysicsAssembly::AddToWorld(PhysicsWorld *world)
    {
        mWorld = world;
        std::cout << "[PhysicsAssembly] 注册组装体到世界, 子对象数量: " << mChildren.size() << std::endl;
        for (auto child : mChildren)
        {
            switch (child->PFGetType())
            {
            case PhysicsObjectEnum::shape:
                world->AddObject(static_cast<PhysicsShape *>(child));
                break;
            case PhysicsObjectEnum::particle:
                world->AddObject(static_cast<PhysicsParticle *>(child));
                break;
            case PhysicsObjectEnum::circle:
                world->AddObject(static_cast<PhysicsCircle *>(child));
                break;
            case PhysicsObjectEnum::line:
                world->AddObject(static_cast<PhysicsLine *>(child));
                break;
            default:
                break;
            }
        }
    }

    void PhysicsAssembly::RemoveFromWorld(PhysicsWorld *world)
    {
        std::cout << "[PhysicsAssembly] 从世界移除组装体, 子对象数量: " << mChildren.size() << std::endl;
        while (!mChildren.empty())
        {
            PhysicsFormwork *child = mChildren.back();
            child->assembly = nullptr;
            mChildren.pop_back();
            world->RemoveObject(child);
        }
        mWorld = nullptr;
        std::cout << "[PhysicsAssembly] 组装体已从世界移除" << std::endl;
    }

}
