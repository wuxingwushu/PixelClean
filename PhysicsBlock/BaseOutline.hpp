#pragma once
#include "BaseGrid.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 外轮廓点集合 */
    class BaseOutline : public BaseGrid
    {
    public:
        Vec2_ *OutlineSet;        // 轮廓点集合
        FLOAT_ *FrictionSet;      // 轮廓点集合
        unsigned int OutlineSize; // 轮廓点数量

        /**
         * @brief 构建函数
         * @param Width 宽度
         * @param Height 高度 */
#if PhysicsBlock_Serialization
        BaseOutline(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : BaseGrid(data)
        {
            int size = width * height * 3;
            if (size < 4)
            {
                size = 4;
            }
            FrictionSet = new FLOAT_[size];
            OutlineSet = new Vec2_[size];
            JsonContrarySerialization(data);
        }
#endif
        BaseOutline(const unsigned int Width, const unsigned int Height);
        ~BaseOutline();

        /**
         * @brief 更新外轮廓点集合 */
        void UpdateOutline(Vec2_ CentreMass_);

        /**
         * @brief 更新外轮廓点集合(轻量版) */
        void UpdateLightweightOutline(Vec2_ CentreMass_);

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