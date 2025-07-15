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
        bool NewBool = false;      // 网格是否是在內部生成的
        GridBlock *Grid;           // 网格
        /**
         * @brief 获取网格
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子
         * @warning 坐标范围不安全 */
        virtual inline GridBlock &at(int x, int y) { return at(x * height + y); }
        /**
         * @brief 获取网格
         * @param pos 坐标
         * @return 格子
         * @warning 坐标范围不安全 */
        virtual inline GridBlock &at(glm::ivec2 pos) { return at(pos.x * height + pos.y); }
        /**
         * @brief 获取网格
         * @param i 数组索引
         * @return 格子
         * @warning 范围不安全 */
        virtual inline GridBlock &at(unsigned int i) { return Grid[i]; }

        /**
         * @brief 获取是否有碰撞
         * @param x 坐标x
         * @param y 坐标y
         * @return 是否碰撞
         * @retval true 有障碍物
         * @retval false 没障碍物
         * @note 坐标范围安全
         * @note 范围外返回false */
        virtual inline bool GetCollision(unsigned int x, unsigned int y)
        {
            if ((x >= width) || (y >= height))
            {
                return false;
            }
            return at(x, y).Collision;
        }

        /**
         * @brief 获取是否有碰撞
         * @param Pos 坐标
         * @return 是否碰撞
         * @retval true 有障碍物
         * @retval false 没障碍物
         * @note 坐标范围安全
         * @note 范围外返回false */
        virtual inline bool GetCollision(glm::uvec2 Pos)
        {
            return GetCollision(Pos.x, Pos.y);
        }

    public:
        /**
         * @brief 构建函数
         * @param Width 宽度
         * @param Height 高度 */
        BaseGrid(const unsigned int Width, const unsigned int Height);
        /**
         * @brief 构建函数
         * @param Width 宽度
         * @param Height 高度
         * @param Gridptr 外部生成网格
         * @warning 网格需要自行销毁 */
        BaseGrid(const unsigned int Width, const unsigned int Height, GridBlock *Gridptr);
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