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
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{0}; }

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
        virtual CollisionInfoD FMBresenhamDetection(glm::dvec2 start, glm::dvec2 end) { return CollisionInfoD{}; }
    };

}