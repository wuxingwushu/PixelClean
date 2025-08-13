#pragma once
#include "BaseStruct.hpp"

namespace PhysicsBlock
{
    /**
     * @brief 地图模板
     * @note 地图模板提示每个地图都应该有的函数功能 */
    class MapFormwork
    {
    public:
        MapFormwork() {};
        ~MapFormwork() {};

        /**
         * @brief 获取对象类型
         * @return PhysicsObjectEnum 类型 */
        virtual PhysicsObjectEnum FMGetType() { return PhysicsObjectEnum::Null; }

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
    };

}