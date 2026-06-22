#pragma once
#include "BaseStruct.hpp"
#include <vector>
#include <functional>

namespace PhysicsBlock
{

    /**
     * @brief 地图轮廓结构体
     * @details 用于存储地图轮廓点的信息 */
    struct MapOutline
    {
        Vec2_ pos;
        Vec2_ face;
        FLOAT_ F;
    };

    /**
     * @brief 地图模板
     * @note 地图模板提示每个地图都应该有的函数功能 */
    class MapFormwork
    {
    public:
        PhysicsObjectEnum type = PhysicsObjectEnum::Null;

        MapFormwork() {};
        virtual ~MapFormwork() {};

        /**
         * @brief 获取对象类型
         * @return PhysicsObjectEnum 类型 */
        PhysicsObjectEnum FMGetType() { return type; }

        /**
         * @brief 获取距离场
         * @return 最小碰撞半径 */
        virtual unsigned char FMGetDistanceField() { return 0; }

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{0}; }

        /**
         * @brief 获取地图中心位置
         * @return 中心位置（用于坐标转换） */
        virtual Vec2_ FMGetCentrality() { return {0, 0}; }

        /**
         * @brief 获取网格是否有障碍物
         * @param start 网格位置
         * @return 是否碰撞（true: 碰撞） */
        virtual bool FMGetCollide(glm::ivec2 start) { return false; }

        /**
         * @brief 获取是否有障碍物
         * @param start 世界位置
         * @return 是否碰撞（true: 碰撞） */
        virtual bool FMGetCollide(Vec2_ start) { return false; }

        /**
         * @brief 获取网格(不安全)
         * @param start 网格位置
         * @warning 范围不安全，超出会报错 */
        virtual GridBlock& FMGetGridBlock(glm::ivec2 start) = 0;

        /**
         * @brief 获取轻量级轮廓
         * @param x_ 起始X坐标（世界坐标）
         * @param y_ 起始Y坐标（世界坐标）
         * @param w_ 结束X坐标（世界坐标）
         * @param h_ 结束Y坐标（世界坐标）
         * @return 轮廓点向量 */
        virtual void FMGetLightweightOutline(int x_, int y_, int w_, int h_, std::vector<MapOutline>& Outline) { }

        /**
         * @brief 获取完整轮廓
         * @param x_ 起始X坐标（世界坐标）
         * @param y_ 起始Y坐标（世界坐标）
         * @param w_ 结束X坐标（世界坐标）
         * @param h_ 结束Y坐标（世界坐标）
         * @return 轮廓点向量 */
        virtual void FMGetOutline(int x_, int y_, int w_, int h_, std::vector<MapOutline>& Outline) { }

        virtual void FMGetMinOutline(int x_, int y_, int w_, int h_, std::vector<MapOutline>& Outline) { }

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(网格位置) */
        virtual CollisionInfoI FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end) { return CollisionInfoI{}; }

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(准确位置) */
        virtual CollisionInfoD FMBresenhamDetection(Vec2_ start, Vec2_ end) { return CollisionInfoD{}; }

        virtual CollisionInfoI FMSafeBresenhamDetection(glm::ivec2 start, glm::ivec2 end) { return CollisionInfoI{}; }

        /**
         * @brief 安全地设置单个格子碰撞状态（带边界检查）
         * @param pos 网格坐标
         * @param state true=开启碰撞, false=关闭碰撞
         * @return true=状态发生了变更, false=坐标越界或状态未变
         */
        virtual bool SafeSetCollision(glm::ivec2 pos, bool state) = 0;

        /**
         * @brief 设置指定像素的摩擦系数
         * @param x 网格坐标 X
         * @param y 网格坐标 Y
         * @param friction 摩擦系数值
         */
        virtual void SetFriction(int x, int y, FLOAT_ friction) = 0;

        /**
         * @brief 获取指定像素的摩擦系数
         * @param x 网格坐标 X
         * @param y 网格坐标 Y
         * @return 摩擦系数值，如果坐标越界返回 0
         */
        virtual FLOAT_ GetFriction(int x, int y) = 0;

    public:
        /**
         * @brief 设置碰撞状态变化通知桥接器
         * @param notifier 通知回调函数（参数：网格坐标，新状态）
         * @details 由 PhysicsWorld::SetMapFormwork 注入，用于在 SafeSetCollision
         * 改变网格碰撞状态时通知 PhysicsCollision 管理器。
         */
        void SetCollisionChangeNotifier(std::function<void(glm::ivec2, bool)> notifier) {
            mCollisionChangeNotifier = std::move(notifier);
        }

        /**
         * @brief 清除碰撞状态变化通知桥接器
         */
        void ClearCollisionChangeNotifier() {
            mCollisionChangeNotifier = nullptr;
        }

    protected:
        /**
         * @brief 碰撞状态变化通知桥接器
         * @details 由 PhysicsWorld 在 SetMapFormwork 时注入，
         * 在 SafeSetCollision 改变网格状态时调用，转发到 PhysicsCollision。
         */
        std::function<void(glm::ivec2, bool)> mCollisionChangeNotifier;
    };

}