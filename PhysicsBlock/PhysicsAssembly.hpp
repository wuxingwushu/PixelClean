#pragma once
#include <vector>
#include "PhysicsFormwork.hpp"
#include "BaseSerialization.hpp"

namespace PhysicsBlock
{

    class PhysicsWorld;

    /**
     * @brief 物理组装体类
     * @details 将多个不同类型、不同数量的碰撞体（shape, particle, circle, line）
     *          组装成一个整体的碰撞体。组装体内部的碰撞体之间不进行碰撞检测。
     *          支持 PhysicsJoint 和 PhysicsJunction 连接。
     *          支持 JSON 序列化与反序列化。
     * 
     * @note 使用方法：
     *   1. 创建 PhysicsAssembly 对象
     *   2. 使用 Add() 添加各类碰撞体
     *   3. 使用 AddToWorld() 将组装体注册到物理世界
     *   4. 可以正常使用 PhysicsJoint / PhysicsJunction 连接内部的碰撞体
     */
    class PhysicsAssembly SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        SerializationVirtualFunction;
        PhysicsAssembly(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); }
#endif
    private:
        std::vector<PhysicsFormwork *> mChildren;
        PhysicsWorld *mWorld = nullptr;

        struct ChildDescriptor
        {
            PhysicsObjectEnum type;
            unsigned int index;
        };
        std::vector<ChildDescriptor> mChildDescriptors;

    public:
        PhysicsAssembly();
        ~PhysicsAssembly();

        /**
         * @brief 添加一个碰撞体到组装体
         * @param child 碰撞体指针（PhysicsShape*, PhysicsParticle*, PhysicsCircle*, PhysicsLine*）
         * @note 添加后会自动设置碰撞体的 assembly 指针，标记其属于本组装体 */
        void Add(PhysicsFormwork *child);

        /**
         * @brief 从组装体中移除一个碰撞体
         * @param child 要移除的碰撞体指针
         * @note 移除后碰撞体的 assembly 指针会被置空 */
        void RemoveChild(PhysicsFormwork *child);

        /**
         * @brief 将组装体中的所有碰撞体注册到物理世界
         * @param world 物理世界指针
         * @note 会调用 world->AddObject() 分别注册每个碰撞体 */
        void AddToWorld(PhysicsWorld *world);

        /**
         * @brief 从物理世界中移除组装体中的所有碰撞体
         * @param world 物理世界指针
         * @note 会调用 world->RemoveObject() 分别移除每个碰撞体 */
        void RemoveFromWorld(PhysicsWorld *world);

        /**
         * @brief 获取子碰撞体数量
         * @return 子碰撞体数量 */
        size_t Size() const { return mChildren.size(); }

        /**
         * @brief 获取指定索引的子碰撞体
         * @param index 索引
         * @return 碰撞体指针 */
        PhysicsFormwork *operator[](size_t index) { return mChildren[index]; }

        /**
         * @brief 获取所有子碰撞体的引用
         * @return 子碰撞体列表 */
        const std::vector<PhysicsFormwork *> &GetChildren() const { return mChildren; }

        /**
         * @brief 反序列化后解析子对象引用
         * @param resolveFn 解析函数，接受 (PhysicsObjectEnum, unsigned int) 返回 PhysicsFormwork*
         * @note 应在 PhysicsWorld 完成所有对象反序列化后调用此方法
         */
        template <typename Resolver>
        void ResolveChildren(const Resolver &resolveFn)
        {
            mChildren.clear();
            for (const auto &desc : mChildDescriptors)
            {
                PhysicsFormwork *child = resolveFn(desc.type, desc.index);
                if (child)
                {
                    child->assembly = this;
                    mChildren.push_back(child);
                }
            }
            mChildDescriptors.clear();
        }

        /**
         * @brief 获取子对象描述符（用于序列化时索引计算）
         * @param getIndexFn 获取指针索引的函数，接受 PhysicsFormwork* 返回 unsigned int
         */
        template <typename IndexLookup>
        void BuildChildDescriptors(const IndexLookup &getIndexFn)
        {
            mChildDescriptors.clear();
            for (const auto &child : mChildren)
            {
                ChildDescriptor desc;
                desc.type = child->PFGetType();
                desc.index = getIndexFn(child);
                mChildDescriptors.push_back(desc);
            }
        }

        /**
         * @brief 获取子对象描述符列表（只读）
         * @return 描述符常量引用 */
        const std::vector<ChildDescriptor> &GetChildDescriptors() const { return mChildDescriptors; }

        /**
         * @brief 设置所属物理世界
         * @param world 物理世界指针 */
        void SetWorld(PhysicsWorld *world) { mWorld = world; }

        /**
         * @brief 获取所属物理世界
         * @return 物理世界指针 */
        PhysicsWorld *GetWorld() const { return mWorld; }
    };

}
