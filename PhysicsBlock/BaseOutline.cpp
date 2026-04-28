#include "BaseOutline.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 构造函数
     * @param Width 网格宽度
     * @param Height 网格高度
     * @details 1. 调用 BaseGrid 构造函数初始化网格
     * 2. 计算轮廓点集合的大小（宽度 * 高度 * 3）
     * 3. 分配轮廓点集合和摩擦力集合的内存
     */
    BaseOutline::BaseOutline(const unsigned int Width, const unsigned int Height) : BaseGrid(Width, Height)
    {
        int size = Width * Height * 3;
        if (size < 4){
            size = 4;
        }
        FrictionSet = new FLOAT_[size];
        OutlineSet = new Vec2_[size];
    }

    /**
     * @brief 析构函数
     * @details 释放轮廓点集合和摩擦力集合的内存
     */
    BaseOutline::~BaseOutline()
    {
        delete OutlineSet;
        delete FrictionSet;
    }

    /**
     * @brief 更新外轮廓点集合（完整版）
     * @param CentreMass_ 质心位置
     * @details 1. 重置轮廓点数量为 0
     * 2. 遍历所有网格，对有碰撞的格子调用轻量版轮廓单元处理
     * 3. 将所有轮廓点坐标转换为相对于质心的坐标
     */
    void BaseOutline::UpdateOutline(Vec2_ CentreMass_)
    {
        OutlineSize = 0;
        for (size_t x = 0; x < width; ++x)
        {
            for (size_t y = 0; y < height; ++y)
            {
                if (at(x, y).Collision)
                {
                    LightweightOutlineUnit(x, y);
                }
            }
        }
        // 将轮廓点坐标转换为相对于质心的坐标
        for (size_t i = 0; i < OutlineSize; i++)
        {
            OutlineSet[i] -= CentreMass_;
        }
    }

    /**
     * @brief 更新外轮廓点集合（轻量版）
     * @param CentreMass_ 质心位置
     * @details 1. 重置轮廓点数量为 0
     * 2. 遍历所有网格，对有碰撞的格子调用轻量版轮廓单元处理
     * 3. 将所有轮廓点坐标转换为相对于质心的坐标
     * @note 与完整版的区别在于使用了更严格的轮廓点判断条件
     */
    void BaseOutline::UpdateLightweightOutline(Vec2_ CentreMass_)
    {
        OutlineSize = 0;
        for (size_t x = 0; x < width; ++x)
        {
            for (size_t y = 0; y < height; ++y)
            {
                if (at(x, y).Collision)
                {
                    LightweightOutlineUnit(x, y);
                }
            }
        }
        // 将轮廓点坐标转换为相对于质心的坐标
        for (size_t i = 0; i < OutlineSize; i++)
        {
            OutlineSet[i] -= CentreMass_;
        }
    }

    /**
     * @brief 外轮廓处理单元（完整版）
     * @param x 格子坐标x
     * @param y 格子坐标y
     * @details 检查指定格子的四个角是否为轮廓点，如果是则添加到轮廓集合中
     * 完整版使用较宽松的条件，会生成更多的轮廓点
     */
    void BaseOutline::OutlineUnit(int x, int y)
    {
        // 左上角
        if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1) || !GetCollision(x - 1, y - 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 左下角
        if (!GetCollision(x, y + 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y + 1};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 右上角
        if (!(GetCollision(x + 1, y - 1) || GetCollision(x + 1, y)))
        {
            OutlineSet[OutlineSize] = Vec2_{x + 1, y};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 右下角
        if (!(GetCollision(x + 1, y) || GetCollision(x + 1, y + 1) || GetCollision(x, y + 1)))
        {
            OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
    }

    /**
     * @brief 外轮廓处理单元（轻量版）
     * @param x 格子坐标x
     * @param y 格子坐标y
     * @details 检查指定格子的四个角是否为轮廓点，如果是则添加到轮廓集合中
     * 轻量版使用较严格的条件，会生成较少的轮廓点，适用于性能要求较高的场景
     */
    void BaseOutline::LightweightOutlineUnit(int x, int y)
    {
        // 左上角
        if (!GetCollision(x - 1, y - 1))
        {
            if (GetCollision(x - 1, y) == GetCollision(x, y - 1))
            {
                OutlineSet[OutlineSize] = Vec2_{x, y};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        else if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 左下角
        if (!GetCollision(x, y + 1)) {
            if (!(GetCollision(x - 1, y) && !GetCollision(x - 1, y + 1))) {
                OutlineSet[OutlineSize] = Vec2_{x, y + 1};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右上角
        if (!GetCollision(x + 1, y)) {
            if ((!GetCollision(x, y - 1) && !GetCollision(x + 1, y - 1))) {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右下角
        if (!GetCollision(x + 1, y + 1)) {
            if ((GetCollision(x + 1, y) == GetCollision(x, y + 1)) && !GetCollision(x + 1, y))
            {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
    }

}