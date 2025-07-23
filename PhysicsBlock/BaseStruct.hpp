#pragma once
#define TranslatorLocality 1 // 手动选择库位置
#if TranslatorLocality
#include <glm/glm.hpp>
#else
#include "../extern/glm/glm.hpp"
#endif

namespace PhysicsBlock
{

#if 0
#define FLOAT_ float
#define Vec2_ glm::vec2
#define Mat2_ glm::mat2
#define MyImGuiDataType ImGuiDataType_Float
#else
#define FLOAT_ double
#define Vec2_ glm::dvec2
#define Mat2_ glm::dmat2
#define MyImGuiDataType ImGuiDataType_Double
#endif

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

        FLOAT_ FrictionFactor; // 摩擦因数

        // 格子在不同状态，有些变量是没必要的（不可能同时存在）
        union
        {
            struct
            {
                unsigned char Healthpoint; // 血量
                FLOAT_ mass;               // 质量
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
     * @brief 碰撞信息(FLOAT_)
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
        Vec2_ pos; // 碰撞位置
    };

    /**
     * @brief 受力分解结果 */
    struct DecompositionForce
    {
        Vec2_ Vartical; // 垂直
        Vec2_ Parallel; // 平行
    };

    struct DecompositionForceVal
    {
        FLOAT_ Vartical; // 垂直
        FLOAT_ Parallel; // 平行
    };

    /**
     * @brief 线段被矩形遮罩的数据 */
    struct SquareFocus
    {
        bool Focus;       // 是否与矩形相交
        Vec2_ start; // 起始位置
        Vec2_ end;   // 结束位置
    };

    /**
     * @brief 物理对象类型 */
    enum PhysicsObjectEnum
    {
        Null,     // 无对象
        shape,    // 形状
        particle, // 点
        circle,   // 圆
    };

}
