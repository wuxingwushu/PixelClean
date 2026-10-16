#include "PhysicsAssembly3D.hpp"
#include "PhysicsWorld3D.hpp"

namespace PhysicsBlock3D
{

    /** @brief 默认构造函数，创建一个空的组装体 */
    PhysicsAssembly3D::PhysicsAssembly3D()
    {
    }

    /** @brief 析构函数
     *  @details 先从物理世界中移除所有子碰撞体，然后释放所有子碰撞体的内存 */
    PhysicsAssembly3D::~PhysicsAssembly3D()
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

    /** @brief 向组装体添加一个子碰撞体
     *  @param child 碰撞体指针
     *  @note `child` 为空时直接返回（修 2D 的 `Add(nullptr)` 崩溃缺陷） */
    void PhysicsAssembly3D::Add(PhysicsFormwork3D *child)
    {
        if (!child) return;
        child->assembly = this;
        mChildren.push_back(child);
    }

    /** @brief 从组装体中移除一个子碰撞体
     *  @param child 要移除的碰撞体指针 */
    void PhysicsAssembly3D::RemoveChild(PhysicsFormwork3D *child)
    {
        if (!child) return;
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

    /** @brief 将组装体中所有子碰撞体注册到物理世界
     *  @param world 物理世界指针 */
    void PhysicsAssembly3D::AddToWorld(PhysicsWorld3D *world)
    {
        mWorld = world;
        if (!world) return;
        for (auto child : mChildren)
        {
            if (!child) continue;
            switch (child->PFGetType())
            {
            case PhysicsObjectEnum::shape:
                world->AddObject(static_cast<PhysicsShape3D *>(child));
                break;
            case PhysicsObjectEnum::particle:
                world->AddObject(static_cast<PhysicsParticle3D *>(child));
                break;
            case PhysicsObjectEnum::circle:
                world->AddObject(static_cast<PhysicsCircle3D *>(child));
                break;
            case PhysicsObjectEnum::line:
                world->AddObject(static_cast<PhysicsLine3D *>(child));
                break;
            default:
                break;
            }
        }
    }

    /** @brief 从物理世界中移除组装体中的所有子碰撞体
     *  @param world 物理世界指针 */
    void PhysicsAssembly3D::RemoveFromWorld(PhysicsWorld3D *world)
    {
        if (!world) return;
        while (!mChildren.empty())
        {
            PhysicsFormwork3D *child = mChildren.back();
            mChildren.pop_back();
            if (!child) continue;
            child->assembly = nullptr;
            world->RemoveObject(child);
        }
        mWorld = nullptr;
    }

#if PhysicsBlock_Serialization3D
    /** @brief JSON序列化 */
    void PhysicsAssembly3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
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

    /** @brief JSON反序列化 */
    void PhysicsAssembly3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
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
