#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 构造函数
     * @param Pos 粒子的初始位置
     * @param Mass 粒子的质量
     * @param Friction 摩擦因数
     * @details 初始化粒子的位置、质量、质量倒数和摩擦因数
     * 如果质量为 FLOAT_MAX，则质量倒数设为 0，表示粒子不可移动
     */
    PhysicsParticle::PhysicsParticle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction) : OldPos(Pos), pos(Pos), mass(Mass), invMass(1.0 / Mass), friction(Friction)
    {
        if (mass == FLOAT_MAX)
        {
            invMass = 0;
        }
    }

    /**
     * @brief 构造函数（无质量参数）
     * @param Pos 粒子的初始位置
     * @details 初始化粒子的位置，质量默认为 0
     * 此时粒子的质量倒数也为 0，表示粒子不可移动
     */
    PhysicsParticle::PhysicsParticle(Vec2_ Pos) : OldPos(Pos), pos(Pos)
    {
    }

    /**
     * @brief 析构函数
     */
    PhysicsParticle::~PhysicsParticle()
    {
    }

    /**
     * @brief 添加一个受力
     * @param Force 要添加的力向量
     * @details 如果粒子的质量倒数为 0（不可移动），则不添加力
     * 否则将力累加到当前的受力上
     */
    void PhysicsParticle::AddForce(Vec2_ Force)
    {
        if (invMass == 0)
        {
            return;
        }
        force += Force; // 累计力
    }

    /**
     * @brief 根据外力和时间更新速度
     * @param time 时间差（秒）
     * @param Ga 重力加速度向量
     * @details 如果粒子的质量倒数为 0（不可移动），则不更新速度
     * 否则根据牛顿第二定律更新速度：v = v + t * (g + F/m)
     * 最后将力重置为零向量
     */
    void inline PhysicsParticle::PhysicsSpeed(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
            return;
#if Define_MinSpoilageBool
        speed *= Define_MinSpoilage; // 应用最小损耗系数，模拟阻力
#endif
        speed += time * (Ga + invMass * force);
        force = {0, 0}; // 重置力为零
    }

    /**
     * @brief 根据速度和时间更新位置
     * @param time 时间差（秒）
     * @param Ga 重力加速度向量
     * @details 如果粒子的质量倒数为 0（不可移动），则不更新位置
     * 否则：
     * 1. 检查粒子是否静止，更新静止次数
     * 2. 如果允许更新旧位置，则保存当前位置到旧位置
     * 3. 根据速度更新位置：p = p + t * v
     * 4. 允许更新旧位置
     */
    void inline PhysicsParticle::PhysicsPos(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
            return;

        // 检查粒子是否静止
        if (OldPos == pos)
        {
            if (StaticNum < 200)
                ++StaticNum; // 增加静止次数
        }
        else
        {
            StaticNum = 0; // 重置静止次数
        }
        
        // 保存当前位置到旧位置
        if (OldPosUpDataBool)
        {
            OldPos = pos;
        }
        
        // 更新位置
        pos += time * speed;
        
        // 允许更新旧位置
        OldPosUpDataBool = true;
    }

#if PhysicsBlock_Serialization
    /**
     * @brief 从 JSON 数据构造粒子
     * @param data JSON 数据
     * @details 通过反序列化从 JSON 数据中恢复粒子的状态
     */
    PhysicsParticle::PhysicsParticle(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        JsonContrarySerialization(data);
    }

    /**
     * @brief 将粒子状态序列化为 JSON
     * @param data 输出的 JSON 数据
     * @details 将粒子的位置、旧位置、速度、力、质量和摩擦因数序列化到 JSON 中
     */
    void PhysicsParticle::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        SerializationVec2(data, pos);
        SerializationVec2(data, OldPos);
        SerializationVec2(data, speed);
        SerializationVec2(data, force);
        data["mass"] = mass;
        data["friction"] = friction;
    }

    /**
     * @brief 从 JSON 数据反序列化粒子状态
     * @param data JSON 数据
     * @details 从 JSON 数据中恢复粒子的位置、旧位置、速度、力、质量和摩擦因数
     * 并根据质量计算质量倒数
     */
    void PhysicsParticle::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        ContrarySerializationVec2(data, pos);
        ContrarySerializationVec2(data, OldPos);
        ContrarySerializationVec2(data, speed);
        ContrarySerializationVec2(data, force);
        mass = data["mass"];
        
        // 计算质量倒数
        if (mass == FLOAT_MAX)
        {
            invMass = 0;
        }
        else
        {
            invMass = FLOAT_(1) / mass;
        }
        
        friction = data["friction"];
    }
#endif

}