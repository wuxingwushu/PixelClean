#pragma once
#include <vector>
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    class PhysicsWorld;

    /**
     * @brief 物理组装体类
     * @details 将多个不同类型、不同数量的碰撞体（shape, particle, circle, line）
     *          组装成一个整体的碰撞体。组装体内部的碰撞体之间不进行碰撞检测。
     *          支持 PhysicsJoint 和 PhysicsJunction 连接。
     * 
     * @note 使用方法：
     *   1. 创建 PhysicsAssembly 对象
     *   2. 使用 Add() 添加各类碰撞体
     *   3. 使用 AddToWorld() 将组装体注册到物理世界
     *   4. 可以正常使用 PhysicsJoint / PhysicsJunction 连接内部的碰撞体
     */
    class PhysicsAssembly
    {
    private:
        std::vector<PhysicsFormwork *> mChildren;
        PhysicsWorld *mWorld = nullptr;

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
    };

}
