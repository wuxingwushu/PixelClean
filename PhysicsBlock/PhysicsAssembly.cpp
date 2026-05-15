#include "PhysicsAssembly.hpp"
#include "PhysicsWorld.hpp"

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
            delete child;
        }
        mChildren.clear();
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
        while (!mChildren.empty())
        {
            PhysicsFormwork *child = mChildren.back();
            child->assembly = nullptr;
            mChildren.pop_back();
            world->RemoveObject(child);
        }
        mWorld = nullptr;
    }

#if PhysicsBlock_Serialization
    void PhysicsAssembly::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        data["childCount"] = mChildren.size();
        nlohmann::json_abi_v3_12_0::basic_json<> &childArray = data["children"];
        childArray = childArray.array();
        for (size_t i = 0; i < mChildDescriptors.size(); ++i)
        {
            childArray[i]["type"] = mChildDescriptors[i].type;
            childArray[i]["childIndex"] = mChildDescriptors[i].index;
        }
    }

    void PhysicsAssembly::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        mChildDescriptors.clear();
        if (data.find("children") != data.end())
        {
            mChildDescriptors.reserve(data["children"].size());
            for (size_t i = 0; i < data["children"].size(); ++i)
            {
                ChildDescriptor desc;
                desc.type = static_cast<PhysicsObjectEnum>(static_cast<int>(data["children"][i]["type"]));
                desc.index = data["children"][i]["childIndex"];
                mChildDescriptors.push_back(desc);
            }
        }
    }
#endif

}
