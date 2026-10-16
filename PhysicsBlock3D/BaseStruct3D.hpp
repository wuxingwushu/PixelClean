#pragma once
#define TranslatorLocality3D 1 // 手动选择库位置
#if TranslatorLocality3D
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#else
#include "../extern/glm/glm.hpp"
#include "../extern/glm/gtc/quaternion.hpp"
#endif

#include <cfloat>

// ── 标量宏：用 #ifndef 保护，使 2D/3D 头文件可在同一翻译单元共存 ──────────
#ifndef FLOAT_
#define FLOAT_ float
#endif
#ifndef FLOAT_MAX
#define FLOAT_MAX FLT_MAX
#endif
#ifndef SQRT_
#define SQRT_ sqrtf
#endif

// ── 3D 专有类型宏 ─────────────────────────────────────────────────────────
#define Vec3_ glm::vec3
#define Vec4_ glm::vec4
#define DVec3_ glm::dvec3
#define Mat3_ glm::mat3
#define Mat4_ glm::mat4
#define Quat_ glm::quat
#define IVec3_ glm::ivec3
#define UVec3_ glm::uvec3
#define MyImGuiDataType3D ImGuiDataType_Float

namespace PhysicsBlock3D
{

    /**
     * @brief 体素（格子）類型
     * @details 与 2D GridBlockType 语义一致，位标志可直接复用。 */
    enum GridBlockType
    {
        Entity = 1 << 0,    // 有实体
        Collision = 1 << 1, // 碰撞
        Event = 1 << 2,     // 碰撞事件
    };

    /**
     * @brief 体素信息
     * @details 由 2D GridBlock 改写：
     *          - FrictionFactor / Healthpoint / mass / DistanceField / DirectionField
     *            全部保留原语义；
     *          - DirectionField 在 3D 下仍是"方向场索引"（0~5 对应 6 个面）；
     *          - 构造函数显式全零初始化（与 2D 版同样的理由：避免 union 成员
     *            在 new GridBlock3D[n] 后是堆垃圾导致模拟不可复现）。 */
    struct GridBlock3D
    {
        FLOAT_ FrictionFactor = 0.2; // 摩擦因数

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

        /**
         * @brief 默认零初始化（约定：全零默认 + 调用方置位） */
        GridBlock3D()
        {
            FrictionFactor = 0.2;
            Healthpoint = 0;
            mass = 0;
            DistanceField = 0;
            DirectionField = 0;
            type = (GridBlockType)0;
        }
    };

    /**
     * @brief 碰撞在体素的那个面上（3D：6 个面）
     * @details 前四个值与 2D CheckDirection 完全一致（Right/Up/Left/Down），
     *          便于 2D 逻辑平移；新增 Forward/Backward 两个 Z 轴面。
     *          法向量映射（与 2D 的 vec2angle({-1,0}, Dir*π/2) 一致）：
     *            Right(0)    → (-1, 0, 0)
     *            Up(1)       → ( 0, 1, 0)
     *            Left(2)     → ( 1, 0, 0)
     *            Down(3)     → ( 0,-1, 0)
     *            Forward(4)  → ( 0, 0,-1)
     *            Backward(5) → ( 0, 0, 1)
     * @note 存储该枚举的位域必须至少 3 bit（2D 是 2 bit）。 */
    enum CheckDirection
    {
        Right = 0U,    // 右（-X 面）
        Up = 1U,       // 上（+Y 面）
        Left = 2U,     // 左（+X 面）
        Down = 3U,     // 下（-Y 面）
        Forward = 4U,  // 前（-Z 面）
        Backward = 5U, // 后（+Z 面）
    };

    /**
     * @brief 6 个面方向的单位法向量（索引即 CheckDirection） */
    inline Vec3_ DirectionNormal(unsigned int direction)
    {
        static const Vec3_ kNormals[6] = {
            Vec3_(-1.0f, 0.0f, 0.0f),
            Vec3_(0.0f, 1.0f, 0.0f),
            Vec3_(1.0f, 0.0f, 0.0f),
            Vec3_(0.0f, -1.0f, 0.0f),
            Vec3_(0.0f, 0.0f, -1.0f),
            Vec3_(0.0f, 0.0f, 1.0f),
        };
        return kNormals[direction % 6U];
    }

    /**
     * @brief 碰撞信息(int) */
    struct CollisionInfoI3D
    {
        bool Collision;   // 是否碰撞
        IVec3_ pos;       // 碰撞位置（体素坐标）
        FLOAT_ Friction;  // 摩擦因数
    };

    /**
     * @brief 碰撞信息(FLOAT_)
     * @note Direction 由 2D 的 2 bit 扩到 3 bit（6 个面）。 */
    struct CollisionInfoD3D
    {
        union
        {
            unsigned short Flag; // 类型（3D 需要 >8 bit：1 + 3 + 对齐）
            struct
            {
                bool Collision : 1;          // 碰撞
                unsigned short Direction : 3; // 碰撞面（存储 CheckDirection 的枚举值）
            };
        };
        Vec3_ pos;        // 碰撞位置（世界坐标）
        IVec3_ GridPos;   // 碰撞体素坐标
        FLOAT_ Friction;  // 摩擦因数
    };

    /**
     * @brief 受力分解结果（3D）
     * @details 2D 版是 {Vartical, Parallel} 两个 Vec2_。3D 下"垂直/平行"必须
     *          相对某个参考面而言，因此显式携带参考法向，语义更明确。 */
    struct DecompositionForce3D
    {
        Vec3_ Vartical; // 垂直（沿参考法向）
        Vec3_ Parallel; // 平行（切向分量）
        Vec3_ Normal;   // 参考法向（分解基准）
    };

    struct DecompositionForceVal3D
    {
        FLOAT_ Vartical; // 垂直分量大小
        FLOAT_ Parallel; // 平行分量大小
    };

    /**
     * @brief 线段被轴对齐盒（AABB）遮罩的数据
     * @details 2D 的 SquareFocus 是"线段被矩形裁剪"，3D 对应 BoxFocus。 */
    struct BoxFocus
    {
        bool Focus;   // 是否与盒相交
        Vec3_ start;  // 起始位置
        Vec3_ end;    // 结束位置
    };

    /**
     * @brief 物理对象类型
     * @details 与 2D 枚举一一对应（_MapStatic / _MapDynamic 保留）。 */
    enum PhysicsObjectEnum
    {
        Null,         // 无对象
        shape,        // 形状（体素块）
        particle,     // 点
        circle,       // 球
        line,         // 胶囊（线段+半径）
        _MapStatic,   // 静态地图
        _MapDynamic,  // 动态地图
    };

    // ── 3D 旋转工具 ──────────────────────────────────────────────────────
    //
    // 2D 用标量 angle + AngleMat{Cos,Sin} 做旋转；3D 用四元数。
    // 下面几个内联函数替代 2D BaseCalculate 里的 vec2angle / AngleMat。

    /**
     * @brief 用四元数旋转一个向量（2D 的 AngleMat::Rotary 对应物） */
    inline Vec3_ QuatRotate(const Quat_ &q, const Vec3_ &v)
    {
        return q * v;
    }

    /**
     * @brief 用四元数逆旋转一个向量（2D 的 AngleMat::Anticlockwise 对应物）
     * @details 把世界向量转到物体局部坐标系。 */
    inline Vec3_ QuatInverseRotate(const Quat_ &q, const Vec3_ &v)
    {
        return glm::inverse(q) * v;
    }

    /**
     * @brief 由角速度（弧度/秒）与时间步构造增量四元数
     * @param angularVelocity 世界系角速度向量
     * @param time 时间步（秒）
     * @details ω 的模是角速率，方向是转轴。0 角速度返回单位四元数。
     * @warning `glm::angleAxis(angle, axis)` 内部已经取半角
     *          （w=cos(angle/2)、xyz=axis*sin(angle/2)），因此这里必须传
     *          **完整转角** `|ω|*time`，不能再乘 0.5，否则实际旋转量只有一半。 */
    inline Quat_ QuatFromAngularVelocity(const Vec3_ &angularVelocity, FLOAT_ time)
    {
        const FLOAT_ speed = glm::length(angularVelocity);
        if (speed <= FLOAT_(1e-9) || time == FLOAT_(0))
        {
            return Quat_(1.0f, 0.0f, 0.0f, 0.0f);
        }
        const Vec3_ axis = angularVelocity / speed;
        const FLOAT_ angle = speed * time;
        return glm::angleAxis(angle, axis);
    }

    /**
     * @brief 单位四元数（2D 的 angle==0 对应物） */
    inline Quat_ IdentityQuat()
    {
        return Quat_(1.0f, 0.0f, 0.0f, 0.0f);
    }

}
