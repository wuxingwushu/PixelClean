#include "MapStatic.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 设置地图的中心位置
     * @param Centrality 中心位置
     * @details 1. 检查中心位置是否在地图范围内
     * 2. 如果在范围内，设置中心位置；否则不做任何操作
     */
    void MapStatic::SetCentrality(Vec2_ Centrality)
    {
        // 限定点在范围内
        if ((Centrality.x < 0) || (Centrality.y < 0) || (Centrality.x > width) || (Centrality.y > height))
        {
            return;
        }
        centrality = Centrality;
    }

    /**
     * @brief 地图线段碰撞检测（整数坐标）
     * @param start 起始位置（网格坐标）
     * @param end 结束位置（网格坐标）
     * @return 第一个碰撞的位置（网格坐标）
     * @details 调用 BaseGrid::BresenhamDetection 进行线段碰撞检测
     */
    CollisionInfoI MapStatic::FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end)
    {
        return BresenhamDetection(start, end);
    }

    /**
     * @brief 地图线段碰撞检测（浮点数坐标）
     * @param start 起始位置（世界坐标）
     * @param end 结束位置（世界坐标）
     * @return 第一个碰撞的位置（准确位置）
     * @details 1. 将世界坐标转换为网格坐标（加上中心偏移）
     * 2. 裁剪线段，使其在地图范围内
     * 3. 调用 BresenhamDetection 进行线段碰撞检测
     * 4. 如果碰撞，将碰撞点转换回世界坐标
     * 5. 返回碰撞结果
     */
    CollisionInfoD MapStatic::FMBresenhamDetection(Vec2_ start, Vec2_ end)
    {
        // 偏移中心位置，对齐网格坐标系
        start += centrality;
        end += centrality;

        // 裁剪线段，让线段都在矩形内
        PhysicsBlock::SquareFocus data = PhysicsBlock::LineSquareFocus(start, end, width, height);
        if (data.Focus)
        {
            // 线段碰撞检测
            CollisionInfoD info = BresenhamDetection(data.start, data.end);
            if (info.Collision)
            {
                // 返回物理坐标系
                info.pos -= centrality;
                return info;
            }
        }
        return {false};
    }

    /**
     * @brief 获取轻量级轮廓
     * @param x_ 起始X坐标（世界坐标）
     * @param y_ 起始Y坐标（世界坐标）
     * @param w_ 结束X坐标（世界坐标）
     * @param h_ 结束Y坐标（世界坐标）
     * @return 轮廓点向量
     * @details 1. 将世界坐标转换为网格坐标（加上中心偏移）
     * 2. 遍历指定区域内的每个格子
     * 3. 如果格子有碰撞，则检查其四个角是否为轮廓点
     * 4. 返回轮廓点向量
     * @note 轻量级轮廓，只返回部分关键点，减少计算量
     */
    std::vector<MapOutline> MapStatic::GetLightweightOutline(int x_, int y_, int w_, int h_)
    {
        x_ += centrality.x;
        y_ += centrality.y;
        w_ += centrality.x;
        h_ += centrality.y;

        std::vector<MapOutline> Outline;

        for (int x = x_; x < w_; ++x)
        {
            for (int y = y_; y < h_; ++y)
            {
                if (!GetCollision(x, y))
                {
                    continue;
                }
                // 左上角
                if (!GetCollision(x - 1, y - 1))
                {
                    if (GetCollision(x - 1, y) == GetCollision(x, y - 1))
                    {
                        Outline.push_back({Vec2_{x, y}, Vec2_{-1, -1}, at(x, y).FrictionFactor});
                    }
                }
                else if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1))
                {
                    Outline.push_back({Vec2_{x, y}, Vec2_{-1, -1}, at(x, y).FrictionFactor});
                }
                // 左下角
                if (!GetCollision(x, y + 1))
                {
                    if (!(GetCollision(x - 1, y) && !GetCollision(x - 1, y + 1)))
                    {
                        Outline.push_back({Vec2_{x, y + 1}, Vec2_{-1, 1}, at(x, y).FrictionFactor});
                    }
                }
                // 右上角
                if (!GetCollision(x + 1, y))
                {
                    if ((!GetCollision(x, y - 1) && !GetCollision(x + 1, y - 1)))
                    {
                        Outline.push_back({Vec2_{x + 1, y}, Vec2_{1, -1}, at(x, y).FrictionFactor});
                    }
                }
                // 右下角
                if (!GetCollision(x + 1, y + 1))
                {
                    if ((GetCollision(x + 1, y) == GetCollision(x, y + 1)) && !GetCollision(x + 1, y))
                    {
                        Outline.push_back({Vec2_{x + 1, y + 1}, Vec2_{1, 1}, at(x, y).FrictionFactor});
                    }
                }
            }
        }
        return Outline;
    }

    /**
     * @brief 获取完整轮廓
     * @param x_ 起始X坐标（世界坐标）
     * @param y_ 起始Y坐标（世界坐标）
     * @param w_ 结束X坐标（世界坐标）
     * @param h_ 结束Y坐标（世界坐标）
     * @return 轮廓点向量
     * @details 1. 将世界坐标转换为网格坐标（加上中心偏移）
     * 2. 遍历指定区域内的每个格子
     * 3. 如果格子有碰撞，则检查其四个角是否为轮廓点
     * 4. 返回轮廓点向量
     * @note 返回所有轮廓点，比轻量级轮廓更完整
     */
    std::vector<MapOutline> MapStatic::GetOutline(int x_, int y_, int w_, int h_)
    {
        x_ += centrality.x;
        y_ += centrality.y;
        w_ += centrality.x;
        h_ += centrality.y;

        std::vector<MapOutline> Outline;

        for (int x = x_; x < w_; ++x)
        {
            for (int y = y_; y < h_; ++y)
            {
                if (!GetCollision(x, y))
                {
                    continue;
                }

                // 左上角
                if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1) || !GetCollision(x - 1, y - 1))
                {
                    Outline.push_back({Vec2_{x, y}, Vec2_{-1, -1}, at(x, y).FrictionFactor});
                }
                // 左下角
                if (!GetCollision(x, y + 1))
                {
                    Outline.push_back({Vec2_{x, y + 1}, Vec2_{-1, 1}, at(x, y).FrictionFactor});
                }
                // 右上角
                if (!(GetCollision(x + 1, y - 1) || GetCollision(x + 1, y)))
                {
                    Outline.push_back({Vec2_{x + 1, y}, Vec2_{1, -1}, at(x, y).FrictionFactor});
                }
                // 右下角
                if (!(GetCollision(x + 1, y) || GetCollision(x + 1, y + 1) || GetCollision(x, y + 1)))
                {
                    Outline.push_back({Vec2_{x + 1, y + 1}, Vec2_{1, 1}, at(x, y).FrictionFactor});
                }
            }
        }
        return Outline;
    }

}