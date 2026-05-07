#pragma once
#include "BaseStruct.hpp"
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>

namespace PhysicsBlock
{
    class PhysicsFormwork;

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
     * @brief 碰撞数据信息结构体
     * @details 存储一次碰撞的完整信息，包括碰撞点位置、法线方向、
     * 相对速度、摩擦系数、冲量大小以及碰撞双方的对象引用。
     *
     * @note 该结构体在两物体分离前持续有效，
     * 外部分引用该数据的回调函数不应缓存其指针。
     */
    struct CollisionData
    {
        /**
         * @brief 碰撞点位置（世界坐标）
         * @details 表示碰撞发生的准确世界坐标系位置
         */
        Vec2_ ContactPoint;

        /**
         * @brief 碰撞法线方向
         * @details 从第一个碰撞体指向第二个碰撞体的单位法向量
         */
        Vec2_ Normal;

        /**
         * @brief 相对速度向量
         * @details 两物体在碰撞点的相对线速度，包含方向和大小
         */
        Vec2_ RelativeVelocity;

        /**
         * @brief 穿透深度
         * @details 两物体在当前帧的重叠距离，正值表示发生了穿透
         */
        FLOAT_ PenetrationDepth = 0.0f;

        /**
         * @brief 法向冲量大小
         * @details 当前帧施加在碰撞点法线方向上的累积冲量大小
         */
        FLOAT_ NormalImpulse = 0.0f;

        /**
         * @brief 切向冲量大小
         * @details 当前帧施加在碰撞点切线方向上的累积冲量大小（摩擦力冲量）
         */
        FLOAT_ TangentImpulse = 0.0f;

        /**
         * @brief 摩擦系数
         * @details 碰撞双方混合后的摩擦系数
         */
        FLOAT_ Friction = 0.2f;

        /**
         * @brief 碰撞对方对象指针
         * @details 指向与此对象发生碰撞的对方 PhysicsFormwork 实例
         */
        PhysicsFormwork *Other = nullptr;

        /**
         * @brief 自身对象指针
         * @details 指向自身的 PhysicsFormwork 实例
         */
        PhysicsFormwork *Self = nullptr;
    };

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
    using CollisionCallback = std::function<void(const CollisionData &)>;

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
