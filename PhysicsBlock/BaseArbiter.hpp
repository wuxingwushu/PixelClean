#pragma once
#include "BaseStruct.hpp"
#include <functional>
#include "BaseSerialization.hpp"

namespace PhysicsBlock
{
    /**
     * @brief 碰撞点结构体
     * @details 存储两个物体碰撞时的详细信息
     */
    struct Contact
    {
        /**
         * @brief 默认构造函数
         * @details 初始化位移影响量和旋转影响量为 0
         */
        Contact() : Pn(0.0), Pt(0.0) {}

        // ========== Collide 阶段数据 ==========
        Vec2_ position;           // 碰撞点位置（世界坐标）
        Vec2_ normal;             // 最佳分离轴的法向量（从物体A指向物体B）
        FLOAT_ separation;        // 最小分离距离（负值表示重叠）
        FLOAT_ friction = 0.2;    // 两个物体间的摩擦系数
        unsigned char w_side = 0; // 小矩形边的分离标识（用于判别力的类型）

        // ========== PreStep 阶段数据 ==========
        FLOAT_ massNormal;  // 计算位移冲量的分母（有效质量的倒数）
        FLOAT_ massTangent; // 计算旋转冲量的分母（切线方向有效质量的倒数）
        FLOAT_ bias;        // 物体位置修正值（用于位置约束）

        // ========== ApplyImpulse 阶段数据 ==========
        Vec2_ r1;      // A物体质心指向碰撞点的向量
        Vec2_ r2;      // B物体质心指向碰撞点的向量
        FLOAT_ Pn = 0; // 累积的法向冲量（位移影响量大小）
        FLOAT_ Pt = 0; // 累积的切向冲量（旋转影响量大小）
    };

    /**
     * @brief 仲裁器键结构体
     * @details 用于唯一标识一对碰撞物体，保证 object1 <= object2
     */
    struct ArbiterKey
    {
        /**
         * @brief 构造函数
         * @param Object1 第一个物体指针
         * @param Object2 第二个物体指针
         * @details 自动将较小的指针赋值给 object1，较大的赋值给 object2
         */
        ArbiterKey(void *Object1, void *Object2)
        {
            if (Object1 > Object2)
            {
                object1 = Object1;
                object2 = Object2;
            }
            else
            {
                object1 = Object2;
                object2 = Object1;
            }
        }
        
        void *object1;  // 碰撞的第一个对象（指针值较大）
        void *object2;  // 碰撞的第二个对象（指针值较小）
        char PoolID;    // 对象池 ID

        /**
         * @brief 小于运算符重载
         * @param a1 要比较的 ArbiterKey
         * @return 如果 this < a1 返回 true
         * @details 用于排序和查找
         */
        inline bool operator<(const ArbiterKey &a1) const
        {
            if (a1.object1 < object1)
                return true;

            if (a1.object1 == object1 && a1.object2 < object2)
                return true;

            return false;
        }

        /**
         * @brief 等于运算符重载
         * @param a1 要比较的 ArbiterKey
         * @return 如果两个键相等返回 true
         * @details 两个键相等当且仅当它们的 object1 和 object2 都相等
         */
        inline bool operator==(const ArbiterKey &a1) const
        {
            return (a1.object1 == object1) && (a1.object2 == object2);
        }
    };

    /**
     * @brief ArbiterKey 的哈希函数
     * @details 用于在 unordered_map 中作为键时计算哈希值
     */
    struct ArbiterKeyHash
    {
        /**
         * @brief 哈希运算符重载
         * @param key 要计算哈希的 ArbiterKey
         * @return 哈希值
         * @details 使用 hash_combine 技术组合两个对象指针的哈希值
         */
        inline std::size_t operator()(const ArbiterKey &key) const
        {
            auto hash_combine = [](std::size_t a, std::size_t b)
            {
                return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
            };

            std::size_t h1 = std::hash<void *>{}(key.object1);
            std::size_t h2 = std::hash<void *>{}(key.object2);
            return hash_combine(h1, h2);
        }
    };

    /**
     * @brief 基础物理裁决器类
     * @details 负责处理两个物理对象之间的碰撞响应
     * 是所有具体裁决器的基类，定义了碰撞处理的接口
     */
    class BaseArbiter SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        /**
         * @brief 将碰撞信息序列化为 JSON
         * @param data 输出的 JSON 对象
         * @details 将所有碰撞点信息序列化到 JSON 中
         */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            auto& dataContacts = data["contacts"];
            dataContacts = dataContacts.array();
            for (size_t i = 0; i < numContacts; ++i)
            {
                SerializationVec2(dataContacts[i], contacts[i].position);
                SerializationVec2(dataContacts[i], contacts[i].normal);
                dataContacts[i]["separation"] = contacts[i].separation;
                dataContacts[i]["friction"] = contacts[i].friction;
                dataContacts[i]["w_side"] = contacts[i].w_side;
                dataContacts[i]["Pn"] = contacts[i].Pn;
                dataContacts[i]["Pt"] = contacts[i].Pt;
            }
        }

        /**
         * @brief 从 JSON 反序列化碰撞信息
         * @param data 输入的 JSON 对象
         * @details 从 JSON 中恢复碰撞点信息
         */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            numContacts = data["contacts"].size();
            auto dataContacts = data["contacts"];
            for (size_t i = 0; i < numContacts; ++i)
            {
                ContrarySerializationVec2(dataContacts[i], contacts[i].position);
                ContrarySerializationVec2(dataContacts[i], contacts[i].normal);
                contacts[i].separation = dataContacts[i]["separation"];
                contacts[i].friction = dataContacts[i]["friction"];
                contacts[i].w_side = dataContacts[i]["w_side"];
                contacts[i].Pn = dataContacts[i]["Pn"];
                contacts[i].Pt = dataContacts[i]["Pt"];
            }
        }

        /**
         * @brief 获取裁决器类型
         * @return 裁决器类型标识，默认返回 0
         */
        virtual unsigned int GetArbiterType() { return 0; }
#endif

    public:
        Contact contacts[20]; // 碰撞点集合，最多支持 20 个碰撞点
        int numContacts = 0;  // 当前碰撞点数量

        ArbiterKey key; // 两个碰撞对象的标识符

    public:
        /**
         * @brief 构造函数
         * @param Object1 第一个碰撞对象
         * @param Object2 第二个碰撞对象
         * @details 创建一个裁决器，用于处理两个对象之间的碰撞
         */
        BaseArbiter(void *Object1, void *Object2) : key(Object1, Object2) {};
        
        /**
         * @brief 析构函数
         */
        ~BaseArbiter() {};

        /**
         * @brief 计算两个物体的碰撞点
         * @details 纯虚函数，由派生类实现具体的碰撞检测算法
         */
        virtual void ComputeCollide() = 0;

        /**
         * @brief 更新碰撞信息
         * @param NewContacts 新的碰撞点数组
         * @param numNewContacts 新碰撞点数量
         * @details 纯虚函数，由派生类实现碰撞点更新逻辑
         */
        virtual void Update(Contact *NewContacts, int numNewContacts) = 0;
        
        /**
         * @brief 预处理（带时间步长）
         * @param inv_dt 时间步长的倒数
         * @details 纯虚函数，在应用冲量前计算必要的物理参数
         */
        virtual void PreStep(FLOAT_ inv_dt) = 0;
        
        /**
         * @brief 预处理（无参数版本）
         * @details 纯虚函数，不带时间步长的预处理版本
         */
        virtual void PreStep() = 0;
        
        /**
         * @brief 应用冲量
         * @details 纯虚函数，将计算得到的冲量应用到两个碰撞物体上
         */
        virtual void ApplyImpulse() = 0;
    };

}
