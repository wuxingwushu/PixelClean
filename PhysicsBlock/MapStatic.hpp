#pragma once
#include "MapFormwork.hpp"
#include "BaseGrid.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 静态地图 */
    class MapStatic : public BaseGrid, public MapFormwork
    {
    public:
        glm::dvec2 centrality{0, 0}; // 中心位置
    public:
        /**
         * @brief 构造函数
         * @param Width 地图宽度
         * @param Height 地图高度
         * @note 单位：格子 */
        MapStatic(const unsigned int Width, const unsigned int Height) : BaseGrid(Width, Height) {};
        ~MapStatic() {};

        /**
         * @brief 设置地图那个位置为中心
         * @param Centrality 中心位置
         * @note 必须在地图范围内 */
        void SetCentrality(glm::dvec2 Centrality);

        /*=========MapFormwork=========*/

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{ width, height }; }

        /**
         * @brief 获取网格是否有障碍物
         * @param start 网格位置
         * @return 是否碰撞（true: 碰撞） */
        virtual bool FMGetCollide(glm::ivec2 start) {
            if((start.x >= width) || (start.y >= width)){
                return false;
            }
            return at(start).Collision;
        }

        /**
         * @brief 获取是否有障碍物
         * @param start 世界位置
         * @return 是否碰撞（true: 碰撞） */
        virtual bool FMGetCollide(glm::dvec2 start) { return FMGetCollide(ToInt(start + centrality)); }

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(网格位置)
         * @warning 位置只可以在地图范围内 */
        virtual CollisionInfoI FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(准确位置)
         * @note 会将线段裁剪出被地图覆盖的部分 */
        virtual CollisionInfoD FMBresenhamDetection(glm::dvec2 start, glm::dvec2 end);
    };

}