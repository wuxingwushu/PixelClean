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
        bool NewBool = false;      // 網格是否是在內部生成的
        GridBlock *Grid;           // 网格
        /**
         * @brief 获取网格
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子
         * @warning 坐標範圍不安全 */
        virtual GridBlock &at(int x, int y) { return at(x * width + y); }
        /**
         * @brief 获取网格
         * @param pos 坐标
         * @return 格子
         * @warning 坐標範圍不安全 */
        virtual GridBlock &at(glm::ivec2 pos) { return at(pos.x * width + pos.y); }
        /**
         * @brief 获取网格
         * @param i 數組索引
         * @return 格子
         * @warning 範圍不安全 */
        virtual GridBlock &at(unsigned int i) { return Grid[i]; }

        /**
         * @brief 獲取是否有碰撞
         * @param x 坐标x
         * @param y 坐标y
         * @return 是否碰撞
         * @note 坐標範圍安全
         * @note 範圍外返回false */
        virtual bool GetCollision(unsigned int x, unsigned int y)
        {
            if ((x >= width) || (y >= height))
            {
                return false;
            }
            return at(x, y).Collision;
        }

        /**
         * @brief 獲取是否有碰撞
         * @param Pos 坐标
         * @return 是否碰撞
         * @note 坐標範圍安全
         * @note 範圍外返回false */
        virtual bool GetCollision(glm::uvec2 Pos)
        {
            if ((Pos.x >= width) || (Pos.y >= height))
            {
                return false;
            }
            return at(Pos.x, Pos.y).Collision;
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
         * @param Gridptr 外部生成網格
         * @warning 網格需要自行銷毀 */
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