#pragma once
#include "BaseStruct.hpp"
#include "BaseArbiter.hpp"
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>

namespace PhysicsBlock
{
    class PhysicsFormwork;
    class MapFormwork;

    /**
     * @brief 触发器事件类型枚举
     * @details 定义三种标准触发回调事件：
     * - Enter: 物体首次进入触发器区域
     * - Stay: 物体持续停留在触发器区域内（每帧触发）
     * - Exit: 物体离开触发器区域
     */
    enum class TriggerEventType
    {
        Enter, ///< 物体进入触发器
        Stay,  ///< 物体停留在触发器内
        Exit   ///< 物体离开触发器
    };

    /**
     * @brief 碰撞事件类型枚举
     * @details 定义三种标准碰撞回调事件：
     * - Enter: 物体首次发生碰撞
     * - Stay: 物体持续碰撞（每帧触发）
     * - Exit: 物体碰撞结束
     */
    enum class CollisionEventType
    {
        Enter, ///< 碰撞开始
        Stay,  ///< 碰撞持续
        Exit   ///< 碰撞结束
    };

    /**
     * @brief 碰撞层级掩码类型
     * @details 使用 32 位无符号整数表示碰撞层级掩码。
     * 每位代表一个层级，支持最多 32 个独立层级。
     * 物体只有在层级位掩码有交集时才能相互碰撞或触发。
     */
    using LayerMask = unsigned int;

    /**
     * @brief 所有层级都参与碰撞/触发的预设掩码
     */
    static constexpr LayerMask LayerMaskAll = 0xFFFFFFFF;

    /**
     * @brief 不参与任何碰撞/触发的预设掩码
     */
    static constexpr LayerMask LayerMaskNone = 0x00000000;

    /**
     * @brief 默认层级（第 0 位）
     */
    static constexpr LayerMask LayerMaskDefault = 1 << 0;


    /**
     * @brief 触发器边界定义结构体
     * @details 定义一个轴对齐矩形触发器区域（AABB），
     * 用于检测哪些物体进入了该区域。
     */
    struct Bounds
    {
        /**
         * @brief 中心位置（世界坐标）
         */
        Vec2_ Center;

        /**
         * @brief 半尺寸（从中心到边的距离）
         * @details 注意是半尺寸而非全尺寸，例如矩形宽为 W 则 HalfSize.x = W/2
         */
        Vec2_ HalfSize;

        /**
         * @brief 默认构造函数，创建一个零尺寸的边界
         */
        Bounds() : Center(0.0f, 0.0f), HalfSize(0.0f, 0.0f) {}

        /**
         * @brief 带参构造函数
         * @param center 中心位置
         * @param halfSize 半尺寸
         */
        Bounds(const Vec2_ &center, const Vec2_ &halfSize)
            : Center(center), HalfSize(halfSize) {}

        /**
         * @brief 检查一个点是否在边界内
         * @param point 要检查的世界坐标位置
         * @return 如果点在边界内返回 true，否则返回 false
         */
        bool Contains(const Vec2_ &point) const
        {
            return (point.x >= Center.x - HalfSize.x) &&
                   (point.x <= Center.x + HalfSize.x) &&
                   (point.y >= Center.y - HalfSize.y) &&
                   (point.y <= Center.y + HalfSize.y);
        }

        /**
         * @brief 检查另一个边界是否与此边界重叠
         * @param other 另一个边界
         * @return 如果两个边界有任何重叠则返回 true
         */
        bool Overlaps(const Bounds &other) const
        {
            return !(Center.x + HalfSize.x < other.Center.x - other.HalfSize.x ||
                     Center.x - HalfSize.x > other.Center.x + other.HalfSize.x ||
                     Center.y + HalfSize.y < other.Center.y - other.HalfSize.y ||
                     Center.y - HalfSize.y > other.Center.y + other.HalfSize.y);
        }
    };

    /**
     * @brief 触发器事件回调函数类型
     * @details 接受一个指向触发碰撞体的指针作为参数
     */
    using TriggerCallback = std::function<void(PhysicsFormwork *)>;

    /**
     * @brief 碰撞事件回调函数类型
     * @details 接受一个碰撞数据的常量引用作为参数
     */
    using CollisionCallback = std::function<void(const PhysicsFormwork *, const PhysicsFormwork *, const BaseArbiter *)>;
    using MapCollisionCallback = std::function<void(const PhysicsFormwork *, const MapFormwork *, const BaseArbiter *)>;

    /**
     * @brief 子弹/粒子命中事件的统一碰撞信息
     * @details 聚合 Contact 的精确位置和法向量，以及从世界坐标反算的网格坐标。
     * 让回调层无需再做估算或截断。所有信息在 AddCollisionPair 阶段从 arbiter->contacts[0] 构造。
     */
    struct BulletHitInfo
    {
        Vec2_ WorldPos;             ///< 碰撞点（世界坐标，子像素浮点精度；来自 Contact::position）
        glm::ivec2 GridPos;         ///< 被击中网格坐标（地图网格坐标系；由 WorldPos + centrality 计算）
        unsigned char Direction;    ///< 碰撞边方向（CheckDirection: 0=Right,1=Up,2=Left,3=Down；仅 Map 路径由法向量反推，Shape 路径不使用）
        Vec2_ Normal;               ///< 碰撞面法向量（世界坐标；来自 Contact::normal）
        FLOAT_ Friction;            ///< 组合摩擦系数（来自 Contact::friction）
        FLOAT_ Separation;          ///< 分离距离（负值=重叠；来自 Contact::separation）
    };

    /**
     * @brief 从轴对齐的法向量反推 Bresenham 碰撞边方向
     * @details 仅用于 Particle→Map 路径（法向量是轴对齐的）。
     * Shape→Particle 路径的法向量经过 Shape->angle 旋转，不能直接用此函数。
     * 映射关系（由 vec2angle({-1,0}, Direction*π/2) 确定）：
     *   Right(0) → (-1,0), Up(1) → (0,1), Left(2) → (1,0), Down(3) → (0,-1)
     */
    inline unsigned char DirectionFromNormal(Vec2_ normal) {
        float ax = std::abs(normal.x), ay = std::abs(normal.y);
        if (ax > ay) return (normal.x > 0) ? 2 : 0;
        else         return (normal.y > 0) ? 1 : 3;
    }

    /**
     * @brief 子弹/粒子命中地形时的回调
     * @param hitInfo 精确碰撞信息（含位置、法向量、网格坐标）
     * @param bullet 命中地形的子弹/粒子对象
     * @param userData 用户自定义数据指针
     * @details 游戏层在此回调中实现：
     * 1. 根据 hitInfo.GridPos 进行地形破坏（直接使用，无需加 centrality 转换）
     * 2. 根据 hitInfo.Normal 做镜面反射（反弹弹）
     * 3. 爆炸弹直接引爆
     */
    using TerrainHitCallback = std::function<void(
        const BulletHitInfo& hitInfo,    ///< 精确碰撞信息
        PhysicsFormwork* bullet,         ///< 子弹对象
        void* userData                   ///< 用户数据
    )>;

    /**
     * @brief 地图碰撞状态变化回调函数类型
     * @param pos 网格坐标
     * @param newState 新的碰撞状态
     * @param userData 用户自定义数据指针
     * @details 当 SafeSetCollision 改变网格碰撞状态时触发。
     * 游戏层在此回调中更新渲染贴图、寻路数据等。
     */
    using MapCollisionChangeCallback = std::function<void(
        glm::ivec2 pos,             // 网格坐标
        bool newState,              // 新的碰撞状态
        void* userData              // 用户数据
    )>;

    /**
     * @brief 基础异常类，用于物理引擎内部的参数验证失败
     */
    class PhysicsException : public std::runtime_error
    {
    public:
        /**
         * @brief 构造函数
         * @param message 错误描述消息
         */
        explicit PhysicsException(const std::string &message) : std::runtime_error(message) {}
    };

    /**
     * @brief 参数为空异常，当传入空指针时抛出
     */
    class ArgumentNullException : public PhysicsException
    {
    public:
        /**
         * @brief 构造函数
         * @param paramName 导致异常的参数名称
         */
        explicit ArgumentNullException(const std::string &paramName)
            : PhysicsException("Parameter '" + paramName + "' cannot be null."), ParamName(paramName) {}

        /**
         * @brief 获取导致异常的参数的名称
         * @return 参数名称字符串
         */
        const std::string &GetParamName() const { return ParamName; }

    private:
        std::string ParamName;
    };

    /**
     * @brief 参数超出范围异常，当参数值不在有效范围内时抛出
     */
    class ArgumentOutOfRangeException : public PhysicsException
    {
    public:
        /**
         * @brief 构造函数
         * @param paramName 导致异常的参数名称
         * @param message 具体的错误描述
         */
        explicit ArgumentOutOfRangeException(const std::string &paramName, const std::string &message)
            : PhysicsException("Parameter '" + paramName + "' is out of range: " + message), ParamName(paramName) {}

        /**
         * @brief 获取导致异常的参数的名称
         * @return 参数名称字符串
         */
        const std::string &GetParamName() const { return ParamName; }

    private:
        std::string ParamName;
    };

}
