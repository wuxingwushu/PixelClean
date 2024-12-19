#pragma once
#include "BaseGrid.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 外轮廓点集合 */
    class BaseOutline : public BaseGrid
    {
    public:
        glm::dvec2 *OutlineSet;   // 轮廓点集合
        unsigned int OutlineSize; // 轮廓点数量
    //public:
        /**
         * @brief 构建函数
         * @param Width 宽度
         * @param Height 高度 */
        BaseOutline(const unsigned int Width, const unsigned int Height);
        ~BaseOutline();

        /**
         * @brief 获取网格是否有碰撞
         * @param x 网格位置x
         * @param y 网格位置y
         * @return 是否有障碍物
         * @retval true 有障碍物
         * @retval false 没障碍物 */
        bool Collision(unsigned int x, unsigned int y) {
            if (x < 0 || y < 0 || x >= width || y >= height)
            {
                return false;
            }
            return at(x, y).Collision;
        }

        /**
         * @brief 更新外轮廓点集合 */
        void UpdateOutline();

    private:
        /**
         * @brief 外骨骼处理單元
         * @param x 格子坐标x
         * @param y 格子坐标y */
        void OutlineUnit(int x, int y);
    };

    
}