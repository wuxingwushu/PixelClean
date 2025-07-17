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
        
        /**
         * @brief 构建函数
         * @param Width 宽度
         * @param Height 高度 */
        BaseOutline(const unsigned int Width, const unsigned int Height);
        ~BaseOutline();

        /**
         * @brief 更新外轮廓点集合 */
        void UpdateOutline(glm::dvec2 CentreMass_);

        /**
         * @brief 更新外轮廓点集合(轻量版) */
        void UpdateLightweightOutline(glm::dvec2 CentreMass_);

    private:
        /**
         * @brief 外骨骼处理單元
         * @param x 格子坐标x
         * @param y 格子坐标y */
        void OutlineUnit(int x, int y);

        /**
         * @brief 外骨骼处理單元(轻量版)
         * @param x 格子坐标x
         * @param y 格子坐标y */
        void LightweightOutlineUnit(int x, int y);
    };

}