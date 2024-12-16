#pragma once
#include "BaseStruct.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 基础网格
     * @note 网格 */
    class BaseGrid
    {
    public:
        const unsigned int width;  // 宽度
        const unsigned int height; // 高度
        bool NewBool = false;
        GridBlock *Grid;// 网格
        /**
         * @brief 获取网格
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子 */
        GridBlock &at(unsigned int x, unsigned int y) { return at(x * width + y); }
        GridBlock &at(glm::ivec2 pos) { return at(pos.x * width + pos.y); }
        GridBlock &at(unsigned int i) { return Grid[i]; }

        bool GetCollision(unsigned int x, unsigned int y) {
            if (x >= width || y >= height) {
                return false;
            }
            return at(x, y).Collision;
        }

    public:
        /**
         * @brief 构建函数
         * @param Width 宽度
         * @param Height 高度 */
        BaseGrid(const unsigned int Width, const unsigned int Height);
        BaseGrid(const unsigned int Width, const unsigned int Height, GridBlock* Gridptr);
        ~BaseGrid();

        
        /**
         * @brief 线段侦测(int)
         * @param start 起始位置
         * @param end 结束位置
         * @return 碰撞结果信息 */
        CollisionInfoI BresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 线段侦测(double)
         * @param start 起始位置
         * @param end 结束位置
         * @return 碰撞结果信息(准确位置) */
        CollisionInfoD BresenhamDetection(glm::dvec2 start, glm::dvec2 end);
    };

}