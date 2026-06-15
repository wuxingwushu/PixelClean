#include "PhysicsAssembly.hpp"
#include "PhysicsWorld.hpp"

namespace PhysicsBlock
{

    /** @brief 默认构造函数，创建一个空的组装体 */
    PhysicsAssembly::PhysicsAssembly()
    {
    }

    /** @brief 析构函数
     *  @details 先从物理世界中移除所有子碰撞体，然后释放所有子碰撞体的内存 */
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

    /** @brief 向组装体添加一个子碰撞体
     *  @details 设置子碰撞体的 assembly 指针为本组装体，并将其加入子对象列表
     *  @param child 碰撞体指针 */
    void PhysicsAssembly::Add(PhysicsFormwork *child)
    {
        child->assembly = this;
        mChildren.push_back(child);
    }

    /** @brief 从组装体中移除一个子碰撞体
     *  @details 将子碰撞体的 assembly 指针置空，使用 swap-and-pop 技巧高效移除
     *  @param child 要移除的碰撞体指针 */
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

    /** @brief 将组装体中所有子碰撞体注册到物理世界
     *  @details 遍历所有子碰撞体，根据其类型分别调用 world->AddObject() 注册
     *  @param world 物理世界指针 */
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

    /** @brief 从物理世界中移除组装体中的所有子碰撞体
     *  @details 遍历子对象列表，将每个子碰撞体的 assembly 指针置空，
     *           并调用 world->RemoveObject() 从物理世界中移除
     *  @param world 物理世界指针 */
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
    /** @brief JSON序列化
     *  @details 将子碰撞体的类型和索引信息序列化到 JSON 对象中
     *  @param data JSON 输出对象 */
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

    /** @brief JSON反序列化
     *  @details 从 JSON 对象中读取子碰撞体的类型和索引信息，
     *           存入 mChildDescriptors，后续通过 ResolveChildren() 解析为实际指针
     *  @param data JSON 输入对象 */
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
