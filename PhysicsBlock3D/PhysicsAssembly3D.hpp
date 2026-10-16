#pragma once
#include <vector>
#include "PhysicsFormwork3D.hpp"
#include "BaseSerialization3D.hpp"

namespace PhysicsBlock3D
{

    class PhysicsWorld3D;

    /**
     * @brief 物理组装体类（3D）
     * @details 将多个不同类型、不同数量的碰撞体（shape, particle, circle, line）
     *          组装成一个整体的碰撞体。组装体内部的碰撞体之间不进行碰撞检测。
     *          支持 PhysicsJoint3D 和 PhysicsJunction3D 连接。
     *          支持 JSON 序列化与反序列化。
     *
     * @note **本类几乎完全与维度无关**（只做指针身份管理与容器操作），
     *       3D 版相对 2D 版的唯一实质变化是类型名加 3D 后缀，
     *       外加修复了 2D 的三处既有缺陷：
     *         - `Add(nullptr)` 会解引用空指针崩溃 → 3D 加空指针保护；
     *         - `RemoveChild(nullptr)` 同样有风险 → 加保护；
     *         - 反序列化后 `ResolveChildren` 若解析失败会得到空成员幽灵组装体
     *           → 3D 在全部解析失败时保持 `mChildren` 为空且清理描述符。
     */
    class PhysicsAssembly3D SerializationInherit3D_
    {
#if PhysicsBlock_Serialization3D
    public:
        SerializationVirtualFunction3D;
        PhysicsAssembly3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); }
#endif
    private:
        std::vector<PhysicsFormwork3D *> mChildren;
        PhysicsWorld3D *mWorld = nullptr;

        struct ChildDescriptor
        {
            PhysicsObjectEnum type;
            unsigned int index;
        };
        std::vector<ChildDescriptor> mChildDescriptors;

    public:
        PhysicsAssembly3D();
        ~PhysicsAssembly3D();

        /**
         * @brief 添加一个碰撞体到组装体
         * @param child 碰撞体指针（PhysicsShape3D*, PhysicsParticle3D*, PhysicsCircle3D*, PhysicsLine3D*）
         * @note 添加后会自动设置碰撞体的 assembly 指针，标记其属于本组装体。
         *       `child` 为 nullptr 时直接返回（修 2D 崩溃缺陷）。 */
        void Add(PhysicsFormwork3D *child);

        /**
         * @brief 从组装体中移除一个碰撞体
         * @param child 要移除的碰撞体指针
         * @note 移除后碰撞体的 assembly 指针会被置空 */
        void RemoveChild(PhysicsFormwork3D *child);

        /**
         * @brief 将组装体中的所有碰撞体注册到物理世界
         * @param world 物理世界指针 */
        void AddToWorld(PhysicsWorld3D *world);

        /**
         * @brief 从物理世界中移除组装体中的所有碰撞体
         * @param world 物理世界指针 */
        void RemoveFromWorld(PhysicsWorld3D *world);

        /** @brief 获取子碰撞体数量 */
        size_t Size() const { return mChildren.size(); }

        /** @brief 获取指定索引的子碰撞体 */
        PhysicsFormwork3D *operator[](size_t index) { return (index < mChildren.size()) ? mChildren[index] : nullptr; }

        /** @brief 获取所有子碰撞体的引用 */
        const std::vector<PhysicsFormwork3D *> &GetChildren() const { return mChildren; }

        /**
         * @brief 反序列化后解析子对象引用
         * @param resolveFn 解析函数，接受 (PhysicsObjectEnum, unsigned int) 返回 PhysicsFormwork3D*
         * @note 应在 PhysicsWorld3D 完成所有对象反序列化后调用此方法 */
        template <typename Resolver>
        void ResolveChildren(const Resolver &resolveFn)
        {
            mChildren.clear();
            for (const auto &desc : mChildDescriptors)
            {
                PhysicsFormwork3D *child = resolveFn(desc.type, desc.index);
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
         * @param getIndexFn 获取指针索引的函数，接受 PhysicsFormwork3D* 返回 unsigned int */
        template <typename IndexLookup>
        void BuildChildDescriptors(const IndexLookup &getIndexFn)
        {
            mChildDescriptors.clear();
            for (const auto &child : mChildren)
            {
                if (!child) continue;
                ChildDescriptor desc;
                desc.type = child->PFGetType();
                desc.index = getIndexFn(child);
                mChildDescriptors.push_back(desc);
            }
        }

        /** @brief 获取子对象描述符列表（只读） */
        const std::vector<ChildDescriptor> &GetChildDescriptors() const { return mChildDescriptors; }

        /** @brief 设置所属物理世界 */
        void SetWorld(PhysicsWorld3D *world) { mWorld = world; }

        /** @brief 获取所属物理世界 */
        PhysicsWorld3D *GetWorld() const { return mWorld; }
    };

}
