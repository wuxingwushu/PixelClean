#pragma once
#include "BaseStruct3D.hpp"
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>

namespace PhysicsBlock3D
{
    class PhysicsFormwork3D;
    class MapFormwork3D;
    class BaseArbiter3D;

    /**
     * @brief 触发器事件类型枚举 */
    enum class TriggerEventType
    {
        Enter, ///< 物体进入触发器
        Stay,  ///< 物体停留在触发器内
        Exit   ///< 物体离开触发器
    };

    /**
     * @brief 碰撞事件类型枚举 */
    enum class CollisionEventType
    {
        Enter, ///< 碰撞开始
        Stay,  ///< 碰撞持续
        Exit   ///< 碰撞结束
    };

    /**
     * @brief 碰撞层级掩码类型 */
    using LayerMask = unsigned int;

    static constexpr LayerMask LayerMaskAll = 0xFFFFFFFF;
    static constexpr LayerMask LayerMaskNone = 0x00000000;
    static constexpr LayerMask LayerMaskDefault = 1 << 0;

    /**
     * @brief 触发器/包围盒定义（3D AABB）
     * @details 由 2D Bounds 改写：Vec2_ → Vec3_，Contains/Overlaps 补 Z 轴。 */
    struct Bounds3D
    {
        Vec3_ Center;   ///< 中心位置（世界坐标）
        Vec3_ HalfSize; ///< 半尺寸（从中心到边的距离）

        Bounds3D() : Center(0.0f, 0.0f, 0.0f), HalfSize(0.0f, 0.0f, 0.0f) {}

        Bounds3D(const Vec3_ &center, const Vec3_ &halfSize)
            : Center(center), HalfSize(halfSize) {}

        /**
         * @brief 检查一个点是否在盒内 */
        bool Contains(const Vec3_ &point) const
        {
            return (point.x >= Center.x - HalfSize.x) &&
                   (point.x <= Center.x + HalfSize.x) &&
                   (point.y >= Center.y - HalfSize.y) &&
                   (point.y <= Center.y + HalfSize.y) &&
                   (point.z >= Center.z - HalfSize.z) &&
                   (point.z <= Center.z + HalfSize.z);
        }

        /**
         * @brief 检查另一个 AABB 是否与此 AABB 重叠 */
        bool Overlaps(const Bounds3D &other) const
        {
            return !(Center.x + HalfSize.x < other.Center.x - other.HalfSize.x ||
                     Center.x - HalfSize.x > other.Center.x + other.HalfSize.x ||
                     Center.y + HalfSize.y < other.Center.y - other.HalfSize.y ||
                     Center.y - HalfSize.y > other.Center.y + other.HalfSize.y ||
                     Center.z + HalfSize.z < other.Center.z - other.HalfSize.z ||
                     Center.z - HalfSize.z > other.Center.z + other.HalfSize.z);
        }

        /**
         * @brief 求包围盒中心到某点的最近点（用于最近点式碰撞检测） */
        Vec3_ ClosestPoint(const Vec3_ &point) const
        {
            return Vec3_(
                point.x < Center.x - HalfSize.x ? Center.x - HalfSize.x
                                                : (point.x > Center.x + HalfSize.x ? Center.x + HalfSize.x : point.x),
                point.y < Center.y - HalfSize.y ? Center.y - HalfSize.y
                                                : (point.y > Center.y + HalfSize.y ? Center.y + HalfSize.y : point.y),
                point.z < Center.z - HalfSize.z ? Center.z - HalfSize.z
                                                : (point.z > Center.z + HalfSize.z ? Center.z + HalfSize.z : point.z));
        }
    };

    /**
     * @brief 触发器事件回调函数类型 */
    using TriggerCallback = std::function<void(PhysicsFormwork3D *)>;

    /**
     * @brief 碰撞事件回调函数类型 */
    using CollisionCallback = std::function<void(const PhysicsFormwork3D *, const PhysicsFormwork3D *, const BaseArbiter3D *)>;
    using MapCollisionCallback = std::function<void(const PhysicsFormwork3D *, const MapFormwork3D *, const BaseArbiter3D *)>;

    /**
     * @brief 子弹/粒子命中事件的统一碰撞信息（3D）
     * @details 由 2D BulletHitInfo 改写：WorldPos/Normal 升 3D，GridPos 升 ivec3，
     *          Direction 扩到 3 bit（6 面）。 */
    struct BulletHitInfo3D
    {
        Vec3_ WorldPos;           ///< 碰撞点（世界坐标，来自 Contact3D::position）
        IVec3_ GridPos;           ///< 被击中体素坐标
        unsigned char Direction;  ///< 碰撞面方向（CheckDirection 0..5）
        Vec3_ Normal;             ///< 碰撞面法向量（世界坐标）
        FLOAT_ Friction;          ///< 组合摩擦系数
        FLOAT_ Separation;        ///< 分离距离（负值=重叠）
    };

    /**
     * @brief 从法向量反推体素碰撞面方向（3D：6 面）
     * @details 2D 版比较 |x| 与 |y| 二选一；3D 取绝对值最大的分量轴，
     *          再按该分量符号决定正/负面。
     *          映射关系与 DirectionNormal() 一致：
     *            Right(0)→(-1,0,0) Up(1)→(0,1,0) Left(2)→(1,0,0)
     *            Down(3)→(0,-1,0)  Forward(4)→(0,0,-1) Backward(5)→(0,0,1) */
    inline unsigned char DirectionFromNormal(Vec3_ normal)
    {
        const float ax = std::abs(normal.x);
        const float ay = std::abs(normal.y);
        const float az = std::abs(normal.z);
        if (ax >= ay && ax >= az)
        {
            return (normal.x > 0.0f) ? 2u : 0u; // Left : Right
        }
        if (ay >= az)
        {
            return (normal.y > 0.0f) ? 1u : 3u; // Up : Down
        }
        return (normal.z > 0.0f) ? 5u : 4u; // Backward : Forward
    }

    /**
     * @brief 子弹/粒子命中地形时的回调（3D） */
    using TerrainHitCallback = std::function<void(
        const BulletHitInfo3D &hitInfo,
        PhysicsFormwork3D *bullet,
        void *userData)>;

    /**
     * @brief 地图碰撞状态变化回调函数类型（3D） */
    using MapCollisionChangeCallback = std::function<void(
        IVec3_ pos,
        bool newState,
        void *userData)>;

    /**
     * @brief 基础异常类 */
    class PhysicsException : public std::runtime_error
    {
    public:
        explicit PhysicsException(const std::string &message) : std::runtime_error(message) {}
    };

    /**
     * @brief 参数为空异常 */
    class ArgumentNullException : public PhysicsException
    {
    public:
        explicit ArgumentNullException(const std::string &paramName)
            : PhysicsException("Parameter '" + paramName + "' cannot be null."), ParamName(paramName) {}

        const std::string &GetParamName() const { return ParamName; }

    private:
        std::string ParamName;
    };

    /**
     * @brief 参数超出范围异常 */
    class ArgumentOutOfRangeException : public PhysicsException
    {
    public:
        explicit ArgumentOutOfRangeException(const std::string &paramName, const std::string &message)
            : PhysicsException("Parameter '" + paramName + "' is out of range: " + message), ParamName(paramName) {}

        const std::string &GetParamName() const { return ParamName; }

    private:
        std::string ParamName;
    };

}
