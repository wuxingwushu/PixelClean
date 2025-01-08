#pragma once
#include <glm/glm.hpp>

namespace PhysicsBlock
{

    /**
     * @brief 格子類型 */
    enum GridBlockType
    {
        Entity = 1 << 0,    // 有实体
        Collision = 1 << 1, // 碰撞
        Event = 1 << 2,     // 碰撞事件

    };

    /**
     * @brief 格子信息 */
    struct GridBlock
    {

        double FrictionFactor; // 摩擦因数

        // 格子在不同状态，有些变量是没必要的（不可能同时存在）
        union
        {
            struct
            {
                unsigned char Healthpoint; // 血量
                double mass;               // 质量
            };
            struct
            {
                unsigned char DistanceField;  // 距离场
                unsigned char DirectionField; // 方向场
            };
        };

        // 可以单独读取bit位
        union
        {
            GridBlockType type; // 类型
            struct
            {
                bool Entity : 1;    // 实体
                bool Collision : 1; // 碰撞
                bool Event : 1;     // 碰撞事件
            };
        };
    };

    // 碰撞在网格的那个边上（x正为右， y正为上）
    enum CheckDirection
    {
        Right = 0U, // 右
        Up = 1U,    // 上
        Left = 2U,  // 左
        Down = 3U,  // 下
    };

    /**
     * @brief 碰撞信息(int)
     * @note 存储碰撞检测后的结果和信息 */
    struct CollisionInfoI
    {
        bool Collision; // 是否碰撞
        glm::ivec2 pos; // 碰撞位置
    };

    /**
     * @brief 碰撞信息(double)
     * @note 存储碰撞检测后的结果和信息 */
    struct CollisionInfoD
    {
        union
        {
            unsigned char Flag; // 类型
            struct
            {
                bool Collision : 1;          // 碰撞
                unsigned char Direction : 2; // 碰撞边（存储 CheckDirection 的枚举值）
                // CheckDirection Direction : 2; // 碰撞边 /* 使用这个后赋值enum值后存储的反而是负值 */
            };
        };
        glm::dvec2 pos; // 碰撞位置
    };

    /**
     * @brief 受力分解结果 */
    struct DecompositionForce
    {
        glm::dvec2 Vartical; // 垂直
        glm::dvec2 Parallel; // 平行
    };

    struct DecompositionForceVal
    {
        double Vartical; // 垂直
        double Parallel; // 平行
    };

    /**
     * @brief 线段被矩形遮罩的数据 */
    struct SquareFocus
    {
        bool Focus;       // 是否与矩形相交
        glm::dvec2 start; // 起始位置
        glm::dvec2 end;   // 结束位置
    };

    /**
     * @brief 物理对象类型 */
    enum PhysicsObjectEnum
    {
        Null,     // 无对象
        shape,    // 形状
        particle, // 点
    };

}
