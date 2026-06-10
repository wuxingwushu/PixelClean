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
        MaxOutlineCentreMass = new Vec2_[size];
    }

    /**
     * @brief 析构函数
     * @details 释放轮廓点集合和摩擦力集合的内存
     */
    BaseOutline::~BaseOutline()
    {
        delete OutlineSet;
        delete FrictionSet;
        delete MaxOutlineCentreMass;
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
                    OutlineUnit(x, y);
                }
            }
        }
        // 将轮廓点坐标转换为相对于质心的坐标
        for (size_t i = 0; i < OutlineSize; i++)
        {
            OutlineSet[i] -= CentreMass_;
            MaxOutlineCentreMass[i] -= CentreMass_;
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
            MaxOutlineCentreMass[i] -= CentreMass_;
        }
    }

    void BaseOutline::UpdateMinOutline(Vec2_ CentreMass_)
    {
        OutlineSize = 0;
        for (size_t x = 0; x < width; ++x)
        {
            for (size_t y = 0; y < height; ++y)
            {
                if (at(x, y).Collision)
                {
                    MinOutlineUnit(x, y);
                }
            }
        }
        // 将轮廓点坐标转换为相对于质心的坐标
        for (size_t i = 0; i < OutlineSize; i++)
        {
            OutlineSet[i] -= CentreMass_;
            MaxOutlineCentreMass[i] -= CentreMass_;
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
            MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 左下角
        if (!GetCollision(x, y + 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y + 1};
            MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 右上角
        if (!(GetCollision(x + 1, y - 1) || GetCollision(x + 1, y)))
        {
            OutlineSet[OutlineSize] = Vec2_{x + 1, y};
            MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 右下角
        if (!(GetCollision(x + 1, y) || GetCollision(x + 1, y + 1) || GetCollision(x, y + 1)))
        {
            OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
            MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
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
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        else if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 左下角
        if (!GetCollision(x, y + 1)) {
            if (!(GetCollision(x - 1, y) && !GetCollision(x - 1, y + 1))) {
                OutlineSet[OutlineSize] = Vec2_{x, y + 1};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右上角
        if (!GetCollision(x + 1, y)) {
            if ((!GetCollision(x, y - 1) && !GetCollision(x + 1, y - 1))) {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右下角
        if (!GetCollision(x + 1, y + 1)) {
            if ((GetCollision(x + 1, y) == GetCollision(x, y + 1)) && !GetCollision(x + 1, y))
            {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
    }

    void BaseOutline::MinOutlineUnit(int x, int y)
    {
        // 左上角
        if (!GetCollision(x - 1, y - 1))
        {
            if (!GetCollision(x - 1, y) && !GetCollision(x, y - 1))
            {
                OutlineSet[OutlineSize] = Vec2_{x, y};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 左下角
        if (!GetCollision(x - 1, y + 1)) {
            if (!GetCollision(x - 1, y) && !GetCollision(x, y + 1)) {
                OutlineSet[OutlineSize] = Vec2_{x, y + 1};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右上角
        if (!GetCollision(x + 1, y - 1)) {
            if (!GetCollision(x, y - 1) && !GetCollision(x + 1, y)) {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右下角
        if (!GetCollision(x + 1, y + 1)) {
            if (!GetCollision(x + 1, y) && !GetCollision(x, y + 1))
            {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
                MaxOutlineCentreMass[OutlineSize] = MaxOutlineCentre(x, y);
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
    }

    /**
     * @brief 计算最大贪婪网格的中间位置
     * @param x 格子坐标x
     * @param y 格子坐标y
     * @return 最大贪婪网格的中间位置
     * @details 计算包含指定格子的最大贪婪网格的中间位置，用于更新 MaxOutlineCentreMass
     */
    Vec2_ BaseOutline::MaxOutlineCentre(int x, int y)
    {
        int best_x_min = x, best_x_max = x;
        int best_y_min = y, best_y_max = y;
        int best_area = 1;

        // 找到列 x 上包含 (x, y) 的连续垂直范围
        int y_top = y;
        while (y_top > 0 && GetCollision(x, y_top - 1))
            --y_top;
        int y_bot = y;
        while (y_bot < (int)height - 1 && GetCollision(x, y_bot + 1))
            ++y_bot;

        const int row_count = y_bot - y_top + 1;
        int* left_bounds  = new int[row_count];
        int* right_bounds = new int[row_count];

        // 预计算每一行在列 x 处的水平连续碰撞范围
        for (int r = y_top; r <= y_bot; ++r)
        {
            int idx = r - y_top;
            int l = x;
            while (l > 0 && GetCollision(l - 1, r)) --l;
            int r_ = x;
            while (r_ < (int)width - 1 && GetCollision(r_ + 1, r)) ++r_;
            left_bounds[idx]  = l;
            right_bounds[idx] = r_;
        }

        const int max_height = y_bot - y_top + 1;

        // 遍历所有 (top, bottom) 组合，求面积最大的矩形
        for (int top = y_top; top <= y; ++top)
        {
            int idx_top = top - y_top;
            int cur_left  = left_bounds[idx_top];
            int cur_right = right_bounds[idx_top];
            int cur_width = cur_right - cur_left + 1;

            // 早停1：当前 top 下，即使扩展到最底部也无法超越最优解
            if (cur_width * (y_bot - top + 1) <= best_area)
                continue;

            for (int bottom = top; bottom <= y_bot; ++bottom)
            {
                if (bottom > top)
                {
                    int idx = bottom - y_top;
                    int lb = left_bounds[idx];
                    int rb = right_bounds[idx];
                    if (lb > cur_left)  cur_left  = lb;
                    if (rb < cur_right) cur_right = rb;
                    cur_width = cur_right - cur_left + 1;
                }
                if (bottom >= y)
                {
                    int area = cur_width * (bottom - top + 1);
                    if (area > best_area)
                    {
                        best_area   = area;
                        best_x_min  = cur_left;
                        best_x_max  = cur_right;
                        best_y_min  = top;
                        best_y_max  = bottom;
                    }
                    // 早停2：当前宽度下扩展到最底部也无法超越最优解
                    if (cur_width * (y_bot - top + 1) <= best_area)
                        break;
                }
            }
        }

        delete[] left_bounds;
        delete[] right_bounds;

        return Vec2_(
            static_cast<FLOAT_>(best_x_min + best_x_max + 1) * FLOAT_(0.5),
            static_cast<FLOAT_>(best_y_min + best_y_max + 1) * FLOAT_(0.5)
        );
    }

}